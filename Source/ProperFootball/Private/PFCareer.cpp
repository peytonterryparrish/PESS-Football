#include "PFCareer.h"
#include "PFFlow.h"
#include "PFGame.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"

namespace
{
	const FLinearColor Gold(1, 0.85f, 0.3f), Pale(0.75f, 0.8f, 0.95f), Dim(0.45f, 0.48f, 0.6f), Good(0.45f, 1.f, 0.55f), Bad(1.f, 0.4f, 0.35f), Blue(0.15f, 0.3f, 0.9f);
	const TCHAR* RoleShort(EPFRole R) { return R == EPFRole::GK ? TEXT("GK") : R == EPFRole::DEF ? TEXT("DF") : R == EPFRole::MID ? TEXT("MF") : TEXT("FW"); }
	const TCHAR* SLOT = TEXT("career");
}

UPFCareer* UPFCareer::Get(const UObject* Ctx)
{
	const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
	UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UPFCareer>() : nullptr;
}

UPFFlow* UPFCareer::F() const { return UPFFlow::Get(this); }

void UPFCareer::Enter() { Back.Reset(); Screen = EPFCScreen::Entry; Sel = 0; }

void UPFCareer::Go(EPFCScreen S, bool bPush)
{
	if (bPush && S != Screen) Back.Add(Screen);
	Screen = S; Sel = 0; Scroll = 0;
}

void UPFCareer::Pop()
{
	if (Back.Num()) { Screen = Back.Pop(); Sel = 0; Scroll = 0; }
	else if (UPFFlow* Fl = F()) Fl->Go(EPFScreen::Main);
}

void UPFCareer::Msg(const FString& Text, EPFCScreen Then) { Message = Text; AfterMessage = Then; Screen = EPFCScreen::Message; }

FString UPFCareer::Money(int64 V) const
{
	const bool bNeg = V < 0; V = FMath::Abs(V);
	FString S = V >= 1000000 ? FString::Printf(TEXT("%.1fM"), V / 1e6) : V >= 1000 ? FString::Printf(TEXT("%lldK"), V / 1000) : FString::Printf(TEXT("%lld"), V);
	return (bNeg ? TEXT("-") : TEXT("")) + FString(TEXT("£")) + S;
}

FString UPFCareer::Stars(int32 Ovr) const
{
	const int32 N = FMath::Clamp((Ovr - 30) / 12 + 1, 1, 5);
	FString S; for (int32 i = 0; i < 5; ++i) S += i < N ? TEXT("*") : TEXT("-");
	return S;
}

TArray<FString> UPFCareer::HubItems() const
{
	return { TEXT("NEXT MATCH"), TEXT("SQUAD"), TEXT("TACTICS"), TEXT("LEAGUE TABLES"), TEXT("FIXTURES"), TEXT("TRANSFERS"), TEXT("YOUTH ACADEMY"), TEXT("SCOUTING"), TEXT("CLUB & BOARD"), TEXT("CAREER HISTORY"), TEXT("SAVE GAME"), TEXT("QUIT TO MENU") };
}

TArray<int32> UPFCareer::SquadList(bool bAcademy) const
{
	if (Sim.S.UserClub < 0) return {};
	TArray<int32> L = bAcademy ? Sim.S.Clubs[Sim.S.UserClub].Academy : Sim.S.Clubs[Sim.S.UserClub].Squad;
	L.Sort([this](int32 A, int32 B) { const FPFPerson* X = Sim.Person(A); const FPFPerson* Y = Sim.Person(B); if (X->Role != Y->Role) return X->Role < Y->Role; return X->Ovr() > Y->Ovr(); });
	return L;
}

TArray<int32> UPFCareer::TransferList() const
{
	TArray<int32> L;
	for (int32 i = 0; i < Sim.S.Clubs.Num(); ++i)
	{
		if (i == Sim.S.UserClub || Sim.S.Clubs[i].Division != TransferDiv) continue;
		for (int32 Id : Sim.S.Clubs[i].Squad) { const FPFPerson* P = Sim.Person(Id); if (P && (TransferRole == 0 || (int32)P->Role == TransferRole - 1)) L.Add(Id); }
	}
	if (TransferDiv == 5)   // free agents
		for (const FPFPerson& P : Sim.S.People) if (P.Club < 0 && !P.bScoutedOnly && (TransferRole == 0 || (int32)P.Role == TransferRole - 1)) L.Add(P.Id);
	L.Sort([this](int32 A, int32 B) { const FPFPerson* X = Sim.Person(A); const FPFPerson* Y = Sim.Person(B);
		if (TransferSort == 1) return X->Age < Y->Age; if (TransferSort == 2) return Sim.Value(*X) < Sim.Value(*Y); return X->Ovr() > Y->Ovr(); });
	if (L.Num() > 120) L.SetNum(120);
	return L;
}

// ------------------------------------------------------------------------------------------------ matches
void UPFCareer::PlayUserMatch(int32 Mode)
{
	const int32 Fi = Sim.UserFixture();
	if (Fi < 0) { AdvanceDay(); return; }
	PendingFixture = Fi;
	if (Mode == 2)
	{
		Sim.QuickSim(Sim.S.Fixtures[Fi]);
		AdvanceDay();
		return;
	}
	FPFMatchSetup Setup = Sim.MatchSetup(Sim.S.Fixtures[Fi], Mode == 0);
	static const float Len[] = { 90.f, 150.f, 240.f, 300.f };
	Setup.HalfSeconds = Mode == 0 ? Len[HalfLenSetting % 4] : 240.f;
	bManage = Mode == 1; bYouthMatch = false;
	if (UPFFlow* Fl = F())
	{
		Fl->bManageView = bManage;
		Fl->StartMatch(Setup, [this](const FPFMatch& M) { OnMatchDone(M, false); });
	}
}

void UPFCareer::OnMatchDone(const FPFMatch& M, bool bYouth)
{
	UPFFlow* Fl = F();
	if (Fl) { Fl->bManageView = false; Fl->Go(EPFScreen::Career); }
	if (bYouth)
	{
		Screen = EPFCScreen::Prospects; Sel = 0;
		Message = TEXT("Every kid from that match is on your list. Sign the ones who stood out.");
		return;
	}
	if (Sim.S.Fixtures.IsValidIndex(PendingFixture) && !Sim.S.Fixtures[PendingFixture].Played())
	{
		FPFFixture& Fx = Sim.S.Fixtures[PendingFixture];
		int32 PW = -1;
		if (Fx.Comp >= 10 && M.Score[0] == M.Score[1]) PW = Sim.Rng.Unit() < 0.5f ? Fx.Home : Fx.Away;   // shoot-out (simulated)
		Sim.ApplyResult(Fx, M.Score[0], M.Score[1], PW);
	}
	AdvanceDay();
}

void UPFCareer::AdvanceDay()
{
	const int32 Before = Sim.S.Season;
	const bool bWasEmployed = !Sim.S.bUnemployed;
	Sim.PlayMatchday();
	Sim.Save(SLOT);
	if (Sim.S.Season != Before)
	{
		Screen = EPFCScreen::Offers; Back.Reset(); Sel = 0;
		return;
	}
	if (bWasEmployed && Sim.S.bUnemployed) { Screen = EPFCScreen::Offers; Back.Reset(); Sel = 0; return; }
	Screen = EPFCScreen::Hub; Back.Reset(); Sel = 0;
}

// ------------------------------------------------------------------------------------------------ input
void UPFCareer::Tick(float Dt, const FPFPadState& Pad)
{
	UPFFlow* Fl = F();
	if (!Fl) return;
	auto Nav = [&](int32 N) { if (N <= 0) return; if (Pad.pDown) { Sel = (Sel + 1) % N; Fl->PlaySfx(TEXT("ui_move"), 0.5f); } if (Pad.pUp) { Sel = (Sel - 1 + N) % N; Fl->PlaySfx(TEXT("ui_move"), 0.5f); } };
	if (Pad.pA) Fl->PlaySfx(TEXT("ui_select"), 0.6f);
	if (Pad.pB) Fl->PlaySfx(TEXT("ui_back"), 0.6f);
	FPFCareerState& S = Sim.S;
	switch (Screen)
	{
	case EPFCScreen::Entry:
	{
		const bool bHasSave = FPFCareerSim::SaveExists(SLOT);
		const int32 N = bHasSave ? 3 : 2;
		Nav(N);
		if (Pad.pB) { Fl->Go(EPFScreen::Main); return; }
		if (Pad.pA)
		{
			const int32 Choice = bHasSave ? Sel : Sel + 1;
			if (Choice == 0 && Sim.Load(SLOT)) { bLoaded = true; Back.Reset(); Screen = S.Phase == TEXT("Offers") || S.bUnemployed ? EPFCScreen::Offers : EPFCScreen::Hub; Sel = 0; }
			else if (Choice == 1)
			{
				Sim.NewCareer(FPlatformTime::Cycles64(), PFWorld::RandomName(Sim.Rng, true));
				bLoaded = true; Back.Reset(); Screen = EPFCScreen::Appointed; Sim.Save(SLOT);
			}
			else if (Choice == 2) Fl->Go(EPFScreen::Main);
		}
		return;
	}
	case EPFCScreen::Appointed:
		if (Pad.pA || Pad.pStart) { Screen = EPFCScreen::Hub; Sel = 0; }
		return;
	case EPFCScreen::Message:
		if (Pad.pA || Pad.pB) { Screen = AfterMessage; Sel = 0; }
		return;
	case EPFCScreen::Hub:
	{
		const TArray<FString> Items = HubItems();
		Nav(Items.Num());
		if (Pad.pB) return;
		if (!Pad.pA) return;
		switch (Sel)
		{
		case 0: if (S.bUnemployed) Go(EPFCScreen::Offers); else if (Sim.UserFixture() < 0) { AdvanceDay(); } else Go(EPFCScreen::MatchChoice); break;
		case 1: Go(EPFCScreen::Squad); break;
		case 2: Go(EPFCScreen::Tactics); break;
		case 3: TableDiv = S.bUnemployed ? 3 : S.Clubs[S.UserClub].Division; Go(EPFCScreen::Table); break;
		case 4: Go(EPFCScreen::Fixtures); break;
		case 5: TransferDiv = FMath::Min(4, S.Clubs[S.UserClub].Division + 1); Go(EPFCScreen::Transfers); break;
		case 6: Go(EPFCScreen::Academy); break;
		case 7: Go(EPFCScreen::Scouting); break;
		case 8: Go(EPFCScreen::Club); break;
		case 9: Go(EPFCScreen::History); break;
		case 10: Msg(Sim.Save(SLOT) ? TEXT("Career saved.") : TEXT("Could not save.")); break;
		case 11: Sim.Save(SLOT); Fl->Go(EPFScreen::Main); break;
		}
		return;
	}
	case EPFCScreen::MatchChoice:
		Nav(4);
		if (Pad.pLeft || Pad.pRight) HalfLenSetting = (HalfLenSetting + (Pad.pLeft ? 3 : 1)) % 4;
		if (Pad.pB) { Pop(); return; }
		if (Pad.pA) { if (Sel == 3) Pop(); else PlayUserMatch(Sel); }
		return;
	case EPFCScreen::Squad: case EPFCScreen::Academy:
	{
		const TArray<int32> L = SquadList(Screen == EPFCScreen::Academy);
		Nav(L.Num());
		if (Pad.pB) { Pop(); return; }
		if (Pad.pA && L.IsValidIndex(Sel)) { SelPerson = L[Sel]; Go(EPFCScreen::PlayerCard); }
		return;
	}
	case EPFCScreen::PlayerCard:
	{
		FPFPerson* P = Sim.Person(SelPerson);
		if (!P) { Pop(); return; }
		TArray<FString> Acts;
		if (P->bAcademy) { Acts = { TEXT("PROMOTE TO FIRST TEAM"), TEXT("RELEASE"), TEXT("BACK") }; }
		else { Acts = { P->bListed ? TEXT("TAKE OFF TRANSFER LIST") : TEXT("TRANSFER LIST"), TEXT("ACCEPT BEST OFFER"), TEXT("RELEASE"), TEXT("BACK") }; }
		Nav(Acts.Num());
		if (Pad.pB) { Pop(); return; }
		if (!Pad.pA) return;
		const FString A = Acts[Sel];
		if (A == TEXT("BACK")) Pop();
		else if (A == TEXT("PROMOTE TO FIRST TEAM")) { Sim.PromoteYouth(P->Id); Msg(FString::Printf(TEXT("%s moves up to the first team."), *P->Name), EPFCScreen::Academy); Back.Pop(); }
		else if (A == TEXT("RELEASE")) { const FString N = P->Name; Sim.ReleasePlayer(P->Id); Msg(FString::Printf(TEXT("%s has been released."), *N), EPFCScreen::Hub); Back.Reset(); }
		else if (A.Contains(TEXT("TRANSFER LIST"))) { P->bListed = !P->bListed; }
		else if (A == TEXT("ACCEPT BEST OFFER"))
		{
			bool bAny = false; for (const FPFBid& B : S.Bids) if (B.Person == P->Id && B.Status == 0 && !B.bByUser) bAny = true;
			if (!bAny) Msg(TEXT("No offers for him yet. Put him on the transfer list."), EPFCScreen::PlayerCard);
			else { Sim.SellToBest(P->Id); Msg(TEXT("Deal done."), EPFCScreen::Hub); Back.Reset(); }
		}
		return;
	}
	case EPFCScreen::Tactics:
	{
		Nav(3);
		if (Pad.pB) { Pop(); return; }
		FPFCareerClub& U = Sim.UserClub();
		static const TCHAR* Forms[] = { TEXT("4-4-2"), TEXT("4-3-3"), TEXT("5-3-2") };
		static const TCHAR* Train[] = { TEXT("Balanced"), TEXT("Youth"), TEXT("Fitness") };
		auto Cycle = [](const FString& Cur, const TCHAR* const* Arr, int32 N, int32 D) { int32 I = 0; for (int32 i = 0; i < N; ++i) if (Cur == Arr[i]) I = i; return FString(Arr[(I + D + N) % N]); };
		const int32 D = Pad.pLeft ? -1 : (Pad.pRight || Pad.pA) ? 1 : 0;
		if (D == 0) return;
		if (Sel == 0) U.Formation = Cycle(U.Formation, Forms, 3, D);
		if (Sel == 1) S.Training = Cycle(S.Training, Train, 3, D);
		if (Sel == 2) HalfLenSetting = (HalfLenSetting + D + 4) % 4;
		return;
	}
	case EPFCScreen::Table:
		if (Pad.pLeft) TableDiv = (TableDiv + 4) % 5;
		if (Pad.pRight) TableDiv = (TableDiv + 1) % 5;
		if (Pad.pDown) Scroll = FMath::Min(Scroll + 1, 10);
		if (Pad.pUp) Scroll = FMath::Max(Scroll - 1, 0);
		if (Pad.pB || Pad.pA) Pop();
		return;
	case EPFCScreen::Fixtures: case EPFCScreen::Club: case EPFCScreen::History:
		if (Pad.pDown) Scroll++;
		if (Pad.pUp) Scroll = FMath::Max(0, Scroll - 1);
		if (Pad.pB || Pad.pA) Pop();
		return;
	case EPFCScreen::Transfers:
	{
		const TArray<int32> L = TransferList();
		Nav(L.Num());
		if (Pad.pLeft) { TransferDiv = (TransferDiv + 5) % 6; Sel = 0; }
		if (Pad.pRight) { TransferDiv = (TransferDiv + 1) % 6; Sel = 0; }
		if (Pad.pY) { TransferRole = (TransferRole + 1) % 5; Sel = 0; }
		if (Pad.pX) { TransferSort = (TransferSort + 1) % 3; Sel = 0; }
		if (Pad.pB) { Pop(); return; }
		if (Pad.pA && L.IsValidIndex(Sel)) { SelPerson = L[Sel]; BidFeeStep = 0; BidWageStep = 0; Go(EPFCScreen::TransferTarget); }
		return;
	}
	case EPFCScreen::TransferTarget:
	{
		FPFPerson* P = Sim.Person(SelPerson);
		if (!P) { Pop(); return; }
		Nav(3);
		if (Pad.pB) { Pop(); return; }
		if (Sel == 0 && (Pad.pLeft || Pad.pRight)) BidFeeStep += Pad.pLeft ? -1 : 1;
		if (Sel == 1 && (Pad.pLeft || Pad.pRight)) BidWageStep += Pad.pLeft ? -1 : 1;
		if (Pad.pA && Sel == 2)
		{
			const int64 Fee = P->Club < 0 ? 0 : FMath::Max<int64>(0, int64(Sim.Value(*P) * (1.f + 0.1f * BidFeeStep)));
			const int32 Wage = FMath::Max(100, int32(Sim.WageDemand(*P, S.UserClub) * (1.f + 0.1f * BidWageStep)));
			Msg(Sim.MakeBid(P->Id, Fee, Wage), EPFCScreen::Transfers);
			Back.Pop();
		}
		return;
	}
	case EPFCScreen::Scouting:
	{
		const int32 N = S.Scouts.Num() + 3;   // scouts, region picker, hire, prospects
		Nav(N);
		if (Pad.pB) { Pop(); return; }
		if (Sel == S.Scouts.Num() && (Pad.pLeft || Pad.pRight)) RegionSel = (RegionSel + (Pad.pLeft ? -1 : 1) + (int32)EPFRegion::Count) % (int32)EPFRegion::Count;
		if (!Pad.pA) return;
		if (Sel < S.Scouts.Num()) { Sim.SendScout(Sel, (EPFRegion)RegionSel); Msg(FString::Printf(TEXT("%s is off to %s for four weeks."), *S.Scouts[Sel].Name, PFRegionName((EPFRegion)RegionSel)), EPFCScreen::Scouting); Back.Pop(); }
		else if (Sel == S.Scouts.Num())
		{
			// watch a youth match in the selected region: every kid in it can be signed afterwards
			FPFMatchSetup M = Sim.YouthMatchSetup((EPFRegion)RegionSel, FPlatformTime::Cycles64());
			for (int32 t = 0; t < 2; ++t) for (const FPFPlayerSetup& PS : (t == 0 ? M.Home : M.Away).XI)
				for (const FPFPerson& P : S.People) if (P.Name == PS.Name && P.Club < 0 && P.Region == (EPFRegion)RegionSel && !S.Prospects.Contains(P.Id)) { S.Prospects.Add(P.Id); break; }
			bYouthMatch = true;
			Fl->bManageView = false;
			Fl->StartMatch(M, [this](const FPFMatch& Mm) { OnMatchDone(Mm, true); });
		}
		else if (Sel == S.Scouts.Num() + 1) { if (S.Scouts.Num() < 4) { Sim.HireScout(); Msg(FString::Printf(TEXT("%s joins as a scout."), *S.Scouts.Last().Name), EPFCScreen::Scouting); Back.Pop(); } }
		else Go(EPFCScreen::Prospects);
		return;
	}
	case EPFCScreen::Prospects:
	{
		TArray<int32> L; for (int32 Id : S.Prospects) if (const FPFPerson* P = Sim.Person(Id)) if (P->Club < 0) L.Add(Id);
		Nav(L.Num());
		if (Pad.pB) { Message.Reset(); Pop(); return; }
		if (Pad.pA && L.IsValidIndex(Sel)) { Msg(Sim.SignProspect(L[Sel]), EPFCScreen::Prospects); }
		return;
	}
	case EPFCScreen::Offers:
	{
		const int32 N = S.Offers.Num() + (S.bUnemployed ? 0 : 1);
		Nav(FMath::Max(1, N));
		if (!Pad.pA) return;
		if (Sel < S.Offers.Num()) Sim.AcceptOffer(Sel);
		else Sim.AcceptOffer(-1);   // stay
		if (S.bUnemployed && !S.Offers.Num()) return;
		Sim.Save(SLOT);
		Back.Reset(); Screen = EPFCScreen::Appointed; Sel = 0;
		return;
	}
	default: if (Pad.pB) Pop(); return;
	}
}

// ------------------------------------------------------------------------------------------------ drawing
void UPFCareer::DrawHeader(UCanvas* C, const FString& Title)
{
	UPFFlow* Fl = F(); const float U = Fl->U;
	Fl->Box(C, 0, 0, C->ClipX, 150 * U, FLinearColor(0.02f, 0.03f, 0.12f, 0.92f));
	Fl->Box(C, 0, 146 * U, C->ClipX, 4 * U, FLinearColor(0.9f, 0.15f, 0.2f, 1));
	if (Sim.S.UserClub >= 0 && !Sim.S.bUnemployed)
	{
		const FPFCareerClub& Cl = Sim.UserClub();
		if (!Cl.Badge.IsEmpty()) Fl->Tex(C, Cl.Badge, 30 * U, 10 * U, 128 * U, 128 * U);
		else { Fl->Box(C, 40 * U, 20 * U, 108 * U, 108 * U, Cl.Kits.Home.Shirt); Fl->Text(C, Cl.Short, 94 * U, 55 * U, 0.9f, FLinearColor::White, EPFAlign::Center); }
		Fl->Text(C, Cl.Name.ToUpper(), 180 * U, 22 * U, 1.1f);
		Fl->Text(C, FString::Printf(TEXT("%s   SEASON %d/%02d   WEEK %d"), *Sim.DivisionName(Cl.Division).ToUpper(), Sim.S.Season, (Sim.S.Season + 1) % 100, Sim.S.Matchday + 1), 180 * U, 82 * U, 0.75f, Pale, EPFAlign::Left, true);
	}
	else Fl->Text(C, FString::Printf(TEXT("%s - OUT OF WORK"), *Sim.S.ManagerName.ToUpper()), 40 * U, 50 * U, 1.1f);
	Fl->Text(C, Title, C->ClipX - 40 * U, 50 * U, 1.2f, Gold, EPFAlign::Right);
}

void UPFCareer::DrawList(UCanvas* C, const FString& Title, const TArray<FString>& Head, const TArray<float>& Cols, const TArray<TArray<FString>>& Rows, const TArray<FLinearColor>& RowCol, float Top)
{
	UPFFlow* Fl = F(); const float U = Fl->U;
	const float X = 60 * U, Y = Top * U, W = C->ClipX - 120 * U, RowH = 44 * U;
	const int32 Visible = FMath::Max(4, int32((C->ClipY - Y - 120 * U) / RowH));
	if (Sel >= 0 && Sel < Scroll) Scroll = Sel;
	if (Sel >= Scroll + Visible) Scroll = Sel - Visible + 1;
	Scroll = FMath::Clamp(Scroll, 0, FMath::Max(0, Rows.Num() - Visible));
	Fl->Box(C, X - 16 * U, Y - 70 * U, W + 32 * U, (Visible + 1) * RowH + 90 * U, FLinearColor(0.01f, 0.02f, 0.08f, 0.85f));
	Fl->Text(C, Title, X, Y - 62 * U, 1.f, Gold);
	for (int32 c = 0; c < Head.Num(); ++c) Fl->Text(C, Head[c], X + Cols[c] * U, Y - 16 * U, 0.65f, Dim, EPFAlign::Left, true);
	for (int32 r = Scroll; r < Rows.Num() && r < Scroll + Visible; ++r)
	{
		const float RY = Y + (r - Scroll + 0.5f) * RowH;
		if (r == Sel) Fl->Box(C, X - 8 * U, RY - 4 * U, W + 16 * U, RowH - 4 * U, FLinearColor(Blue.R, Blue.G, Blue.B, 0.55f));
		const FLinearColor Col = RowCol.IsValidIndex(r) ? RowCol[r] : FLinearColor::White;
		for (int32 c = 0; c < Rows[r].Num() && c < Cols.Num(); ++c) Fl->Text(C, Rows[r][c], X + Cols[c] * U, RY, 0.72f, r == Sel ? FLinearColor::White : Col, EPFAlign::Left, true);
	}
	if (!Rows.Num()) Fl->Text(C, TEXT("NOTHING HERE YET"), X, Y + 20 * U, 0.8f, Dim, EPFAlign::Left, true);
}

void UPFCareer::DrawPersonCard(UCanvas* C, const FPFPerson& P, bool bHidden, float X, float Y)
{
	UPFFlow* Fl = F(); const float U = Fl->U;
	Fl->Box(C, X, Y, 760 * U, 640 * U, FLinearColor(0.01f, 0.02f, 0.08f, 0.88f));
	Fl->Text(C, P.Name.ToUpper(), X + 30 * U, Y + 24 * U, 1.1f);
	Fl->Text(C, FString::Printf(TEXT("%s   AGE %d   %s"), RoleShort(P.Role), P.Age, PFRegionName(P.Region)), X + 30 * U, Y + 84 * U, 0.75f, Pale, EPFAlign::Left, true);
	if (bHidden)
	{
		Fl->Text(C, TEXT("STATS UNKNOWN"), X + 30 * U, Y + 160 * U, 1.f, Gold);
		Fl->Text(C, TEXT("WATCH HIM PLAY TO JUDGE HIS TALENT."), X + 30 * U, Y + 220 * U, 0.75f, Pale, EPFAlign::Left, true);
		return;
	}
	const FPFStats& St = P.Stats;
	const TPair<const TCHAR*, int32> Rows[] = { {TEXT("PACE"), St.Pace}, {TEXT("ACCELERATION"), St.Accel}, {TEXT("AGILITY"), St.Agility}, {TEXT("BALL CONTROL"), St.Control},
		{TEXT("PASSING"), St.Passing}, {TEXT("SHOOTING"), St.Shooting}, {TEXT("HEADING"), St.Heading}, {TEXT("TACKLING"), St.Tackling}, {TEXT("REACTIONS"), St.Reactions}, {TEXT("KEEPING"), St.Keeping} };
	for (int32 i = 0; i < 10; ++i)
	{
		const float RY = Y + 150 * U + i * 44 * U;
		Fl->Text(C, Rows[i].Key, X + 30 * U, RY, 0.7f, Pale, EPFAlign::Left, true);
		Fl->Box(C, X + 330 * U, RY + 6 * U, 300 * U, 20 * U, FLinearColor(0, 0, 0, 0.6f));
		const float T = Rows[i].Value / 99.f;
		Fl->Box(C, X + 330 * U, RY + 6 * U, 300 * U * T, 20 * U, FLinearColor::LerpUsingHSV(FLinearColor(1, 0.2f, 0.15f), FLinearColor(0.3f, 1, 0.4f), T));
		Fl->Text(C, FString::FromInt(Rows[i].Value), X + 700 * U, RY, 0.7f, FLinearColor::White, EPFAlign::Right, true);
	}
	Fl->Text(C, FString::Printf(TEXT("OVERALL %d    VALUE %s    WAGE %s/WK    CONTRACT %dY"), P.Ovr(), *Money(Sim.Value(P)), *Money(P.Wage), P.ContractYears), X + 30 * U, Y + 600 * U, 0.65f, Gold, EPFAlign::Left, true);
}

void UPFCareer::DrawHub(UCanvas* C)
{
	UPFFlow* Fl = F(); const float U = Fl->U;
	const FPFCareerState& S = Sim.S;
	DrawHeader(C, TEXT("OFFICE"));
	const TArray<FString> Items = HubItems();
	TArray<FPFMenuItem> MI; for (const FString& I : Items) MI.Add({ I });
	Fl->DrawMenu(C, S.ManagerName.ToUpper(), MI, Sel, 90 * U, 300 * U, 560 * U);
	// right: next fixture, board, finances, news
	const float RX = 760 * U, RW = C->ClipX - RX - 60 * U;
	Fl->Box(C, RX, 220 * U, RW, 250 * U, FLinearColor(0.01f, 0.02f, 0.08f, 0.85f));
	const int32 Fi = Sim.UserFixture();
	if (Fi >= 0)
	{
		const FPFFixture& Fx = S.Fixtures[Fi];
		const FPFCareerClub& H = S.Clubs[Fx.Home]; const FPFCareerClub& A = S.Clubs[Fx.Away];
		Fl->Text(C, FString::Printf(TEXT("NEXT: %s"), *Sim.CompName(Fx.Comp).ToUpper()), RX + 24 * U, 236 * U, 0.75f, Gold, EPFAlign::Left, true);
		if (!H.Badge.IsEmpty()) Fl->Tex(C, H.Badge, RX + 24 * U, 290 * U, 150 * U, 150 * U);
		if (!A.Badge.IsEmpty()) Fl->Tex(C, A.Badge, RX + RW - 174 * U, 290 * U, 150 * U, 150 * U);
		Fl->Text(C, H.Name, RX + RW * 0.5f, 310 * U, 0.85f, FLinearColor::White, EPFAlign::Center, true);
		Fl->Text(C, TEXT("V"), RX + RW * 0.5f, 350 * U, 1.f, Gold, EPFAlign::Center);
		Fl->Text(C, A.Name, RX + RW * 0.5f, 400 * U, 0.85f, FLinearColor::White, EPFAlign::Center, true);
	}
	else Fl->Text(C, S.bUnemployed ? TEXT("YOU NEED A CLUB") : TEXT("NO MATCH THIS WEEK - CONTINUE"), RX + 24 * U, 300 * U, 0.85f, Pale, EPFAlign::Left, true);
	// board + money
	Fl->Box(C, RX, 490 * U, RW, 150 * U, FLinearColor(0.01f, 0.02f, 0.08f, 0.85f));
	Fl->Text(C, TEXT("BOARD CONFIDENCE"), RX + 24 * U, 506 * U, 0.7f, Pale, EPFAlign::Left, true);
	Fl->Box(C, RX + 24 * U, 548 * U, RW * 0.45f, 22 * U, FLinearColor(0, 0, 0, 0.6f));
	const float BC = S.BoardConfidence / 100.f;
	Fl->Box(C, RX + 24 * U, 548 * U, RW * 0.45f * BC, 22 * U, FLinearColor::LerpUsingHSV(Bad, Good, BC));
	if (S.UserClub >= 0)
	{
		const FPFCareerClub& Uc = S.Clubs[S.UserClub];
		Fl->Text(C, FString::Printf(TEXT("BUDGET %s"), *Money(Uc.TransferBudget)), RX + RW * 0.55f, 506 * U, 0.7f, Gold, EPFAlign::Left, true);
		Fl->Text(C, FString::Printf(TEXT("WAGES %s / %s"), *Money(Sim.SquadWages(S.UserClub)), *Money(Uc.WageBudget)), RX + RW * 0.55f, 548 * U, 0.7f, Pale, EPFAlign::Left, true);
		const TArray<FPFTableRow> T = Sim.Table(Uc.Division);
		for (int32 i = 0; i < T.Num(); ++i) if (T[i].Club == S.UserClub)
			Fl->Text(C, FString::Printf(TEXT("POSITION %d   PTS %d   EXPECTED %d"), i + 1, T[i].Pts, Sim.Expectation(S.UserClub)), RX + 24 * U, 590 * U, 0.7f, FLinearColor::White, EPFAlign::Left, true);
	}
	// news
	Fl->Box(C, RX, 660 * U, RW, C->ClipY - 720 * U, FLinearColor(0.01f, 0.02f, 0.08f, 0.85f));
	Fl->Text(C, TEXT("NEWS"), RX + 24 * U, 676 * U, 0.75f, Gold, EPFAlign::Left, true);
	for (int32 i = 0; i < S.News.Num() && i < 7; ++i) Fl->Text(C, S.News[i].Text, RX + 24 * U, 720 * U + i * 40 * U, 0.6f, i == 0 ? FLinearColor::White : Pale, EPFAlign::Left, true);
}

void UPFCareer::Draw(UCanvas* C, UMaterialInterface* SpriteMat)
{
	UPFFlow* Fl = F();
	if (!Fl) return;
	const float U = Fl->U;
	const FPFCareerState& S = Sim.S;
	Fl->Box(C, 0, 0, C->ClipX, C->ClipY, FLinearColor(0.01f, 0.015f, 0.05f, 1));
	if (!Fl->BgPath.IsEmpty()) { Fl->Tex(C, Fl->BgPath, 0, 0, C->ClipX, C->ClipY); Fl->Box(C, 0, 0, C->ClipX, C->ClipY, FLinearColor(0, 0, 0, 0.45f)); }
	const float CX = C->ClipX * 0.5f;
	switch (Screen)
	{
	case EPFCScreen::Entry:
	{
		TArray<FPFMenuItem> MI;
		if (FPFCareerSim::SaveExists(SLOT)) MI.Add({ TEXT("CONTINUE CAREER") });
		MI.Add({ TEXT("NEW CAREER") }); MI.Add({ TEXT("BACK") });
		Fl->Text(C, TEXT("CAREER"), CX, 140 * U, 2.4f, FLinearColor::White, EPFAlign::Center);
		Fl->Text(C, TEXT("START AT A RANDOM LEAGUE TWO CLUB. GET RELEGATED AND YOU ARE OUT."), CX, 260 * U, 0.75f, Pale, EPFAlign::Center, true);
		Fl->DrawMenu(C, TEXT("MANAGER"), MI, Sel, CX - 260 * U, C->ClipY * 0.45f, 520 * U);
		break;
	}
	case EPFCScreen::Appointed:
	{
		const FPFCareerClub& Cl = Sim.UserClub();
		Fl->Box(C, CX - 700 * U, 160 * U, 1400 * U, 740 * U, FLinearColor(0.01f, 0.02f, 0.08f, 0.88f));
		Fl->Text(C, TEXT("NEW MANAGER"), CX, 190 * U, 1.4f, Gold, EPFAlign::Center);
		if (!Cl.Badge.IsEmpty()) Fl->Tex(C, Cl.Badge, CX - 128 * U, 270 * U, 256 * U, 256 * U);
		Fl->Text(C, Cl.Name.ToUpper(), CX, 560 * U, 1.6f, FLinearColor::White, EPFAlign::Center);
		Fl->Text(C, FString::Printf(TEXT("%s  -  %s TAKES CHARGE"), *Sim.DivisionName(Cl.Division).ToUpper(), *S.ManagerName.ToUpper()), CX, 650 * U, 0.8f, Pale, EPFAlign::Center, true);
		Fl->Text(C, FString::Printf(TEXT("THE BOARD EXPECT A FINISH AROUND %d. STAY UP AT ALL COSTS."), Sim.Expectation(S.UserClub)), CX, 700 * U, 0.75f, Pale, EPFAlign::Center, true);
		if (FMath::Fmod(Fl->Time, 1.f) < 0.7f) Fl->Text(C, TEXT("PRESS A"), CX, 800 * U, 1.f, Gold, EPFAlign::Center);
		break;
	}
	case EPFCScreen::Message:
		Fl->Box(C, CX - 700 * U, C->ClipY * 0.38f, 1400 * U, 220 * U, FLinearColor(0.01f, 0.02f, 0.08f, 0.92f));
		Fl->Text(C, Message, CX, C->ClipY * 0.38f + 70 * U, 0.85f, FLinearColor::White, EPFAlign::Center, true);
		Fl->Text(C, TEXT("A  OK"), CX, C->ClipY * 0.38f + 150 * U, 0.7f, Gold, EPFAlign::Center, true);
		break;
	case EPFCScreen::Hub: DrawHub(C); break;
	case EPFCScreen::MatchChoice:
	{
		DrawHeader(C, TEXT("MATCH DAY"));
		static const TCHAR* L[] = { TEXT("1.5 MIN"), TEXT("2.5 MIN"), TEXT("4 MIN"), TEXT("5 MIN") };
		TArray<FPFMenuItem> MI;
		MI.Add({ TEXT("PLAY IT"), [this]() { static const TCHAR* LL[] = { TEXT("1.5 MIN"), TEXT("2.5 MIN"), TEXT("4 MIN"), TEXT("5 MIN") }; return FString::Printf(TEXT("HALVES %s"), LL[HalfLenSetting % 4]); }, [](int32) {} });
		MI.Add({ TEXT("MANAGE IT (HIGHLIGHTS)") }); MI.Add({ TEXT("INSTANT RESULT") }); MI.Add({ TEXT("BACK") });
		Fl->DrawMenu(C, TEXT("HOW DO YOU WANT IT?"), MI, Sel, CX - 420 * U, 360 * U, 840 * U);
		(void)L;
		break;
	}
	case EPFCScreen::Squad: case EPFCScreen::Academy:
	{
		const bool bAc = Screen == EPFCScreen::Academy;
		DrawHeader(C, bAc ? TEXT("YOUTH ACADEMY") : TEXT("SQUAD"));
		const TArray<int32> L = SquadList(bAc);
		TArray<TArray<FString>> Rows; TArray<FLinearColor> Col;
		const TArray<int32> XI = bAc ? TArray<int32>() : Sim.BestXI(S.UserClub);
		for (int32 Id : L)
		{
			const FPFPerson* P = Sim.Person(Id);
			Rows.Add({ FString::FromInt(P->Number), P->Name, RoleShort(P->Role), FString::FromInt(P->Age), FString::FromInt(P->Ovr()), Stars(P->Ovr()), Money(Sim.Value(*P)),
				FString::Printf(TEXT("%d/%d"), P->SeasonApps, P->SeasonGoals), P->InjuryWeeks > 0 ? FString::Printf(TEXT("INJ %dW"), P->InjuryWeeks) : (P->bListed ? TEXT("LISTED") : TEXT("")) });
			Col.Add(P->InjuryWeeks > 0 ? Bad : XI.Contains(Id) ? FLinearColor::White : Pale);
		}
		DrawList(C, bAc ? TEXT("ACADEMY  -  A: OPTIONS") : TEXT("FIRST TEAM  -  WHITE = STARTING XI  -  A: OPTIONS"), { TEXT("NO"), TEXT("NAME"), TEXT("POS"), TEXT("AGE"), TEXT("OVR"), TEXT("RATING"), TEXT("VALUE"), TEXT("APP/GLS"), TEXT("") },
			{ 0, 80, 520, 620, 720, 820, 1000, 1180, 1380 }, Rows, Col);
		break;
	}
	case EPFCScreen::PlayerCard:
	{
		const FPFPerson* P = Sim.Person(SelPerson);
		DrawHeader(C, TEXT("PLAYER"));
		if (!P) break;
		DrawPersonCard(C, *P, false, 80 * U, 200 * U);
		TArray<FPFMenuItem> MI;
		if (P->bAcademy) { MI.Add({ TEXT("PROMOTE TO FIRST TEAM") }); MI.Add({ TEXT("RELEASE") }); MI.Add({ TEXT("BACK") }); }
		else { MI.Add({ P->bListed ? TEXT("TAKE OFF TRANSFER LIST") : TEXT("TRANSFER LIST") }); MI.Add({ TEXT("ACCEPT BEST OFFER") }); MI.Add({ TEXT("RELEASE") }); MI.Add({ TEXT("BACK") }); }
		Fl->DrawMenu(C, TEXT("OPTIONS"), MI, Sel, 1000 * U, 320 * U, 700 * U);
		float Y = 600 * U;
		for (const FPFBid& B : S.Bids) if (B.Person == P->Id && B.Status == 0 && !B.bByUser)
		{ Fl->Text(C, FString::Printf(TEXT("%s OFFER %s"), *S.Clubs[B.FromClub].Name.ToUpper(), *Money(B.Fee)), 1000 * U, Y, 0.65f, Gold, EPFAlign::Left, true); Y += 40 * U; }
		break;
	}
	case EPFCScreen::Tactics:
	{
		DrawHeader(C, TEXT("TACTICS"));
		static const TCHAR* L[] = { TEXT("1.5 MIN"), TEXT("2.5 MIN"), TEXT("4 MIN"), TEXT("5 MIN") };
		const FString Form = Sim.UserClub().Formation, Train = S.Training, Len = L[HalfLenSetting % 4];
		TArray<FPFMenuItem> MI;
		MI.Add({ TEXT("FORMATION"), [Form]() { return Form; }, [](int32) {} });
		MI.Add({ TEXT("TRAINING FOCUS"), [Train]() { return Train.ToUpper(); }, [](int32) {} });
		MI.Add({ TEXT("HALF LENGTH (PLAY)"), [Len]() { return Len; }, [](int32) {} });
		Fl->DrawMenu(C, TEXT("SET UP"), MI, Sel, 90 * U, 320 * U, 760 * U);
		// formation board with the club's shirts
		const float BX = 980 * U, BY = 220 * U, BW = 820 * U, BH = 560 * U;
		Fl->Box(C, BX, BY, BW, BH, FLinearColor(0.08f, 0.35f, 0.12f, 0.95f));
		Fl->Box(C, BX + BW * 0.5f - 1, BY, 2, BH, FLinearColor(1, 1, 1, 0.5f));
		const TArray<FVector2D> Slots = PFWorld::FormationSlots(Form);
		const TArray<int32> XI = Sim.BestXI(S.UserClub);
		const FString Jersey = Sim.UserClub().ClubId.StartsWith(TEXT("C")) ? FString::Printf(TEXT("GameData/Jerseys/%s_home.png"), *Sim.UserClub().ClubId) : FString();
		for (int32 i = 0; i < Slots.Num(); ++i)
		{
			const float PX = BX + 60 * U + FMath::Min(1.f, Slots[i].X / 0.78f) * (BW - 120 * U), PY = BY + BH * 0.5f + Slots[i].Y * (BH * 0.42f);
			if (!Jersey.IsEmpty()) Fl->Tex(C, i == 0 ? FString::Printf(TEXT("GameData/Jerseys/%s_keeper1.png"), *Sim.UserClub().ClubId) : Jersey, PX - 36 * U, PY - 32 * U, 72 * U, 64 * U);
			else Fl->Box(C, PX - 20 * U, PY - 20 * U, 40 * U, 40 * U, Sim.UserClub().Kits.Home.Shirt);
			if (XI.IsValidIndex(i)) Fl->Text(C, Sim.Person(XI[i])->Name, PX, PY + 34 * U, 0.5f, FLinearColor::White, EPFAlign::Center, true);
		}
		Fl->Text(C, TEXT("YOUTH FOCUS GROWS THE ACADEMY FASTER. THE BEST XI IS PICKED FOR YOU."), 90 * U, C->ClipY - 90 * U, 0.65f, Pale, EPFAlign::Left, true);
		break;
	}
	case EPFCScreen::Table:
	{
		DrawHeader(C, TEXT("TABLES"));
		const TArray<FPFTableRow> T = Sim.Table(TableDiv);
		TArray<TArray<FString>> Rows; TArray<FLinearColor> Col;
		const int32 N = T.Num();
		for (int32 i = 0; i < N; ++i)
		{
			const FPFTableRow& R = T[i];
			Rows.Add({ FString::FromInt(i + 1), S.Clubs[R.Club].Name, FString::FromInt(R.P), FString::FromInt(R.W), FString::FromInt(R.D), FString::FromInt(R.L), FString::FromInt(R.GF), FString::FromInt(R.GA), FString::FromInt(R.GD()), FString::FromInt(R.Pts) });
			const bool bUp = TableDiv > 0 && i < (TableDiv == 4 ? 2 : TableDiv == 3 ? 4 : 3);
			const bool bDown = TableDiv < 4 && i >= N - (TableDiv == 3 ? 2 : TableDiv == 2 ? 4 : 3);
			Col.Add(R.Club == S.UserClub ? Gold : bUp ? Good : bDown ? Bad : Pale);
		}
		const int32 KeepSel = Sel; Sel = -1;
		DrawList(C, FString::Printf(TEXT("< %s >"), *Sim.DivisionName(TableDiv).ToUpper()), { TEXT("POS"), TEXT("CLUB"), TEXT("P"), TEXT("W"), TEXT("D"), TEXT("L"), TEXT("F"), TEXT("A"), TEXT("GD"), TEXT("PTS") },
			{ 0, 90, 760, 860, 960, 1060, 1160, 1260, 1360, 1480 }, Rows, Col, 230.f + 0);
		Sel = KeepSel;
		break;
	}
	case EPFCScreen::Fixtures:
	{
		DrawHeader(C, TEXT("FIXTURES"));
		TArray<TArray<FString>> Rows; TArray<FLinearColor> Col;
		for (const FPFFixture& Fx : S.Fixtures)
		{
			if (Fx.Home != S.UserClub && Fx.Away != S.UserClub) continue;
			const bool bHome = Fx.Home == S.UserClub;
			FString Res = TEXT("-");
			FLinearColor RC = Pale;
			if (Fx.Played())
			{
				Res = FString::Printf(TEXT("%d - %d%s"), Fx.HG, Fx.AG, Fx.bPens ? TEXT(" P") : TEXT(""));
				const int32 GF = bHome ? Fx.HG : Fx.AG, GA = bHome ? Fx.AG : Fx.HG;
				RC = GF > GA || (Fx.bPens && Fx.PenWinner == S.UserClub) ? Good : GF < GA || Fx.bPens ? Bad : FLinearColor::White;
			}
			Rows.Add({ FString::FromInt(Fx.Matchday + 1), Sim.CompName(Fx.Comp), bHome ? TEXT("H") : TEXT("A"), S.Clubs[bHome ? Fx.Away : Fx.Home].Name, Res });
			Col.Add(RC);
		}
		Rows.Sort([](const TArray<FString>& A, const TArray<FString>& B) { return FCString::Atoi(*A[0]) < FCString::Atoi(*B[0]); });
		Sel = Scroll;
		DrawList(C, TEXT("YOUR SEASON"), { TEXT("WK"), TEXT("COMPETITION"), TEXT(""), TEXT("OPPONENT"), TEXT("RESULT") }, { 0, 90, 600, 680, 1300 }, Rows, Col);
		Sel = 0;
		break;
	}
	case EPFCScreen::Transfers:
	{
		DrawHeader(C, TEXT("TRANSFERS"));
		const TArray<int32> L = TransferList();
		TArray<TArray<FString>> Rows; TArray<FLinearColor> Col;
		for (int32 Id : L)
		{
			const FPFPerson* P = Sim.Person(Id);
			Rows.Add({ P->Name, RoleShort(P->Role), FString::FromInt(P->Age), FString::FromInt(P->Ovr()), Stars(P->Ovr()), P->Club >= 0 ? S.Clubs[P->Club].Name : TEXT("FREE AGENT"), Money(Sim.Value(*P)), P->bListed ? TEXT("LISTED") : TEXT("") });
			Col.Add(P->bListed ? Good : Pale);
		}
		static const TCHAR* Roles[] = { TEXT("ALL"), TEXT("GK"), TEXT("DF"), TEXT("MF"), TEXT("FW") };
		static const TCHAR* Sorts[] = { TEXT("RATING"), TEXT("AGE"), TEXT("VALUE") };
		const FString Src = TransferDiv == 5 ? TEXT("FREE AGENTS") : Sim.DivisionName(TransferDiv).ToUpper();
		DrawList(C, FString::Printf(TEXT("< %s >  Y:%s  X:%s  %s"), *Src, Roles[TransferRole], Sorts[TransferSort], S.bWindowOpen ? TEXT("WINDOW OPEN") : TEXT("WINDOW SHUT")),
			{ TEXT("NAME"), TEXT("POS"), TEXT("AGE"), TEXT("OVR"), TEXT("RATING"), TEXT("CLUB"), TEXT("VALUE"), TEXT("") }, { 0, 420, 520, 620, 720, 900, 1450, 1640 }, Rows, Col);
		break;
	}
	case EPFCScreen::TransferTarget:
	{
		const FPFPerson* P = Sim.Person(SelPerson);
		DrawHeader(C, TEXT("MAKE A BID"));
		if (!P) break;
		DrawPersonCard(C, *P, false, 80 * U, 200 * U);
		const int64 Fee = P->Club < 0 ? 0 : FMath::Max<int64>(0, int64(Sim.Value(*P) * (1.f + 0.1f * BidFeeStep)));
		const int32 Wage = FMath::Max(100, int32(Sim.WageDemand(*P, S.UserClub) * (1.f + 0.1f * BidWageStep)));
		TArray<FPFMenuItem> MI;
		const FString FS = Money(Fee), WS = Money(Wage) + TEXT("/WK");
		MI.Add({ TEXT("FEE"), [FS]() { return FS; }, [](int32) {} });
		MI.Add({ TEXT("WAGE"), [WS]() { return WS; }, [](int32) {} });
		MI.Add({ TEXT("SUBMIT BID") });
		Fl->DrawMenu(C, P->Club >= 0 ? S.Clubs[P->Club].Name.ToUpper() : TEXT("FREE AGENT"), MI, Sel, 1000 * U, 320 * U, 760 * U);
		Fl->Text(C, FString::Printf(TEXT("YOUR BUDGET %s   WAGES %s / %s"), *Money(Sim.UserClub().TransferBudget), *Money(Sim.SquadWages(S.UserClub)), *Money(Sim.UserClub().WageBudget)), 1000 * U, 600 * U, 0.65f, Gold, EPFAlign::Left, true);
		float Y = 660 * U;
		for (const FPFBid& B : S.Bids) if (B.Person == P->Id && B.bByUser) { Fl->Text(C, B.Note.IsEmpty() ? TEXT("BID PENDING") : B.Note, 1000 * U, Y, 0.6f, Pale, EPFAlign::Left, true); Y += 36 * U; }
		break;
	}
	case EPFCScreen::Scouting:
	{
		DrawHeader(C, TEXT("SCOUTING"));
		TArray<FPFMenuItem> MI;
		for (const FPFScout& Sc : S.Scouts)
		{
			const FString V = Sc.WeeksLeft > 0 ? FString::Printf(TEXT("%s  %dW"), PFRegionName(Sc.Region), Sc.WeeksLeft) : FString(TEXT("IDLE - SEND"));
			MI.Add({ FString::Printf(TEXT("%s %s"), *Sc.Name.ToUpper(), *FString::ChrN(Sc.Quality, '*')), [V]() { return V; } });
		}
		const FString Reg = FString(PFRegionName((EPFRegion)RegionSel)).ToUpper();
		MI.Add({ TEXT("WATCH YOUTH MATCH"), [Reg]() { return Reg; }, [](int32) {} });
		MI.Add({ S.Scouts.Num() < 4 ? TEXT("HIRE A SCOUT") : TEXT("SCOUT TEAM FULL") });
		int32 NP = 0; for (int32 Id : S.Prospects) if (const FPFPerson* P = Sim.Person(Id)) if (P->Club < 0) NP++;
		MI.Add({ FString::Printf(TEXT("PROSPECTS (%d)"), NP) });
		Fl->DrawMenu(C, TEXT("SCOUT NETWORK"), MI, Sel, 90 * U, 320 * U, 1100 * U);
		Fl->Text(C, FString::Printf(TEXT("REGION FOR SCOUTS AND YOUTH MATCHES: < %s >  (LEFT/RIGHT ON 'WATCH YOUTH MATCH')"), *Reg), 90 * U, C->ClipY - 140 * U, 0.65f, Gold, EPFAlign::Left, true);
		Fl->Text(C, TEXT("YOU NEVER SEE A KID'S STATS. WATCH THE MATCH: THE TALENTED ONES ARE FASTER, SHARPER AND CLEANER ON THE BALL."), 90 * U, C->ClipY - 90 * U, 0.6f, Pale, EPFAlign::Left, true);
		break;
	}
	case EPFCScreen::Prospects:
	{
		DrawHeader(C, TEXT("PROSPECTS"));
		TArray<TArray<FString>> Rows; TArray<FLinearColor> Col;
		for (int32 Id : S.Prospects) if (const FPFPerson* P = Sim.Person(Id)) if (P->Club < 0)
		{ Rows.Add({ P->Name, RoleShort(P->Role), FString::FromInt(P->Age), PFRegionName(P->Region), TEXT("? ? ?") }); Col.Add(Pale); }
		DrawList(C, Message.IsEmpty() ? TEXT("A: SIGN FOR THE ACADEMY") : Message, { TEXT("NAME"), TEXT("POS"), TEXT("AGE"), TEXT("REGION"), TEXT("ABILITY") }, { 0, 520, 640, 760, 1300 }, Rows, Col);
		break;
	}
	case EPFCScreen::Club: case EPFCScreen::History:
	{
		DrawHeader(C, Screen == EPFCScreen::Club ? TEXT("CLUB") : TEXT("HISTORY"));
		TArray<TArray<FString>> Rows; TArray<FLinearColor> Col;
		if (Screen == EPFCScreen::Club && S.UserClub >= 0)
		{
			const FPFCareerClub& Uc = S.Clubs[S.UserClub];
			Rows.Add({ TEXT("MANAGER"), S.ManagerName }); Rows.Add({ TEXT("REPUTATION"), FString::Printf(TEXT("%d / 100"), S.Reputation) });
			Rows.Add({ TEXT("BOARD CONFIDENCE"), FString::Printf(TEXT("%d / 100"), S.BoardConfidence) }); Rows.Add({ TEXT("SEASONS AT CLUB"), FString::FromInt(S.SeasonsAtClub) });
			Rows.Add({ TEXT("TROPHIES"), FString::FromInt(S.Trophies) }); Rows.Add({ TEXT("BANK"), Money(Uc.Balance) }); Rows.Add({ TEXT("TRANSFER BUDGET"), Money(Uc.TransferBudget) });
			Rows.Add({ TEXT("WAGE BILL / BUDGET"), FString::Printf(TEXT("%s / %s"), *Money(Sim.SquadWages(S.UserClub)), *Money(Uc.WageBudget)) });
			Rows.Add({ TEXT("CLUB STATURE"), Stars(30 + Uc.Prestige * 12) }); Rows.Add({ TEXT("ACADEMY"), FString::Printf(TEXT("LEVEL %d  -  %d PLAYERS"), Uc.FacilityLevel, Uc.Academy.Num()) });
			Rows.Add({ TEXT("BOARD EXPECTS"), FString::Printf(TEXT("FINISH AROUND %d"), Sim.Expectation(S.UserClub)) });
		}
		else for (const FString& H : S.History) Rows.Add({ H });
		for (int32 i = 0; i < Rows.Num(); ++i) Col.Add(Pale);
		Sel = Scroll;
		DrawList(C, Screen == EPFCScreen::Club ? TEXT("THE CLUB") : TEXT("SEASON BY SEASON"), { TEXT(""), TEXT("") }, { 0, 700 }, Rows, Col);
		Sel = 0;
		break;
	}
	case EPFCScreen::Offers:
	{
		Fl->Box(C, CX - 760 * U, 120 * U, 1520 * U, 860 * U, FLinearColor(0.01f, 0.02f, 0.08f, 0.9f));
		Fl->Text(C, S.bUnemployed ? TEXT("OUT OF WORK") : TEXT("THE SUMMER"), CX, 150 * U, 1.5f, S.bUnemployed ? Bad : Gold, EPFAlign::Center);
		if (S.History.Num()) Fl->Text(C, S.History.Last(), CX, 240 * U, 0.75f, FLinearColor::White, EPFAlign::Center, true);
		for (int32 i = 0; i < 3 && i < S.News.Num(); ++i) Fl->Text(C, S.News[i].Text, CX, 290 * U + i * 40 * U, 0.6f, Pale, EPFAlign::Center, true);
		TArray<FPFMenuItem> MI;
		for (const FPFOffer& O : S.Offers)
		{
			const FPFCareerClub& Oc = S.Clubs[O.Club];
			const FString V = Sim.DivisionName(Oc.Division).ToUpper();
			MI.Add({ FString::Printf(TEXT("JOIN %s"), *Oc.Name.ToUpper()), [V]() { return V; } });
		}
		if (!S.bUnemployed) MI.Add({ FString::Printf(TEXT("STAY AT %s"), *Sim.UserClub().Name.ToUpper()) });
		Fl->DrawMenu(C, S.Offers.Num() ? TEXT("JOB OFFERS") : TEXT("NO OFFERS - CARRY ON"), MI, Sel, CX - 560 * U, 520 * U, 1120 * U);
		break;
	}
	default: break;
	}
}
