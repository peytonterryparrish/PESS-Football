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
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Sound/SoundBase.h"
#include "Components/AudioComponent.h"
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

void UPFFlow::RunFeelTest()
{
	// headless: the human player in a friendly, stick scripted. Measures how fast the controls answer, for a slow and a fast player.
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
		UE_LOG(LogPF, Display, TEXT("PFSIM passes per match %.1f: completed %.0f%%, intercepted %.0f%%, dead %.0f%%, avg length %.1f m; of the intercepted, never came within 2 m of the receiver %lld, taken in the air %lld"), Dg[42] / Nn, Dg[42] ? 100.f * Dg[43] / Dg[42] : 0.f, Dg[42] ? 100.f * Dg[45] / Dg[42] : 0.f, Dg[42] ? 100.f * Dg[46] / Dg[42] : 0.f, Dg[42] ? float(Dg[44]) / Dg[42] : 0.f, Dg[47], Dg[39]);
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
			if (K == TEXT("cam")) CameraOpt = FMath::Clamp(FCString::Atoi(*V), 0, 2);
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
	static float Hs = -1.f;
	if (Hs < 0.f) { Hs = 1.f; FParse::Value(FCommandLine::Get(), TEXT("pfscale="), Hs); Hs = FMath::Clamp(Hs, 0.25f, 2.f); }
	FPFMatch::HeightScale = Hs;
	static const float Z[] = { 6.24f, 4.68f, 3.12f };   // near / medium / far: sprite pixels 2:1, 1.5:1, 1:1 at 1080p (at any HeightScale)
	FPFMatch::CamZoom = Z[FMath::Clamp(CameraOpt, 0, 2)] * 0.5f / Hs;
}

void UPFFlow::SaveOptions()
{
	ApplyCamera();
	FFileHelper::SaveStringToFile(FString::Printf(TEXT("music=%d\nsfx=%d\nhalflen=%d\ncam=%d\n"), MusicVol, SfxVol, FriendlyLen, CameraOpt), *(FPaths::ProjectSavedDir() / TEXT("pf_options.txt")));
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
	Screen = S; Sel = 0; bPause = false;
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
			Items.Add({ TEXT("QUIT MATCH"), nullptr, nullptr, [this, &M]() { bPause = false; M.bQuit = true; M.bFinished = true; } });
			MenuInput(Items, PauseSel, Pad);
			if (Pad.pB) bPause = false;
			break;
		}
		FPFPadInput In;
		In.Move = Pad.Move; In.bSprint = Pad.bSprint;
		// v0.7 (owner): A pass, B shoot (hold for power), X cross, Y through ball, LB + B chip, LB on its own switches; right stick flicks = skills
		In.bPass = Pad.pA; In.bLob = Pad.pX; In.bThrough = Pad.pY; In.bSwitch = Pad.pLB;
		In.bShootHeld = Pad.bB; In.bShootPressed = Pad.pB; In.bShootReleased = Pad.rB; In.bChipMod = Pad.bLB;
		In.bFeintL = Pad.pFlickL; In.bFeintR = Pad.pFlickR;
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
		if (Pad.pA || Pad.pB || Pad.pStart) Go(EPFScreen::Main);
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
		Items.Add({ TEXT("CAMERA"), nullptr, [this](int32 D) { CameraOpt = (CameraOpt + D + 3) % 3; SaveOptions(); } });
		Items.Add({ TEXT("BACK"), nullptr, nullptr, [this]() { Go(EPFScreen::Main); } });
		MenuInput(Items, Sel, Pad);
		if (Pad.pB) Go(EPFScreen::Main);
		break;
	}
	case EPFScreen::PreMatch:
		PreMatchT += Dt;
		if (Pad.pA || Pad.pStart || (MatchActor && MatchActor->Match.Setup.bWatchOnly && PreMatchT > 2.f) || ShotsLeft > 0 || (FParse::Param(FCommandLine::Get(), TEXT("pfautopad")) && PreMatchT > 2.f)) { Go(EPFScreen::Match); PlaySfx(TEXT("whistle_short"), 0.7f); }
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
			if (bManageView && MatchActor->Match.Highlight <= 0.f && MatchActor->Match.Phase == EPFPhase::Play) DrawDots(C, MatchActor->Match);
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
			float Ly = 540 * U;
			for (int32 e = 0; e < M.Events.Num() && e < 4; ++e)
			{
				const FPFMatchEvent& E = M.Events[e];
				Text(C, E.Text, E.Team == 0 ? CX - 80 * U : CX + 80 * U, Ly, 0.8f, FLinearColor(0.85f, 0.9f, 1.f), E.Team == 0 ? EPFAlign::Right : EPFAlign::Left, true);
				Ly += 36 * U;
			}
			const float PT = FMath::Max(1.f, M.PossT[0] + M.PossT[1]);
			const int32 PossH = FMath::RoundToInt(100.f * M.PossT[0] / PT);
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
		static const TCHAR* CamL[] = { TEXT("NEAR"), TEXT("MEDIUM"), TEXT("FAR") };
		const FString Cm = CamL[FMath::Clamp(CameraOpt, 0, 2)];
		Items.Add({ TEXT("CAMERA"), [Cm]() { return Cm; }, [](int32) {} });
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
