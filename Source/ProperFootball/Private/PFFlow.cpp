#include "PFFlow.h"
#include "PFGame.h"
#include "PFWorld.h"
#include "PFCareer.h"
#include "PFPresent.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "CanvasItem.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UnrealClient.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Sound/SoundBase.h"
#include "Components/AudioComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"

static void PFPassTests();   // v0.9.8 (RunFeelTest)
static void PFAttackTests();   // v0.10 (RunFeelTest, -pfattacktest)

// ------------------------------------------------------------------------------------------------ font
bool FPFFont::Load(const FString& Name)
{
	TSharedPtr<FJsonObject> J = PF::LoadJson(FString::Printf(TEXT("GameData/UI/font_%s.json"), *Name));
	if (!J.IsValid()) return false;
	Tex = PF::LoadTexture(FString::Printf(TEXT("GameData/UI/font_%s.png"), *Name));
	Line = J->GetIntegerField(TEXT("line"));
	Space = J->GetIntegerField(TEXT("space"));
	const TArray<TSharedPtr<FJsonValue>>& Sh = J->GetArrayField(TEXT("sheet"));
	Sheet = FVector2D(Sh[0]->AsNumber(), Sh[1]->AsNumber());
	for (const auto& KV : J->GetObjectField(TEXT("glyphs"))->Values)
	{
		const TArray<TSharedPtr<FJsonValue>>& A = KV.Value->AsArray();
		Glyphs.Add(FCString::Atoi(*KV.Key), { (int32)A[0]->AsNumber(), (int32)A[1]->AsNumber(), (int32)A[2]->AsNumber(), (int32)A[3]->AsNumber(), (int32)A[4]->AsNumber(), (int32)A[5]->AsNumber() });
	}
	return Tex != nullptr;
}

float FPFFont::Measure(const FString& S, float Scale) const
{
	float W = 0;
	for (TCHAR Ch : S)
	{
		if (Ch == ' ') { W += Space * Scale; continue; }
		const FGlyph* G = Glyphs.Find((int32)FChar::ToUpper(Ch) == Ch ? (int32)Ch : (int32)Ch);
		if (!G) G = Glyphs.Find((int32)FChar::ToUpper(Ch));
		W += (G ? G->Adv + 1 : Space) * Scale;
	}
	return W;
}

// ------------------------------------------------------------------------------------------------ subsystem
UPFFlow* UPFFlow::Get(const UObject* Ctx)
{
	const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
	UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UPFFlow>() : nullptr;
}

void UPFFlow::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FontScore.Load(TEXT("score"));
	FontSquad.Load(TEXT("squad"));
	if (FontScore.Tex) Keep.Add(FontScore.Tex);
	if (FontSquad.Tex) Keep.Add(FontSquad.Tex);
	PFWorld::Clubs();
	PickBackground(TEXT("England"));
	LoadOptions();
}

void UPFFlow::PickBackground(const FString& Collection)
{
	TSharedPtr<FJsonObject> J = PF::LoadJson(TEXT("GameData/UI/backgrounds.json"));
	if (!J.IsValid()) return;
	TArray<FString> Paths;
	for (const TSharedPtr<FJsonValue>& V : J->GetArrayField(TEXT("items")))
	{
		const TSharedPtr<FJsonObject>& O = V->AsObject();
		const FString Col = O->GetStringField(TEXT("collection"));
		if (Collection.IsEmpty() || Col.Contains(Collection) || (Collection == TEXT("England") && (Col.Contains(TEXT("Terrace")) || Col.Contains(TEXT("Floodlit")) || Col.Contains(TEXT("Broadcast_Arcade")))))
			Paths.Add(O->GetStringField(TEXT("path")));
	}
	if (Paths.Num()) BgPath = Paths[FMath::RandRange(0, Paths.Num() - 1)];
}

void UPFFlow::Begin(UWorld* World)
{
	WorldRef = World;
	if (!MatchActor || !IsValid(MatchActor))
	{
		MatchActor = World->SpawnActor<APFMatchActor>();
	}
	const TArray<FPFClub>& C = PFWorld::Clubs();
	if (C.Num() > 1) { FriendlyHome = FMath::RandRange(0, C.Num() - 1); FriendlyAway = (FriendlyHome + 1 + FMath::RandRange(0, C.Num() - 2)) % C.Num(); }
	// command line: -pfmatch jumps straight into a friendly (autotests, captures)
	{
		// -pfhome=<slug> -pfaway=<slug> pick the friendly's clubs (captures of a club's own sprites)
		FString HS, AS;
		if (FParse::Value(FCommandLine::Get(), TEXT("pfhome="), HS)) for (int32 i = 0; i < C.Num(); ++i) if (C[i].Slug == HS) FriendlyHome = i;
		if (FParse::Value(FCommandLine::Get(), TEXT("pfaway="), AS)) for (int32 i = 0; i < C.Num(); ++i) if (C[i].Slug == AS) FriendlyAway = i;
		FParse::Value(FCommandLine::Get(), TEXT("pfstage="), FriendlyStage);   // 0 3 pm, 1 evening, 2 floodlights
	}
	FParse::Value(FCommandLine::Get(), TEXT("pfshots="), ShotsLeft);
	FParse::Value(FCommandLine::Get(), TEXT("pfshotgap="), ShotGap);
	if (FParse::Param(FCommandLine::Get(), TEXT("pfwatch"))) FriendlySide = 2;
	if (FParse::Param(FCommandLine::Get(), TEXT("pfmanage"))) { FriendlySide = 2; bManageView = true; }
	int32 TestMatches = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("pfsimtest="), TestMatches)) { RunMatchTest(TestMatches); return; }
	if (FParse::Param(FCommandLine::Get(), TEXT("pffeeltest"))) { RunFeelTest(); return; }
	if (FParse::Param(FCommandLine::Get(), TEXT("pfattacktest"))) { PFAttackTests(); QuitAt = Time + 0.5f; return; }   // v0.10: attacking runs and the cross bar
	if (FParse::Param(FCommandLine::Get(), TEXT("pfpasstest"))) { PFPassTests(); QuitAt = Time + 0.5f; return; }   // v0.9.8: only the pad passing tests
	if (FParse::Param(FCommandLine::Get(), TEXT("pfsetpiecetest"))) { PFSetPieceTests(); QuitAt = Time + 0.5f; return; }   // v0.10: the pad's set pieces (PFSetPiece.cpp)
	int32 TestSeasons = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("pfcareertest="), TestSeasons)) { RunCareerTest(TestSeasons); return; }
	FString CScreen;
	if (FParse::Value(FCommandLine::Get(), TEXT("pfcareerscreen="), CScreen))
	{
		if (UPFCareer* Car = UPFCareer::Get(this))
		{
			Car->Sim.NewCareer(7, TEXT("Test Gaffer"));
			for (int32 i = 0; i < 6; ++i) Car->Sim.PlayMatchday();
			Car->Screen = (EPFCScreen)FCString::Atoi(*CScreen);
			if (Car->Screen == EPFCScreen::Table) Car->TableDiv = Car->Sim.UserClub().Division;
			if (Car->Screen == EPFCScreen::PlayerCard) Car->SelPerson = Car->Sim.UserClub().Squad[0];
			Screen = EPFScreen::Career;
		}
		return;
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("pfmatch"))) StartFriendly();
	else Go(EPFScreen::Title);
}

// v0.9.8 pad passing test (owner: "if I tap it, it's a very short pass to the closest player ... the longer I hold it, the
// further the ball will travel to a different player"; "wherever I'm aiming is where it actually passes"; "the through
// balls are atrocious"). The stick is aimed on screen at one team-mate (exactly, or a thumb's 6-7 degrees off), A is held
// for that man's distance on the power bar; did the ball go to him? Through balls: Y aimed at the space ahead of a
// forward, held for the lead; did he run onto it in stride, onside?
// how long a player holds the button for that distance (the bar: a tap, then 12-42 m / a 5-24 m lead over 0.12-0.45 s)
static float PFPassHoldFor(float Dist) { return Dist < FPFMatch::PassMinD + 1.f ? 0.05f : FPFMatch::PassTapT + (FPFMatch::PassFullT - FPFMatch::PassTapT) * FMath::Clamp((Dist - FPFMatch::PassMinD) / (FPFMatch::PassMaxD - FPFMatch::PassMinD), 0.f, 1.f); }
static float PFThroughHoldFor(float Lead) { return Lead < 6.5f ? 0.05f : FPFMatch::PassTapT + (FPFMatch::PassFullT - FPFMatch::PassTapT) * FMath::Clamp((Lead - FPFMatch::ThroughMinLead) / (FPFMatch::ThroughMaxLead - FPFMatch::ThroughMinLead), 0.f, 1.f); }
static FVector2D PFRotScreen(FVector2D S, float Deg) { const float R = FMath::DegreesToRadians(Deg), Cc = FMath::Cos(R), Ss = FMath::Sin(R); return FVector2D(S.X * Cc - S.Y * Ss, S.X * Ss + S.Y * Cc); }
// v0.10 attacking play and the cross bar (owner: "nobody's making runs ... X should have a power bar as well")
// PFATTACK: the pad's man has the ball with his side in its defending shape and the opponents live; how the team-mates
// move in the next 3 s (wide men up, strikers split, men ahead of the ball, runs). PFCROSS: from the wing, X tapped,
// held to the middle, held full - who it picks, how high it goes, where it comes down, who wins it.
static void PFAttackTests()
{
	const TArray<FPFClub>& C = PFWorld::Clubs();
	auto NewMatch = [&C](FPFMatch& M, uint64 Seed, int32 Ci)
	{
		FPFMatchSetup S; S.Seed = Seed;
		S.Home = PFWorld::FriendlyTeam(C[(Ci * 5) % C.Num()], false, 11 + Ci); S.Away = PFWorld::FriendlyTeam(C[(Ci * 7 + 3) % C.Num()], true, 13 + Ci);
		S.Home.bHuman = true; S.HalfSeconds = 150.f; S.StadiumSlug = C[0].Stadium;
		M.Init(S);
		{ FPFPadInput In; In.bPass = true; M.Tick(1.f / 120.f, In); }
		FPFPadInput In; for (int32 f = 0; f < 60; ++f) M.Tick(1.f / 120.f, In);
		for (FPFPlayer& Q : M.P) { Q.Act = EPFAct::None; Q.PendingKick = EPFKick::None; Q.NoTouch = 0; Q.RunT = 0; Q.BoostT = 0; Q.AtkRun = 0; Q.AtkRunCool = 0; }
	};
	auto GiveBall = [](FPFMatch& M, int32 Who, FVector2D At)
	{
		FPFPlayer& H = M.P[Who];
		H.Pos = At; H.Vel = FVector2D::ZeroVector; H.Facing = FVector2D(M.AttackSign(0), 0);
		M.Human = Who; M.Phase = EPFPhase::Play;
		M.Ball = FPFBall(); M.Ball.Owner = Who; M.Ball.Pos = FVector(H.Pos, 0.11f); M.Ball.LastTouch = Who; M.Ball.LastTeam = 0;
	};
	// ---- PFATTACK
	{
		int32 N = 0; float WideUp = 0, Split = 0, Ahead0 = 0, Ahead1 = 0, Line0 = 0, Line1 = 0; int64 Runs = 0, RunsHold = 0; int32 Kept = 0;
		static const float Starts[] = { -15.f, 0.f, 15.f };
		for (int32 s = 0; s < 3; ++s)
			for (int32 k = 0; k < 6; ++k)
			{
				FPFMatch M; NewMatch(M, 7100 + s * 10 + k, k);
				const int32 Ad = M.AttackSign(0);
				int32 Cm = -1;   // a centre mid has it (owner: "my left mid and right mid should be moving up the field as my center mids have the ball")
				for (const FPFPlayer& Q : M.P) if (Q.Team == 0 && Q.Setup.Role == EPFRole::MID && FMath::Abs(Q.Setup.Slot.Y) < 0.5f && (Cm < 0 || (k & 1))) Cm = Q.Index;
				GiveBall(M, Cm, FVector2D(Ad * Starts[s], (k % 3 - 1) * 6.f));
				M.TestTakeShape();
				auto Snap = [&](float& Ahead, float& Line)
				{
					int32 A = 0; float L = 0; int32 Nn = 0;
					for (const FPFPlayer& Q : M.P) if (Q.Team == 0 && !Q.bKeeper && !Q.bSentOff) { L += Q.Pos.X * Ad; Nn++; if (Q.Index != M.Ball.Owner && (Q.Pos.X - M.Ball.Pos.X) * Ad > 3.f) A++; }
					Ahead += A; Line += Nn ? L / Nn : 0.f;
				};
				TMap<int32, float> X0;
				for (const FPFPlayer& Q : M.P) if (Q.Team == 0 && Q.Setup.Role == EPFRole::MID && FMath::Abs(Q.Setup.Slot.Y) >= 0.6f) X0.Add(Q.Index, Q.Pos.X * Ad);
				float A0 = 0, L0 = 0, A1 = 0, L1 = 0;
				Snap(A0, L0); Ahead0 += A0; Line0 += L0;
				const int64 R0 = FPFMatch::AtkDiag[10] + FPFMatch::AtkDiag[12] + FPFMatch::AtkDiag[13] + FPFMatch::AtkDiag[14] + FPFMatch::AtkDiag[17];
				// he shields it (stick idle) for 2.5 s, then holds A for 0.4 s (a forward should set off for a long pass)
				FPFPadInput In; bool bLost = false;
				for (int32 f = 0; f < 300 && !bLost; ++f) { M.Tick(1.f / 120.f, In); if (M.Ball.Owner < 0 || M.P[M.Ball.Owner].Team != 0) bLost = true; }
				const int64 R1 = FPFMatch::AtkDiag[10] + FPFMatch::AtkDiag[12] + FPFMatch::AtkDiag[13] + FPFMatch::AtkDiag[14] + FPFMatch::AtkDiag[17];
				int64 R2 = R1;
				if (!bLost)
				{
					Kept++;
					FPFPadInput Q; Q.bPass = true; Q.bPassHeld = true;
					M.Tick(1.f / 120.f, Q); Q.bPass = false;
					for (int32 f = 0; f < 40 && M.Ball.Owner == Cm; ++f) M.Tick(1.f / 120.f, Q);
					R2 = FPFMatch::AtkDiag[10] + FPFMatch::AtkDiag[12] + FPFMatch::AtkDiag[13] + FPFMatch::AtkDiag[14] + FPFMatch::AtkDiag[17];
				}
				Snap(A1, L1); Ahead1 += A1; Line1 += L1;
				float Wu = 0; for (const auto& KV : X0) Wu += M.P[KV.Key].Pos.X * Ad - KV.Value;
				WideUp += X0.Num() ? Wu / X0.Num() : 0.f;
				TArray<float> SY; for (const FPFPlayer& Q : M.P) if (Q.Team == 0 && Q.Setup.Role == EPFRole::FWD) SY.Add(Q.Pos.Y);
				if (SY.Num() >= 2) Split += FMath::Abs(SY[0] - SY[1]);
				Runs += R1 - R0; RunsHold += R2 - R1; N++;
				UE_LOG(LogPF, Display, TEXT("PFATTACK %2d: ball at %+.0f m, %s; wide mids moved up %.1f m, strikers %.1f m apart, men ahead of the ball %d -> %d, runs %lld (+%lld with A held)"),
					N, Starts[s], bLost ? TEXT("lost it") : TEXT("kept it"), X0.Num() ? Wu / X0.Num() : 0.f, SY.Num() >= 2 ? FMath::Abs(SY[0] - SY[1]) : 0.f, int32(A0), int32(A1), R1 - R0, R2 - R1);
			}
		UE_LOG(LogPF, Display, TEXT("PFATTACK total %d cases (kept it %d): wide mids moved up %.1f m on average in 2.5 s, strikers %.1f m apart, men ahead of the ball %.1f -> %.1f, team line %+.1f -> %+.1f m, runs started %.2f a case, %.2f more with A held"),
			N, Kept, N ? WideUp / N : 0.f, N ? Split / N : 0.f, N ? Ahead0 / N : 0.f, N ? Ahead1 / N : 0.f, N ? Line0 / N : 0.f, N ? Line1 / N : 0.f, N ? float(Runs) / N : 0.f, Kept ? float(RunsHold) / Kept : 0.f);
	}
	// ---- PFCROSS
	{
		static const float Holds[] = { 0.05f, 0.22f, 0.5f };
		static const TCHAR* Names[] = { TEXT("tap"), TEXT("middle"), TEXT("full") };
		for (int32 h = 0; h < 3; ++h)
		{
			int32 N = 0, Near = 0, Mid = 0, Far = 0, None = 0, Won = 0, Heads = 0; float PeakSum = 0, LandSum = 0;
			for (int32 k = 0; k < 8; ++k)
			{
				FPFMatch M; NewMatch(M, 7300 + h * 20 + k, k);
				const int32 Ad = M.AttackSign(0);
				const float Side = (k & 1) ? 1.f : -1.f;
				int32 Wm = -1; for (const FPFPlayer& Q : M.P) if (Q.Team == 0 && Q.Setup.Role == EPFRole::MID && FMath::Sign(Q.Setup.Slot.Y * Ad) == Side && FMath::Abs(Q.Setup.Slot.Y) >= 0.6f) Wm = Q.Index;
				if (Wm < 0) continue;
				GiveBall(M, Wm, FVector2D(Ad * (52.5f - 9.f - (k % 3) * 3.f), Side * 25.f));
				M.TestTakeShape();
				// three in the box: near post, the spot, far post; the rest of ours and theirs out of the way but their keeper
				TArray<int32> Box;
				for (FPFPlayer& Q : M.P)
				{
					if (Q.Index == Wm || Q.bKeeper) continue;
					if (Q.Team == 0 && Box.Num() < 3 && (Q.Setup.Role == EPFRole::FWD || Q.Setup.Role == EPFRole::MID)) { Box.Add(Q.Index); continue; }
					if (Q.Team == 1 && Q.Setup.Role == EPFRole::DEF && k >= 4) { Q.Pos = FVector2D(Ad * (52.5f - 7.f), (Q.Setup.Slot.Y) * 10.f); Q.Vel = FVector2D::ZeroVector; continue; }   // half the cases: their back four in the box
					if (Q.Team == 1 && Q.Setup.Role == EPFRole::DEF && FMath::Abs(Q.Setup.Slot.Y) > 0.6f) { Q.Pos = FVector2D(Ad * 51.5f, FMath::Sign(Q.Setup.Slot.Y) * 31.f); Q.Vel = FVector2D::ZeroVector; continue; }   // else two on the goal line out wide (an onside line, out of the way)
					Q.bSentOff = true; Q.Pos = FVector2D(0, 90);
				}
				if (Box.Num() < 3) continue;
				const FVector2D Spots[3] = { FVector2D(Ad * 47.5f, Side * 2.f), FVector2D(Ad * 41.5f, 0.f), FVector2D(Ad * 46.5f, -Side * 6.7f) };
				for (int32 i = 0; i < 3; ++i) { FPFPlayer& Q = M.P[Box[i]]; Q.Pos = Spots[i]; Q.Vel = FVector2D::ZeroVector; Q.bSettled = true; }
				FPFPadInput Q; Q.bLob = true; Q.bLobHeld = true;
				M.Tick(1.f / 120.f, Q); Q.bLob = false;
				float T = 1.f / 120.f;
				while (T < Holds[h] && M.Ball.Owner == Wm) { M.Tick(1.f / 120.f, Q); T += 1.f / 120.f; }
				Q.bLobHeld = false;
				for (int32 f = 0; f < 4 && M.Ball.Owner == Wm; ++f) M.Tick(1.f / 120.f, Q);
				if (M.Ball.Owner == Wm) continue;
				N++;
				const int32 Pick = M.P[Wm].KickTarget;
				if (Pick == Box[0]) Near++; else if (Pick == Box[1]) Mid++; else if (Pick == Box[2]) Far++; else None++;
				const FVector2D K0(M.Ball.Pos.X, M.Ball.Pos.Y);
				float Peak = 0, Land = 0; bool bLanded = false; FPFPadInput Idle;
				for (int32 f = 0; f < 480; ++f)
				{
					M.Tick(1.f / 120.f, Idle);
					Peak = FMath::Max(Peak, (float)M.Ball.Pos.Z);
					if (!bLanded && M.Ball.Vel.Z < 0.f && M.Ball.Pos.Z < 1.8f) { bLanded = true; Land = FVector2D::Distance(K0, FVector2D(M.Ball.Pos.X, M.Ball.Pos.Y)); }
					if (M.Ball.LastTouch != Wm) break;
				}
				if (!bLanded) Land = FVector2D::Distance(K0, FVector2D(M.Ball.Pos.X, M.Ball.Pos.Y));   // touched before it dropped to head height
				PeakSum += Peak; LandSum += Land;
				const int32 Lt = M.Ball.LastTouch;
				if (M.P.IsValidIndex(Lt) && M.P[Lt].Team == 0 && Lt != Wm) { Won++; if (M.Ball.LastKind == int32(EPFKick::Header)) Heads++; }
				UE_LOG(LogPF, Display, TEXT("PFCROSS %s %d: picked %s, peak %.1f m, came down to head height %.1f m out, won by %s (kind %d)"), Names[h], k,
					Pick == Box[0] ? TEXT("near post") : Pick == Box[1] ? TEXT("the spot") : Pick == Box[2] ? TEXT("far post") : TEXT("nobody"), Peak, Land,
					!M.P.IsValidIndex(Lt) || Lt == Wm ? TEXT("nobody") : M.P[Lt].Team == 0 ? TEXT("us") : M.P[Lt].bKeeper ? TEXT("their keeper") : TEXT("them"), M.Ball.LastKind);
			}
			UE_LOG(LogPF, Display, TEXT("PFCROSS %s total %d crosses: near post %d, the spot %d, far post %d, nobody %d; peak %.1f m, head height reached %.1f m from the crosser; won by us %d (headers %d)"),
				Names[h], N, Near, Mid, Far, None, N ? PeakSum / N : 0.f, N ? LandSum / N : 0.f, Won, Heads);
		}
	}
}

static void PFPassTests()
{
	const TArray<FPFClub>& C = PFWorld::Clubs();
	auto NewMatch = [&C](FPFMatch& M, uint64 Seed, int32 Ci)
	{
		FPFMatchSetup S; S.Seed = Seed;
		S.Home = PFWorld::FriendlyTeam(C[(Ci * 5) % C.Num()], false, 11 + Ci); S.Away = PFWorld::FriendlyTeam(C[(Ci * 7 + 3) % C.Num()], true, 13 + Ci);
		S.Home.bHuman = true; S.HalfSeconds = 150.f; S.StadiumSlug = C[0].Stadium;
		M.Init(S);
		{ FPFPadInput In; In.bPass = true; M.Tick(1.f / 120.f, In); }
		FPFPadInput In; for (int32 f = 0; f < 60; ++f) M.Tick(1.f / 120.f, In);
		for (FPFPlayer& Q : M.P) { Q.Act = EPFAct::None; Q.PendingKick = EPFKick::None; Q.NoTouch = 0; Q.RunT = 0; Q.BoostT = 0; }
	};
	auto GiveBall = [](FPFMatch& M, int32 Who)
	{
		FPFPlayer& H = M.P[Who];
		H.Vel = FVector2D::ZeroVector; H.Facing = FVector2D(M.AttackSign(0), 0);
		M.Human = Who; M.Phase = EPFPhase::Play;
		M.Ball = FPFBall(); M.Ball.Owner = Who; M.Ball.Pos = FVector(H.Pos, 0.11f); M.Ball.LastTouch = Who; M.Ball.LastTeam = 0;
	};
	// press, hold for Hold seconds with the stick on Stick, release; returns the receiver the passer picked (-2: no kick)
	auto PressHold = [](FPFMatch& M, bool bThrough, FVector2D Stick, float Hold)
	{
		const int32 From = M.Human;
		FPFPadInput Q; Q.Move = Stick;
		if (bThrough) { Q.bThrough = true; Q.bThroughHeld = true; } else { Q.bPass = true; Q.bPassHeld = true; }
		M.Tick(1.f / 120.f, Q); Q.bPass = Q.bThrough = false;
		float T = 1.f / 120.f;
		while (T < Hold && M.Ball.Owner == From) { M.Tick(1.f / 120.f, Q); T += 1.f / 120.f; }
		Q.bPassHeld = Q.bThroughHeld = false;
		for (int32 f = 0; f < 3 && M.Ball.Owner == From; ++f) M.Tick(1.f / 120.f, Q);
		return M.Ball.Owner == From ? -2 : M.P[From].KickTarget;
	};
	// ---- A: aim, nobody in the way (8 team-mates round the passer, opponents off), the stick on each, 0 / +7 / -7 degrees
	{
		static const FVector2D Lay[] = { {10, 0}, {24, 3}, {12, 12}, {2, 10}, {16, -14}, {30, -6}, {-8, -8}, {6, -22} };
		int32 N = 0, Picked = 0, Got = 0, PickedJ = 0, NJ = 0, Space = 0;
		for (int32 Yp = 0; Yp < 3; ++Yp)
			for (int32 t = 0; t < 8; ++t)
				for (int32 j = 0; j < 3; ++j)
				{
					FPFMatch M; NewMatch(M, 4100 + Yp * 100 + t * 10 + j, t);
					const int32 Ad = M.AttackSign(0);
					int32 Pa = -1; TArray<int32> Mates;
					for (const FPFPlayer& Q : M.P) if (Q.Team == 0 && !Q.bKeeper && Pa < 0 && Q.Setup.Role == EPFRole::MID) Pa = Q.Index;
					for (const FPFPlayer& Q : M.P) if (Q.Team == 0 && !Q.bKeeper && Q.Index != Pa) Mates.Add(Q.Index);
					{ int32 Kept = 0; for (FPFPlayer& Q : M.P) if (Q.Team == 1) { if (Q.bKeeper || (Q.Setup.Role == EPFRole::DEF && Kept++ == 0)) { Q.Pos = FVector2D(Ad * (Q.bKeeper ? 51.f : 46.f), Q.bKeeper ? 0.f : 31.f); Q.Vel = FVector2D::ZeroVector; } else { Q.bSentOff = true; Q.Pos = FVector2D(0, -80); } } }   // their keeper and one defender deep (a real offside line)
					const FVector2D From(Ad * -12.f, (Yp - 1) * 14.f);
					M.P[Pa].Pos = From;
					for (int32 i = 0; i < Mates.Num(); ++i)
					{
						FPFPlayer& Q = M.P[Mates[i]];
						if (i < 8) { Q.Pos = From + FVector2D(Ad * Lay[i].X, Lay[i].Y * Ad); Q.Vel = FVector2D::ZeroVector; Q.bSettled = true; }
						else { Q.bSentOff = true; Q.Pos = FVector2D(0, 80); }
					}
					GiveBall(M, Pa);
					const int32 Tg = Mates[t];
					const float Jit = j == 0 ? 0.f : j == 1 ? 7.f : -7.f;
					const FVector2D Stick = PFRotScreen(M.StickFor(M.P[Tg].Pos - From), Jit);
					const float Hold = PFPassHoldFor(FVector2D::Distance(M.P[Tg].Pos, From));
					const int32 R = PressHold(M, false, Stick, Hold);
					float T = 0; while (T < 4.f && M.Ball.Owner < 0 && M.Phase == EPFPhase::Play) { M.Tick(1.f / 120.f, FPFPadInput()); T += 1.f / 120.f; }
					++N; Picked += R == Tg; Got += M.Ball.Owner == Tg; Space += R == -1;
					if (j) { ++NJ; PickedJ += R == Tg; }
					if (R != Tg) UE_LOG(LogPF, Display, TEXT("PFPASS A miss: aimed at mate %d (%.0f m, %+.0f deg thumb, hold %.2f s) -> picked %d, ball with %d"), t, FVector2D::Distance(M.P[Tg].Pos, From), Jit, Hold, R, M.Ball.Owner);
				}
		UE_LOG(LogPF, Display, TEXT("PFPASS A (open, aimed) %d passes: picked the aimed man %d (%.0f%%; %d / %d with the thumb 7 deg off), he got it %d (%.0f%%), into space %d"),
			N, Picked, 100.f * Picked / FMath::Max(1, N), PickedJ, NJ, Got, 100.f * Got / FMath::Max(1, N), Space);
	}
	// ---- B: a full side in shape against a full side; aim at each team-mate 6-40 m away (thumb 0 / +-6 deg), hold for his distance
	{
		int32 N = 0, Picked = 0, Got = 0, Lost = 0, Bn[3] = { 0, 0, 0 }, Bp[3] = { 0, 0, 0 };
		for (int32 k = 0; k < 12; ++k)
			for (int32 t = 0; t < 10; ++t)
			{
				FPFMatch M; NewMatch(M, 5200 + k, k);
				const int32 Ad = M.AttackSign(0);
				int32 Pa = -1;
				for (const FPFPlayer& Q : M.P) if (Q.Team == 0 && Q.Setup.Role == EPFRole::MID && (Pa < 0 || FMath::Abs(Q.Setup.Slot.Y) < FMath::Abs(M.P[Pa].Setup.Slot.Y))) Pa = Q.Index;
				M.P[Pa].Pos = FVector2D(Ad * (-20.f + (k % 4) * 10.f), ((k / 4) % 3 - 1) * 12.f);
				GiveBall(M, Pa);
				M.TestTakeShape();
				TArray<int32> Mates;
				for (const FPFPlayer& Q : M.P) { const float D = FVector2D::Distance(Q.Pos, M.P[Pa].Pos); if (Q.Team == 0 && !Q.bKeeper && Q.Index != Pa && D > 6.f && D < 40.f) Mates.Add(Q.Index); }
				if (t >= Mates.Num()) break;
				const int32 Tg = Mates[t];
				const FVector2D From = M.P[Pa].Pos;
				const float Jit = ((k + t) % 3 == 0) ? 0.f : ((k + t) % 3 == 1) ? 6.f : -6.f;
				const float D = FVector2D::Distance(M.P[Tg].Pos, From);
				const FVector2D Stick = PFRotScreen(M.StickFor(M.P[Tg].Pos - From), Jit);
				const int32 R = PressHold(M, false, Stick, PFPassHoldFor(D));
				float T = 0; while (T < 4.f && M.Ball.Owner < 0 && M.Phase == EPFPhase::Play) { M.Tick(1.f / 120.f, FPFPadInput()); T += 1.f / 120.f; }
				const int32 Band = D < 15.f ? 0 : D < 27.f ? 1 : 2;
				++N; ++Bn[Band]; Picked += R == Tg; Bp[Band] += R == Tg; Got += M.Ball.Owner == Tg; Lost += M.Ball.Owner >= 0 && M.P[M.Ball.Owner].Team != 0;
				if (R != Tg) { UE_LOG(LogPF, Display, TEXT("PFPASS B miss: aimed at %d (%.0f m, %+.0f deg) -> picked %d (%.0f m), ball with %d"), Tg, D, Jit, R, R >= 0 ? (float)FVector2D::Distance(M.P[R].Pos, From) : -1.f, M.Ball.Owner); }
				else if (M.Ball.Owner != Tg) UE_LOG(LogPF, Display, TEXT("PFPASS B picked but not got: %d (%.0f m, %+.0f deg), ball with %d (team %d) after %.2f s, ball %.1f m from him"), Tg, D, Jit, M.Ball.Owner, M.Ball.Owner >= 0 ? M.P[M.Ball.Owner].Team : -1, T, (float)FVector2D::Distance(M.P[Tg].Pos, FVector2D(M.Ball.Pos.X, M.Ball.Pos.Y)));
			}
		UE_LOG(LogPF, Display, TEXT("PFPASS B (in shape, aimed) %d passes: picked the aimed man %d (%.0f%%: under 15 m %d/%d, 15-27 m %d/%d, 27-40 m %d/%d), he got it %d (%.0f%%), lost to the opponents %d"),
			N, Picked, 100.f * Picked / FMath::Max(1, N), Bp[0], Bn[0], Bp[1], Bn[1], Bp[2], Bn[2], Got, 100.f * Got / FMath::Max(1, N), Lost);
	}
	// ---- C: tap vs hold along one line: two team-mates on the stick's line at 9 m and 26 m; a tap should find the near
	// one, a long hold the far one
	{
		int32 TapNear = 0, HoldFar = 0, N = 0;
		for (int32 k = 0; k < 12; ++k)
		{
			for (int32 Mode = 0; Mode < 2; ++Mode)
			{
				FPFMatch M; NewMatch(M, 6100 + k, k);
				const int32 Ad = M.AttackSign(0);
				int32 Pa = -1; TArray<int32> Mates;
				for (const FPFPlayer& Q : M.P) if (Q.Team == 0 && !Q.bKeeper) { if (Pa < 0) Pa = Q.Index; else Mates.Add(Q.Index); }
				{ int32 Kept = 0; for (FPFPlayer& Q : M.P) if (Q.Team == 1) { if (Q.bKeeper || (Q.Setup.Role == EPFRole::DEF && Kept++ == 0)) { Q.Pos = FVector2D(Ad * (Q.bKeeper ? 51.f : 46.f), Q.bKeeper ? 0.f : 31.f); Q.Vel = FVector2D::ZeroVector; } else { Q.bSentOff = true; Q.Pos = FVector2D(0, -80); } } }   // their keeper and one defender deep (a real offside line)
				const FVector2D From(Ad * -10.f, (k % 3 - 1) * 10.f);
				const float Ang = FMath::DegreesToRadians(-50.f + k * 9.f);
				const FVector2D Line = FVector2D(FMath::Cos(Ang) * Ad, FMath::Sin(Ang));
				M.P[Pa].Pos = From;
				for (int32 i = 0; i < Mates.Num(); ++i) { FPFPlayer& Q = M.P[Mates[i]]; if (i < 2) { Q.Pos = From + Line * (i == 0 ? 9.f : 26.f) + FVector2D(0, i == 0 ? 0.8f : -1.2f); Q.bSettled = true; } else { Q.bSentOff = true; Q.Pos = FVector2D(0, 80); } }
				GiveBall(M, Pa);
				const int32 R = PressHold(M, false, M.StickFor(Line), Mode == 0 ? 0.03f : PFPassHoldFor(26.f));
				if (Mode == 0) TapNear += R == Mates[0]; else HoldFar += R == Mates[1];
			}
			++N;
		}
		UE_LOG(LogPF, Display, TEXT("PFPASS C (two men on one line, 9 m and 26 m) %d lines: a tap found the near man %d, a long hold the far man %d"), N, TapNear, HoldFar);
	}
	// ---- D: through balls with the pad: a full side in shape against a full side; Y aimed at the space 6 or 12 m ahead of
	// a forward (thumb 0 / +-5 deg), held for that lead. Did he run onto it in stride, onside?
	{
		int32 N = 0, Picked = 0, Got = 0, Mate = 0, Off = 0, ByDef = 0, ByKeeper = 0, Out = 0, Dead = 0, Stride = 0, Carried = 0;
		float SpdSum = 0, TSum = 0, LeadSum = 0;
		for (int32 k = 0; k < 36; ++k)
		{
			FPFMatch M; NewMatch(M, 7300 + k, k);
			const int32 Ad = M.AttackSign(0);
			int32 Pa = -1;
			for (const FPFPlayer& Q : M.P) if (Q.Team == 0 && Q.Setup.Role == EPFRole::MID && (Pa < 0 || FMath::Abs(Q.Setup.Slot.Y) < FMath::Abs(M.P[Pa].Setup.Slot.Y))) Pa = Q.Index;
			M.P[Pa].Pos = FVector2D(Ad * (-8.f + (k % 3) * 7.f), ((k / 3) % 3 - 1) * 10.f);
			GiveBall(M, Pa);
			M.TestTakeShape();
			FPFPadInput Idle; for (int32 f = 0; f < 20; ++f) M.Tick(1.f / 120.f, Idle);
			if (M.Ball.Owner != Pa || M.Phase != EPFPhase::Play) continue;
			TArray<int32> Fw;
			for (const FPFPlayer& Q : M.P) if (Q.Team == 0 && !Q.bKeeper && Q.Index != Pa && (Q.Pos.X - M.P[Pa].Pos.X) * Ad > 6.f && Q.Setup.Role != EPFRole::DEF) Fw.Add(Q.Index);
			if (!Fw.Num()) continue;
			const int32 Tg = Fw[k % Fw.Num()];
			const float Lead = (k / 9) % 2 ? 12.f : 6.f;
			const FVector2D From = M.P[Pa].Pos, RFrom = M.P[Tg].Pos;
			const FVector2D Space = RFrom + FVector2D(Ad * Lead, -FMath::Sign(RFrom.Y) * FMath::Min(FMath::Abs(RFrom.Y) * 0.15f, 3.f));
			const float Jit = (k % 3 == 0) ? 0.f : (k % 3 == 1) ? 5.f : -5.f;
			const int32 R = PressHold(M, true, PFRotScreen(M.StickFor(Space - From), Jit), PFThroughHoldFor(Lead));
			if (R == -2) continue;
			++N; Picked += R == Tg;
			const int32 Run = R >= 0 ? R : Tg;
			const FVector2D BallDir0 = FVector2D(M.Ball.Vel.X, M.Ball.Vel.Y).GetSafeNormal();
			const float BallV0 = FVector2D(M.Ball.Vel.X, M.Ball.Vel.Y).Size();
			const float Margin0 = M.DbgThroughMargin;
			const float RunTop = M.P[Run].A.MaxSpeed * M.P[Run].A.SprintMul;
			float T = 0;
			while (T < 6.f && M.Ball.Owner < 0 && M.Phase == EPFPhase::Play) { M.Tick(1.f / 120.f, FPFPadInput()); T += 1.f / 120.f; }
			const TCHAR* What = TEXT("dead");
			if (M.Ball.Owner >= 0 && M.P[M.Ball.Owner].Team == 0)
			{
				const FPFPlayer& O = M.P[M.Ball.Owner];
				const float Sp = O.Vel.Size(); const FVector2D Dir0 = O.Vel.GetSafeNormal();
				if (M.Ball.Owner == Run) ++Got; else ++Mate;
				SpdSum += Sp / RunTop; TSum += T; LeadSum += FVector2D::Distance(RFrom, O.Pos);
				const bool bStride = Sp > 0.7f * RunTop && FVector2D::DotProduct(Dir0, BallDir0) > 0.2f;   // at speed, running on with the ball (a diagonal run onto it counts, checking back does not)
				Stride += bStride;
				// his first touch carries him on (the stick left alone)
				const FVector2D P0 = O.Pos; const int32 Who = M.Ball.Owner;
				for (int32 f = 0; f < 48 && M.Ball.Owner == Who; ++f) M.Tick(1.f / 120.f, FPFPadInput());
				Carried += M.Ball.Owner == Who && (M.P[Who].Pos - P0).Size() > 2.f;
				What = Who == Run ? (bStride ? TEXT("runner, in stride") : TEXT("runner, checked")) : TEXT("team-mate");
				UE_LOG(LogPF, Display, TEXT("PFTHRU2 %2d: aimed %.0f m ahead of %d (%+.0f deg) -> picked %d, %s after %.2f s at %.1f m/s (%.0f%% of his sprint, %.0f deg off the ball's line), %.1f m from where he started; ball launched %.1f m/s"),
					k, Lead, Tg, Jit, R, What, T, Sp, 100.f * Sp / RunTop, FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp((float)FVector2D::DotProduct(Dir0, BallDir0), -1.f, 1.f))), (float)FVector2D::Distance(RFrom, P0), BallV0);
				continue;
			}
			else if (M.Ball.Owner >= 0) { if (M.P[M.Ball.Owner].bKeeper) { ++ByKeeper; What = TEXT("keeper"); } else { ++ByDef; What = TEXT("defender"); } }
			else if (M.Banner == TEXT("OFFSIDE")) { ++Off; What = TEXT("offside"); }
			else if (M.Phase != EPFPhase::Play) { ++Out; What = TEXT("out of play"); }
			else ++Dead;
			UE_LOG(LogPF, Display, TEXT("PFTHRU2 %2d: aimed %.0f m ahead of %d (%+.0f deg) -> picked %d, %s after %.2f s (ball launched %.1f m/s, now %.1f, travelled %.1f m; runner %.1f m from it; predicted margin %.2f)"), k, Lead, Tg, Jit, R, What, T,
				BallV0, FVector2D(M.Ball.Vel.X, M.Ball.Vel.Y).Size(), (float)FVector2D::Distance(From, FVector2D(M.Ball.Pos.X, M.Ball.Pos.Y)), (float)FVector2D::Distance(M.P[Run].Pos, FVector2D(M.Ball.Pos.X, M.Ball.Pos.Y)), Margin0);
		}
		const int32 Dn = Got + Mate;
		UE_LOG(LogPF, Display, TEXT("PFTHRU2 total %d pad through balls: picked the aimed runner %d (%.0f%%), runner got it %d (%.0f%%), a team-mate %d, defender %d, keeper %d, offside %d, out %d, dead %d; of those received: in stride %d, first touch carried him on %d, at %.0f%% of sprint on average, after %.2f s, %.1f m from his start"),
			N, Picked, 100.f * Picked / FMath::Max(1, N), Got, 100.f * Got / FMath::Max(1, N), Mate, ByDef, ByKeeper, Off, Out, Dead, Stride, Carried, Dn ? 100.f * SpdSum / Dn : 0.f, Dn ? TSum / Dn : 0.f, Dn ? LeadSum / Dn : 0.f);
	}
	// ---- E: the goal kick routine (owner: "the keeper steps back about four steps, then runs up and kicks when the button
	// is pressed"): the ball goes out off an opponent over our goal line; the keeper's steps and when it is struck. Pad
	// (X pressed at once, or A for a short one) and AI.
	for (int32 Mode = 0; Mode < 3; ++Mode)
	{
		FPFMatch M; NewMatch(M, 8100 + Mode, Mode);
		if (Mode == 2) M.Setup.Home.bHuman = false;
		const int32 Ad = M.AttackSign(0);
		int32 Opp = -1; for (const FPFPlayer& Q : M.P) if (Q.Team == 1 && !Q.bKeeper) { Opp = Q.Index; break; }
		M.Phase = EPFPhase::Play;
		M.Ball = FPFBall(); M.Ball.Pos = FVector(-Ad * 51.5f, 12.f, 0.11f); M.Ball.Vel = FVector(-Ad * 6.f, 0.f, 0.f); M.Ball.LastTouch = Opp; M.Ball.LastTeam = 1;
		float T = 0, MaxBack = 0, KickAt = -1; FVector2D Start(0, 0); int32 Kp = -1; bool bSeen = false; float Launch = 0;
		bool bPressed = false;
		while (T < 8.f)
		{
			FPFPadInput Q;
			if (M.Phase == EPFPhase::Restart && !bSeen) { bSeen = true; for (const FPFPlayer& K : M.P) if (K.Team == 0 && K.bKeeper) { Kp = K.Index; Start = K.Pos; } T = 0; }
			if (bSeen && Mode < 2 && !bPressed && T > 0.3f) { bPressed = true; if (Mode == 0) Q.bLob = true; else Q.bPass = true; }
			M.Tick(1.f / 120.f, Q); T += 1.f / 120.f;
			if (bSeen && Kp >= 0) MaxBack = FMath::Max(MaxBack, (float)FVector2D::Distance(M.P[Kp].Pos, Start));
			if (bSeen && KickAt < 0 && M.Ball.Owner < 0 && M.Phase == EPFPhase::Play && FVector2D(M.Ball.Vel.X, M.Ball.Vel.Y).Size() > 3.f) { KickAt = T; Launch = M.Ball.Vel.Size(); break; }
		}
		UE_LOG(LogPF, Display, TEXT("PFGK %s: goal kick seen %d, keeper stepped back up to %.1f m, ball struck %.2f s after the restart at %.1f m/s (kind %d)"),
			Mode == 0 ? TEXT("pad X ") : Mode == 1 ? TEXT("pad A ") : TEXT("AI    "), bSeen ? 1 : 0, MaxBack, KickAt, Launch, M.Ball.LastKind);
	}
}

void UPFFlow::RunFeelTest()
{
	// headless: the human player in a friendly, stick scripted. Measures how fast the controls answer, for a slow and a fast player.
	FPFMatch::GameSpeed = FPFMatch::NormalSpeed;   // v0.10: tests run at the default pace
	const TArray<FPFClub>& C = PFWorld::Clubs();
	for (int32 Variant = 0; Variant < 4; ++Variant)
	{
		FPFMatch M;
		FPFMatchSetup S; S.Seed = 77;
		S.Home = PFWorld::FriendlyTeam(C[0], false, 11); S.Away = PFWorld::FriendlyTeam(C[1], true, 13);
		S.Home.bHuman = true; S.HalfSeconds = 150.f; S.StadiumSlug = C[0].Stadium;
		M.Init(S);
		auto Run = [&M](float Secs, FVector2D Stick, bool bSprint) { FPFPadInput In; In.Move = Stick; In.bSprint = bSprint; for (int32 f = 0; f < int32(Secs * 120); ++f) M.Tick(1.f / 120.f, In); };
		// get into open play
		{ FPFPadInput In; In.bPass = true; M.Tick(1.f / 120.f, In); }
		Run(0.5f, FVector2D::ZeroVector, false);
		FPFPlayer& H = M.P[M.Human];
		H.Setup.Stats.Pace = H.Setup.Stats.Accel = H.Setup.Stats.Agility = Variant < 2 ? 30 : 90;
		H.A = PFAbility(H.Setup.Stats, false);
		H.Pos = FVector2D(0, 20); H.Vel = FVector2D::ZeroVector;
		for (FPFPlayer& O : M.P) if (O.Index != M.Human) O.bSentOff = true;   // an empty pitch: nothing but the controls
		const bool bBall = (Variant % 2) == 1;
		if (bBall) { M.Ball.Owner = M.Human; M.Ball.Pos = FVector(H.Pos, 0.11f); } else { M.Ball.Owner = -1; M.Ball.Pos = FVector(0, -30, 0.11f); M.Ball.Vel = FVector::ZeroVector; }
		const FVector2D R(1, 0), L(-1, 0);
		// time to top speed from a standstill
		float T = 0; FPFPadInput In; In.Move = R;
		while (T < 3.f) { M.Tick(1.f / 120.f, In); T += 1.f / 120.f; if (H.Vel.Size() > H.A.MaxSpeed * (bBall ? 0.76f : 0.95f)) break; }
		const float TTop = T;
		Run(0.6f, R, false);
		// reversal: time until moving the other way, time until facing it, speed after 0.1 s
		const FVector2D Before = H.Vel.GetSafeNormal();
		T = 0; float TMove = -1, TFace = -1; In.Move = L; float Sp100 = 0;
		while (T < 1.f)
		{
			M.Tick(1.f / 120.f, In); T += 1.f / 120.f;
			if (TMove < 0 && FVector2D::DotProduct(H.Vel.GetSafeNormal(), Before) < -0.9f) TMove = T;
			if (TFace < 0 && FVector2D::DotProduct(H.Facing, Before) < -0.9f) TFace = T;
			if (FMath::IsNearlyEqual(T, 0.1f, 0.005f)) Sp100 = H.Vel.Size();
		}
		// stop
		T = 0; In.Move = FVector2D::ZeroVector; const int32 HumanBefore = M.Human;
		while (T < 1.f && H.Vel.Size() > 0.2f) { M.Tick(1.f / 120.f, In); T += 1.f / 120.f; }
		if (M.Human != HumanBefore) UE_LOG(LogPF, Display, TEXT("PFFEEL control switched during the stop test (%d -> %d)"), HumanBefore, M.Human);
		UE_LOG(LogPF, Display, TEXT("PFFEEL %s player%s: top speed %.1f m/s reached in %.2f s; reverse: moving back in %.3f s, facing back in %.3f s, %.1f m/s after 0.1 s; stop in %.2f s"),
			Variant < 2 ? TEXT("slow") : TEXT("fast"), bBall ? TEXT(" dribbling") : TEXT(""), H.A.MaxSpeed, TTop, TMove, TFace, Sp100, T);
	}
	// passing at a high frame rate (300 fps: most frames run no 120 Hz step) and how far the dribble runs from the boots
	for (int32 Trial = 0; Trial < 2; ++Trial)
	{
		FPFMatch M;
		FPFMatchSetup S; S.Seed = 91 + Trial;
		S.Home = PFWorld::FriendlyTeam(C[0], false, 11); S.Away = PFWorld::FriendlyTeam(C[1], true, 13);
		S.Home.bHuman = true; S.HalfSeconds = 150.f; S.StadiumSlug = C[0].Stadium;
		M.Init(S);
		{ FPFPadInput In; In.bPass = true; M.Tick(1.f / 120.f, In); }
		FPFPadInput In; for (int32 f = 0; f < 60; ++f) M.Tick(1.f / 120.f, In);
		for (FPFPlayer& O : M.P) if (O.Team == 1) O.bSentOff = true;
		FPFPlayer& H = M.P[M.Human];
		H.Pos = FVector2D(-10, 0); H.Vel = FVector2D::ZeroVector; H.Act = EPFAct::None;
		M.Phase = EPFPhase::Play;   // v0.9.4: some seeds had the kick-off go out (a restart): the dribble was never measured
		M.Ball.Owner = M.Human; M.Ball.Pos = FVector(H.Pos, 0.11f); M.Ball.Vel = FVector::ZeroVector;
		In.Move = FVector2D(1, 0); In.bSprint = Trial == 1;
		float MaxAhead = 0;
		for (int32 f = 0; f < 300; ++f) { M.Tick(1.f / 300.f, In); if (M.Ball.Owner == M.Human) MaxAhead = FMath::Max(MaxAhead, (float)FVector2D::Distance(H.Pos, FVector2D(M.Ball.Pos.X, M.Ball.Pos.Y))); }
		int32 Presses = 0, Passed = 0; float LatSum = 0, LatMax = 0;
		for (int32 k = 0; k < 20; ++k)
		{
			// give the ball back, run a few frames, press on one frame only
			FPFPlayer& Hk = M.P[M.Human];
			Hk.Act = EPFAct::None; M.Ball.Owner = M.Human; M.Ball.Pos = FVector(Hk.Pos, 0.11f); M.Ball.Vel = FVector::ZeroVector;
			FPFPadInput Q; Q.Move = FVector2D(1, 0);
			for (int32 f = 0; f < 7 + k; ++f) M.Tick(1.f / 300.f, Q);
			const int32 Who = M.Human;
			Q.bPass = true; M.Tick(1.f / 300.f, Q); Q.bPass = false; ++Presses;
			float T = 1.f / 300.f;
			while (T < 0.5f && M.Ball.Owner == Who) { M.Tick(1.f / 300.f, Q); T += 1.f / 300.f; }
			if (M.Ball.Owner != Who) { ++Passed; LatSum += T; LatMax = FMath::Max(LatMax, T); }
			for (int32 f = 0; f < 150; ++f) M.Tick(1.f / 300.f, Q);
		}
		UE_LOG(LogPF, Display, TEXT("PFFEEL pass at 300 fps: %d / %d presses passed, ball away in %.3f s on average (worst %.3f s); dribble%s: ball at most %.2f m from the player"),
			Passed, Presses, Passed ? LatSum / Passed : -1.f, LatMax, Trial == 1 ? TEXT(" sprinting") : TEXT(""), MaxAhead);
	}
	// v0.9.3 aim test (owner: "if I aim far left the ball should go far left ... near post, near post"): shots from three
	// spots at the near post, the centre and the far post, on an empty goal; where they cross the line
	for (int32 Spot = 0; Spot < 3; ++Spot)
		for (int32 Tg = 0; Tg < 3; ++Tg)
		{
			FPFMatch M;
			FPFMatchSetup S; S.Seed = 300 + Spot * 3 + Tg;
			S.Home = PFWorld::FriendlyTeam(C[0], false, 11); S.Away = PFWorld::FriendlyTeam(C[1], true, 13);
			S.Home.bHuman = true; S.HalfSeconds = 150.f; S.StadiumSlug = C[0].Stadium;
			M.Init(S);
			{ FPFPadInput In; In.bPass = true; M.Tick(1.f / 120.f, In); }
			FPFPadInput In; for (int32 f = 0; f < 60; ++f) M.Tick(1.f / 120.f, In);
			for (FPFPlayer& O : M.P) if (O.Index != M.Human) { O.bSentOff = true; O.Act = EPFAct::None; O.PendingKick = EPFKick::None; O.Pos = FVector2D(0, -60); }
			FPFPlayer& H = M.P[M.Human];
			H.Setup.Stats.Shooting = 75; H.A = PFAbility(H.Setup.Stats, false);
			const FVector2D G = M.AttackGoal(H.Team);
			const float Sg = G.X > 0 ? 1.f : -1.f;
			H.Pos = FVector2D(G.X - Sg * 18.f, (Spot - 1) * 10.f); H.Vel = FVector2D::ZeroVector; H.Act = EPFAct::None; H.Facing = FVector2D(Sg, 0);
			M.Ball.Owner = M.Human; M.Ball.Pos = FVector(H.Pos, 0.11f); M.Ball.Vel = FVector::ZeroVector;
			M.Phase = EPFPhase::Play;   // some seeds put the earlier kick-off out of play (a throw-in)
			const float Side = H.Pos.Y > 0 ? 1.f : H.Pos.Y < 0 ? -1.f : 1.f;
			const FVector2D Target(G.X, Tg == 1 ? 0.f : (Tg == 0 ? Side : -Side) * 3.0f);   // Tg 0 near post, 1 centre, 2 far post
			FPFPadInput Q; Q.Move = M.StickFor(Target - H.Pos);
			Q.bShootPressed = true; Q.bShootHeld = true; M.Tick(1.f / 120.f, Q); Q.bShootPressed = false;
			for (int32 f = 0; f < 24; ++f) M.Tick(1.f / 120.f, Q);
			Q.bShootHeld = false; Q.bShootReleased = true; M.Tick(1.f / 120.f, Q); Q.bShootReleased = false; Q.Move = FVector2D::ZeroVector;
			float T = 0; while (T < 3.f && FMath::Abs(M.Ball.Pos.X) < FMath::Abs(G.X)) { M.Tick(1.f / 120.f, Q); T += 1.f / 120.f; }
			UE_LOG(LogPF, Display, TEXT("PFAIM from y %+.0f aiming y %+.1f: crossed the line at y %+.2f (z %.1f) after %.2f s; shots %d, last kick %d, owner %d, phase %d, stick %.2f %.2f"), H.Pos.Y, Target.Y, M.Ball.Pos.Y, M.Ball.Pos.Z, T, M.Shots[0] + M.Shots[1], M.Ball.LastKind, M.Ball.Owner, int32(M.Phase), M.StickFor(Target - H.Pos).X, M.StickFor(Target - H.Pos).Y);
		}
	// v0.9.6 placement test (owner: "everything goes near post ... far post or near post ... bad shot right at the goalie
	// ... good: left or right, the goalie saves it or misses it ... amazing: in the goal"): 54 shots against a real keeper
	// from the left, centre and right at 17 m, the stick toward goal / up / down, B tapped, held right, or held to the end
	{
		int32 N = 0, Near = 0, Far = 0, Gl[3] = { 0, 0, 0 }, Nn[3] = { 0, 0, 0 };
		for (int32 k = 0; k < 54; ++k)
		{
			const int32 Spot = k % 3, Stk = (k / 3) % 3, Hold = (k / 9) % 3, Shooter = (k / 27) % 2;
			FPFMatch M;
			FPFMatchSetup S; S.Seed = 900 + k;
			S.Home = PFWorld::FriendlyTeam(C[0], false, 11); S.Away = PFWorld::FriendlyTeam(C[1], true, 13);
			S.Home.bHuman = true; S.HalfSeconds = 150.f; S.StadiumSlug = C[0].Stadium;
			M.Init(S);
			{ FPFPadInput In; In.bPass = true; M.Tick(1.f / 120.f, In); }
			FPFPadInput In; for (int32 f = 0; f < 60; ++f) M.Tick(1.f / 120.f, In);
			FPFPlayer& H = M.P[M.Human];
			const FVector2D G = M.AttackGoal(H.Team);
			const float Sg = G.X > 0 ? 1.f : -1.f;
			int32 Kp = -1;
			for (FPFPlayer& O : M.P)
				if (O.Index != M.Human)
				{
					O.Act = EPFAct::None; O.PendingKick = EPFKick::None;
					if (O.bKeeper && O.Team != H.Team) { Kp = O.Index; O.Pos = FVector2D(G.X - Sg * 1.5f, 0); O.Vel = FVector2D::ZeroVector; }
					else { O.bSentOff = true; O.Pos = FVector2D(0, -60); }
				}
			H.Setup.Stats.Shooting = Shooter ? 85 : 60; H.A = PFAbility(H.Setup.Stats, false);
			H.Pos = FVector2D(G.X - Sg * 17.f, (Spot - 1) * 8.f); H.Vel = FVector2D::ZeroVector; H.Act = EPFAct::None; H.Facing = FVector2D(Sg, 0);
			M.Ball.Owner = M.Human; M.Ball.Pos = FVector(H.Pos, 0.11f); M.Ball.Vel = FVector::ZeroVector; M.Phase = EPFPhase::Play;
			const FVector2D StickW = Stk == 0 ? FVector2D(Sg, 0) : FVector2D(Sg * 0.5f, Stk == 1 ? 1.f : -1.f);
			FPFPadInput Q; Q.Move = M.StickFor(StickW);
			Q.bShootPressed = true; Q.bShootHeld = true; M.Tick(1.f / 120.f, Q); Q.bShootPressed = false;
			const int32 Frames = Hold == 0 ? 8 : Hold == 1 ? 37 : 60;
			for (int32 f = 0; f < Frames && M.Ball.Owner == M.Human; ++f) M.Tick(1.f / 120.f, Q);
			Q.bShootHeld = false; Q.bShootReleased = true; M.Tick(1.f / 120.f, Q); Q.bShootReleased = false; Q.Move = FVector2D::ZeroVector;
			const FString Note = M.LastShotNote; const int32 Sc0 = M.Score[H.Team];
			float LineY = 99, LineZ = 0;
			for (int32 f = 0; f < 300; ++f)
			{
				M.Tick(1.f / 120.f, Q);
				if (FMath::Abs(M.Ball.Pos.X) > FMath::Abs(G.X) - 0.2f && LineY > 98.f) { LineY = M.Ball.Pos.Y; LineZ = M.Ball.Pos.Z; }
				if (M.Score[H.Team] != Sc0 || (M.Ball.Owner == Kp && Kp >= 0)) break;
			}
			const bool bGoal = M.Score[H.Team] != Sc0;
			const int32 Gr = Note.Contains(TEXT("amazing")) ? 2 : Note.Contains(TEXT("good")) ? 1 : 0;
			++N; Near += Note.StartsWith(TEXT("near")); Far += Note.StartsWith(TEXT("far")); Nn[Gr]++; Gl[Gr] += bGoal;
			UE_LOG(LogPF, Display, TEXT("PFPLACE  from y %+.0f stick %s hold %d shooting %d: %s -> %s, at the line y %+.1f z %.1f"),
				H.Pos.Y, Stk == 0 ? TEXT("goal") : Stk == 1 ? TEXT("up  ") : TEXT("down"), Frames, H.Setup.Stats.Shooting, *Note,
				bGoal ? TEXT("GOAL") : M.Ball.Owner == Kp ? TEXT("caught") : TEXT("no goal"), LineY, LineZ);
		}
		UE_LOG(LogPF, Display, TEXT("PFPLACE %d shots: near post %d, far post %d; bad %d (goals %d), good %d (goals %d), amazing %d (goals %d)"),
			N, Near, Far, Nn[0], Gl[0], Nn[1], Gl[1], Nn[2], Gl[2]);
	}
	// v0.9.3 save test (owner: saves should be "a proper dive, catching the ball"): 18 shots at the near and far post
	// against a real keeper; how he deals with each
	{
		int32 Dives = 0, Standing = 0, Goals = 0, Caught = 0, N = 0;
		for (int32 k = 0; k < 18; ++k)
		{
			FPFMatch M;
			FPFMatchSetup S; S.Seed = 500 + k;
			S.Home = PFWorld::FriendlyTeam(C[0], false, 11); S.Away = PFWorld::FriendlyTeam(C[1], true, 13);
			S.Home.bHuman = true; S.HalfSeconds = 150.f; S.StadiumSlug = C[0].Stadium;
			M.Init(S);
			{ FPFPadInput In; In.bPass = true; M.Tick(1.f / 120.f, In); }
			FPFPadInput In; for (int32 f = 0; f < 60; ++f) M.Tick(1.f / 120.f, In);
			FPFPlayer& H = M.P[M.Human];
			const FVector2D G = M.AttackGoal(H.Team);
			const float Sg = G.X > 0 ? 1.f : -1.f;
			int32 Kp = -1;
			for (FPFPlayer& O : M.P)
				if (O.Index != M.Human)
				{
					O.Act = EPFAct::None; O.PendingKick = EPFKick::None;
					if (O.bKeeper && O.Team != H.Team) { Kp = O.Index; O.Pos = FVector2D(G.X - Sg * 1.5f, 0); O.Vel = FVector2D::ZeroVector; }
					else { O.bSentOff = true; O.Pos = FVector2D(0, -60); }
				}
			H.Setup.Stats.Shooting = 70; H.A = PFAbility(H.Setup.Stats, false);
			H.Pos = FVector2D(G.X - Sg * 17.f, (k % 3 - 1) * 8.f); H.Vel = FVector2D::ZeroVector; H.Act = EPFAct::None; H.Facing = FVector2D(Sg, 0);
			M.Ball.Owner = M.Human; M.Ball.Pos = FVector(H.Pos, 0.11f); M.Ball.Vel = FVector::ZeroVector; M.Phase = EPFPhase::Play;
			const float Ty = ((k / 3) % 2 == 0 ? 1.f : -1.f) * 2.9f;
			FPFPadInput Q; Q.Move = M.StickFor(FVector2D(G.X, Ty) - H.Pos);
			Q.bShootPressed = true; Q.bShootHeld = true; M.Tick(1.f / 120.f, Q); Q.bShootPressed = false;
			for (int32 f = 0; f < 30; ++f) M.Tick(1.f / 120.f, Q);
			Q.bShootHeld = false; Q.bShootReleased = true; M.Tick(1.f / 120.f, Q); Q.bShootReleased = false; Q.Move = FVector2D::ZeroVector;
			bool bDived = false, bStood = false; const int32 Sc0 = M.Score[H.Team];
			float MinD = 99, LineY = 0, LineZ = 0, DiveAt = -1, T = 0; FName Clip; const float V0 = M.Ball.Vel.Size();
			for (int32 f = 0; f < 300; ++f)
			{
				M.Tick(1.f / 120.f, Q); T += 1.f / 120.f;
				if (Kp >= 0) { const FPFPlayer& K = M.P[Kp]; if (K.Act == EPFAct::Dive && !K.DiveDir.IsNearlyZero()) { if (!bDived) { DiveAt = T; Clip = K.Clip; } bDived = true; } else if (K.Act != EPFAct::None && !bDived) bStood = true;
					MinD = FMath::Min(MinD, (float)FVector2D::Distance(K.Pos, FVector2D(M.Ball.Pos.X, M.Ball.Pos.Y))); }
				if (FMath::Abs(M.Ball.Pos.X) > FMath::Abs(G.X) - 0.2f && LineZ == 0) { LineY = M.Ball.Pos.Y; LineZ = M.Ball.Pos.Z; }
				if (M.Score[H.Team] != Sc0 || (M.Ball.Owner == Kp && Kp >= 0)) break;
			}
			if (Kp >= 0) UE_LOG(LogPF, Display, TEXT("PFSAVE   shot %2d at %.0f m/s, keeper %d (reach %.1f m): %s, dived at %.2f s (%s), nearest the ball %.2f m, ball at the line y %+.1f z %.1f"),
				k, V0, M.P[Kp].Setup.Stats.Keeping, M.P[Kp].A.KeeperReach, M.Score[H.Team] != Sc0 ? TEXT("GOAL") : M.Ball.Owner == Kp ? TEXT("caught") : TEXT("kept out"), DiveAt, *Clip.ToString(), MinD, LineY, LineZ);
			++N; Dives += bDived; Standing += (!bDived && bStood); Goals += M.Score[H.Team] != Sc0; Caught += (Kp >= 0 && M.Ball.Owner == Kp);
		}
		UE_LOG(LogPF, Display, TEXT("PFSAVE %d shots at the corners: keeper dived %d, stood %d, caught %d, goals %d"), N, Dives, Standing, Caught, Goals);
	}
	// v0.9.4 through-ball test (owner: "those through balls are broken ... the balls need to go much further ... the
	// player that is supposed to run along with it and retrieve it"): a midfielder in his own half or around halfway,
	// a full side against a full side in shape. Pad: Y with the stick up the pitch (every other one after an LB call);
	// AI: the carrier's through ball. Completed = a team-mate has it before the opponents do.
	for (int32 Mode = 0; Mode < 2; ++Mode)
	{
		int32 N = 0, Done = 0, ToMan = 0, Off = 0, ByKeeper = 0, ByDef = 0, Dead = 0, OutN = 0;
		float BallSum = 0, BallMax = 0, TSum = 0, RunSum = 0, LeadSum = 0, BoostSum = 0;
		for (int32 k = 0; k < 24; ++k)
		{
			FPFMatch M;
			FPFMatchSetup S; S.Seed = 700 + k;
			S.Home = PFWorld::FriendlyTeam(C[(k * 5) % C.Num()], false, 11 + k); S.Away = PFWorld::FriendlyTeam(C[(k * 7 + 3) % C.Num()], true, 13 + k);
			S.Home.bHuman = true; S.HalfSeconds = 150.f; S.StadiumSlug = C[0].Stadium;
			M.Init(S);
			{ FPFPadInput In; In.bPass = true; M.Tick(1.f / 120.f, In); }
			FPFPadInput In; for (int32 f = 0; f < 60; ++f) M.Tick(1.f / 120.f, In);
			const int32 Ad = M.AttackSign(0);
			int32 Cr = -1;
			for (const FPFPlayer& Q : M.P) if (Q.Team == 0 && Q.Setup.Role == EPFRole::MID && (Cr < 0 || FMath::Abs(Q.Setup.Slot.Y) < FMath::Abs(M.P[Cr].Setup.Slot.Y))) Cr = Q.Index;
			if (Cr < 0) continue;
			for (FPFPlayer& Q : M.P) { Q.Act = EPFAct::None; Q.PendingKick = EPFKick::None; Q.NoTouch = 0; Q.RunT = 0; }
			{
				FPFPlayer& H = M.P[Cr];
				H.Pos = FVector2D(Ad * (-10.f + (k % 4) * 5.f), ((k / 4) % 3 - 1) * 12.f); H.Vel = FVector2D::ZeroVector; H.Facing = FVector2D(Ad, 0);
				M.Human = Cr; M.Phase = EPFPhase::Play;
				M.Ball = FPFBall(); M.Ball.Owner = Cr; M.Ball.Pos = FVector(H.Pos, 0.11f); M.Ball.LastTouch = Cr; M.Ball.LastTeam = 0;
			}
			// both sides in shape round him (he shields it a moment, the stick idle)
			M.TestTakeShape();
			for (int32 f = 0; f < 30 && M.Ball.Owner == Cr; ++f) M.Tick(1.f / 120.f, In);
			if (M.Ball.Owner != Cr || M.Phase != EPFPhase::Play) continue;
			int32 Rc = -1;
			if (Mode == 0)
			{
				if (k % 2) { FPFPadInput Lb; Lb.bSwitch = true; M.Tick(1.f / 120.f, Lb); for (int32 f = 0; f < 50; ++f) M.Tick(1.f / 120.f, In); }
				if (M.Ball.Owner != Cr) continue;
				FPFPadInput Y; Y.bThrough = true; Y.Move = M.StickFor(FVector2D(Ad, 0)); M.Tick(1.f / 120.f, Y);
				Rc = M.P[Cr].KickTarget;
			}
			else
			{
				M.Setup.Home.bHuman = false; M.Human = -1;
				Rc = M.TestThroughBall(Cr, FVector2D(Ad, 0));
			}
			if (Rc < 0 || M.Ball.Owner == Cr) continue;
			const FVector2D From(M.Ball.Pos.X, M.Ball.Pos.Y), RcFrom = M.P[Rc].Pos;
			const float RcAhead = (RcFrom.X - From.X) * Ad, Lead = FVector2D::Distance(RcFrom, M.Ball.ThroughAim), Boost = M.P[Rc].BoostMul;
			float T = 0;
			TArray<FVector2D> DefFrom; for (const FPFPlayer& Q : M.P) DefFrom.Add(Q.Pos);
			float RcTop = 0, RcSum = 0; int32 RcN = 0; const float BallZ0 = M.Ball.Vel.Z, BallV0 = FVector2D(M.Ball.Vel.X, M.Ball.Vel.Y).Size();
			while (T < 7.f && M.Ball.Owner < 0 && M.Phase == EPFPhase::Play)
			{
				M.Tick(1.f / 120.f, FPFPadInput()); T += 1.f / 120.f;
				const float V = M.P[Rc].Vel.Size(); RcTop = FMath::Max(RcTop, V); RcSum += V; ++RcN;
			}
			UE_LOG(LogPF, Display, TEXT("PFTHRU   predicted margin %+.2f s (best ground %+.2f), runner there in %.2f s; ball launched %.1f m/s (up %.1f); runner top %.1f m/s, average %.1f (his top speed %.1f, sprint %.1f), boost left %.2f"), M.DbgThroughMargin, M.DbgMaxM, M.DbgThroughRun, BallV0, BallZ0, RcTop, RcN ? RcSum / RcN : 0.f, M.P[Rc].A.MaxSpeed, M.P[Rc].A.MaxSpeed * M.P[Rc].A.SprintMul, M.P[Rc].BoostT);
			++N;
			const FVector2D At(M.Ball.Pos.X, M.Ball.Pos.Y);
			const float BallD = FVector2D::Distance(From, At);
			const TCHAR* What = TEXT("dead");
			if (M.Ball.Owner >= 0 && M.P[M.Ball.Owner].Team == 0)
			{
				++Done; ToMan += M.Ball.Owner == Rc; BallSum += BallD; BallMax = FMath::Max(BallMax, BallD); TSum += T;
				RunSum += FVector2D::Distance(RcFrom, M.P[M.Ball.Owner].Pos); What = M.Ball.Owner == Rc ? TEXT("runner") : TEXT("team-mate");
			}
			else if (M.Ball.Owner >= 0)
			{
				if (M.P[M.Ball.Owner].bKeeper) { ++ByKeeper; What = TEXT("keeper"); } else { ++ByDef; What = TEXT("defender"); }
				UE_LOG(LogPF, Display, TEXT("PFTHRU   lost: the runner was %.1f m from the ball (ball z %.1f, %.1f from the space), the winner (role %d) ran %.1f m"), FVector2D::Distance(M.P[Rc].Pos, At), M.Ball.Pos.Z, FVector2D::Distance(At, M.Ball.ThroughAim), int32(M.P[M.Ball.Owner].Setup.Role), FVector2D::Distance(DefFrom[M.Ball.Owner], At));
			}
			else if (M.Banner == TEXT("OFFSIDE")) { ++Off; What = TEXT("offside"); }
			else if (M.Phase != EPFPhase::Play) { ++OutN; What = TEXT("out of play"); }
			else ++Dead;
			LeadSum += Lead; BoostSum += Boost;
			UE_LOG(LogPF, Display, TEXT("PFTHRU %s %2d: receiver %d (%.0f m ahead of the ball, burst x%.2f), played %.0f m beyond him -> %s after %.2f s, ball travelled %.1f m"), Mode == 0 ? TEXT("pad") : TEXT("ai "), k, Rc, RcAhead, Boost, Lead, What, T, BallD);
		}
		UE_LOG(LogPF, Display, TEXT("PFTHRU %s total: %d through balls, completed %d (%.0f%%, to the runner %d), cut out by a defender %d, keeper %d, offside %d, out %d, dead %d; completed ones travelled %.1f m on average (longest %.1f m) in %.2f s, the runner ran %.1f m; played %.1f m beyond the runner on average, burst x%.2f"),
			Mode == 0 ? TEXT("pad") : TEXT("ai"), N, Done, N ? 100.f * Done / N : 0.f, ToMan, ByDef, ByKeeper, Off, OutN, Dead, Done ? BallSum / Done : 0.f, BallMax, Done ? TSum / Done : 0.f, Done ? RunSum / Done : 0.f, N ? LeadSum / N : 0.f, N ? BoostSum / N : 0.f);
	}
	PFPassTests();   // v0.9.8
	PFAttackTests();   // v0.10
	QuitAt = Time + 0.5f;
}

void UPFFlow::RunMatchTest(int32 N)
{
	// headless AI v AI matches through the real match engine: goals, shots, restarts, stuck time
	const TArray<FPFClub>& C = PFWorld::Clubs();
	int32 TotGoals = 0, TotShots = 0, Stuck = 0;
	TMap<int32, int32> GoalHist;
	const double T0 = FPlatformTime::Seconds();
	FMemory::Memzero(FPFMatch::Diag, sizeof(FPFMatch::Diag));
	FMemory::Memzero(FPFMatch::AtkDiag, sizeof(FPFMatch::AtkDiag));
	FPFMatch::GameSpeed = FPFMatch::NormalSpeed;   // v0.10: the sim always runs at the default pace (not the player's option)
	for (int32 m = 0; m < N; ++m)
	{
		FPFMatch M;
		FPFMatchSetup S;
		S.Seed = 1000 + m;
		const int32 H = (m * 7) % C.Num(), A = (m * 13 + 5) % C.Num();
		S.Home = PFWorld::FriendlyTeam(C[H], false, S.Seed * 3);
		S.Away = PFWorld::FriendlyTeam(C[A], true, S.Seed * 5);
		S.bWatchOnly = true; S.HalfSeconds = 150.f; S.StadiumSlug = C[H].Stadium;
		M.Init(S);
		int32 Restarts = 0; float BallStill = 0, MaxStill = 0;
		EPFPhase Prev = M.Phase;
		for (int32 f = 0; f < 60 * 60 * 12 && !M.bFinished; ++f)
		{
			M.Tick(1.f / 60.f, FPFPadInput());
			if (M.Phase == EPFPhase::Restart && Prev != EPFPhase::Restart) Restarts++;
			Prev = M.Phase;
			if (M.Phase == EPFPhase::Play && M.Ball.Vel.Size() < 0.2f && M.Ball.Owner < 0) { BallStill += 1.f / 60.f; MaxStill = FMath::Max(MaxStill, BallStill); } else BallStill = 0;
		}
		if (MaxStill > 6.f) Stuck++;
		TotGoals += M.Score[0] + M.Score[1]; TotShots += M.Shots[0] + M.Shots[1];
		GoalHist.FindOrAdd(M.Score[0] + M.Score[1])++;
		UE_LOG(LogPF, Display, TEXT("PFSIM %2d  %s %d-%d %s  shots %d-%d  fouls %d yel %d red %d  restarts %d  maxstill %.1fs  finished %d  (str %d v %d)"), m, *S.Home.Short, M.Score[0], M.Score[1], *S.Away.Short,
			M.Shots[0], M.Shots[1], M.Fouls[0] + M.Fouls[1], M.Yellows[0] + M.Yellows[1], M.Reds[0] + M.Reds[1], Restarts, MaxStill, M.bFinished ? 1 : 0, C[H].Tier, C[A].Tier);
	}
	{
		const int64* Dg = FPFMatch::Diag; const float Nn = float(FMath::Max(1, N));
		UE_LOG(LogPF, Display, TEXT("PFSIM diag per match: shots %.1f (avg %.1f m, %.1f inside 12 m), standing saves %.1f, AI tackles %.1f won %.1f, slides %.1f, lost on cut %.1f, passes %.1f"),
			Dg[0] / Nn, Dg[0] ? Dg[1] / 10.f / Dg[0] : 0.f, Dg[9] / Nn, Dg[2] / Nn, Dg[3] / Nn, Dg[4] / Nn, Dg[5] / Nn, Dg[6] / Nn, Dg[7] / Nn);
		UE_LOG(LogPF, Display, TEXT("PFSIM keepers per match: dives %.1f, dive saves %.1f, goals with keeper off his line %.1f; raw cut/og %lld passes/gkgoal %lld"), Dg[8] / Nn, Dg[10] / Nn, Dg[11] / Nn, Dg[6], Dg[7]);
		UE_LOG(LogPF, Display, TEXT("PFSIM goals by last kick: none %lld pass %lld lob %lld through %lld shot %lld clear %lld volley %lld header %lld punt %lld throw %lld poke %lld"), Dg[16], Dg[17], Dg[18], Dg[19], Dg[20], Dg[21], Dg[22], Dg[23], Dg[24], Dg[25], Dg[31]);
		UE_LOG(LogPF, Display, TEXT("PFSIM goal ball: loose %lld, carried in by attacker %lld, by defender %lld; blocks per match %.1f"), Dg[26], Dg[27], Dg[28], Dg[29] / Nn);
		UE_LOG(LogPF, Display, TEXT("PFSIM laws per match: offsides %.1f, keeper 8 s corners %.1f, keeper hand pick-ups %.1f; kicks %.1f with %.1f players offside; goals ruled out %lld; shots gathered by keeper %.1f, shots wide/over %.1f, shots taken by a defender %.1f / by a team-mate %.1f"), Dg[32] / Nn, Dg[33] / Nn, Dg[34] / Nn, Dg[36] / Nn, Dg[35] / Nn, Dg[37], Dg[38] / Nn, Dg[39] / Nn, Dg[40] / Nn, Dg[41] / Nn);
		UE_LOG(LogPF, Display, TEXT("PFSIM passes per match %.1f: completed %.0f%%, intercepted %.0f%%, dead %.0f%%, avg length %.1f m; of the intercepted, never came within 2 m of the receiver %lld, taken in the air %lld"), Dg[42] / Nn, Dg[42] ? 100.f * Dg[43] / Dg[42] : 0.f, Dg[42] ? 100.f * Dg[45] / Dg[42] : 0.f, Dg[42] ? 100.f * Dg[46] / Dg[42] : 0.f, Dg[42] ? float(Dg[44]) / Dg[42] : 0.f, Dg[47], Dg[30]);
		{
			// v0.10 attack (owner: "nobody's making runs"): the side in possession, both teams together
			const int64* Ak = FPFMatch::AtkDiag; const float Tk = float(FMath::Max<int64>(1, Ak[0]));
			UE_LOG(LogPF, Display, TEXT("PFATTACK sim per match: line height in possession %+.1f m (mean of the ten, + = their half), width %.1f m, depth spread %.1f m, men ahead of the ball %.2f; runs seen %.1f; completed passes %.1f, to a man on a run %.1f (%.0f%%), received beyond their line %.1f; crosses %.1f won %.1f; AI runs started: in behind %.1f, check short %.1f, channel %.1f, overlap %.1f, third man %.1f, pass and go %.1f, recycled %.1f"),
				Ak[1] / 100.f / Tk, Ak[2] / 100.f / Tk, Ak[9] / 100.f / Tk, Ak[16] / Tk, Ak[3] / Nn, Ak[4] / Nn, Ak[5] / Nn, Ak[4] ? 100.f * Ak[5] / Ak[4] : 0.f, Ak[6] / Nn, Ak[7] / Nn, Ak[8] / Nn,
				Ak[10] / Nn, Ak[11] / Nn, Ak[12] / Nn, Ak[13] / Nn, Ak[14] / Nn, Ak[17] / Nn, Ak[15] / Nn);
			UE_LOG(LogPF, Display, TEXT("PFATTACK sim ball: in possession it is at %+.1f m on average, %.0f%% of the time in the final third, %.1f entries into it a match, %.2f shots an entry"),
				Ak[18] / 100.f / Tk, 100.f * Ak[19] / Tk, Ak[20] / Nn, Ak[20] ? float(Dg[0]) / Ak[20] : 0.f);
		}
		UE_LOG(LogPF, Display, TEXT("PFSIM presser: ticks %lld, avg dist to ball %.2f m, in reach %.1f%%, avg speed %.2f"), Dg[12], Dg[12] ? Dg[13] / 100.f / Dg[12] : 0.f, Dg[12] ? 100.f * Dg[14] / Dg[12] : 0.f, Dg[12] ? Dg[15] / 100.f / Dg[12] : 0.f);
	}
	UE_LOG(LogPF, Display, TEXT("PFSIM total: %d matches, %.2f goals/match, %.1f shots/match, stuck %d, %.1fs"), N, TotGoals / float(FMath::Max(1, N)), TotShots / float(FMath::Max(1, N)), Stuck, FPlatformTime::Seconds() - T0);
	QuitAt = Time + 0.5f;
}

void UPFFlow::RunCareerTest(int32 Seasons)
{
	// headless: N seasons with instant results, logging tables, promotions, the manager's moves, a save/load round trip
	UPFCareer* Car = UPFCareer::Get(this);
	if (!Car) return;
	FPFCareerSim& Sim = Car->Sim;
	const double T0 = FPlatformTime::Seconds();
	Sim.NewCareer(12345, TEXT("Test Gaffer"));
	UE_LOG(LogPF, Display, TEXT("PFTEST start: %s at %s (%s), clubs %d, people %d"), *Sim.S.ManagerName, *Sim.UserClub().Name, *Sim.DivisionName(Sim.UserClub().Division), Sim.S.Clubs.Num(), Sim.S.People.Num());
	for (int32 D = 0; D < 5; ++D) { int32 N = 0; for (const FPFCareerClub& C : Sim.S.Clubs) if (C.Division == D) N++; UE_LOG(LogPF, Display, TEXT("PFTEST div %d: %d clubs"), D, N); }
	int32 Guard = 0;
	for (int32 s = 0; s < Seasons && Guard < 4000; ++Guard)
	{
		if (Sim.S.Phase == TEXT("Offers") || Sim.S.bUnemployed)
		{
			UE_LOG(LogPF, Display, TEXT("PFTEST season end: %s"), Sim.S.History.Num() ? *Sim.S.History.Last() : TEXT("-"));
			for (int32 D = 0; D < 5; ++D) { const TArray<FPFTableRow> T = Sim.Table(D); }
			UE_LOG(LogPF, Display, TEXT("PFTEST rep %d board %d unemployed %d offers %d people %d"), Sim.S.Reputation, Sim.S.BoardConfidence, Sim.S.bUnemployed ? 1 : 0, Sim.S.Offers.Num(), Sim.S.People.Num());
			if (Sim.S.Phase == TEXT("Offers")) s++;
			Sim.AcceptOffer(Sim.S.bUnemployed ? 0 : (Sim.S.Offers.Num() ? (Sim.Rng.Unit() < 0.5f ? 0 : -1) : -1));
			if (Sim.S.UserClub >= 0) UE_LOG(LogPF, Display, TEXT("PFTEST now managing %s (%s)"), *Sim.UserClub().Name, *Sim.DivisionName(Sim.UserClub().Division));
			continue;
		}
		if (Sim.S.Matchday == 30 && !Sim.S.bUnemployed) for (int32 Id : TArray<int32>(Sim.UserClub().Squad)) if (const FPFPerson* P = Sim.Person(Id)) if (P->ContractYears <= 1 && P->Ovr() >= Sim.UserClub().Strength - 6) Sim.OfferContract(Id);
		Sim.PlayMatchday();
		if (Sim.S.Matchday == 20 && s == 0)
		{
			// a youth scouting trip and a user bid mid-season
			Sim.SendScout(0, EPFRegion::WestAfrica);
			const TArray<FPFTableRow> T = Sim.Table(Sim.UserClub().Division);
			UE_LOG(LogPF, Display, TEXT("PFTEST md20 leader %s %d pts"), *Sim.S.Clubs[T[0].Club].Name, T[0].Pts);
		}
	}
	// a youth match through the real engine (scouting path)
	{
		FPFMatchSetup Y = Sim.YouthMatchSetup(EPFRegion::WestAfrica, 99);
		Y.HalfSeconds = 60.f;
		FPFMatch M; M.Init(Y);
		for (int32 f = 0; f < 60 * 60 * 6 && !M.bFinished; ++f) M.Tick(1.f / 60.f, FPFPadInput());
		float Best = 0, Worst = 99; for (const FPFPlayerSetup& PS : Y.Home.XI) { Best = FMath::Max(Best, PFAbility(PS.Stats, false).MaxSpeed); Worst = FMath::Min(Worst, PFAbility(PS.Stats, false).MaxSpeed); }
		UE_LOG(LogPF, Display, TEXT("PFTEST youth match %s %d-%d %s finished %d, kids' top speed %.1f .. %.1f m/s"), *Y.Home.Short, M.Score[0], M.Score[1], *Y.Away.Short, M.bFinished ? 1 : 0, Worst, Best);
		// does the best prospect stand out? top-speed vs potential across the region's kids
		TArray<TPair<int32, float>> PV;
		for (const FPFPerson& Pp : Sim.S.People) if (Pp.bScoutedOnly && Pp.Club < 0 && Pp.Region == EPFRegion::WestAfrica) PV.Add({ Pp.Potential, PFAbility(Pp.Stats, false).MaxSpeed });
		PV.Sort([](const TPair<int32, float>& A, const TPair<int32, float>& B) { return A.Key > B.Key; });
		for (int32 i = 0; i < PV.Num(); i += FMath::Max(1, PV.Num() / 6)) UE_LOG(LogPF, Display, TEXT("PFTEST kid potential %d -> top speed %.1f m/s"), PV[i].Key, PV[i].Value);
	}
	// division sizes stay intact
	for (int32 D = 0; D < 5; ++D) { int32 N = 0; for (const FPFCareerClub& C : Sim.S.Clubs) if (C.Division == D) N++; UE_LOG(LogPF, Display, TEXT("PFTEST end div %d: %d clubs"), D, N); }
	const bool bSaved = Sim.Save(TEXT("pftest"));
	FPFCareerSim Copy; const bool bLoad = Copy.Load(TEXT("pftest"));
	UE_LOG(LogPF, Display, TEXT("PFTEST save %d load %d same people %d same season %d prospects %d"), bSaved, bLoad, Copy.S.People.Num() == Sim.S.People.Num(), Copy.S.Season == Sim.S.Season, Sim.S.Prospects.Num());
	int32 Wonder = 0; for (const FPFPerson& P : Sim.S.People) if (P.Age <= 19 && P.Potential >= 85) Wonder++;
	UE_LOG(LogPF, Display, TEXT("PFTEST wonderkids (<=19, pot 85+): %d   seconds %.1f"), Wonder, FPlatformTime::Seconds() - T0);
	QuitAt = Time + 0.5f;
}

void UPFFlow::LoadOptions()
{
	FString S;
	if (FFileHelper::LoadFileToString(S, *(FPaths::ProjectSavedDir() / TEXT("pf_options.txt"))))
	{
		TArray<FString> L; S.ParseIntoArrayLines(L);
		for (const FString& Ln : L)
		{
			FString K, V;
			if (!Ln.Split(TEXT("="), &K, &V)) continue;
			if (K == TEXT("music")) MusicVol = FMath::Clamp(FCString::Atoi(*V), 0, 10);
			if (K == TEXT("sfx")) SfxVol = FMath::Clamp(FCString::Atoi(*V), 0, 10);
			if (K == TEXT("halflen")) FriendlyLen = FMath::Clamp(FCString::Atoi(*V), 0, 3);
			if (K == TEXT("cam7")) CameraOpt = FMath::Clamp(FCString::Atoi(*V), 0, 5);   // v0.9.2: six zooms, WIDE the default (older keys are ignored)
			if (K == TEXT("size")) SizeOpt = FMath::Clamp(FCString::Atoi(*V), 0, 2);
			if (K == TEXT("speed")) SpeedOpt = FMath::Clamp(FCString::Atoi(*V), 0, 2);
		}
	}
	ApplyCamera();
}

void UPFFlow::ApplyCamera()
{
	// v0.9 (owner: "running across the field feels like an eternity", "hard to move the ball around"): players are drawn
	// at true size against the pitch (HeightScale 1, was 0.5), and the camera pulls back by the same amount, so the
	// sprites are exactly as big on screen as before but you see twice as much of the pitch (about 28 m across, ISS
	// shows 22 m) and the pitch is half as many screens long. -pfscale=0.5 brings back the v0.8 framing.
	// v0.9.5 (owner: players "look too thin on the field"; the ISS study: its players stand about 2x real size against
	// the pitch, ours 0.6x): a PLAYER SIZE option, MEDIUM the default. -pfscale=X overrides it (captures, A/B).
	// v0.9.6 (owner: LARGE made the players "way too big for the field"; "revert back to the same size ... and zoom in a
	// touch more"): the v0.9.1-v0.9.4 size again, no PLAYER SIZE option, and every camera 15% closer.
	float Hs = 0.6f;
	FParse::Value(FCommandLine::Get(), TEXT("pfscale="), Hs); Hs = FMath::Clamp(Hs, 0.25f, 2.f);
	FPFMatch::HeightScale = Hs;
	// v0.9 (owner: "5 different camera angles, 2 even closer and 3 even further away"): six zooms, the v0.9 default
	// third. Sprite pixels at 1080p: 3:1, 2.5:1, 2:1 (default), 1.67:1, 1.33:1, 1:1. -pfcam=N picks one (captures).
	static const float Z[] = { 4.68f * 1.15f, 3.90f * 1.15f, 3.12f * 1.15f, 2.60f * 1.15f, 2.08f * 1.15f, 1.56f * 1.15f };
	{ int32 Cf = -1; if (FParse::Value(FCommandLine::Get(), TEXT("pfcam="), Cf)) CameraOpt = FMath::Clamp(Cf, 0, 5); }
	// v0.10 GAME SPEED (owner: "a touch too fast, probably 15% too fast"): NORMAL is 15% slower than v0.9.9, FAST is v0.9.9
	{ static const float Sp[] = { 0.80f, FPFMatch::NormalSpeed, 1.05f }; FPFMatch::GameSpeed = Sp[FMath::Clamp(SpeedOpt, 0, 2)]; }
	FPFMatch::CamZoom = Z[FMath::Clamp(CameraOpt, 0, 5)];   // v0.9.1: the zoom frames the pitch; HeightScale alone sets the players' size (owner: "players 40% smaller")
}

void UPFFlow::SaveOptions()
{
	ApplyCamera();
	FFileHelper::SaveStringToFile(FString::Printf(TEXT("music=%d\nsfx=%d\nhalflen=%d\ncam7=%d\nsize=%d\nspeed=%d\n"), MusicVol, SfxVol, FriendlyLen, CameraOpt, SizeOpt, SpeedOpt), *(FPaths::ProjectSavedDir() / TEXT("pf_options.txt")));
	if (Music) Music->SetVolumeMultiplier((MusicName == TEXT("crowd_loop") ? 0.55f * SfxVol : 0.45f * MusicVol) / 10.f);
}

void UPFFlow::PlaySfx(FName Name, float Volume)
{
	Volume *= SfxVol / 10.f;
	if (Volume <= 0.f) return;
	if (Name.IsNone() || !WorldRef.IsValid()) return;
	TObjectPtr<USoundBase>* F = Sounds.Find(Name);
	if (!F) { USoundBase* S = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/PF/Audio/%s.%s"), *Name.ToString(), *Name.ToString())); F = &Sounds.Add(Name, S); }
	if (*F) UGameplayStatics::PlaySound2D(WorldRef.Get(), *F, Volume);
}

void UPFFlow::SetMusic(FName Name)
{
	if (Name == MusicName) return;
	MusicName = Name;
	if (Music) { Music->Stop(); Music = nullptr; }
	if (Name.IsNone() || !WorldRef.IsValid()) return;
	if (USoundBase* S = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/PF/Audio/%s.%s"), *Name.ToString(), *Name.ToString())))
	{
		Music = UGameplayStatics::CreateSound2D(WorldRef.Get(), S, (Name == TEXT("crowd_loop") ? 0.55f * SfxVol : 0.45f * MusicVol) / 10.f + 0.0001f, 1.f, 0.f, nullptr, true);
		if (Music) Music->Play();
	}
}

void UPFFlow::Go(EPFScreen S)
{
	Screen = S; Sel = 0; bPause = false; ScreenT = 0;
	if (S == EPFScreen::Main || S == EPFScreen::Title) PickBackground(TEXT("England"));
	SetMusic(S == EPFScreen::Title ? FName(TEXT("music_title")) : S == EPFScreen::Match ? FName(TEXT("crowd_loop")) : FName(TEXT("music_menu")));
}

UTexture2D* UPFFlow::GetTex(const FString& Path)
{
	if (TObjectPtr<UTexture2D>* T = TexCache.Find(Path)) return *T;
	UTexture2D* T = PF::LoadTexture(Path);
	TexCache.Add(Path, T);
	return T;
}

void UPFFlow::StartMatch(const FPFMatchSetup& S, TFunction<void(const FPFMatch&)> OnDone)
{
	if (!MatchActor && WorldRef.IsValid()) MatchActor = WorldRef->SpawnActor<APFMatchActor>();
	MatchActor->StartMatch(S);
	MatchDone = OnDone;
	Go(EPFScreen::PreMatch);
	PreMatchT = 0;
}

void UPFFlow::StartFriendly()
{
	const TArray<FPFClub>& C = PFWorld::Clubs();
	if (!C.Num()) return;
	const FPFClub& H = C[FriendlyHome];
	const FPFClub& A = C[FriendlyAway];
	FPFMatchSetup S;
	S.Seed = FPlatformTime::Cycles64();
	S.Home = PFWorld::FriendlyTeam(H, false, S.Seed * 31 + 1);
	S.Away = PFWorld::FriendlyTeam(A, false, S.Seed * 31 + 2);
	S.Away.Kit = PFWorld::AwayKitFor(S.Home.Kit, A.Kits, S.Away.bAwayKit);
	S.Away.bKeeper2 = PFWorld::KitsClash(S.Home.KeeperKit, A.Kits.Keeper1);
	S.Away.KeeperKit = S.Away.bKeeper2 ? A.Kits.Keeper2 : A.Kits.Keeper1;
	S.Home.bHuman = FriendlySide == 0;
	S.Away.bHuman = FriendlySide == 1;
	S.bWatchOnly = FriendlySide == 2;
	S.StadiumSlug = H.Stadium;
	static const TCHAR* Stages[] = { TEXT("Daylight"), TEXT("Sunset"), TEXT("Floodlit_Night") };
	S.Stage = Stages[FriendlyStage % 3];
	static const float Len[] = { 90.f, 150.f, 240.f, 300.f };
	S.HalfSeconds = Len[FriendlyLen % 4];
	StartMatch(S, [this](const FPFMatch&) { Go(EPFScreen::Result); });
}

// ------------------------------------------------------------------------------------------------ tick
void UPFFlow::Tick(float Dt, const FPFPadState& Pad)
{
	Time += Dt;
	// capture mode (-pfshots=N -pfshotgap=S): timed screenshots to Saved/Screenshots, then quit
	if (ShotsLeft > 0 && Time > NextShot)
	{
		NextShot = Time + ShotGap;
		FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("pf_%02d.png"), ShotIndex++), false, false);
		if (--ShotsLeft == 0) QuitAt = Time + 1.5f;
	}
	if (QuitAt > 0 && Time > QuitAt && WorldRef.IsValid()) { QuitAt = -1; UKismetSystemLibrary::QuitGame(WorldRef.Get(), nullptr, EQuitPreference::Quit, false); }
	const TArray<FPFClub>& C = PFWorld::Clubs();
	switch (Screen)
	{
	case EPFScreen::Title:
		if (Pad.pA || Pad.pStart) Go(EPFScreen::Main);
		break;
	case EPFScreen::Main:
	{
		TArray<FPFMenuItem> Items;
		Items.Add({ TEXT("CAREER"), nullptr, nullptr, [this]() { Go(EPFScreen::Career); if (UPFCareer* Car = UPFCareer::Get(this)) Car->Enter(); } });
		Items.Add({ TEXT("FRIENDLY MATCH"), nullptr, nullptr, [this]() { Go(EPFScreen::Friendly); } });
		Items.Add({ TEXT("OPTIONS"), nullptr, nullptr, [this]() { Go(EPFScreen::Options); } });
		Items.Add({ TEXT("QUIT"), nullptr, nullptr, [this]() { if (WorldRef.IsValid()) UKismetSystemLibrary::QuitGame(WorldRef.Get(), nullptr, EQuitPreference::Quit, false); } });
		MenuInput(Items, Sel, Pad);
		if (Pad.pB) Go(EPFScreen::Title);
		break;
	}
	case EPFScreen::Friendly:
	{
		TArray<FPFMenuItem> Items;
		const int32 N = C.Num();
		// v0.9.4: the two cyclers skip each other's club (a club can't play itself)
		Items.Add({ TEXT("HOME"), [this, &C]() { return C.IsValidIndex(FriendlyHome) ? C[FriendlyHome].Name : FString(); }, [this, N](int32 D) { FriendlyHome = (FriendlyHome + D + N) % N; if (FriendlyHome == FriendlyAway && N > 1) FriendlyHome = (FriendlyHome + D + N) % N; } });
		Items.Add({ TEXT("AWAY"), [this, &C]() { return C.IsValidIndex(FriendlyAway) ? C[FriendlyAway].Name : FString(); }, [this, N](int32 D) { FriendlyAway = (FriendlyAway + D + N) % N; if (FriendlyAway == FriendlyHome && N > 1) FriendlyAway = (FriendlyAway + D + N) % N; } });
		Items.Add({ TEXT("YOU PLAY"), [this]() { return FString(FriendlySide == 0 ? TEXT("HOME") : FriendlySide == 1 ? TEXT("AWAY") : TEXT("WATCH")); }, [this](int32 D) { FriendlySide = (FriendlySide + D + 3) % 3; } });
		Items.Add({ TEXT("KICK-OFF"), [this]() { return FString(FriendlyStage == 0 ? TEXT("3 PM") : FriendlyStage == 1 ? TEXT("EVENING") : TEXT("FLOODLIGHTS")); }, [this](int32 D) { FriendlyStage = (FriendlyStage + D + 3) % 3; } });
		Items.Add({ TEXT("HALF LENGTH"), [this]() { static const TCHAR* L[] = { TEXT("1.5 MIN"), TEXT("2.5 MIN"), TEXT("4 MIN"), TEXT("5 MIN") }; return FString(L[FriendlyLen % 4]); }, [this](int32 D) { FriendlyLen = (FriendlyLen + D + 4) % 4; SaveOptions(); } });   // v0.9.4: kept, as in Options
		Items.Add({ TEXT("PLAY"), nullptr, nullptr, [this]() { StartFriendly(); } });
		MenuInput(Items, Sel, Pad);
		if (Pad.pB) Go(EPFScreen::Main);
		break;
	}
	case EPFScreen::Match:
	{
		if (!MatchActor) break;
		FPFMatch& M = MatchActor->Match;
		if (Pad.pStart) { bPause = !bPause; PauseSel = 0; }
		if (bPause)
		{
			TArray<FPFMenuItem> Items;
			Items.Add({ TEXT("RESUME"), nullptr, nullptr, [this]() { bPause = false; } });
			Items.Add({ TEXT("QUIT MATCH"), nullptr, nullptr, [this, &M]() { bPause = false; M.bQuit = true; M.bFinished = true; } });
			MenuInput(Items, PauseSel, Pad);
			if (Pad.pB) bPause = false;
			break;
		}
		FPFPadInput In;
		In.Move = Pad.Move; In.bSprint = Pad.bSprint;
		// v0.7 (owner): A pass, B shoot (hold for power), X cross, Y through ball, LB + B chip, LB on its own switches; right stick flicks = skills
		In.bPass = Pad.pA; In.bLob = Pad.pX; In.bThrough = Pad.pY; In.bSwitch = Pad.pLB;
		In.bPassHeld = Pad.bA; In.bThroughHeld = Pad.bY; In.bLobHeld = Pad.bX;   // v0.10: hold X to fill the cross bar   // v0.9.8: hold A / Y to fill the pass power bar
		In.bShootHeld = Pad.bB; In.bShootPressed = Pad.pB; In.bShootReleased = Pad.rB; In.bChipMod = Pad.bLB;
		In.bFeintL = Pad.pFlickL; In.bFeintR = Pad.pFlickR; In.Look = Pad.Look;   // v0.10: right stick up / down = set-piece height
		const int32 Sub = (bManageView && M.Highlight <= 0.f) ? 8 : 1;
		for (int32 k = 0; k < Sub && !M.bFinished; ++k) M.Tick(Dt, k == 0 ? In : FPFPadInput());
		// sounds (muted while a managed match runs at speed, except the big moments)
		TSet<FName> Played;
		for (FName N : M.Sfx)
		{
			if (N.IsNone() || Played.Contains(N)) continue;
			Played.Add(N);
			if (Sub > 1 && !N.ToString().StartsWith(TEXT("whistle")) && N != FName(TEXT("crowd_goal")) && N != FName(TEXT("net"))) continue;
			PlaySfx(N, N.ToString().StartsWith(TEXT("crowd")) ? 0.8f : 0.7f);
		}
		M.Sfx.Reset();
		if (M.bFinished)
		{
			auto Done = MatchDone; MatchDone = nullptr;
			if (Done) Done(M); else Go(EPFScreen::Result);
		}
		break;
	}
	case EPFScreen::Result:
		ScreenT += Dt;
		if (ScreenT > 1.f && (Pad.pA || Pad.pB || Pad.pStart)) Go(EPFScreen::Main);   // v0.9.4: a pass pressed at the whistle no longer skips it
		{
			// long playtests (-pfautopad): straight into the next friendly, new clubs, ground and kick-off time
			static const bool bAuto = FParse::Param(FCommandLine::Get(), TEXT("pfautopad"));
			static float ResT = 0; ResT += Dt;
			if (bAuto && ResT > 3.f && C.Num() > 1)
			{
				ResT = 0;
				FriendlyHome = FMath::RandRange(0, C.Num() - 1); FriendlyAway = (FriendlyHome + 1 + FMath::RandRange(0, C.Num() - 2)) % C.Num();
				FriendlyStage = FMath::RandRange(0, 2); FriendlySide = 0;
				StartFriendly();
			}
		}
		break;
	case EPFScreen::Options:
	{
		TArray<FPFMenuItem> Items;
		Items.Add({ TEXT("MUSIC"), nullptr, [this](int32 D) { MusicVol = FMath::Clamp(MusicVol + D, 0, 10); SaveOptions(); } });
		Items.Add({ TEXT("SOUND EFFECTS & CROWD"), nullptr, [this](int32 D) { SfxVol = FMath::Clamp(SfxVol + D, 0, 10); SaveOptions(); PlaySfx(TEXT("ui_select")); } });
		Items.Add({ TEXT("HALF LENGTH"), nullptr, [this](int32 D) { FriendlyLen = (FriendlyLen + D + 4) % 4; SaveOptions(); } });
		Items.Add({ TEXT("CAMERA"), nullptr, [this](int32 D) { CameraOpt = (CameraOpt + D + 6) % 6; SaveOptions(); ApplyCamera(); } });
		Items.Add({ TEXT("GAME SPEED"), nullptr, [this](int32 D) { SpeedOpt = (SpeedOpt + D + 3) % 3; SaveOptions(); } });
		Items.Add({ TEXT("BACK"), nullptr, nullptr, [this]() { Go(EPFScreen::Main); } });
		MenuInput(Items, Sel, Pad);
		if (Pad.pB) Go(EPFScreen::Main);
		break;
	}
	case EPFScreen::PreMatch:
		PreMatchT += Dt;
		if (Pad.pB && !MatchDone && ShotsLeft <= 0) { Go(EPFScreen::Friendly); break; }   // v0.9.4: back out of a friendly before kick-off
		if (Pad.pA || Pad.pStart || (MatchActor && MatchActor->Match.Setup.bWatchOnly && PreMatchT > 2.f) || ShotsLeft > 0 || ((FParse::Param(FCommandLine::Get(), TEXT("pfautopad")) || FCString::Strifind(FCommandLine::Get(), TEXT("pfrestart=")) != nullptr) && PreMatchT > 2.f)) { Go(EPFScreen::Match); PlaySfx(TEXT("whistle_short"), 0.7f); }
		break;
	case EPFScreen::Career:
		if (UPFCareer* Car = UPFCareer::Get(this)) Car->Tick(Dt, Pad);
		break;
	default: break;
	}
}

bool UPFFlow::MenuInput(TArray<FPFMenuItem>& Items, int32& S, const FPFPadState& Pad)
{
	if (!Items.Num()) return false;
	S = FMath::Clamp(S, 0, Items.Num() - 1);
	if (Pad.pDown) { S = (S + 1) % Items.Num(); PlaySfx(TEXT("ui_move"), 0.5f); }
	if (Pad.pUp) { S = (S - 1 + Items.Num()) % Items.Num(); PlaySfx(TEXT("ui_move"), 0.5f); }
	FPFMenuItem& It = Items[S];
	if (It.OnChange && (Pad.pLeft || Pad.pRight)) It.OnChange(Pad.pLeft ? -1 : 1);
	if (Pad.pA && It.bEnabled)
	{
		PlaySfx(TEXT("ui_select"), 0.6f);
		if (It.OnSelect) { It.OnSelect(); return true; }
		if (It.OnChange) It.OnChange(1);
	}
	return false;
}

// ------------------------------------------------------------------------------------------------ drawing
void UPFFlow::Text(UCanvas* C, const FString& S, float X, float Y, float Scale, FLinearColor Col, EPFAlign Align, bool bSquad)
{
	const FPFFont& F = bSquad ? FontSquad : FontScore;
	if (!F.Tex) { C->SetDrawColor(Col.ToFColor(true)); C->DrawText(GEngine->GetMediumFont(), S, X, Y, Scale * U, Scale * U); return; }
	const float Sc = FMath::Max(1.f, FMath::RoundToFloat(Scale * U * 2.f)) / 2.f;   // half-pixel steps keep glyphs crisp
	float W = F.Measure(S, Sc);
	float Cx = Align == EPFAlign::Center ? X - W * 0.5f : Align == EPFAlign::Right ? X - W : X;
	Cx = FMath::RoundToFloat(Cx); Y = FMath::RoundToFloat(Y);
	for (TCHAR Ch : S)
	{
		if (Ch == ' ') { Cx += F.Space * Sc; continue; }
		const FPFFont::FGlyph* G = F.Glyphs.Find((int32)Ch);
		if (!G) G = F.Glyphs.Find((int32)FChar::ToUpper(Ch));
		if (!G) { Cx += F.Space * Sc; continue; }
		FCanvasTileItem T(FVector2D(Cx - G->InkX * Sc, Y), F.Tex->GetResource(), FVector2D(G->W * Sc, G->H * Sc),
			FVector2D(G->X / F.Sheet.X, G->Y / F.Sheet.Y), FVector2D((G->X + G->W) / F.Sheet.X, (G->Y + G->H) / F.Sheet.Y), Col);
		T.BlendMode = SE_BLEND_Translucent;
		C->DrawItem(T);
		Cx += (G->Adv + 1) * Sc;
	}
}

void UPFFlow::Tex(UCanvas* C, const FString& Path, float X, float Y, float W, float H, FLinearColor Tint)
{
	UTexture2D* T = GetTex(Path);
	if (!T) return;
	FCanvasTileItem It(FVector2D(FMath::RoundToFloat(X), FMath::RoundToFloat(Y)), T->GetResource(), FVector2D(FMath::RoundToFloat(W), FMath::RoundToFloat(H)), Tint);
	It.BlendMode = SE_BLEND_Translucent;
	C->DrawItem(It);
}

void UPFFlow::Box(UCanvas* C, float X, float Y, float W, float H, FLinearColor Col)
{
	FCanvasTileItem It(FVector2D(X, Y), GWhiteTexture, FVector2D(W, H), Col);
	It.BlendMode = SE_BLEND_Translucent;
	C->DrawItem(It);
}

void UPFFlow::DrawBackground(UCanvas* C, float Dim)
{
	if (!BgPath.IsEmpty()) Tex(C, BgPath, 0, 0, C->ClipX, C->ClipY);
	else Box(C, 0, 0, C->ClipX, C->ClipY, FLinearColor(0.02f, 0.02f, 0.06f, 1));
	if (Dim > 0) Box(C, 0, 0, C->ClipX, C->ClipY, FLinearColor(0, 0, 0, Dim));
}

void UPFFlow::DrawMenu(UCanvas* C, const FString& Title, TArray<FPFMenuItem>& Items, int32 S, float X, float Y, float W)
{
	const float RowH = 58 * U;
	Box(C, X - 24 * U, Y - 80 * U, W + 48 * U, RowH * Items.Num() + 110 * U, FLinearColor(0.01f, 0.02f, 0.08f, 0.82f));
	Box(C, X - 24 * U, Y - 80 * U, W + 48 * U, 4 * U, FLinearColor(0.9f, 0.15f, 0.2f, 1));
	Text(C, Title, X, Y - 66 * U, 1.2f, FLinearColor(1, 0.85f, 0.3f));
	for (int32 i = 0; i < Items.Num(); ++i)
	{
		const float RY = Y + i * RowH;
		const bool bSel = i == S;
		if (bSel)
		{
			const float Pulse = 0.75f + 0.25f * FMath::Sin(Time * 6.f);
			Box(C, X - 12 * U, RY - 4 * U, W + 24 * U, RowH - 8 * U, FLinearColor(0.15f, 0.3f, 0.9f, 0.55f * Pulse + 0.2f));
		}
		const FLinearColor Col = !Items[i].bEnabled ? FLinearColor(0.4f, 0.4f, 0.45f) : bSel ? FLinearColor::White : FLinearColor(0.75f, 0.8f, 0.95f);
		Text(C, Items[i].Label, X, RY, 1.f, Col);
		if (Items[i].Value)
		{
			const FString V = Items[i].Value();
			Text(C, bSel && Items[i].OnChange ? FString::Printf(TEXT("< %s >"), *V) : V, X + W, RY, 1.f, bSel ? FLinearColor(1, 0.9f, 0.4f) : Col, EPFAlign::Right);
		}
	}
}

void UPFFlow::DrawTitle(UCanvas* C)
{
	DrawBackground(C, 0.15f);
	const float CX = C->ClipX * 0.5f, CY = C->ClipY * 0.5f;
	Box(C, 0, CY - 150 * U, C->ClipX, 300 * U, FLinearColor(0, 0, 0.03f, 0.7f));
	Text(C, TEXT("PROPER FOOTBALL"), CX, CY - 110 * U, 4.f, FLinearColor(1, 1, 1), EPFAlign::Center);
	Text(C, TEXT("ENGLISH FOOTBALL  -  EVERY TIER  -  NO END"), CX, CY + 40 * U, 1.f, FLinearColor(0.6f, 0.8f, 1.f), EPFAlign::Center, true);
	if (FMath::Fmod(Time, 1.f) < 0.65f) Text(C, TEXT("PRESS A"), CX, CY + 160 * U, 1.4f, FLinearColor(1, 0.85f, 0.25f), EPFAlign::Center);
	static const FString Ver = []() { FString V = TEXT("0.9.4"); GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectVersion"), V, GGameIni); return TEXT("v") + V; }();   // v0.9.4: was a fixed "v0.1.0"
	Text(C, Ver, C->ClipX - 30 * U, C->ClipY - 50 * U, 0.7f, FLinearColor(0.6f, 0.6f, 0.7f), EPFAlign::Right, true);
}

void UPFFlow::DrawMatchHUD(UCanvas* C, const FPFMatch& M)
{
	const float W = C->ClipX, H = C->ClipY;
	// v0.10 goal show: during the replay only the REPLAY tag (and the fade) are drawn (PFPresent.cpp)
	if (M.InReplay()) { float Ny = 0.f; PFPresent::DrawGoalOverlay(C, M, U, Ny); return; }
	auto Outlined = [&](const FString& S, float TX, float TY, float Sc, FLinearColor Col, EPFAlign Al)
	{
		const float O = FMath::Max(1.f, FMath::RoundToFloat(2 * U));
		for (int32 k = 0; k < 4; ++k) Text(C, S, TX + (k == 0 ? -O : k == 1 ? O : 0.f), TY + (k == 2 ? -O : k == 3 ? O : 0.f), Sc, FLinearColor(0.05f, 0.04f, 0.1f, 0.95f), Al);
		Text(C, S, TX, TY, Sc, Col, Al);
	};
	// v0.10 (ISS layout, fable advice v2 section 8): top-left straight on the grass, no box - badge, code under it, the
	// score in yellow with a dark edge, badge, code
	const float X = 36 * U, Y = 24 * U;
	Tex(C, M.Setup.Home.BadgePath, X, Y, 48 * U, 48 * U);
	Outlined(M.Setup.Home.Short, X + 24 * U, Y + 52 * U, 0.6f, FLinearColor::White, EPFAlign::Center);
	Outlined(FString::Printf(TEXT("%d - %d"), M.Score[0], M.Score[1]), X + 142 * U, Y + 6 * U, 1.3f, FLinearColor(1.f, 0.88f, 0.2f), EPFAlign::Center);
	Tex(C, M.Setup.Away.BadgePath, X + 236 * U, Y, 48 * U, 48 * U);
	Outlined(M.Setup.Away.Short, X + 260 * U, Y + 52 * U, 0.6f, FLinearColor::White, EPFAlign::Center);
	// v0.9.2 (owner: "overall of the players, overall of the teams ... who's good, who sucks"): team ratings, small, under
	// the score; each player's rating over his head (red under 55, amber to 69, green 70 and up)
	auto RCol = [](int32 R) { return R >= 70 ? FLinearColor(0.45f, 1.f, 0.45f) : R >= 55 ? FLinearColor(1.f, 0.8f, 0.3f) : FLinearColor(1.f, 0.38f, 0.32f); };
	{
		const int32 H0 = M.TeamOverall(0), A0 = M.TeamOverall(1);
		Outlined(TEXT("OVR"), X + 142 * U, Y + 56 * U, 0.5f, FLinearColor(0.8f, 0.82f, 0.9f), EPFAlign::Center);
		Outlined(FString::FromInt(H0), X + 100 * U, Y + 56 * U, 0.5f, RCol(H0), EPFAlign::Center);
		Outlined(FString::FromInt(A0), X + 184 * U, Y + 56 * U, 0.5f, RCol(A0), EPFAlign::Center);
	}
	// the clock, top-right, with a small clock face
	{
		const int32 Min = FMath::FloorToInt(M.MatchClock), Sec = FMath::FloorToInt(FMath::Fmod(M.MatchClock, 1.f) * 60.f);
		const float CX = W - 40 * U, CY = Y + 8 * U, R = 15 * U, O = FMath::Max(1.f, FMath::RoundToFloat(3 * U));
		Outlined(FString::Printf(TEXT("%02d:%02d"), Min, Sec), CX, CY, 1.1f, FLinearColor::White, EPFAlign::Right);
		const float IX = CX - 170 * U, IY = CY + 22 * U;
		Box(C, IX - R - O, IY - R - O, 2 * (R + O), 2 * (R + O), FLinearColor(0.05f, 0.04f, 0.1f, 0.9f));
		Box(C, IX - R, IY - R, 2 * R, 2 * R, FLinearColor(0.95f, 0.95f, 0.98f, 1.f));
		Box(C, IX - O * 0.5f, IY - R * 0.8f, O, R * 0.8f, FLinearColor(0.1f, 0.1f, 0.2f, 1.f));
		Box(C, IX, IY - O * 0.5f, R * 0.6f, O, FLinearColor(0.1f, 0.1f, 0.2f, 1.f));
	}
	for (int32 i = 0; i < M.P.Num() && i < M.HeadScreen.Num() && !bDotsNow; ++i)   // v0.9.4: not over the manage view's dots
	{
		const FVector2D Hs = M.HeadScreen[i];
		if (Hs.X < -1e8f || M.P[i].bSentOff) continue;
		const int32 R = M.P[i].Setup.Stats.Overall(M.P[i].bKeeper);
		Text(C, FString::Printf(TEXT("%d"), R), Hs.X, Hs.Y - 26 * U, 0.55f, RCol(R), EPFAlign::Center, true);
	}
	// v0.9.8 pass power bar, over the controlled player while A / Y is held: three bands (a tap: the nearest man that way;
	// medium; long and driven), ticks where they meet; green for a pass, cyan for a through ball
	if (M.bPassCharging && M.Human >= 0 && M.HeadScreen.IsValidIndex(M.Human) && M.HeadScreen[M.Human].X > -1e8f && !bDotsNow && !M.Setup.bWatchOnly)
	{
		const FVector2D Hs = M.HeadScreen[M.Human];
		const float BW = FMath::RoundToFloat(72 * U), BH = FMath::RoundToFloat(10 * U), Px = FMath::Max(1.f, FMath::RoundToFloat(2 * U));
		const float X0 = FMath::RoundToFloat(Hs.X - BW * 0.5f), Y0 = FMath::RoundToFloat(Hs.Y - 50 * U);
		const bool bCross = M.bPassChargeLob;   // v0.10: X's cross bar (orange): low / head height / high and long
		const float T1 = FPFMatch::PassTapT / M.BarFullT(), T2 = (bCross ? FPFMatch::CrossMidT : 0.28f) / M.BarFullT();
		const float Ch = FMath::Clamp(M.PassCharge, 0.f, 1.f);
		const bool bThru = M.bPassChargeThrough && !bCross;
		FLinearColor Band = Ch < T1 ? (bThru ? FLinearColor(0.55f, 0.95f, 1.f) : FLinearColor(0.55f, 1.f, 0.45f))
			: Ch < T2 ? (bThru ? FLinearColor(0.3f, 0.75f, 1.f) : FLinearColor(1.f, 0.88f, 0.25f)) : (bThru ? FLinearColor(0.35f, 0.45f, 1.f) : FLinearColor(1.f, 0.45f, 0.2f));
		if (bCross) Band = Ch < T1 ? FLinearColor(1.f, 0.8f, 0.45f) : Ch < T2 ? FLinearColor(1.f, 0.6f, 0.15f) : FLinearColor(0.95f, 0.32f, 0.1f);
		Box(C, X0 - Px, Y0 - Px, BW + 2 * Px, BH + 2 * Px, FLinearColor(0.f, 0.f, 0.f, 0.85f));
		Box(C, X0, Y0, BW, BH, FLinearColor(0.08f, 0.1f, 0.16f, 0.9f));
		Box(C, X0, Y0, FMath::RoundToFloat(BW * FMath::Max(Ch, 0.04f)), BH, Band);
		Box(C, X0, Y0, FMath::RoundToFloat(BW * FMath::Max(Ch, 0.04f)), Px, FLinearColor(1.f, 1.f, 1.f, 0.35f));   // top highlight
		Box(C, FMath::RoundToFloat(X0 + BW * T1), Y0, Px, BH, FLinearColor(0.f, 0.f, 0.f, 0.85f));
		Box(C, FMath::RoundToFloat(X0 + BW * T2), Y0, Px, BH, FLinearColor(0.f, 0.f, 0.f, 0.85f));
	}
	// v0.10 set pieces: over the taker, the bar (the pass bar's look, in orange) and a three-step height meter beside it;
	// a line of the controls under the minimap's top edge
	if (M.bSPAim && M.Human >= 0 && M.HeadScreen.IsValidIndex(M.Human) && M.HeadScreen[M.Human].X > -1e8f && !bDotsNow && !M.Setup.bWatchOnly)
	{
		const FVector2D Hs = M.HeadScreen[M.Human];
		const bool bThrow = M.SetPieceKind() == EPFRestart::ThrowIn;
		const float BW = FMath::RoundToFloat(72 * U), BH = FMath::RoundToFloat(10 * U), Px = FMath::Max(1.f, FMath::RoundToFloat(2 * U));
		const float X0 = FMath::RoundToFloat(Hs.X - BW * 0.5f), Y0 = FMath::RoundToFloat(Hs.Y - 50 * U);
		const float Ch = FMath::Clamp(M.SPCharge, 0.f, 1.f);
		const FLinearColor Fill = Ch < 0.5f ? FLinearColor(1.f, 0.78f, 0.2f) : Ch < 0.85f ? FLinearColor(1.f, 0.55f, 0.1f) : FLinearColor(1.f, 0.3f, 0.12f);
		Box(C, X0 - Px, Y0 - Px, BW + 2 * Px, BH + 2 * Px, FLinearColor(0.f, 0.f, 0.f, 0.85f));
		Box(C, X0, Y0, BW, BH, FLinearColor(0.08f, 0.1f, 0.16f, 0.9f));
		if (M.bSPCharging) { Box(C, X0, Y0, FMath::RoundToFloat(BW * FMath::Max(Ch, 0.04f)), BH, Fill); Box(C, X0, Y0, FMath::RoundToFloat(BW * FMath::Max(Ch, 0.04f)), Px, FLinearColor(1.f, 1.f, 1.f, 0.35f)); }
		if (M.SetPieceKind() == EPFRestart::Penalty || M.bSPShot) Box(C, FMath::RoundToFloat(X0 + BW * 0.8f), Y0, Px, BH, FLinearColor(1.f, 0.2f, 0.15f, 0.95f));   // past here it can fly over
		if (!bThrow)
		{
			// LOW / MED / HIGH: three steps rising to the right of the bar, lit up to the chosen one
			const float SX = X0 + BW + 8 * U, SW = FMath::RoundToFloat(7 * U);
			for (int32 k = 0; k < 3; ++k)
			{
				const float SH = FMath::RoundToFloat((6 + 6 * k) * U), XX = FMath::RoundToFloat(SX + k * (SW + 3 * U)), YY = Y0 + BH - SH;
				Box(C, XX - Px, YY - Px, SW + 2 * Px, SH + 2 * Px, FLinearColor(0.f, 0.f, 0.f, 0.85f));
				Box(C, XX, YY, SW, SH, k <= M.SPHeight ? FLinearColor(1.f, 0.55f, 0.08f) : FLinearColor(0.2f, 0.22f, 0.28f, 0.9f));
			}
			static const TCHAR* HN[3] = { TEXT("LOW"), TEXT("MED"), TEXT("HIGH") };
			const FString Lbl = M.bSPShot ? FString::Printf(TEXT("SHOT  %s"), HN[FMath::Clamp(M.SPHeight, 0, 2)]) : FString(HN[FMath::Clamp(M.SPHeight, 0, 2)]);
			Text(C, Lbl, Hs.X, Y0 - 26 * U, 0.55f, FLinearColor(1.f, 0.75f, 0.3f), EPFAlign::Center, true);
		}
		const FString Help = bThrow ? FString(TEXT("STICK AIM    TAP A SHORT    HOLD A LONGER"))
			: M.SetPieceKind() == EPFRestart::Penalty ? FString(TEXT("STICK AIM    Y / X HEIGHT    HOLD B POWER"))
			: FString(TEXT("STICK AIM    Y / X HEIGHT    HOLD B POWER    TAP A SHORT"));
		const float HW = 760 * U, HY = 120 * U;   // top centre: the ground below the taker stays clear
		Box(C, W * 0.5f - HW * 0.5f, HY, HW, 36 * U, FLinearColor(0.02f, 0.04f, 0.14f, 0.75f));
		Text(C, Help, W * 0.5f, HY + 8 * U, 0.6f, FLinearColor(1.f, 0.8f, 0.4f), EPFAlign::Center, true);
	}
	// banner
	if (M.BannerT > 0 && !M.Banner.IsEmpty() && !(M.GoalT >= 0.f && M.Banner == TEXT("GOAL!")))   // v0.10: the goal show draws its own letters
	{
		const float Big = M.Banner == TEXT("GOAL!") ? 3.f : 1.4f;
		Box(C, W * 0.5f - 400 * U, H * 0.22f - 10 * U, 800 * U, (Big * 38 + 30) * U, FLinearColor(0.02f, 0.03f, 0.1f, 0.75f));
		Text(C, M.Banner, W * 0.5f, H * 0.22f, Big, M.Banner == TEXT("GOAL!") ? FLinearColor(1, 0.85f, 0.2f) : FLinearColor::White, EPFAlign::Center);
	}
	// v0.10 (ISS layout): one translucent grey band across the bottom - my man left (name, position and number, stamina),
	// the radar in the middle standing up out of it, the opponent nearest the ball right
	{
		const float BH = FMath::RoundToFloat(H * 0.09f), BY = H - BH;
		Box(C, 0, BY, W, BH, FLinearColor(0.5f, 0.5f, 0.55f, 0.55f));
		Box(C, 0, BY, W, FMath::Max(1.f, 2 * U), FLinearColor(1.f, 1.f, 1.f, 0.35f));
		auto Nearest = [&](int32 Team) { int32 Bi = -1; float Bd = 1e9f; for (const FPFPlayer& Q : M.P) if (Q.Team == Team && !Q.bSentOff) { const float D = FVector2D::DistSquared(Q.Pos, FVector2D(M.Ball.Pos.X, M.Ball.Pos.Y)); if (D < Bd) { Bd = D; Bi = Q.Index; } } return Bi; };
		const bool bPad = M.Human >= 0 && !M.Setup.bWatchOnly && M.P.IsValidIndex(M.Human);
		const int32 L = bPad ? M.Human : Nearest(0);
		const int32 R = M.P.IsValidIndex(L) ? Nearest(1 - M.P[L].Team) : Nearest(1);
		auto Plate = [&](int32 Idx, bool bLeft)
		{
			if (!M.P.IsValidIndex(Idx)) return;
			const FPFPlayer& Q = M.P[Idx];
			static const TCHAR* Pos[] = { TEXT("GK"), TEXT("DF"), TEXT("MF"), TEXT("FW") };
			const float TX = bLeft ? 40 * U : W - 40 * U; const EPFAlign Al = bLeft ? EPFAlign::Left : EPFAlign::Right;
			// the name in yellow over a brown drop (ISS's yellow-to-brown lettering)
			Text(C, Q.Setup.Name.ToUpper(), TX + 2 * U, BY + 14 * U, 1.f, FLinearColor(0.45f, 0.25f, 0.08f, 1.f), Al);
			Text(C, Q.Setup.Name.ToUpper(), TX, BY + 12 * U, 1.f, FLinearColor(1.f, 0.86f, 0.25f), Al);
			const FString Sub = FString::Printf(TEXT("%s %d"), Pos[FMath::Clamp(int32(Q.Setup.Role), 0, 3)], Q.Setup.Number);
			Text(C, Sub, TX, BY + 54 * U, 0.6f, FLinearColor(0.95f, 0.95f, 1.f), Al, true);
			const int32 Rt = Q.Setup.Stats.Overall(Q.bKeeper);
			Text(C, FString::FromInt(Rt), bLeft ? TX + 110 * U : TX - 110 * U, BY + 54 * U, 0.6f, RCol(Rt), Al, true);
			const float SW = 160 * U, SX = bLeft ? TX + 170 * U : TX - 170 * U - SW;
			Box(C, SX, BY + 60 * U, SW, 8 * U, FLinearColor(0.f, 0.f, 0.f, 0.6f));
			Box(C, SX + U, BY + 61 * U, (SW - 2 * U) * FMath::Clamp(Q.Stamina, 0.f, 1.f), 6 * U, FLinearColor(0.4f, 0.95f, 0.4f, 0.95f));
		};
		Plate(L, true); Plate(R, false);
		// radar: 36% of the width, standing up out of the band; fades when someone on the pitch is behind it
		const float MW = FMath::RoundToFloat(W * 0.36f), MH = FMath::RoundToFloat(MW * 0.3f), MX = FMath::RoundToFloat(W * 0.5f - MW * 0.5f), MY = H - MH - 8 * U;
		bool bBehind = false;
		for (int32 i = 0; i < M.HeadScreen.Num(); ++i) { const FVector2D Hs = M.HeadScreen[i]; if (Hs.X > MX && Hs.X < MX + MW && Hs.Y + 70 * U > MY) bBehind = true; }
		const float Al = bBehind ? 0.3f : 1.f;
		Box(C, MX, MY, MW, MH, FLinearColor(0.5f, 0.5f, 0.55f, 0.55f * Al));
		const FLinearColor Ln(1, 1, 1, 0.7f * Al);
		const float Lw = FMath::Max(1.f, FMath::RoundToFloat(2 * U));
		auto RS = [&](float WX, float WY) { return FVector2D(MX + (WX / 105.f + 0.5f) * MW, MY + (0.5f - WY / 68.f) * MH); };
		Box(C, MX, MY, MW, Lw, Ln); Box(C, MX, MY + MH - Lw, MW, Lw, Ln); Box(C, MX, MY, Lw, MH, Ln); Box(C, MX + MW - Lw, MY, Lw, MH, Ln);
		Box(C, MX + MW * 0.5f - Lw * 0.5f, MY, Lw, MH, Ln);
		{ const FVector2D A = RS(-9.15f, 9.15f), B = RS(9.15f, -9.15f); Box(C, A.X, A.Y, B.X - A.X, Lw, Ln); Box(C, A.X, B.Y - Lw, B.X - A.X, Lw, Ln); Box(C, A.X, A.Y, Lw, B.Y - A.Y, Ln); Box(C, B.X - Lw, A.Y, Lw, B.Y - A.Y, Ln); }   // the centre circle, square at this size
		for (int32 g = -1; g <= 1; g += 2)
		{
			const FVector2D A = RS(g * 52.5f, 20.15f), B = RS(g * (52.5f - 16.5f), -20.15f);
			const float X0 = FMath::Min(A.X, B.X), X1 = FMath::Max(A.X, B.X);
			Box(C, X0, A.Y, X1 - X0, Lw, Ln); Box(C, X0, B.Y - Lw, X1 - X0, Lw, Ln); Box(C, g < 0 ? X1 - Lw : X0, A.Y, Lw, B.Y - A.Y, Ln);
		}
		const float Dd = FMath::Max(3.f, FMath::RoundToFloat(6 * U));
		const bool bBlink = FMath::Fmod(M.MatchClock * 60.f, 0.5f) < 0.3f;
		for (const FPFPlayer& Pl : M.P)
		{
			if (Pl.bSentOff) continue;
			const FVector2D S = RS(Pl.Pos.X, Pl.Pos.Y);
			if (bPad && Pl.Index == M.Human && bBlink) Box(C, S.X - Dd * 0.5f - Lw, S.Y - Dd * 0.5f - Lw, Dd + 2 * Lw, Dd + 2 * Lw, FLinearColor(1, 1, 1, Al));
			Box(C, S.X - Dd * 0.5f, S.Y - Dd * 0.5f, Dd, Dd, Pl.Team == 0 ? FLinearColor(0.25f, 0.45f, 1.f, Al) : FLinearColor(1.f, 0.25f, 0.2f, Al));
		}
		const FVector2D Bs = RS(M.Ball.Pos.X, M.Ball.Pos.Y);
		Box(C, Bs.X - Dd * 0.4f, Bs.Y - Dd * 0.4f, Dd * 0.8f, Dd * 0.8f, FLinearColor(1, 1, 1, Al));
	}
	// the shot bar over the shooter's head, like the pass bar (fable section 8)
	if (M.Human >= 0 && !M.Setup.bWatchOnly && M.P.IsValidIndex(M.Human) && M.P[M.Human].bCharging && M.HeadScreen.IsValidIndex(M.Human) && M.HeadScreen[M.Human].X > -1e8f && !bDotsNow)
	{
		const FVector2D Hs = M.HeadScreen[M.Human];
		const float BW = FMath::RoundToFloat(72 * U), BH = FMath::RoundToFloat(10 * U), Px = FMath::Max(1.f, FMath::RoundToFloat(2 * U));
		const float X0 = FMath::RoundToFloat(Hs.X - BW * 0.5f), Y0 = FMath::RoundToFloat(Hs.Y - 50 * U);
		Box(C, X0 - Px, Y0 - Px, BW + 2 * Px, BH + 2 * Px, FLinearColor(0.f, 0.f, 0.f, 0.85f));
		Box(C, X0, Y0, BW, BH, FLinearColor(0.08f, 0.1f, 0.16f, 0.9f));
		Box(C, X0, Y0, FMath::RoundToFloat(BW * FMath::Clamp(M.P[M.Human].Charge, 0.04f, 1.f)), BH, FLinearColor(1.f, 0.75f, 0.1f, 1.f));
	}
	// v0.10 goal show: G-O-A-L, the score card with the scorer under it, the fade to the kick-off (PFPresent.cpp)
	{
		float Ny = 0.f;
		if (PFPresent::DrawGoalOverlay(C, M, U, Ny))
		{
			Text(C, M.GoalWho, W * 0.5f + 2 * U, Ny + 2 * U, 1.2f, FLinearColor(0.02f, 0.04f, 0.2f, 0.9f), EPFAlign::Center);
			Text(C, M.GoalWho, W * 0.5f, Ny, 1.2f, FLinearColor::White, EPFAlign::Center);
			const FString& Bp = M.GoalTeam == 0 ? M.Setup.Home.BadgePath : M.Setup.Away.BadgePath;
			Tex(C, Bp, W * 0.5f - 32 * U, Ny + 52 * U, 64 * U, 64 * U);
		}
	}
}

void UPFFlow::DrawDots(UCanvas* C, const FPFMatch& M)
{
	// a flat 2D pitch with dots: the fast part of a managed match
	const float W = C->ClipX, H = C->ClipY;
	Box(C, 0, 0, W, H, FLinearColor(0.01f, 0.02f, 0.05f, 1));
	const float PW = W * 0.8f, PH = PW * 68.f / 105.f, PX = (W - PW) * 0.5f, PY = (H - PH) * 0.5f + 40 * U;
	for (int32 s = 0; s < 10; ++s) Box(C, PX + s * PW / 10.f, PY, PW / 10.f, PH, s % 2 ? FLinearColor(0.07f, 0.33f, 0.1f, 1) : FLinearColor(0.08f, 0.38f, 0.12f, 1));
	const FLinearColor Line(1, 1, 1, 0.6f);
	Box(C, PX, PY, PW, 2, Line); Box(C, PX, PY + PH - 2, PW, 2, Line); Box(C, PX, PY, 2, PH, Line); Box(C, PX + PW - 2, PY, 2, PH, Line); Box(C, PX + PW * 0.5f - 1, PY, 2, PH, Line);
	const float BoxW = PW * 16.5f / 105.f, BoxH = PH * 40.3f / 68.f;
	Box(C, PX, PY + (PH - BoxH) * 0.5f, BoxW, 2, Line); Box(C, PX, PY + (PH + BoxH) * 0.5f, BoxW, 2, Line); Box(C, PX + BoxW, PY + (PH - BoxH) * 0.5f, 2, BoxH, Line);
	Box(C, PX + PW - BoxW, PY + (PH - BoxH) * 0.5f, BoxW, 2, Line); Box(C, PX + PW - BoxW, PY + (PH + BoxH) * 0.5f, BoxW, 2, Line); Box(C, PX + PW - BoxW, PY + (PH - BoxH) * 0.5f, 2, BoxH, Line);
	auto ToS = [&](FVector2D P) { return FVector2D(PX + (P.X / 105.f + 0.5f) * PW, PY + (0.5f - P.Y / 68.f) * PH); };
	for (const FPFPlayer& Pl : M.P)
	{
		const FVector2D S = ToS(Pl.Pos);
		const FLinearColor Col = Pl.bKeeper ? (Pl.Team == 0 ? M.Setup.Home.KeeperKit.Shirt : M.Setup.Away.KeeperKit.Shirt) : (Pl.Team == 0 ? M.Setup.Home.Kit.Shirt : M.Setup.Away.Kit.Shirt);
		Box(C, S.X - 11 * U, S.Y - 11 * U, 22 * U, 22 * U, FLinearColor(0, 0, 0, 0.8f));
		Box(C, S.X - 8 * U, S.Y - 8 * U, 16 * U, 16 * U, Col);
	}
	const FVector2D B = ToS(FVector2D(M.Ball.Pos.X, M.Ball.Pos.Y));
	Box(C, B.X - 6 * U, B.Y - 6 * U - M.Ball.Pos.Z * 4 * U, 12 * U, 12 * U, FLinearColor::White);
}

void UPFFlow::Draw(UCanvas* C, UMaterialInterface* SpriteMat)
{
	if (!C) return;
	U = C->ClipY / 1080.f;
	const TArray<FPFClub>& Cl = PFWorld::Clubs();
	switch (Screen)
	{
	case EPFScreen::Title: DrawTitle(C); break;
	case EPFScreen::Main:
	{
		DrawBackground(C, 0.25f);
		TArray<FPFMenuItem> Items;
		Items.Add({ TEXT("CAREER") }); Items.Add({ TEXT("FRIENDLY MATCH") }); Items.Add({ TEXT("OPTIONS") }); Items.Add({ TEXT("QUIT") });
		Text(C, TEXT("PROPER FOOTBALL"), C->ClipX * 0.5f, 120 * U, 2.4f, FLinearColor::White, EPFAlign::Center);
		DrawMenu(C, TEXT("MAIN MENU"), Items, Sel, C->ClipX * 0.5f - 260 * U, C->ClipY * 0.45f, 520 * U);
		Text(C, TEXT("A  SELECT      B  BACK"), C->ClipX * 0.5f, C->ClipY - 70 * U, 0.8f, FLinearColor(0.7f, 0.75f, 0.9f), EPFAlign::Center, true);
		break;
	}
	case EPFScreen::Friendly:
	{
		DrawBackground(C, 0.35f);
		TArray<FPFMenuItem> Items;
		auto V = [](FString S) { return [S]() { return S; }; };
		const FString HN = Cl.IsValidIndex(FriendlyHome) ? Cl[FriendlyHome].Name : TEXT(""), AN = Cl.IsValidIndex(FriendlyAway) ? Cl[FriendlyAway].Name : TEXT("");
		Items.Add({ TEXT("HOME"), V(HN), [](int32) {} });
		Items.Add({ TEXT("AWAY"), V(AN), [](int32) {} });
		Items.Add({ TEXT("YOU PLAY"), V(FriendlySide == 0 ? TEXT("HOME") : FriendlySide == 1 ? TEXT("AWAY") : TEXT("WATCH")), [](int32) {} });
		Items.Add({ TEXT("KICK-OFF"), V(FriendlyStage == 0 ? TEXT("3 PM") : FriendlyStage == 1 ? TEXT("EVENING") : TEXT("FLOODLIGHTS")), [](int32) {} });
		static const TCHAR* L[] = { TEXT("1.5 MIN"), TEXT("2.5 MIN"), TEXT("4 MIN"), TEXT("5 MIN") };
		Items.Add({ TEXT("HALF LENGTH"), V(L[FriendlyLen % 4]), [](int32) {} });
		Items.Add({ TEXT("PLAY") });
		const float CX = C->ClipX * 0.5f;
		DrawMenu(C, TEXT("FRIENDLY"), Items, Sel, CX - 380 * U, C->ClipY * 0.52f, 760 * U);
		if (Cl.IsValidIndex(FriendlyHome)) Tex(C, Cl[FriendlyHome].Badge, CX - 420 * U, 110 * U, 256 * U, 256 * U);
		if (Cl.IsValidIndex(FriendlyAway)) Tex(C, Cl[FriendlyAway].Badge, CX + 164 * U, 110 * U, 256 * U, 256 * U);
		Text(C, TEXT("V"), CX, 210 * U, 2.f, FLinearColor(1, 0.85f, 0.3f), EPFAlign::Center);
		break;
	}
	case EPFScreen::Match:
		if (MatchActor)
		{
			bDotsNow = bManageView && MatchActor->Match.Highlight <= 0.f && MatchActor->Match.Phase == EPFPhase::Play;
			if (bDotsNow) DrawDots(C, MatchActor->Match);
			else MatchActor->Match.Draw(C, SpriteMat);
			DrawMatchHUD(C, MatchActor->Match);
			if (bManageView) Text(C, MatchActor->Match.Highlight > 0.f ? TEXT("HIGHLIGHT") : TEXT("SIMULATING  x8"), C->ClipX - 40 * U, 40 * U, 0.8f, FLinearColor(1, 0.85f, 0.3f), EPFAlign::Right);
			if (bPause)
			{
				Box(C, 0, 0, C->ClipX, C->ClipY, FLinearColor(0, 0, 0, 0.5f));
				TArray<FPFMenuItem> Items; Items.Add({ TEXT("RESUME") }); Items.Add({ TEXT("QUIT MATCH") });
				DrawMenu(C, TEXT("PAUSED"), Items, PauseSel, C->ClipX * 0.5f - 220 * U, C->ClipY * 0.45f, 440 * U);
			}
		}
		break;
	case EPFScreen::Result:
		if (MatchActor)
		{
			const FPFMatch& M = MatchActor->Match;
			DrawBackground(C, 0.4f);
			const float CX = C->ClipX * 0.5f;
			Box(C, CX - 700 * U, 140 * U, 1400 * U, 760 * U, FLinearColor(0.01f, 0.02f, 0.08f, 0.85f));
			Text(C, TEXT("FULL TIME"), CX, 170 * U, 1.6f, FLinearColor(1, 0.85f, 0.3f), EPFAlign::Center);
			Tex(C, M.Setup.Home.BadgePath, CX - 640 * U, 260 * U, 200 * U, 200 * U);
			Tex(C, M.Setup.Away.BadgePath, CX + 440 * U, 260 * U, 200 * U, 200 * U);
			Text(C, FString::Printf(TEXT("%d - %d"), M.Score[0], M.Score[1]), CX, 300 * U, 3.f, FLinearColor::White, EPFAlign::Center);
			Text(C, M.Setup.Home.Name, CX - 540 * U, 480 * U, 0.9f, FLinearColor::White, EPFAlign::Center, true);
			Text(C, M.Setup.Away.Name, CX + 540 * U, 480 * U, 0.9f, FLinearColor::White, EPFAlign::Center, true);
			// v0.9.4: each side's scorers in its own column (a 3-3 used to show only the first four events)
			float Ly[2] = { 540 * U, 540 * U };
			for (int32 e = 0; e < M.Events.Num(); ++e)
			{
				const FPFMatchEvent& E = M.Events[e];
				const int32 Tm = FMath::Clamp(E.Team, 0, 1);
				if (Ly[Tm] > 650 * U) continue;
				Text(C, E.Text, Tm == 0 ? CX - 80 * U : CX + 80 * U, Ly[Tm], 0.8f, FLinearColor(0.85f, 0.9f, 1.f), Tm == 0 ? EPFAlign::Right : EPFAlign::Left, true);
				Ly[Tm] += 36 * U;
			}
			const float PT = M.PossT[0] + M.PossT[1];
			const int32 PossH = PT < 1.f ? 50 : FMath::RoundToInt(100.f * M.PossT[0] / PT);   // v0.9.4: 50-50 before a ball is kicked
			const FString Rows[][3] = {
				{ FString::FromInt(PossH) + TEXT("%"), TEXT("POSSESSION"), FString::FromInt(100 - PossH) + TEXT("%") },
				{ FString::FromInt(M.Shots[0]), TEXT("SHOTS"), FString::FromInt(M.Shots[1]) },
				{ FString::FromInt(M.Corners[0]), TEXT("CORNERS"), FString::FromInt(M.Corners[1]) },
				{ FString::FromInt(M.Fouls[0]), TEXT("FOULS"), FString::FromInt(M.Fouls[1]) },
				{ FString::FromInt(M.Yellows[0]), TEXT("YELLOW CARDS"), FString::FromInt(M.Yellows[1]) },
				{ FString::FromInt(M.Reds[0]), TEXT("RED CARDS"), FString::FromInt(M.Reds[1]) } };
			for (int32 r = 0; r < 6; ++r)
			{
				const float RY = 700 * U + r * 32 * U;
				Text(C, Rows[r][0], CX - 260 * U, RY, 0.7f, FLinearColor::White, EPFAlign::Right, true);
				Text(C, Rows[r][1], CX, RY, 0.7f, FLinearColor(0.7f, 0.8f, 1.f), EPFAlign::Center, true);
				Text(C, Rows[r][2], CX + 260 * U, RY, 0.7f, FLinearColor::White, EPFAlign::Left, true);
			}
		}
		break;
	case EPFScreen::Career:
		if (UPFCareer* Car = UPFCareer::Get(this)) Car->Draw(C, SpriteMat);
		break;
	case EPFScreen::Options:
	{
		DrawBackground(C, 0.35f);
		auto Bar = [](int32 V) { FString S; for (int32 i = 0; i < 10; ++i) S += i < V ? TEXT("|") : TEXT("."); return S; };
		static const TCHAR* L[] = { TEXT("1.5 MIN"), TEXT("2.5 MIN"), TEXT("4 MIN"), TEXT("5 MIN") };
		const FString M = Bar(MusicVol), S2 = Bar(SfxVol), H = L[FriendlyLen % 4];
		TArray<FPFMenuItem> Items;
		Items.Add({ TEXT("MUSIC"), [M]() { return M; }, [](int32) {} });
		Items.Add({ TEXT("SOUND EFFECTS & CROWD"), [S2]() { return S2; }, [](int32) {} });
		Items.Add({ TEXT("HALF LENGTH"), [H]() { return H; }, [](int32) {} });
		static const TCHAR* CamL[] = { TEXT("CLOSEST"), TEXT("CLOSER"), TEXT("STANDARD"), TEXT("WIDE"), TEXT("WIDER"), TEXT("WIDEST") };
		const FString Cm = CamL[FMath::Clamp(CameraOpt, 0, 5)];
		Items.Add({ TEXT("CAMERA"), [Cm]() { return Cm; }, [](int32) {} });
		static const TCHAR* SpL[] = { TEXT("SLOW"), TEXT("NORMAL"), TEXT("FAST") };
		const FString Spd = SpL[FMath::Clamp(SpeedOpt, 0, 2)];
		Items.Add({ TEXT("GAME SPEED"), [Spd]() { return Spd; }, [](int32) {} });
		Items.Add({ TEXT("BACK") });
		DrawMenu(C, TEXT("OPTIONS"), Items, Sel, C->ClipX * 0.5f - 420 * U, C->ClipY * 0.4f, 840 * U);
		break;
	}
	case EPFScreen::PreMatch:
		if (MatchActor)
		{
			const FPFMatchSetup& S = MatchActor->Match.Setup;
			DrawBackground(C, 0.35f);
			const float CX = C->ClipX * 0.5f;
			Box(C, CX - 760 * U, 150 * U, 1520 * U, 760 * U, FLinearColor(0.01f, 0.02f, 0.08f, 0.88f));
			Text(C, S.bWatchOnly ? TEXT("TODAY'S MATCH") : TEXT("KICK-OFF"), CX, 180 * U, 1.3f, FLinearColor(1, 0.85f, 0.3f), EPFAlign::Center);
			Tex(C, S.Home.BadgePath, CX - 660 * U, 290 * U, 256 * U, 256 * U);
			Tex(C, S.Away.BadgePath, CX + 404 * U, 290 * U, 256 * U, 256 * U);
			Text(C, TEXT("V"), CX, 380 * U, 2.4f, FLinearColor::White, EPFAlign::Center);
			Text(C, S.Home.Name.ToUpper(), CX - 532 * U, 580 * U, 0.8f, FLinearColor::White, EPFAlign::Center, true);
			Text(C, S.Away.Name.ToUpper(), CX + 532 * U, 580 * U, 0.8f, FLinearColor::White, EPFAlign::Center, true);
			// kit swatches (shirt / shorts / socks) so you know who is who
			auto Swatch = [&](float X, const FPFKit& K)
			{
				Box(C, X - 60 * U, 630 * U, 120 * U, 60 * U, K.Shirt); Box(C, X - 60 * U, 690 * U, 120 * U, 34 * U, K.Shorts); Box(C, X - 60 * U, 724 * U, 120 * U, 20 * U, K.Socks);
				if (K.Pattern == 1) for (int32 i = 0; i < 6; i += 2) Box(C, X - 60 * U + i * 20 * U, 630 * U, 20 * U, 60 * U, K.Trim);
			};
			Swatch(CX - 532 * U, S.Home.Kit); Swatch(CX + 532 * U, S.Away.Kit);
			static const TMap<FString, FString> StageName = { { TEXT("Daylight"), TEXT("3PM KICK-OFF") }, { TEXT("Sunset"), TEXT("EVENING KICK-OFF") }, { TEXT("Floodlit_Night"), TEXT("UNDER THE LIGHTS") } };
			const FString* SN = StageName.Find(S.Stage);
			Text(C, SN ? *SN : S.Stage, CX, 620 * U, 0.75f, FLinearColor(0.7f, 0.8f, 1.f), EPFAlign::Center, true);
			if (!S.bWatchOnly && FMath::Fmod(Time, 1.f) < 0.7f) Text(C, TEXT("PRESS A"), CX, 820 * U, 1.1f, FLinearColor(1, 0.85f, 0.3f), EPFAlign::Center);
		}
		break;
	default: break;
	}
}
