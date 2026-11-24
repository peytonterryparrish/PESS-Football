#include "PFFlow.h"
#include "PFGame.h"
#include "PFWorld.h"
#include "PFCareer.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "CanvasItem.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UnrealClient.h"
#include "Misc/CommandLine.h"

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
	FParse::Value(FCommandLine::Get(), TEXT("pfshots="), ShotsLeft);
	FParse::Value(FCommandLine::Get(), TEXT("pfshotgap="), ShotGap);
	if (FParse::Param(FCommandLine::Get(), TEXT("pfwatch"))) FriendlySide = 2;
	if (FParse::Param(FCommandLine::Get(), TEXT("pfmatch"))) StartFriendly();
	else Go(EPFScreen::Title);
}

void UPFFlow::Go(EPFScreen S)
{
	Screen = S; Sel = 0; bPause = false;
	if (S == EPFScreen::Main || S == EPFScreen::Title) PickBackground(TEXT("England"));
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
	Go(EPFScreen::Match);
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
	if (PFWorld::KitsClash(S.Home.Kit, S.Away.Kit)) S.Away.Kit = A.Kits.Away;
	S.Away.KeeperKit = PFWorld::KitsClash(S.Home.KeeperKit, A.Kits.Keeper1) ? A.Kits.Keeper2 : A.Kits.Keeper1;
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
		Items.Add({ TEXT("QUIT"), nullptr, nullptr, [this]() { if (WorldRef.IsValid()) UKismetSystemLibrary::QuitGame(WorldRef.Get(), nullptr, EQuitPreference::Quit, false); } });
		MenuInput(Items, Sel, Pad);
		if (Pad.pB) Go(EPFScreen::Title);
		break;
	}
	case EPFScreen::Friendly:
	{
		TArray<FPFMenuItem> Items;
		const int32 N = C.Num();
		Items.Add({ TEXT("HOME"), [this, &C]() { return C.IsValidIndex(FriendlyHome) ? C[FriendlyHome].Name : FString(); }, [this, N](int32 D) { FriendlyHome = (FriendlyHome + D + N) % N; } });
		Items.Add({ TEXT("AWAY"), [this, &C]() { return C.IsValidIndex(FriendlyAway) ? C[FriendlyAway].Name : FString(); }, [this, N](int32 D) { FriendlyAway = (FriendlyAway + D + N) % N; } });
		Items.Add({ TEXT("YOU PLAY"), [this]() { return FString(FriendlySide == 0 ? TEXT("HOME") : FriendlySide == 1 ? TEXT("AWAY") : TEXT("WATCH")); }, [this](int32 D) { FriendlySide = (FriendlySide + D + 3) % 3; } });
		Items.Add({ TEXT("KICK-OFF"), [this]() { return FString(FriendlyStage == 0 ? TEXT("3 PM") : FriendlyStage == 1 ? TEXT("EVENING") : TEXT("FLOODLIGHTS")); }, [this](int32 D) { FriendlyStage = (FriendlyStage + D + 3) % 3; } });
		Items.Add({ TEXT("HALF LENGTH"), [this]() { static const TCHAR* L[] = { TEXT("1.5 MIN"), TEXT("2.5 MIN"), TEXT("4 MIN"), TEXT("5 MIN") }; return FString(L[FriendlyLen % 4]); }, [this](int32 D) { FriendlyLen = (FriendlyLen + D + 4) % 4; } });
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
			Items.Add({ TEXT("QUIT MATCH"), nullptr, nullptr, [this, &M]() { bPause = false; M.bFinished = true; } });
			MenuInput(Items, PauseSel, Pad);
			if (Pad.pB) bPause = false;
			break;
		}
		FPFPadInput In;
		In.Move = Pad.Move; In.bSprint = Pad.bSprint;
		In.bPass = Pad.pA; In.bLob = Pad.pB; In.bThrough = Pad.pY; In.bSwitch = Pad.pLB;
		In.bShootHeld = Pad.bX; In.bShootPressed = Pad.pX; In.bShootReleased = Pad.rX;
		M.Tick(Dt, In);
		if (M.bFinished)
		{
			auto Done = MatchDone; MatchDone = nullptr;
			if (Done) Done(M); else Go(EPFScreen::Result);
		}
		break;
	}
	case EPFScreen::Result:
		if (Pad.pA || Pad.pB || Pad.pStart) Go(EPFScreen::Main);
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
	if (Pad.pDown) S = (S + 1) % Items.Num();
	if (Pad.pUp) S = (S - 1 + Items.Num()) % Items.Num();
	FPFMenuItem& It = Items[S];
	if (It.OnChange && (Pad.pLeft || Pad.pRight)) It.OnChange(Pad.pLeft ? -1 : 1);
	if (Pad.pA && It.bEnabled)
	{
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
	Text(C, TEXT("v0.1.0"), C->ClipX - 30 * U, C->ClipY - 50 * U, 0.7f, FLinearColor(0.6f, 0.6f, 0.7f), EPFAlign::Right, true);
}

void UPFFlow::DrawMatchHUD(UCanvas* C, const FPFMatch& M)
{
	const float W = C->ClipX, H = C->ClipY;
	// scoreboard (top-left): badge, short codes, score, clock
	const float X = 40 * U, Y = 34 * U;
	Box(C, X, Y, 540 * U, 64 * U, FLinearColor(0.02f, 0.04f, 0.14f, 0.85f));
	Box(C, X, Y + 60 * U, 540 * U, 4 * U, FLinearColor(0.2f, 0.45f, 1.f, 1));
	Tex(C, M.Setup.Home.BadgePath, X + 6 * U, Y + 4 * U, 56 * U, 56 * U);
	Text(C, M.Setup.Home.Short, X + 70 * U, Y + 12 * U, 1.1f);
	Text(C, FString::Printf(TEXT("%d - %d"), M.Score[0], M.Score[1]), X + 270 * U, Y + 12 * U, 1.1f, FLinearColor(1, 0.9f, 0.4f), EPFAlign::Center);
	Text(C, M.Setup.Away.Short, X + 470 * U, Y + 12 * U, 1.1f, FLinearColor::White, EPFAlign::Right);
	Tex(C, M.Setup.Away.BadgePath, X + 478 * U, Y + 4 * U, 56 * U, 56 * U);
	const int32 Min = FMath::FloorToInt(M.MatchClock), Sec = FMath::FloorToInt(FMath::Fmod(M.MatchClock, 1.f) * 60.f);
	Box(C, X, Y + 70 * U, 160 * U, 50 * U, FLinearColor(0.02f, 0.04f, 0.14f, 0.85f));
	Text(C, FString::Printf(TEXT("%02d:%02d"), Min, Sec), X + 80 * U, Y + 76 * U, 1.f, FLinearColor(0.5f, 1.f, 0.6f), EPFAlign::Center);
	// banner
	if (M.BannerT > 0 && !M.Banner.IsEmpty())
	{
		const float Big = M.Banner == TEXT("GOAL!") ? 3.f : 1.4f;
		Box(C, W * 0.5f - 400 * U, H * 0.22f - 10 * U, 800 * U, (Big * 38 + 30) * U, FLinearColor(0.02f, 0.03f, 0.1f, 0.75f));
		Text(C, M.Banner, W * 0.5f, H * 0.22f, Big, M.Banner == TEXT("GOAL!") ? FLinearColor(1, 0.85f, 0.2f) : FLinearColor::White, EPFAlign::Center);
	}
	// controlled player nameplate (bottom-left) and the ball carrier (bottom-right)
	if (M.Human >= 0 && !M.Setup.bWatchOnly && M.P.IsValidIndex(M.Human))
	{
		const FPFPlayer& Hp = M.P[M.Human];
		Box(C, 40 * U, H - 110 * U, 470 * U, 60 * U, FLinearColor(0.05f, 0.12f, 0.45f, 0.85f));
		Text(C, FString::Printf(TEXT("%d  %s"), Hp.Setup.Number, *Hp.Setup.Name.ToUpper()), 56 * U, H - 100 * U, 1.f);
		if (Hp.bCharging)
		{
			Box(C, 40 * U, H - 44 * U, 470 * U, 18 * U, FLinearColor(0, 0, 0, 0.7f));
			Box(C, 42 * U, H - 42 * U, 466 * U * Hp.Charge, 14 * U, FLinearColor(1, 0.75f, 0.1f, 1));
		}
	}
	if (M.Ball.Owner >= 0)
	{
		const FPFPlayer& O = M.P[M.Ball.Owner];
		Box(C, W - 510 * U, H - 110 * U, 470 * U, 60 * U, O.Team == 0 ? FLinearColor(0.05f, 0.12f, 0.45f, 0.85f) : FLinearColor(0.5f, 0.08f, 0.06f, 0.85f));
		Text(C, FString::Printf(TEXT("%s  %d"), *O.Setup.Name.ToUpper(), O.Setup.Number), W - 56 * U, H - 100 * U, 1.f, FLinearColor::White, EPFAlign::Right);
	}
	// minimap (bottom centre)
	const float MW = 300 * U, MH = 194 * U, MX = W * 0.5f - MW * 0.5f, MY = H - MH - 30 * U;
	Box(C, MX, MY, MW, MH, FLinearColor(0.02f, 0.1f, 0.18f, 0.7f));
	Box(C, MX + MW * 0.5f - 1, MY, 2, MH, FLinearColor(1, 1, 1, 0.35f));
	for (const FPFPlayer& Pl : M.P)
	{
		const float PX = MX + (Pl.Pos.X / 105.f + 0.5f) * MW, PY = MY + (0.5f - Pl.Pos.Y / 68.f) * MH;
		Box(C, PX - 4 * U, PY - 4 * U, 8 * U, 8 * U, Pl.Team == 0 ? FLinearColor(0.3f, 0.55f, 1.f) : FLinearColor(1.f, 0.3f, 0.25f));
	}
	Box(C, MX + (M.Ball.Pos.X / 105.f + 0.5f) * MW - 3 * U, MY + (0.5f - M.Ball.Pos.Y / 68.f) * MH - 3 * U, 6 * U, 6 * U, FLinearColor::White);
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
		Items.Add({ TEXT("CAREER") }); Items.Add({ TEXT("FRIENDLY MATCH") }); Items.Add({ TEXT("QUIT") });
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
			MatchActor->Match.Draw(C, SpriteMat);
			DrawMatchHUD(C, MatchActor->Match);
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
			float Ly = 560 * U;
			for (const FPFMatchEvent& E : M.Events)
			{
				Text(C, E.Text, E.Team == 0 ? CX - 80 * U : CX + 80 * U, Ly, 0.8f, FLinearColor(0.85f, 0.9f, 1.f), E.Team == 0 ? EPFAlign::Right : EPFAlign::Left, true);
				Ly += 40 * U;
			}
			Text(C, FString::Printf(TEXT("SHOTS  %d - %d"), M.Shots[0], M.Shots[1]), CX, 820 * U, 0.9f, FLinearColor(0.7f, 0.8f, 1.f), EPFAlign::Center, true);
		}
		break;
	case EPFScreen::Career:
		if (UPFCareer* Car = UPFCareer::Get(this)) Car->Draw(C, SpriteMat);
		break;
	default: break;
	}
}
