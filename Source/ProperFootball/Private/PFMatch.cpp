#include "PFMatch.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "Engine/Engine.h"
#include "CanvasItem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PFWorld.h"

namespace
{
	constexpr float HalfLen = 52.5f, HalfWid = 34.f, GoalHalf = 3.66f, GoalH = 2.44f;
	constexpr float G = 9.81f;
	inline float T01(uint8 S) { return FMath::Clamp((float(S) - 1.f) / 98.f, 0.f, 1.f); }
	inline FVector2D V2(const FVector& V) { return FVector2D(V.X, V.Y); }
	inline FVector2D SafeN(FVector2D V, FVector2D Def = FVector2D(1, 0)) { const double L = V.Size(); return L > 1e-4 ? V / L : Def; }
	inline FVector2D Rot(FVector2D V, float Deg) { const float R = FMath::DegreesToRadians(Deg); const float C = FMath::Cos(R), S = FMath::Sin(R); return FVector2D(V.X * C - V.Y * S, V.X * S + V.Y * C); }
	FLinearColor SRGBf(const FLinearColor& C) { return C; } // kit colours are passed as sRGB code values; the material linearises
}

// ------------------------------------------------------------------------------------------------ ability curves
int32 FPFStats::Overall(bool bKeeper) const
{
	if (bKeeper) return FMath::RoundToInt(Keeping * 0.6f + Reactions * 0.2f + Positioning * 0.1f + Passing * 0.1f);
	return FMath::RoundToInt((Pace + Accel + Agility + Control + Passing + Shooting + Tackling + Positioning + Reactions + Heading) / 10.f);
}

FPFAbility PFAbility(const FPFStats& S, bool bKeeper)
{
	// Wide, non-linear spreads on purpose (docs/DESIGN.md, "Talent you can see").
	FPFAbility A;
	const float Pace = T01(S.Pace), Acc = T01(S.Accel), Agi = T01(S.Agility), Ctl = T01(S.Control), Pas = T01(S.Passing),
		Sho = T01(S.Shooting), Tac = T01(S.Tackling), Rea = T01(S.Reactions), Hea = T01(S.Heading), Kee = T01(S.Keeping);
	A.MaxSpeed = 3.4f + 5.9f * FMath::Pow(Pace, 1.35f);          // 3.4 m/s (plodder) .. 9.3 m/s (flyer)
	A.SprintMul = 1.12f + 0.16f * Pace;
	A.Accel = 3.5f + 24.f * FMath::Pow(Acc, 1.5f);               // sluggish .. explosive
	A.TurnRate = 160.f + 820.f * FMath::Pow(Agi, 1.3f);          // deg/s
	A.TouchDist = 2.9f - 2.35f * FMath::Pow(Ctl, 0.8f);          // 2.9 m heavy touch .. 0.55 m glued
	A.FirstTouchErr = 0.95f * FMath::Pow(1.f - Ctl, 1.4f);
	A.PassErrDeg = 15.f - 14.f * FMath::Pow(Pas, 0.9f);
	A.PassPower = 0.85f + 0.3f * Pas;
	A.ShotErrDeg = 19.f - 15.f * FMath::Pow(Sho, 0.9f);
	A.ShotPower = 17.f + 16.f * Sho;
	A.ReactSec = 0.65f - 0.55f * Rea;
	A.AnimRate = 0.72f + 0.6f * (Pace * 0.5f + Agi * 0.5f);
	A.TackleReach = 0.9f + 0.9f * Tac;
	A.TackleWin = 0.15f + 0.75f * Tac;
	A.HeadReach = 1.6f + 1.2f * Hea;
	A.KeeperReach = bKeeper ? 1.8f + 2.4f * FMath::Pow(Kee, 1.1f) : 0.8f;
	A.KeeperReact = bKeeper ? 0.32f - 0.27f * Kee : 0.6f;
	return A;
}

// ------------------------------------------------------------------------------------------------ setup
int32 FPFMatch::SpriteFor(const FString& Id)
{
	if (const int32* I = SetIndex.Find(Id)) return *I;
	FPFSpriteSet S;
	if (!S.Load(Id))
	{
		// not built (yet): a deterministic stand-in of the same kind, so a person always looks the same
		const TArray<FString>& Avail = Id.StartsWith(TEXT("K")) ? PFWorld::KeeperSprites() : PFWorld::OutfieldSprites();
		const FString Alt = Avail.Num() ? Avail[GetTypeHash(Id) % Avail.Num()] : FString();
		if (!Alt.IsEmpty() && Alt != Id && !Id.StartsWith(TEXT("R"))) { const int32 I = SpriteFor(Alt); SetIndex.Add(Id, I); return I; }
		UE_LOG(LogPF, Warning, TEXT("PF sprite set %s missing"), *Id);
		return Sets.Num() ? 0 : -1;
	}
	const int32 I = Sets.Add(MoveTemp(S));
	SetIndex.Add(Id, I);
	return I;
}

static UTexture2D* MakeEllipse(int32 W, int32 H, bool bRing)
{
	UTexture2D* T = UTexture2D::CreateTransient(W, H, PF_B8G8R8A8);
	T->Filter = TF_Nearest; T->SRGB = true; T->NeverStream = true;
	FColor* D = static_cast<FColor*>(T->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
	for (int32 y = 0; y < H; ++y)
		for (int32 x = 0; x < W; ++x)
		{
			const float nx = (x + 0.5f - W * 0.5f) / (W * 0.5f), ny = (y + 0.5f - H * 0.5f) / (H * 0.5f);
			const float r = FMath::Sqrt(nx * nx + ny * ny);
			FColor C(0, 0, 0, 0);
			if (bRing) { if (r <= 1.f && r > 0.72f) C = FColor(255, 255, 255, 255); }
			else if (r <= 1.f) C = FColor(0, 0, 0, 96);
			D[y * W + x] = C;
		}
	T->GetPlatformData()->Mips[0].BulkData.Unlock();
	T->UpdateResource();
	return T;
}

void FPFMatch::Init(const FPFMatchSetup& InSetup)
{
	Setup = InSetup;
	Rng = PF::FRng(Setup.Seed);
	Proj.Load();
	Stadium = PF::LoadTexture(FString::Printf(TEXT("ArtSource/Packs/R08_Final_93_Club_Stadiums_4K/Clubs/%s/%s_Stadium_4K.png"), *Setup.StadiumSlug, *Setup.Stage));
	BallTex = PF::LoadTexture(TEXT("GameData/Ball/ball_sheet.png"));
	ShadowTex = MakeEllipse(24, 10, false);
	RingTex = MakeEllipse(30, 12, true);

	// goal layers (atlas-resolution set, so they share the stadium's pixel grid)
	if (TSharedPtr<FJsonObject> GL = PF::LoadJson(TEXT("ArtSource/Packs/R08_Final_93_Club_Stadiums_4K/Goals/Goal_Layout.json")))
	{
		const TSharedPtr<FJsonObject>* Stages;
		if (GL->TryGetObjectField(TEXT("stages"), Stages))
		{
			const TSharedPtr<FJsonObject>* St;
			if ((*Stages)->TryGetObjectField(Setup.Stage, St))
				for (int32 g = 0; g < 2; ++g)
				{
					const TSharedPtr<FJsonObject>* Gg;
					if (!(*St)->TryGetObjectField(g == 0 ? TEXT("South_Goal") : TEXT("North_Goal"), Gg)) continue;
					const TSharedPtr<FJsonObject>& At = (*Gg)->GetObjectField(TEXT("Atlas"));
					auto LoadLayer = [&](const TCHAR* Key, FGoalLayer& L)
					{
						const TSharedPtr<FJsonObject>* O;
						if (!At->TryGetObjectField(Key, O)) return;
						L.Tex = PF::LoadTexture(TEXT("ArtSource/Packs/R08_Final_93_Club_Stadiums_4K/Goals/") + (*O)->GetStringField(TEXT("filename")));
						const TArray<TSharedPtr<FJsonValue>>& R = (*O)->GetArrayField(TEXT("xywh_4k_px"));
						L.XY = FVector2D(R[0]->AsNumber(), R[1]->AsNumber()); L.WH = FVector2D(R[2]->AsNumber(), R[3]->AsNumber());
					};
					LoadLayer(TEXT("Shadow"), GoalShadow[g]); LoadLayer(TEXT("Rear_Net"), GoalRear[g]); LoadLayer(TEXT("Front_Frame"), GoalFront[g]);
				}
		}
	}

	P.Reset();
	for (int32 t = 0; t < 2; ++t)
	{
		const FPFTeamSetup& TS = t == 0 ? Setup.Home : Setup.Away;
		for (const FPFPlayerSetup& PS : TS.XI)
		{
			FPFPlayer Pl;
			Pl.Setup = PS; Pl.Team = t; Pl.Index = P.Num();
			Pl.bKeeper = PS.Role == EPFRole::GK;
			Pl.A = PFAbility(PS.Stats, Pl.bKeeper);
			Pl.SpriteSet = SpriteFor(PS.Sprite);
			Pl.AIThink = Rng.Range(0, 0.3f);
			P.Add(Pl);
		}
	}
	Officials.Reset();
	for (int32 o = 0; o < 3; ++o)
	{
		FPFPlayer Of;
		Of.Setup.Name = o == 0 ? TEXT("Referee") : TEXT("Assistant");
		Of.Setup.Stats.Pace = 60; Of.Setup.Stats.Accel = 60; Of.Setup.Stats.Agility = 60;
		Of.A = PFAbility(Of.Setup.Stats, false);
		Of.SpriteSet = SpriteFor(o == 0 ? TEXT("R01") : TEXT("R02"));
		Of.Team = 2; Of.Index = o;
		Of.Pos = o == 0 ? FVector2D(-8, -6) : FVector2D(0, o == 1 ? -34.8f : 34.8f);
		Of.Facing = o == 1 ? FVector2D(0, 1) : FVector2D(0, -1);
		Officials.Add(Of);
	}
	Managers.Reset();
	for (int32 t = 0; t < 2; ++t)
	{
		FPFPlayer Mg;
		Mg.Setup.Name = TEXT("Manager");
		Mg.Setup.Stats.Pace = 30; Mg.A = PFAbility(Mg.Setup.Stats, false);
		Mg.SpriteSet = SpriteFor(FString::Printf(TEXT("C%02d"), 1 + int32((Setup.Seed + t * 7) % 3)));
		Mg.Team = 3; Mg.Index = t;
		Mg.Pos = FVector2D(t == 0 ? -9.f : 9.f, HalfWid + 3.2f);
		Mg.Home = Mg.Pos;
		Mg.Facing = FVector2D(0, -1);
		Managers.Add(Mg);
	}
	Crew.Reset();
	{
		static const TCHAR* Cam[] = { TEXT("sid_camera_operator_001"), TEXT("sid_camera_operator_004"), TEXT("sid_camera_operator_007") };
		static const TCHAR* Boy[] = { TEXT("sid_ball_boy_002"), TEXT("sid_ball_boy_005"), TEXT("sid_ball_boy_008"), TEXT("sid_ball_boy_003") };
		const FVector2D CamPos[] = { FVector2D(-30, HalfWid + 4.5f), FVector2D(0, HalfWid + 6.f), FVector2D(30, HalfWid + 4.5f) };
		const FVector2D BoyPos[] = { FVector2D(-HalfLen - 3.f, HalfWid - 4.f), FVector2D(HalfLen + 3.f, HalfWid - 4.f), FVector2D(-HalfLen - 3.f, -HalfWid + 6.f), FVector2D(HalfLen + 3.f, -HalfWid + 6.f) };
		for (int32 i = 0; i < 3; ++i) Crew.Add({ PF::LoadTexture(FString::Printf(TEXT("GameData/UI/%s.png"), Cam[i])), CamPos[i] });
		for (int32 i = 0; i < 4; ++i) Crew.Add({ PF::LoadTexture(FString::Printf(TEXT("GameData/UI/%s.png"), Boy[i])), BoyPos[i] });
	}
	Score[0] = Score[1] = 0;
	Half = 1; HalfT = 0; MatchClock = 0;
	KickoffTeam = Rng.Int(2);
	PlaceForKickoff(KickoffTeam);
	CamCenter = Proj.GroundToAtlas(FVector2D::ZeroVector);
}

void FPFMatch::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Stadium);
	Collector.AddReferencedObject(BallTex);
	Collector.AddReferencedObject(ShadowTex);
	Collector.AddReferencedObject(RingTex);
	for (int32 g = 0; g < 2; ++g) { Collector.AddReferencedObject(GoalShadow[g].Tex); Collector.AddReferencedObject(GoalRear[g].Tex); Collector.AddReferencedObject(GoalFront[g].Tex); }
	for (FPFSpriteSet& S : Sets) Collector.AddReferencedObject(S.Atlas);
	for (auto& KV : Mids) Collector.AddReferencedObject(KV.Value);
	for (FCrew& Cr : Crew) Collector.AddReferencedObject(Cr.Tex);
}

int32 FPFMatch::AttackDir(int32 Team) const
{
	const int32 D = Team == 0 ? 1 : -1;
	return Half == 1 ? D : -D;
}

FVector2D FPFMatch::GoalCentre(int32 Team, bool bOpponents) const
{
	const int32 D = AttackDir(Team);
	return FVector2D((bOpponents ? D : -D) * HalfLen, 0);
}

FVector2D FPFMatch::FormationSpot(const FPFPlayer& Pl) const
{
	const int32 D = AttackDir(Pl.Team);
	const bool bInPoss = Ball.Owner >= 0 && P[Ball.Owner].Team == Pl.Team;
	const float BallRel = Ball.Pos.X * D;   // ball position along our attack (+ = near their goal)
	float Depth = -46.f + Pl.Setup.Slot.X * 78.f;
	if (Pl.bKeeper) Depth = -50.f;
	else
	{
		Depth += FMath::Clamp(BallRel * 0.55f, -16.f, 24.f) + (bInPoss ? 7.f : -3.f);
		Depth = FMath::Clamp(Depth, -48.f, 47.f);
		// back line never past the ball by much when defending
		if (!bInPoss && Pl.Setup.Role == EPFRole::DEF) Depth = FMath::Min(Depth, BallRel - 4.f);
	}
	const float Width = bInPoss ? 27.f : 20.f;
	float Y = Pl.Setup.Slot.Y * Width * D + Ball.Pos.Y * (Pl.bKeeper ? 0.f : 0.3f);
	return FVector2D(Depth * D, FMath::Clamp(Y, -HalfWid + 2.f, HalfWid - 2.f));
}

void FPFMatch::PlaceForKickoff(int32 KickTeam)
{
	Ball = FPFBall();
	Ball.Pos = FVector(0, 0, 0.11f);
	for (FPFPlayer& Pl : P)
	{
		const int32 D = AttackDir(Pl.Team);
		FVector2D S = FormationSpot(Pl);
		S.X = FMath::Clamp(S.X * D, -50.f, -1.5f) * D;   // own half
		if (Pl.Team != KickTeam && FMath::Abs(S.X) < 10.f && FMath::Abs(S.Y) < 10.f) S.X = -10.5f * D;
		Pl.Pos = S; Pl.Vel = FVector2D::ZeroVector; Pl.Facing = FVector2D(D, 0);
		Pl.Act = EPFAct::None; Pl.PendingKick = EPFKick::None; Pl.bCharging = false; Pl.NoTouch = 0;
	}
	// two strikers on the spot for the kicking team
	int32 Taker = -1, Mate = -1;
	for (FPFPlayer& Pl : P)
		if (Pl.Team == KickTeam && !Pl.bKeeper)
		{
			if (Taker < 0 || Pl.Setup.Slot.X > P[Taker].Setup.Slot.X) { Mate = Taker; Taker = Pl.Index; }
			else if (Mate < 0 || Pl.Setup.Slot.X > P[Mate].Setup.Slot.X) Mate = Pl.Index;
		}
	if (Taker >= 0) { P[Taker].Pos = FVector2D(-0.6f * AttackDir(KickTeam), 0.f); Ball.Owner = -1; }
	if (Mate >= 0) P[Mate].Pos = FVector2D(-1.f * AttackDir(KickTeam), 6.f);
	Restart = EPFRestart::Kickoff; RestartTeam = KickTeam; RestartTaker = Taker;
	Phase = EPFPhase::PreKick; PhaseT = 0;
	// human = the taker if the human team kicks off, else its nearest to the ball
	for (int32 t = 0; t < 2; ++t)
	{
		const FPFTeamSetup& TS = t == 0 ? Setup.Home : Setup.Away;
		if (TS.bHuman && !Setup.bWatchOnly) SwitchHuman(t == KickTeam ? Taker : NearestToBall(t, true));
	}
}

void FPFMatch::PlaceForRestart()
{
	Ball = FPFBall();
	Ball.Pos = FVector(RestartSpot.X, RestartSpot.Y, 0.11f);
	Ball.LastTeam = 1 - RestartTeam;
	int32 Taker = -1;
	if (Restart == EPFRestart::GoalKick)
	{
		for (FPFPlayer& Pl : P) if (Pl.Team == RestartTeam && Pl.bKeeper) Taker = Pl.Index;
	}
	if (Taker < 0)
	{
		float Best = 1e9;
		for (FPFPlayer& Pl : P)
			if (Pl.Team == RestartTeam && !Pl.bKeeper)
			{
				const float Dd = FVector2D::DistSquared(Pl.Pos, RestartSpot);
				if (Dd < Best) { Best = Dd; Taker = Pl.Index; }
			}
	}
	for (FPFPlayer& Pl : P)
	{
		Pl.Act = EPFAct::None; Pl.PendingKick = EPFKick::None; Pl.bCharging = false;
		if (Restart == EPFRestart::Corner && !Pl.bKeeper)
		{
			// crowd the box
			const FVector2D GoalC = FVector2D(FMath::Sign(RestartSpot.X) * HalfLen, 0);
			if (Pl.Setup.Role != EPFRole::DEF || Pl.Team != RestartTeam)
				Pl.Pos = GoalC + FVector2D(-FMath::Sign(RestartSpot.X) * Rng.Range(4.f, 14.f), Rng.Range(-9.f, 9.f));
		}
	}
	if (Taker >= 0)
	{
		FPFPlayer& T = P[Taker];
		const FVector2D In = (Restart == EPFRestart::Penalty || Restart == EPFRestart::FreeKick) ? SafeN(GoalCentre(RestartTeam, true) - RestartSpot) : SafeN(FVector2D(-RestartSpot.X * 0.3f, -RestartSpot.Y));
		T.Pos = RestartSpot - In * 0.6f;
		T.Facing = In; T.Vel = FVector2D::ZeroVector;
	}
	RestartTaker = Taker;
	Phase = EPFPhase::Restart; PhaseT = 0;
	const FPFTeamSetup& TS = RestartTeam == 0 ? Setup.Home : Setup.Away;
	if (TS.bHuman && !Setup.bWatchOnly && Taker >= 0) SwitchHuman(Taker);
}

void FPFMatch::SwitchHuman(int32 NewIdx)
{
	if (NewIdx < 0) return;
	if (Human >= 0 && Human < P.Num()) { P[Human].bCharging = false; }
	Human = NewIdx;
}

int32 FPFMatch::NearestToBall(int32 Team, bool bOutfieldOnly) const
{
	int32 Best = -1; float BD = 1e9;
	for (const FPFPlayer& Pl : P)
	{
		if (Pl.Team != Team || (bOutfieldOnly && Pl.bKeeper)) continue;
		const float D = FVector2D::DistSquared(Pl.Pos, V2(Ball.Pos));
		if (D < BD) { BD = D; Best = Pl.Index; }
	}
	return Best;
}

void FPFMatch::Log(int32 Team, const FString& S)
{
	Events.Add({ MatchClock, Team, S });
}

// ------------------------------------------------------------------------------------------------ tick
void FPFMatch::Tick(float Dt, const FPFPadInput& Pad)
{
	Dt = FMath::Min(Dt, 0.1f);
	BannerT = FMath::Max(0.f, BannerT - Dt);
	// fixed 120 Hz steps; pressed-this-frame buttons go to the first step only
	Accum += Dt;
	FPFPadInput In = Pad;
	while (Accum >= 1.f / 120.f)
	{
		Step(1.f / 120.f, In);
		In.bPass = In.bLob = In.bThrough = In.bSwitch = In.bShootPressed = In.bShootReleased = false;
		Accum -= 1.f / 120.f;
	}
	// camera: follow the ball with a little lead, in atlas pixels
	const FVector2D Target = Proj.ToAtlas(FVector(Ball.Pos.X + Ball.Vel.X * 0.25f, Ball.Pos.Y + Ball.Vel.Y * 0.25f, 0));
	const float K = 1.f - FMath::Exp(-Dt * 4.5f);
	CamCenter += (Target - CamCenter) * K;
}

void FPFMatch::Step(float Dt, const FPFPadInput& Pad)
{
	PhaseT += Dt;
	switch (Phase)
	{
	case EPFPhase::PreKick:
		// kickoff: the taker plays it back after a beat (the human can play it immediately)
		if (RestartTaker >= 0)
		{
			FPFPlayer& T = P[RestartTaker];
			Ball.Owner = -1;
			const bool bHumanTaker = RestartTaker == Human;
			if ((bHumanTaker && (Pad.bPass || Pad.bLob || Pad.bShootReleased || PhaseT > 5.f)) || (!bHumanTaker && PhaseT > 1.2f))
			{
				Phase = EPFPhase::Play; PhaseT = 0;
				Sfx.Add(TEXT("whistle_short"));
				Ball.Owner = RestartTaker; Ball.LastTouch = RestartTaker; Ball.LastTeam = T.Team;
				int32 R = BestPassTarget(T, FVector2D(-AttackDir(T.Team), 0.3f), false);
				if (R >= 0) Kick(T, EPFKick::Pass, P[R].Pos, 0.6f, R);
				SetBanner(TEXT(""), 0);
			}
		}
		else { Phase = EPFPhase::Play; }
		return;
	case EPFPhase::Goal:
		Highlight = FMath::Max(0.f, Highlight - Dt);
		for (FPFPlayer& Pl : P) { Pl.Vel *= 0.95f; if (Pl.Act != EPFAct::None) { Pl.ActT += Dt; if (Pl.ActT > Pl.ActLen) Pl.Act = EPFAct::None; } }
		BallPhysics(Dt);
		if (PhaseT > 3.2f) PlaceForKickoff(KickoffTeam);
		return;
	case EPFPhase::HalfTime:
		if (PhaseT > 3.f) { Half = 2; HalfT = 0; PlaceForKickoff(1 - KickoffTeam); }
		return;
	case EPFPhase::FullTime:
		bFinished = PhaseT > 3.f;
		return;
	case EPFPhase::Restart:
	{
		// players drift to shape while the taker waits
		for (FPFPlayer& Pl : P) if (Pl.Index != RestartTaker && Pl.Index != Human) { const FVector2D S = FormationSpot(Pl); Pl.Wish = SafeN(S - Pl.Pos, FVector2D::ZeroVector) * FMath::Clamp(FVector2D::Distance(S, Pl.Pos) / 3.f, 0.f, 0.6f); }
		MovePlayers(Dt);
		if (RestartTaker >= 0)
		{
			FPFPlayer& T = P[RestartTaker];
			T.Pos = RestartSpot - SafeN(T.Facing) * 0.6f; T.Vel = FVector2D::ZeroVector;
			if (Restart == EPFRestart::ThrowIn) T.Pos.Y = FMath::Sign(RestartSpot.Y) * (HalfWid + 0.4f);
			Ball.Pos = FVector(Restart == EPFRestart::ThrowIn ? T.Pos.X : RestartSpot.X, Restart == EPFRestart::ThrowIn ? FMath::Sign(RestartSpot.Y) * (HalfWid - 0.25f) : RestartSpot.Y, Restart == EPFRestart::ThrowIn ? 1.9f : 0.11f); Ball.Vel = FVector::ZeroVector; Ball.Owner = -1;
			const bool bHumanTaker = RestartTaker == Human;
			if (bHumanTaker && Pad.Move.SizeSquared() > 0.04f) T.Facing = SafeN(Proj.ScreenDirToWorld(Pad.Move));
			const bool bGo = bHumanTaker ? (Pad.bPass || Pad.bLob || Pad.bThrough || Pad.bShootReleased || PhaseT > 6.f) : PhaseT > 1.4f;
			if (bGo)
			{
				Phase = EPFPhase::Play; PhaseT = 0;
				Ball.Owner = RestartTaker; Ball.LastTouch = RestartTaker; Ball.LastTeam = T.Team;
				const int32 D = AttackDir(T.Team);
				if (Restart == EPFRestart::ThrowIn)
				{
					int32 R = BestPassTarget(T, bHumanTaker && Pad.Move.SizeSquared() > 0.04f ? T.Facing : FVector2D(D, -FMath::Sign(RestartSpot.Y) * 0.5f), false);
					StartClip(T, TEXT("throwin"), EPFKick::Throw);
					T.KickTarget = R; T.KickAim = R >= 0 ? P[R].Pos : T.Pos + T.Facing * 15.f; T.KickPower = 0.5f;
				}
				else if (Restart == EPFRestart::Corner)
				{
					const FVector2D Box = FVector2D(FMath::Sign(RestartSpot.X) * (HalfLen - 8.f), Rng.Range(-5.f, 5.f));
					int32 R = -1; float BD = 1e9;
					for (FPFPlayer& Pl : P) if (Pl.Team == T.Team && Pl.Index != T.Index) { float Dd = FVector2D::DistSquared(Pl.Pos, Box); if (Dd < BD) { BD = Dd; R = Pl.Index; } }
					StartClip(T, TEXT("lob"), EPFKick::Lob); T.KickTarget = R; T.KickAim = R >= 0 ? P[R].Pos : Box; T.KickPower = 0.7f;
				}
				else if (Restart == EPFRestart::Penalty)
				{
					const FVector2D Goal = GoalCentre(T.Team, true);
					StartClip(T, TEXT("shot"), EPFKick::Shot);
					float Aim = Rng.Range(-GoalHalf + 0.4f, GoalHalf - 0.4f);
					if (bHumanTaker && Pad.Move.SizeSquared() > 0.04f) Aim = FMath::Clamp(FVector2D::CrossProduct(SafeN(Goal - T.Pos), SafeN(Proj.ScreenDirToWorld(Pad.Move))) * -6.f * (AttackDir(T.Team) > 0 ? 1 : -1), -GoalHalf + 0.4f, GoalHalf - 0.4f);
					T.KickAim = Goal + FVector2D(0, Aim); T.KickPower = 0.85f; T.KickTarget = -1;
				}
				else if (Restart == EPFRestart::FreeKick && FVector2D::Distance(T.Pos, GoalCentre(T.Team, true)) < 30.f)
				{
					StartClip(T, TEXT("shot"), EPFKick::Shot);
					T.KickAim = GoalCentre(T.Team, true) + FVector2D(0, Rng.Range(-GoalHalf + 0.5f, GoalHalf - 0.5f)); T.KickPower = 0.8f; T.KickTarget = -1;
				}
				else if (Restart == EPFRestart::GoalKick)
				{
					StartClip(T, T.bKeeper ? TEXT("gk_punt") : TEXT("lob"), EPFKick::Punt);
					T.KickAim = FVector2D(D * Rng.Range(0.f, 20.f), Rng.Range(-20.f, 20.f)); T.KickPower = 1.f; T.KickTarget = -1;
				}
				else
				{
					int32 R = BestPassTarget(T, T.Facing, false);
					StartClip(T, TEXT("pass"), EPFKick::Pass); T.KickTarget = R; T.KickAim = R >= 0 ? P[R].Pos : T.Pos + T.Facing * 15.f; T.KickPower = 0.6f;
				}
			}
		}
		return;
	}
	default: break;
	}

	// ---- open play
	HalfT += Dt;
	MatchClock = (Half - 1) * 45.f + FMath::Min(45.f, HalfT / Setup.HalfSeconds * 45.f);
	if (HalfT >= Setup.HalfSeconds && (Ball.Owner < 0 || FMath::Abs(Ball.Pos.X) < 30.f || HalfT > Setup.HalfSeconds + 8.f))
	{
		if (Half == 1) { Phase = EPFPhase::HalfTime; PhaseT = 0; SetBanner(TEXT("HALF TIME"), 3.f); Sfx.Add(TEXT("whistle_long")); return; }
		Phase = EPFPhase::FullTime; PhaseT = 0; SetBanner(TEXT("FULL TIME"), 3.f); Sfx.Add(TEXT("whistle_three")); return;
	}

	for (FPFPlayer& Pl : P) { Pl.NoTouch = FMath::Max(0.f, Pl.NoTouch - Dt); Pl.Wish = FVector2D::ZeroVector; Pl.bSprint = false; }
	Highlight = FMath::Max(0.f, Highlight - Dt);
	if (Ball.Owner >= 0 && FVector2D::Distance(P[Ball.Owner].Pos, GoalCentre(P[Ball.Owner].Team, true)) < 26.f) Highlight = FMath::Max(Highlight, 1.2f);
	TeamAI(0, Dt);
	TeamAI(1, Dt);
	if (Human >= 0 && !Setup.bWatchOnly) HumanControl(Dt, Pad);
	MovePlayers(Dt);
	MoveOfficials(Dt);
	MoveManagers(Dt);
	Possession(Dt);
	BallPhysics(Dt);
	CheckOut();
}

void FPFMatch::MoveManagers(float Dt)
{
	for (FPFPlayer& Mg : Managers)
	{
		if (Mg.Act != EPFAct::None) { Mg.ActT += Dt; if (Mg.ActT > Mg.ActLen) Mg.Act = EPFAct::None; Mg.Vel = FVector2D::ZeroVector; continue; }
		Mg.AIThink -= Dt;
		if (Mg.AIThink <= 0)
		{
			Mg.AIThink = Rng.Range(2.f, 5.f);
			// pace the technical area, or bark an instruction
			if (Rng.Unit() < 0.3f) { Mg.Act = EPFAct::Clip; Mg.Clip = Rng.Unit() < 0.5f ? FName(TEXT("coach_shout")) : FName(TEXT("coach_point_forward")); Mg.ActT = 0; Mg.ActLen = 1.4f; Mg.Facing = FVector2D(0, -1); }
			else Mg.Wish = FVector2D(Mg.Home.X + Rng.Range(-3.f, 3.f) - Mg.Pos.X, 0);
		}
		const float Dx = Mg.Wish.X;
		Mg.Vel = FVector2D(FMath::Abs(Dx) > 0.2f ? FMath::Sign(Dx) * 1.3f : 0.f, 0);
		Mg.Wish.X -= Mg.Vel.X * Dt;
		Mg.Pos += Mg.Vel * Dt;
		Mg.Stride += FMath::Abs(Mg.Vel.X) * Dt;
		if (Mg.Vel.SizeSquared() > 0.01f) Mg.Facing = SafeN(Mg.Vel); else Mg.Facing = FVector2D(0, -1);
	}
}

void FPFMatch::MoveOfficials(float Dt)
{
	for (int32 o = 0; o < Officials.Num(); ++o)
	{
		FPFPlayer& Of = Officials[o];
		FVector2D Target;
		if (o == 0) Target = V2(Ball.Pos) + FVector2D(-FMath::Sign(Ball.Pos.X + 0.01f) * 9.f, Ball.Pos.Y > 0 ? -7.f : 7.f);   // diagonal system, goal-side
		else
		{
			// each assistant runs one touchline, level with the second-last defender of the half he covers (approx: the ball, clamped)
			const float Side = o == 1 ? -1.f : 1.f;
			const float X = FMath::Clamp(Ball.Pos.X, Side < 0 ? -HalfLen : 0.f, Side < 0 ? 0.f : HalfLen);
			Target = FVector2D(X, Side * (HalfWid + 0.9f));
		}
		const FVector2D To = Target - Of.Pos;
		const float D = To.Size();
		const FVector2D Want = D > 0.8f ? SafeN(To) * FMath::Min(1.f, D / 5.f) : FVector2D::ZeroVector;
		Of.Vel = FMath::Lerp(Of.Vel, Want * 7.f, FMath::Min(1.f, Dt * 3.f));
		Of.Pos += Of.Vel * Dt;
		Of.Stride += Of.Vel.Size() * Dt;
		if (Of.Vel.SizeSquared() > 0.5f) Of.Facing = SafeN(Of.Vel);
		else Of.Facing = SafeN(V2(Ball.Pos) - Of.Pos);
		if (Of.Act != EPFAct::None) { Of.ActT += Dt; if (Of.ActT > Of.ActLen) Of.Act = EPFAct::None; }
	}
}

// ------------------------------------------------------------------------------------------------ human
void FPFMatch::HumanControl(float Dt, const FPFPadInput& Pad)
{
	FPFPlayer& H = P[Human];
	const FVector2D Stick = Pad.Move.SizeSquared() > 0.03f ? Pad.Move : FVector2D::ZeroVector;
	FVector2D WDir = Stick.IsZero() ? FVector2D::ZeroVector : SafeN(Proj.ScreenDirToWorld(Stick));
	const float Mag = FMath::Clamp(Stick.Size(), 0.f, 1.f);
	if (H.Act == EPFAct::None || H.Act == EPFAct::Clip) { H.Wish = WDir * Mag; H.bSprint = Pad.bSprint; }
	const FVector2D AimDir = WDir.IsZero() ? H.Facing : WDir;
	const bool bHasBall = Ball.Owner == Human;

	if (bHasBall || (Ball.Owner < 0 && Ball.LastTouch == Human && FVector2D::Distance(H.Pos, V2(Ball.Pos)) < 2.f))
	{
		if (H.Act != EPFAct::None) return;
		if (Pad.bPass) { const int32 R = BestPassTarget(H, AimDir, false); StartClip(H, TEXT("pass"), EPFKick::Pass); H.KickTarget = R; H.KickAim = R >= 0 ? P[R].Pos : H.Pos + AimDir * 14.f; H.KickPower = 0.6f; return; }
		if (Pad.bThrough) { const int32 R = BestPassTarget(H, AimDir, true); StartClip(H, TEXT("pass"), EPFKick::Through); H.KickTarget = R; H.KickAim = R >= 0 ? P[R].Pos : H.Pos + AimDir * 20.f; H.KickPower = 0.75f; return; }
		if (Pad.bLob) { const int32 R = BestPassTarget(H, AimDir, false); StartClip(H, TEXT("lob"), EPFKick::Lob); H.KickTarget = R; H.KickAim = R >= 0 ? P[R].Pos : H.Pos + AimDir * 28.f; H.KickPower = 0.8f; return; }
		if (Pad.bShootPressed) { H.bCharging = true; H.Charge = 0; }
		if (H.bCharging)
		{
			H.Charge = FMath::Min(1.f, H.Charge + Dt / 0.55f);
			if (Pad.bShootReleased || !Pad.bShootHeld || H.Charge >= 1.f)
			{
				H.bCharging = false;
				const FVector2D Goal = GoalCentre(H.Team, true);
				// stick steers the shot across the goal mouth
				const FVector2D ToGoal = SafeN(Goal - H.Pos);
				const float Side = FVector2D::CrossProduct(ToGoal, AimDir);
				FVector2D Aim = Goal + FVector2D(0, FMath::Clamp(-Side * 6.f, -GoalHalf + 0.4f, GoalHalf - 0.4f) * (AttackDir(H.Team) > 0 ? 1 : -1));
				if (FVector2D::Distance(H.Pos, Goal) > 40.f) Aim = H.Pos + AimDir * 40.f;
				StartClip(H, TEXT("shot"), EPFKick::Shot); H.KickAim = Aim; H.KickPower = FMath::Max(0.25f, H.Charge); H.KickTarget = -1;
			}
		}
		return;
	}

	// without the ball
	H.bCharging = false;
	if (Pad.bSwitch || Pad.bPass)
	{
		// next-best teammate to the ball (ISS style)
		int32 Best = -1; float BD = 1e9;
		for (const FPFPlayer& Pl : P)
			if (Pl.Team == H.Team && !Pl.bKeeper && Pl.Index != Human)
			{
				const float D = FVector2D::DistSquared(Pl.Pos, V2(Ball.Pos));
				if (D < BD) { BD = D; Best = Pl.Index; }
			}
		SwitchHuman(Best);
		return;
	}
	if (H.Act == EPFAct::None)
	{
		if (Pad.bShootPressed)
		{
			H.Act = EPFAct::Slide; H.Clip = TEXT("slide"); H.ActT = 0; H.ActLen = PFClips::Get(H.Clip).Length() / H.A.AnimRate;
			H.Facing = AimDir; H.Vel = AimDir * FMath::Max(H.Vel.Size() + 2.5f, 7.f);
			Sfx.Add(TEXT("slide"));
		}
		else if (Pad.bLob || Pad.bThrough)
		{
			StartClip(H, TEXT("tackle"));
			if (Ball.Owner >= 0 && P[Ball.Owner].Team != H.Team && FVector2D::Distance(H.Pos, V2(Ball.Pos)) < H.A.TackleReach)
			{
				FPFPlayer& O = P[Ball.Owner];
				const float Win = H.A.TackleWin - O.A.TouchDist * 0.08f + (1.f - O.A.FirstTouchErr) * -0.15f;
				if (Rng.Unit() < FMath::Clamp(Win, 0.1f, 0.9f))
				{
					Ball.Owner = -1; Ball.Vel = FVector(SafeN(V2(Ball.Pos) - H.Pos + H.Facing) * 4.f, 0.f); O.NoTouch = 0.5f; Ball.LastTouch = Human; Ball.LastTeam = H.Team;
				}
			}
		}
	}
}

int32 FPFMatch::BestPassTarget(const FPFPlayer& From, FVector2D Dir, bool bThrough) const
{
	Dir = SafeN(Dir, From.Facing);
	int32 Best = -1; float BestScore = -1e9;
	for (const FPFPlayer& Pl : P)
	{
		if (Pl.Team != From.Team || Pl.Index == From.Index) continue;
		const FVector2D To = Pl.Pos - From.Pos;
		const float D = To.Size();
		if (D < 4.f || D > 55.f) continue;
		const float Cos = FVector2D::DotProduct(To / D, Dir);
		if (Cos < 0.35f) continue;
		// open = nearest opponent to the lane
		float Open = 99;
		for (const FPFPlayer& O : P)
			if (O.Team != From.Team)
			{
				const FVector2D R = O.Pos - From.Pos;
				const float Tt = FMath::Clamp(FVector2D::DotProduct(R, To) / (D * D), 0.f, 1.f);
				Open = FMath::Min(Open, (float)FVector2D::Distance(O.Pos, From.Pos + To * Tt));
			}
		const float Forward = To.X * AttackDir(From.Team);
		const float S = Cos * 30.f - D * 0.35f + FMath::Min(Open, 6.f) * 3.f + (bThrough ? Forward * 0.4f : Forward * 0.1f) - (Pl.bKeeper ? 25.f : 0.f);
		if (S > BestScore) { BestScore = S; Best = Pl.Index; }
	}
	return Best;
}

// ------------------------------------------------------------------------------------------------ AI
void FPFMatch::TeamAI(int32 Team, float Dt)
{
	const bool bInPoss = Ball.Owner >= 0 && P[Ball.Owner].Team == Team;
	const FPFTeamSetup& TS = Team == 0 ? Setup.Home : Setup.Away;
	const bool bHumanTeam = TS.bHuman && !Setup.bWatchOnly;
	// presser = nearest outfielder to the ball (the human team's AI presses too, except the controlled player)
	int32 Press = -1, Cover = -1; float B1 = 1e9, B2 = 1e9;
	const FVector2D BallG = V2(Ball.Pos) + V2(Ball.Vel) * 0.35f;
	for (const FPFPlayer& Pl : P)
	{
		if (Pl.Team != Team || Pl.bKeeper) continue;
		const float D = FVector2D::DistSquared(Pl.Pos, BallG) / FMath::Square(FMath::Max(Pl.A.MaxSpeed, 1.f));
		if (D < B1) { B2 = B1; Cover = Press; B1 = D; Press = Pl.Index; }
		else if (D < B2) { B2 = D; Cover = Pl.Index; }
	}
	for (FPFPlayer& Pl : P)
	{
		if (Pl.Team != Team || Pl.bSentOff) continue;
		if (Pl.Index == Human && bHumanTeam) continue;
		if (Pl.bKeeper) { KeeperAI(Pl, Dt); continue; }
		if (Pl.Act == EPFAct::Slide || Pl.Act == EPFAct::Fall || Pl.Act == EPFAct::Celebrate) continue;
		if (Ball.Owner == Pl.Index) { CarrierAI(Pl, Dt); continue; }
		FVector2D Target = FormationSpot(Pl);
		float Urgency = 0.55f;
		// a pass is coming to me: go and meet it
		if (Ball.Owner < 0 && Ball.LastTeam == Team && P.IsValidIndex(Ball.LastTouch) && P[Ball.LastTouch].KickTarget == Pl.Index && Ball.Vel.SizeSquared() > 1.f)
		{
			Target = V2(Ball.Pos) + V2(Ball.Vel) * 0.3f; Urgency = 1.f;
		}
		else if (!bInPoss && Pl.Index == Press)
		{
			Target = BallG; Urgency = 1.f; Pl.bSprint = FVector2D::Distance(Pl.Pos, BallG) > 6.f;
			// tackle attempts (reaction-gated so poor players are late)
			Pl.AIThink -= Dt;
			if (Ball.Owner >= 0 && Pl.AIThink <= 0 && Pl.Act == EPFAct::None)
			{
				Pl.AIThink = Pl.A.ReactSec + Rng.Range(0.1f, 0.5f);
				const float D = FVector2D::Distance(Pl.Pos, V2(Ball.Pos));
				if (D < Pl.A.TackleReach)
				{
					FPFPlayer& O = P[Ball.Owner];
					StartClip(Pl, TEXT("tackle"));
					if (Rng.Unit() < FMath::Clamp(Pl.A.TackleWin * 0.7f + O.A.TouchDist * 0.08f, 0.08f, 0.85f))
					{
						Ball.Owner = -1; Ball.Vel = FVector(SafeN(V2(Ball.Pos) - Pl.Pos) * 3.5f, 0.f); O.NoTouch = 0.45f; Ball.LastTouch = Pl.Index; Ball.LastTeam = Team;
					}
				}
				else if (D < 3.2f && Rng.Unit() < 0.18f)
				{
					Pl.Act = EPFAct::Slide; Pl.Clip = TEXT("slide"); Pl.ActT = 0; Pl.ActLen = PFClips::Get(Pl.Clip).Length() / Pl.A.AnimRate;
					Pl.Facing = SafeN(V2(Ball.Pos) - Pl.Pos); Pl.Vel = Pl.Facing * FMath::Max(7.f, Pl.Vel.Size() + 2.f);
					Sfx.Add(TEXT("slide"));
				}
			}
		}
		else if (!bInPoss && Pl.Index == Cover)
		{
			// goal-side of the ball
			const FVector2D OwnGoal = GoalCentre(Team, false);
			Target = BallG + SafeN(OwnGoal - BallG) * 7.f; Urgency = 0.85f;
		}
		else if (bInPoss && Pl.Setup.Role == EPFRole::FWD)
		{
			// forwards make runs beyond
			Target.X += AttackDir(Team) * 4.f;
			Urgency = 0.75f;
		}
		const FVector2D To = Target - Pl.Pos;
		const float D = To.Size();
		Pl.Wish = D > 0.6f ? SafeN(To) * FMath::Clamp(D / 4.f, 0.f, 1.f) * Urgency : FVector2D::ZeroVector;
		if (D < 0.6f && Ball.Owner != Pl.Index) Pl.Facing = SafeN(FMath::Lerp(Pl.Facing, SafeN(V2(Ball.Pos) - Pl.Pos), 0.05f));
	}
}

void FPFMatch::CarrierAI(FPFPlayer& Pl, float Dt)
{
	const FVector2D Goal = GoalCentre(Pl.Team, true);
	const float DG = FVector2D::Distance(Pl.Pos, Goal);
	// nearest opponent pressure
	float Near = 99; FVector2D NearPos;
	for (const FPFPlayer& O : P) if (O.Team != Pl.Team) { const float D = FVector2D::Distance(O.Pos, Pl.Pos); if (D < Near) { Near = D; NearPos = O.Pos; } }
	FVector2D Dir = SafeN(Goal - Pl.Pos);
	if (Near < 5.f) Dir = SafeN(Dir * 1.f + SafeN(Pl.Pos - NearPos) * 0.6f);
	Pl.Wish = Dir; Pl.bSprint = Near > 3.f && DG > 25.f;
	Pl.AIThink -= Dt;
	if (Pl.AIThink > 0 || Pl.Act != EPFAct::None) return;
	Pl.AIThink = Pl.A.ReactSec * 0.6f + Rng.Range(0.05f, 0.25f);
	const float Angle = FMath::Abs(FMath::Atan2(FMath::Abs(Pl.Pos.Y), FMath::Abs(Goal.X - Pl.Pos.X)));
	const float ShootWill = DG < 12.f ? 0.7f : DG < 20.f ? 0.35f : DG < 28.f ? 0.12f * (0.5f + T01(Pl.Setup.Stats.Shooting)) : 0.f;
	if (Angle < 0.85f && Rng.Unit() < ShootWill)
	{
		StartClip(Pl, TEXT("shot"), EPFKick::Shot);
		Pl.KickAim = Goal + FVector2D(0, Rng.Range(-GoalHalf + 0.5f, GoalHalf - 0.5f)); Pl.KickPower = Rng.Range(0.55f, 1.f); Pl.KickTarget = -1;
		return;
	}
	const bool bPressured = Near < 3.5f;
	if (bPressured || (DG > 30.f && Rng.Unit() < 0.25f))
	{
		const int32 R = BestPassTarget(Pl, SafeN(Goal - Pl.Pos) * 0.6f + Pl.Facing * 0.4f, Rng.Unit() < 0.3f);
		if (R >= 0)
		{
			const float D = FVector2D::Distance(P[R].Pos, Pl.Pos);
			const bool bLob = D > 30.f || (Rng.Unit() < 0.15f);
			StartClip(Pl, bLob ? TEXT("lob") : TEXT("pass"), bLob ? EPFKick::Lob : EPFKick::Pass);
			Pl.KickTarget = R; Pl.KickAim = P[R].Pos + P[R].Vel * 0.6f; Pl.KickPower = 0.6f;
		}
	}
}

void FPFMatch::KeeperAI(FPFPlayer& K, float Dt)
{
	if (K.Act == EPFAct::Dive) return;
	const FVector2D Goal = GoalCentre(K.Team, false);
	const FVector2D BallG = V2(Ball.Pos);
	if (Ball.Owner == K.Index)
	{
		// hold briefly then distribute
		K.Wish = FVector2D::ZeroVector;
		K.AIThink -= Dt;
		if (K.AIThink <= 0 && K.Act == EPFAct::None)
		{
			const int32 R = BestPassTarget(K, FVector2D(AttackDir(K.Team), 0), false);
			if (R >= 0 && FVector2D::Distance(P[R].Pos, K.Pos) < 30.f) { StartClip(K, TEXT("gk_throw"), EPFKick::Throw); K.KickTarget = R; K.KickAim = P[R].Pos; K.KickPower = 0.6f; }
			else { StartClip(K, TEXT("gk_punt"), EPFKick::Punt); K.KickAim = FVector2D(AttackDir(K.Team) * Rng.Range(-5.f, 15.f), Rng.Range(-18.f, 18.f)); K.KickPower = 1.f; K.KickTarget = -1; }
		}
		return;
	}
	// shot coming? predict where it crosses the line
	const float VX = Ball.Vel.X;
	const bool bToward = (Goal.X > 0 && VX > 4.f) || (Goal.X < 0 && VX < -4.f);
	if (Ball.Owner < 0 && bToward && FMath::Abs(Ball.Pos.X - Goal.X) < 35.f && K.Act == EPFAct::None)
	{
		const float TimeTo = (K.Pos.X - Ball.Pos.X) / VX;
		if (TimeTo > 0 && TimeTo < 1.8f)
		{
			const float Y = Ball.Pos.Y + Ball.Vel.Y * TimeTo;
			const float Z = Ball.Pos.Z + Ball.Vel.Z * TimeTo - 0.5f * G * TimeTo * TimeTo;
			const float Off = Y - K.Pos.Y;
			if (FMath::Abs(Y) < GoalHalf + 1.f && K.ReactWait <= 0 && TimeTo < 1.2f)
			{
				K.ReactWait = K.A.KeeperReact;
			}
			if (K.ReactWait > 0)
			{
				K.ReactWait -= Dt;
				if (K.ReactWait <= 0)
				{
					const bool bRight = (Off * (Goal.X > 0 ? -1 : 1)) > 0;  // keeper's right while facing the pitch
					const bool bHigh = Z > 1.3f;
					if (FMath::Abs(Off) < 0.9f) { StartClip(K, bHigh ? TEXT("gk_catch_high") : TEXT("gk_catch")); K.Act = EPFAct::Dive; K.DiveDir = FVector2D::ZeroVector; }
					else
					{
						const FName C = bHigh ? (bRight ? TEXT("gk_dive_high_R") : TEXT("gk_dive_high_L")) : (bRight ? TEXT("gk_dive_low_R") : TEXT("gk_dive_low_L"));
						StartClip(K, C); K.Act = EPFAct::Dive;
						K.DiveDir = FVector2D(0, FMath::Sign(Off));
					}
					K.Facing = FVector2D(-FMath::Sign(Goal.X), 0);
				}
			}
			return;
		}
	}
	K.ReactWait = 0;
	// positioning: on the line between ball and goal centre, coming out a little
	const float Dist = FVector2D::Distance(BallG, Goal);
	FVector2D Spot = Goal + SafeN(BallG - Goal) * FMath::Clamp(Dist * 0.12f, 1.f, 6.f);
	Spot.Y = FMath::Clamp(Spot.Y, -GoalHalf, GoalHalf);
	// rush a loose ball in the box
	if (Ball.Owner < 0 && Dist < 15.f && FMath::Abs(BallG.Y) < 18.f && Ball.Pos.Z < 1.f) Spot = BallG;
	const FVector2D To = Spot - K.Pos;
	K.Wish = To.Size() > 0.3f ? SafeN(To) * FMath::Clamp(To.Size() / 3.f, 0.f, 1.f) : FVector2D::ZeroVector;
	if (To.Size() < 0.5f) K.Facing = SafeN(BallG - K.Pos);
}

// ------------------------------------------------------------------------------------------------ movement
void FPFMatch::StartClip(FPFPlayer& Pl, FName Clip, EPFKick Kick)
{
	const FPFClip& C = PFClips::Get(Clip);
	Pl.Act = EPFAct::Clip; Pl.Clip = Clip; Pl.ActT = 0; Pl.ActLen = C.Length() / Pl.A.AnimRate;
	Pl.PendingKick = Kick; Pl.bEventFired = false;
}

void FPFMatch::MovePlayers(float Dt)
{
	for (FPFPlayer& Pl : P)
	{
		// actions
		if (Pl.Act != EPFAct::None)
		{
			Pl.ActT += Dt;
			const FPFClip& C = PFClips::Get(Pl.Clip);
			if (!Pl.bEventFired && C.EventIndex >= 0)
			{
				int32 Idx = 0; C.At(Pl.ActT * Pl.A.AnimRate, &Idx);
				if (Idx >= C.EventIndex)
				{
					Pl.bEventFired = true;
					if (Pl.PendingKick != EPFKick::None) FireKick(Pl);
				}
			}
			if (Pl.Act == EPFAct::Slide)
			{
				Pl.Vel *= FMath::Exp(-Dt * 2.2f);
				// slide hits the ball
				if (Pl.ActT < 0.02f) Pl.bWonBallThisSlide = false;
				if (Pl.ActT < Pl.ActLen * 0.55f && Ball.Pos.Z < 0.6f && FVector2D::Distance(Pl.Pos + Pl.Facing * 0.7f, V2(Ball.Pos)) < 0.95f && Ball.Owner != Pl.Index)
				{
					Pl.bWonBallThisSlide = true;
					if (Ball.Owner >= 0) { P[Ball.Owner].NoTouch = 0.6f; }
					Ball.Owner = -1; Ball.Vel = FVector(SafeN(Pl.Facing + FVector2D(Rng.Range(-0.4f, 0.4f), Rng.Range(-0.4f, 0.4f))) * Rng.Range(5.f, 9.f), 0.5f);
					Ball.LastTouch = Pl.Index; Ball.LastTeam = Pl.Team; Pl.NoTouch = 0.4f;
				}
				// or the man (foul-free arcade fall for now)
				for (FPFPlayer& O : P)
					if (O.Team != Pl.Team && !O.bSentOff && O.Act == EPFAct::None && FVector2D::Distance(O.Pos, Pl.Pos + Pl.Facing * 0.6f) < 0.75f && Pl.ActT < Pl.ActLen * 0.5f)
					{
						O.Act = EPFAct::Fall; O.Clip = TEXT("fall"); O.ActT = 0; O.ActLen = PFClips::Get(O.Clip).Length(); O.Vel *= 0.3f;
						const bool bHadBall = Ball.Owner == O.Index || FVector2D::Distance(O.Pos, V2(Ball.Pos)) < 2.5f;
						if (Ball.Owner == O.Index) { Ball.Owner = -1; }
						// man before ball (or from behind) is a foul; a clean ball-first slide is not
						const bool bBehind = FVector2D::DotProduct(O.Facing, Pl.Facing) > 0.5f;
						if (!Pl.bWonBallThisSlide && bHadBall && Phase == EPFPhase::Play && Rng.Unit() < (bBehind ? 0.9f : 0.6f)) { Foul(Pl, O); return; }
					}
			}
			if (Pl.Act == EPFAct::Dive)
			{
				const float Prog = FMath::Clamp(Pl.ActT / 0.35f, 0.f, 1.f);
				const FVector2D Body = Pl.Pos + Pl.DiveDir * Pl.A.KeeperReach * Prog;
				const float ReachZ = (Pl.Clip.ToString().Contains(TEXT("high")) || Pl.Clip == TEXT("gk_catch_high")) ? 2.6f : 1.4f;
				if (Ball.Owner < 0 && Pl.NoTouch <= 0 && FVector2D::Distance(Body, V2(Ball.Pos)) < 0.9f + (Pl.DiveDir.IsZero() ? 0.4f : 0.f) && Ball.Pos.Z < ReachZ)
				{
					const float Speed = Ball.Vel.Size();
					const bool bCatch = Speed < 16.f + 12.f * T01(Pl.Setup.Stats.Keeping) && Rng.Unit() < 0.45f + 0.5f * T01(Pl.Setup.Stats.Keeping);
					if (bCatch) { Ball.Owner = Pl.Index; Ball.Vel = FVector::ZeroVector; Pl.AIThink = 1.2f; SetBanner(TEXT("SAVE!"), 1.0f); Sfx.Add(TEXT("catch")); }
					else
					{
						Ball.Vel = FVector(-Ball.Vel.X * 0.35f, Ball.Vel.Y * 0.4f + Pl.DiveDir.Y * 5.f, 3.f);
						SetBanner(TEXT("SAVE!"), 1.0f); Sfx.Add(TEXT("crowd_ooh"));
					}
					Ball.LastTouch = Pl.Index; Ball.LastTeam = Pl.Team; Pl.NoTouch = 0.4f;
				}
				if (Prog < 1.f) Pl.Pos += Pl.DiveDir * Pl.A.KeeperReach / 0.35f * Dt;
				Pl.Vel = FVector2D::ZeroVector;
			}
			if (Pl.ActT >= Pl.ActLen)
			{
				Pl.Act = EPFAct::None; Pl.PendingKick = EPFKick::None;
			}
		}
		if (Pl.Act == EPFAct::Slide || Pl.Act == EPFAct::Fall || Pl.Act == EPFAct::Dive || Pl.Act == EPFAct::Celebrate)
		{
			if (Pl.Act == EPFAct::Fall) Pl.Vel *= FMath::Exp(-Dt * 5.f);
			if (Pl.Act == EPFAct::Celebrate) { Pl.Vel *= FMath::Exp(-Dt * 1.2f); }
			Pl.Pos += Pl.Vel * Dt;
			continue;
		}
		const bool bKicking = Pl.Act == EPFAct::Clip && !Pl.bEventFired && Pl.PendingKick != EPFKick::None;
		float MaxV = Pl.A.MaxSpeed * (Pl.bSprint ? Pl.A.SprintMul : 1.f) * (Pl.bKeeper ? 0.85f : 1.f);
		if (Ball.Owner == Pl.Index) MaxV *= 0.9f;
		if (bKicking) MaxV *= 0.35f;
		FVector2D Wish = Pl.Wish;
		if (Wish.SizeSquared() > 1.f) Wish = SafeN(Wish);
		// turning: rotate facing toward the wish at a stat-limited rate (clumsy players swing wide)
		if (Wish.SizeSquared() > 0.01f && !bKicking)
		{
			const FVector2D Want = SafeN(Wish);
			const float Ang = FMath::RadiansToDegrees(FMath::Atan2(FVector2D::CrossProduct(Pl.Facing, Want), FVector2D::DotProduct(Pl.Facing, Want)));
			const float MaxTurn = Pl.A.TurnRate * Dt * (Pl.Vel.Size() > 5.f ? 0.7f : 1.f);
			Pl.Facing = SafeN(Rot(Pl.Facing, FMath::Clamp(Ang, -MaxTurn, MaxTurn)));
		}
		const FVector2D TargetV = Wish.IsNearlyZero() ? FVector2D::ZeroVector : Pl.Facing * MaxV * Wish.Size();
		const FVector2D DV = TargetV - Pl.Vel;
		const float Acc = (TargetV.SizeSquared() < Pl.Vel.SizeSquared() ? Pl.A.Accel * 1.6f : Pl.A.Accel);
		Pl.Vel += DV.GetClampedToMaxSize(Acc * Dt);
		Pl.Pos += Pl.Vel * Dt;
		Pl.Stride += Pl.Vel.Size() * Dt;
		Pl.Pos.X = FMath::Clamp(Pl.Pos.X, -HalfLen - 4.f, HalfLen + 4.f);
		Pl.Pos.Y = FMath::Clamp(Pl.Pos.Y, -HalfWid - 3.f, HalfWid + 3.f);
	}
	// light separation so bodies do not stack
	for (int32 i = 0; i < P.Num(); ++i)
		for (int32 j = i + 1; j < P.Num(); ++j)
		{
			FVector2D D = P[j].Pos - P[i].Pos;
			const float L = D.Size();
			if (L < 0.55f && L > 1e-3f) { const FVector2D Push = D / L * (0.55f - L) * 0.5f; P[i].Pos -= Push; P[j].Pos += Push; }
		}
}

// ------------------------------------------------------------------------------------------------ ball
void FPFMatch::FireKick(FPFPlayer& Pl)
{
	const bool bInReach = Ball.Owner == Pl.Index || (Ball.Owner < 0 && FVector2D::Distance(Pl.Pos, V2(Ball.Pos)) < 1.6f && Ball.Pos.Z < 1.2f)
		|| (Pl.PendingKick == EPFKick::Throw) || (Pl.PendingKick == EPFKick::Punt && Ball.Owner == Pl.Index);
	if (!bInReach) return;
	FVector2D Aim = Pl.KickAim;
	if (Pl.KickTarget >= 0 && Pl.PendingKick != EPFKick::Shot)
	{
		const FPFPlayer& R = P[Pl.KickTarget];
		const float D = FVector2D::Distance(R.Pos, Pl.Pos);
		const float Lead = Pl.PendingKick == EPFKick::Through ? 0.9f : 0.35f;
		Aim = R.Pos + R.Vel * (D / 18.f) * (1.f + Lead) + (Pl.PendingKick == EPFKick::Through ? SafeN(R.Vel, FVector2D(AttackDir(R.Team), 0)) * 5.f : FVector2D::ZeroVector);
	}
	Kick(Pl, Pl.PendingKick, Aim, Pl.KickPower, Pl.KickTarget);
	Pl.PendingKick = EPFKick::None;
}

void FPFMatch::Kick(FPFPlayer& Pl, EPFKick Type, FVector2D Target, float Power, int32 Receiver)
{
	const FVector2D From = V2(Ball.Pos);
	FVector2D To = Target - From;
	const float D = FMath::Max(1.f, (float)To.Size());
	FVector2D Dir = To / D;
	FVector V;
	float Err = 0;
	switch (Type)
	{
	case EPFKick::Shot:
	{
		Err = Pl.A.ShotErrDeg * (0.6f + Power * 0.7f) * (Pl.bSprint ? 1.4f : 1.f);
		Dir = Rot(Dir, Rng.Gauss() * Err * 0.5f);
		const float Spd = FMath::Lerp(13.f, Pl.A.ShotPower, Power);
		V = FVector(Dir * Spd, 1.0f + Power * 4.5f + Rng.Range(-0.5f, 1.5f) + FMath::Max(0.f, (D - 18.f) * 0.08f));
		Shots[Pl.Team]++;
		LastShotTeam = Pl.Team;
		Highlight = FMath::Max(Highlight, 2.5f);
		break;
	}
	case EPFKick::Lob: case EPFKick::Punt: case EPFKick::Clear:
	{
		Err = Pl.A.PassErrDeg * 1.2f;
		Dir = Rot(Dir, Rng.Gauss() * Err * 0.5f);
		const float Dd = D * (1.f + Rng.Gauss() * Err * 0.006f);
		const float Vh = FMath::Clamp(Dd * 0.55f + 5.f, 9.f, Type == EPFKick::Punt ? 30.f : 26.f);
		const float Tflight = Dd / Vh * 1.08f;
		V = FVector(Dir * Vh, FMath::Clamp(0.5f * G * Tflight, 3.f, 16.f));
		break;
	}
	case EPFKick::Throw:
	{
		const float Vh = FMath::Clamp(D * 0.9f + 3.f, 6.f, 16.f);
		V = FVector(Dir * Vh, Pl.bKeeper ? 1.f : 3.5f);
		break;
	}
	case EPFKick::Header:
	{
		Err = 12.f * (1.f - T01(Pl.Setup.Stats.Heading)) + 3.f;
		Dir = Rot(Dir, Rng.Gauss() * Err * 0.5f);
		V = FVector(Dir * FMath::Lerp(8.f, 18.f, T01(Pl.Setup.Stats.Heading)), Rng.Range(-1.f, 2.f));
		break;
	}
	default:  // ground pass / through ball
	{
		Err = Pl.A.PassErrDeg * (Pl.bSprint ? 1.4f : 1.f);
		Dir = Rot(Dir, Rng.Gauss() * Err * 0.5f);
		const float Spd = FMath::Clamp((7.f + D * 0.75f) * Pl.A.PassPower, 8.f, 27.f);
		V = FVector(Dir * Spd, 0.4f);
		break;
	}
	}
	Ball.Owner = -1;
	Ball.Vel = V;
	Ball.Pos.Z = FMath::Max(Ball.Pos.Z, 0.11f);
	Sfx.Add(Type == EPFKick::Shot || Type == EPFKick::Punt || Type == EPFKick::Clear ? FName(TEXT("kick_shot")) : Type == EPFKick::Header ? FName(TEXT("kick_header")) : Type == EPFKick::Throw ? FName() : FName(TEXT("kick_pass")));
	Ball.LastTouch = Pl.Index; Ball.LastTeam = Pl.Team;
	Ball.Curl = 0;
	Pl.NoTouch = 0.3f;
	Pl.KickTarget = Receiver;
	// control follows the ball to the receiver on the human team (ISS style)
	const FPFTeamSetup& TS = Pl.Team == 0 ? Setup.Home : Setup.Away;
	if (TS.bHuman && !Setup.bWatchOnly && Receiver >= 0 && Type != EPFKick::Shot) SwitchHuman(Receiver);
}

void FPFMatch::BallPhysics(float Dt)
{
	if (Ball.Owner >= 0)
	{
		FPFPlayer& O = P[Ball.Owner];
		const FVector2D Foot = O.Pos + O.Facing * 0.55f;
		const FVector2D BP = V2(Ball.Pos);
		const float Spd = O.Vel.Size();
		if (O.bKeeper && O.Act != EPFAct::Clip) { Ball.Pos = FVector(O.Pos + O.Facing * 0.3f, 1.0f); Ball.Vel = FVector::ZeroVector; return; }
		if (O.Act == EPFAct::Clip && (O.PendingKick == EPFKick::Throw || (O.bKeeper && O.PendingKick == EPFKick::Punt)))
		{
			// held in the hands until the release frame (throw-ins are taken from just outside the line)
			FVector2D Hold = O.Pos + O.Facing * 0.3f;
			Hold.Y = FMath::Clamp(Hold.Y, -HalfWid + 0.25f, HalfWid - 0.25f);
			Ball.Pos = FVector(Hold, O.bKeeper ? 1.0f : 1.9f); Ball.Vel = FVector::ZeroVector;
			return;
		}
		if (Spd < 1.2f)
		{
			// standing: ball settles at the feet
			const FVector2D NP = FMath::Lerp(BP, Foot, FMath::Min(1.f, Dt * 8.f));
			Ball.Vel = FVector((NP - BP) / Dt, 0); Ball.Pos = FVector(NP, 0.11f);
		}
		else
		{
			// dribbling: discrete touches. Ahead of the foot and slower than the player = touch it on.
			const FVector2D Rel = BP - O.Pos;
			const float Ahead = FVector2D::DotProduct(Rel, O.Facing);
			const float Side = FMath::Abs(FVector2D::CrossProduct(O.Facing, Rel));
			const FVector2D BV = V2(Ball.Vel);
			if (Ahead < 0.65f || Side > 0.7f || FVector2D::DotProduct(BV, O.Facing) < Spd * 0.85f)
			{
				if (FVector2D::Distance(BP, Foot) < 1.0f || Ahead < 0.65f)
				{
					const float Heavy = O.A.TouchDist * (O.bSprint ? 1.6f : 1.f);
					const FVector2D NV = O.Facing * (Spd + Heavy * 2.2f) + FVector2D(Rng.Gauss(), Rng.Gauss()) * Heavy * 0.25f;
					Ball.Vel = FVector(NV, 0);
					Ball.Pos = FVector(Foot, 0.11f);
				}
			}
			// lose it if it runs away
			if (FVector2D::Distance(BP, O.Pos) > 3.2f) { Ball.Owner = -1; O.NoTouch = 0.25f; }
		}
	}
	// integrate
	FVector& V = Ball.Vel;
	if (Ball.Pos.Z > 0.12f || V.Z > 0.1f)
	{
		V.Z -= G * Dt;
		const float S = V.Size();
		V -= V * (0.012f * S) * Dt;     // air drag
	}
	else
	{
		FVector2D H(V.X, V.Y);
		const float S = H.Size();
		const float Dec = (2.0f + 0.06f * S) * Dt;
		H = S > Dec ? H * ((S - Dec) / S) : FVector2D::ZeroVector;
		V.X = H.X; V.Y = H.Y; V.Z = 0;
	}
	Ball.Pos += V * Dt;
	if (Ball.Pos.Z < 0.11f)
	{
		Ball.Pos.Z = 0.11f;
		if (V.Z < -1.8f) { if (V.Z < -4.f) Sfx.Add(TEXT("ball_bounce")); V.Z = -V.Z * 0.52f; V.X *= 0.82f; V.Y *= 0.82f; }
		else V.Z = 0;
	}
	Ball.Roll += FVector2D(V.X, V.Y).Size() * Dt;
	// keepers block shots that come at them (no dive needed)
	if (Ball.Owner < 0 && FVector2D(V.X, V.Y).SizeSquared() > 64.f)
		for (FPFPlayer& K : P)
		{
			if (!K.bKeeper || K.NoTouch > 0 || K.Act == EPFAct::Dive || Ball.LastTeam == K.Team) continue;
			const float Kee = T01(K.Setup.Stats.Keeping);
			if (FVector2D::Distance(K.Pos, V2(Ball.Pos)) < 0.75f + Kee * 0.6f && Ball.Pos.Z < 2.3f)
			{
				const bool bCatch = Rng.Unit() < 0.3f + 0.6f * Kee;
				if (bCatch) { Ball.Owner = K.Index; Ball.Vel = FVector::ZeroVector; K.AIThink = 1.2f; StartClip(K, Ball.Pos.Z > 1.4f ? TEXT("gk_catch_high") : TEXT("gk_catch")); K.Act = EPFAct::None; }
				else { V = FVector(-V.X * 0.3f, V.Y * 0.5f + Rng.Range(-4.f, 4.f), 2.f); }
				Ball.LastTouch = K.Index; Ball.LastTeam = K.Team; K.NoTouch = 0.3f;
				SetBanner(TEXT("SAVE!"), 0.8f);
				Sfx.Add(bCatch ? FName(TEXT("catch")) : FName(TEXT("crowd_ooh")));
				break;
			}
		}
	// goal frame: posts and bar
	for (int32 g = -1; g <= 1; g += 2)
	{
		const float GX = g * HalfLen;
		if (FMath::Abs(Ball.Pos.X - GX) < 0.25f)
		{
			for (float PY : { -GoalHalf, GoalHalf })
				if (FMath::Abs(Ball.Pos.Y - PY) < 0.2f && Ball.Pos.Z < GoalH) { V.Y = -V.Y * 0.6f; V.X = -V.X * 0.5f; Ball.Pos.Y = PY + FMath::Sign(Ball.Pos.Y - PY) * 0.21f; SetBanner(TEXT("POST!"), 0.8f); Sfx.Add(TEXT("post")); Sfx.Add(TEXT("crowd_ooh")); }
			if (FMath::Abs(Ball.Pos.Y) < GoalHalf && FMath::Abs(Ball.Pos.Z - GoalH) < 0.2f) { V.Z = -FMath::Abs(V.Z) * 0.5f; V.X = -V.X * 0.5f; SetBanner(TEXT("CROSSBAR!"), 0.8f); Sfx.Add(TEXT("post")); Sfx.Add(TEXT("crowd_ooh")); }
		}
	}
}

void FPFMatch::Possession(float Dt)
{
	if (Ball.Owner >= 0) return;
	// headers: ball at head height near a player who is not busy
	if (Ball.Pos.Z > 1.3f && Ball.Pos.Z < 2.7f)
	{
		for (FPFPlayer& Pl : P)
		{
			if (Pl.bKeeper || Pl.NoTouch > 0 || Pl.Act != EPFAct::None) continue;
			if (FVector2D::Distance(Pl.Pos, V2(Ball.Pos)) > 0.5f + Pl.A.HeadReach * 0.25f) continue;
			if (Rng.Unit() > 0.15f + 0.6f * T01(Pl.Setup.Stats.Heading)) { Pl.NoTouch = 0.3f; continue; }
			StartClip(Pl, TEXT("header"));
			const FVector2D Goal = GoalCentre(Pl.Team, true);
			FVector2D Aim;
			if (FVector2D::Distance(Pl.Pos, Goal) < 18.f) { Aim = Goal + FVector2D(0, Rng.Range(-3.f, 3.f)); Shots[Pl.Team]++; LastShotTeam = Pl.Team; }
			else { const int32 R = BestPassTarget(Pl, FVector2D(AttackDir(Pl.Team), 0), false); Aim = R >= 0 ? P[R].Pos : Pl.Pos + FVector2D(AttackDir(Pl.Team) * 15.f, 0); }
			Kick(Pl, EPFKick::Header, Aim, 0.7f, -1);
			return;
		}
	}
	if (Ball.Pos.Z > 1.2f) return;
	int32 Best = -1; float BD = 1e9;
	for (FPFPlayer& Pl : P)
	{
		if (Pl.bSentOff || Pl.NoTouch > 0 || Pl.Act == EPFAct::Fall || Pl.Act == EPFAct::Celebrate || Pl.Act == EPFAct::Dive) continue;
		const float Reach = Pl.bKeeper ? 1.1f : 0.85f;
		const float D = FVector2D::Distance(Pl.Pos + Pl.Facing * 0.3f, V2(Ball.Pos));
		if (D < Reach && D < BD) { BD = D; Best = Pl.Index; }
	}
	if (Best < 0) return;
	FPFPlayer& Pl = P[Best];
	const float BallSpeed = Ball.Vel.Size();
	// first touch: fast balls and poor control = it bounces off
	const float MisP = Pl.A.FirstTouchErr * FMath::Clamp((BallSpeed - 6.f) / 18.f, 0.f, 1.f) + (Pl.bKeeper ? 0.f : 0.03f);
	if (!Pl.bKeeper && Rng.Unit() < MisP)
	{
		Ball.Vel = FVector(SafeN(FVector2D(Ball.Vel.X, Ball.Vel.Y)) * BallSpeed * 0.35f + FVector2D(Rng.Gauss(), Rng.Gauss()) * 2.5f, 0.6f);
		Pl.NoTouch = 0.35f; Ball.LastTouch = Pl.Index; Ball.LastTeam = Pl.Team;
		return;
	}
	const int32 PrevTeam = Ball.LastTeam;
	Ball.Owner = Best; Ball.LastTouch = Best; Ball.LastTeam = Pl.Team;
	Ball.Vel *= 0.3f;
	if (Pl.bKeeper) { Pl.AIThink = 1.0f; }
	const FPFTeamSetup& TS = Pl.Team == 0 ? Setup.Home : Setup.Away;
	if (TS.bHuman && !Setup.bWatchOnly && !Pl.bKeeper) SwitchHuman(Best);
	// auto-switch the human's defender when the opposition wins it far from him
	for (int32 t = 0; t < 2; ++t)
	{
		const FPFTeamSetup& HT = t == 0 ? Setup.Home : Setup.Away;
		if (!HT.bHuman || Setup.bWatchOnly || t == Pl.Team || Human < 0) continue;
		const int32 N = NearestToBall(t, true);
		if (N >= 0 && FVector2D::Distance(P[Human].Pos, V2(Ball.Pos)) > FVector2D::Distance(P[N].Pos, V2(Ball.Pos)) + 8.f) SwitchHuman(N);
	}
	(void)PrevTeam;
}

void FPFMatch::CheckOut()
{
	const FVector& B = Ball.Pos;
	if (FMath::Abs(B.X) > HalfLen + 0.11f)
	{
		const float Side = FMath::Sign(B.X);
		if (FMath::Abs(B.Y) < GoalHalf && B.Z < GoalH)
		{
			// which team attacks this goal?
			const int32 Scorer = AttackDir(0) == int32(Side) ? 0 : 1;
			Goal(Scorer);
			return;
		}
		const int32 Defending = AttackDir(0) == int32(Side) ? 1 : 0;
		if (Ball.LastTeam == Defending)
		{
			Restart = EPFRestart::Corner; RestartTeam = 1 - Defending;
			RestartSpot = FVector2D(Side * (HalfLen - 0.3f), FMath::Sign(B.Y) * (HalfWid - 0.3f));
			SetBanner(TEXT("CORNER KICK"));
		}
		else
		{
			Restart = EPFRestart::GoalKick; RestartTeam = Defending;
			RestartSpot = FVector2D(Side * (HalfLen - 5.5f), FMath::Sign(B.Y) * 4.f);
			SetBanner(TEXT("GOAL KICK"));
		}
		PlaceForRestart();
		return;
	}
	if (FMath::Abs(B.Y) > HalfWid + 0.11f)
	{
		const int32 L = B.Y < 0 ? 1 : 2;
		if (Officials.IsValidIndex(L)) { Officials[L].Act = EPFAct::Clip; Officials[L].Clip = TEXT("lino_flag"); Officials[L].ActT = 0; Officials[L].ActLen = 1.4f; }
		Restart = EPFRestart::ThrowIn; RestartTeam = 1 - Ball.LastTeam;
		RestartSpot = FVector2D(FMath::Clamp(B.X, -HalfLen + 1.f, HalfLen - 1.f), FMath::Sign(B.Y) * (HalfWid - 0.25f));
		SetBanner(TEXT("THROW IN"));
		PlaceForRestart();
	}
}

void FPFMatch::Goal(int32 ScoringTeam)
{
	Score[ScoringTeam]++;
	const bool bOwnGoal = Ball.LastTeam != ScoringTeam;
	FString Who = P.IsValidIndex(Ball.LastTouch) ? P[Ball.LastTouch].Setup.Name : TEXT("");
	Log(ScoringTeam, FString::Printf(TEXT("%d' %s%s"), FMath::Max(1, FMath::CeilToInt(MatchClock)), *Who, bOwnGoal ? TEXT(" (o.g.)") : TEXT("")));
	SetBanner(TEXT("GOAL!"), 3.f);
	Highlight = 3.5f;
	Sfx.Add(TEXT("net")); Sfx.Add(TEXT("crowd_goal")); Sfx.Add(TEXT("whistle_short"));
	if (Officials.Num()) { Officials[0].Act = EPFAct::Clip; Officials[0].Clip = TEXT("ref_whistle"); Officials[0].ActT = 0; Officials[0].ActLen = 1.2f; }
	for (int32 t = 0; t < Managers.Num(); ++t)
	{
		FPFPlayer& Mg = Managers[t];
		Mg.Act = EPFAct::Clip; Mg.ActT = 0; Mg.ActLen = 2.5f; Mg.Facing = FVector2D(0, -1);
		Mg.Clip = t == ScoringTeam ? FName(TEXT("coach_celebrate")) : FName(TEXT("coach_frustrated"));
	}
	Phase = EPFPhase::Goal; PhaseT = 0;
	KickoffTeam = 1 - ScoringTeam;
	// scorer celebrates, conceding keeper despairs
	for (FPFPlayer& Pl : P)
	{
		if (Pl.Index == Ball.LastTouch && !bOwnGoal)
		{
			Pl.Act = EPFAct::Celebrate; Pl.Clip = Rng.Unit() < 0.4f ? TEXT("knee_slide") : TEXT("celebrate"); Pl.ActT = 0; Pl.ActLen = 3.2f;
			Pl.Vel = Pl.Facing * 5.f;
		}
		else if (Pl.Team != ScoringTeam && Pl.bKeeper)
		{
			Pl.Act = EPFAct::Celebrate; Pl.Clip = TEXT("dejected"); Pl.ActT = 0; Pl.ActLen = 3.2f; Pl.Vel = FVector2D::ZeroVector;
		}
	}
	Ball.Vel *= 0.2f;
}

void FPFMatch::Foul(FPFPlayer& Off, FPFPlayer& Vic)
{
	Fouls[Off.Team]++;
	Sfx.Add(TEXT("whistle_short"));
	// cards
	const float R = Rng.Unit();
	FString Card;
	if (R < 0.03f || (R < 0.30f && Off.bBooked))
	{
		Off.bSentOff = true; Reds[Off.Team]++; Card = TEXT("RED CARD");
		Off.Pos = FVector2D(Off.Pos.X, -HalfWid - 6.f); Off.Vel = FVector2D::ZeroVector;
		Log(Off.Team, FString::Printf(TEXT("%d' %s sent off"), FMath::Max(1, FMath::CeilToInt(MatchClock)), *Off.Setup.Name));
		if (Off.Index == Human) { const int32 N = NearestToBall(Off.Team, true); if (N >= 0) SwitchHuman(N); }
	}
	else if (R < 0.30f) { Off.bBooked = true; Yellows[Off.Team]++; Card = TEXT("YELLOW CARD"); }
	if (Officials.Num())
	{
		FPFPlayer& Ref = Officials[0];
		Ref.Act = EPFAct::Clip; Ref.Clip = Card.IsEmpty() ? TEXT("ref_whistle") : TEXT("ref_card"); Ref.ActT = 0; Ref.ActLen = Card.IsEmpty() ? 0.8f : 1.9f;
		Ref.Facing = SafeN(Off.Pos - Ref.Pos);
	}
	// where: inside the box = penalty
	const FVector2D Spot = Vic.Pos;
	const int32 D = AttackDir(Vic.Team);
	const bool bInBox = (Spot.X * D > HalfLen - 16.5f) && FMath::Abs(Spot.Y) < 20.15f;
	RestartTeam = Vic.Team;
	if (bInBox)
	{
		Restart = EPFRestart::Penalty;
		RestartSpot = FVector2D(D * (HalfLen - 11.f), 0);
		SetBanner(Card.IsEmpty() ? TEXT("PENALTY!") : FString::Printf(TEXT("PENALTY!  %s"), *Card), 2.2f);
	}
	else
	{
		Restart = EPFRestart::FreeKick;
		RestartSpot = FVector2D(FMath::Clamp(Spot.X, -HalfLen + 2.f, HalfLen - 2.f), FMath::Clamp(Spot.Y, -HalfWid + 1.f, HalfWid - 1.f));
		SetBanner(Card.IsEmpty() ? TEXT("FOUL!") : Card, 1.8f);
	}
	PlaceForRestart();
	if (Restart == EPFRestart::Penalty)
	{
		// everyone else out of the box, keepers on the line
		for (FPFPlayer& Pl : P)
		{
			if (Pl.Index == RestartTaker || Pl.bSentOff) continue;
			if (Pl.bKeeper && Pl.Team != Vic.Team) { Pl.Pos = FVector2D(D * (HalfLen - 0.3f), 0); Pl.Facing = FVector2D(-D, 0); continue; }
			if (Pl.Pos.X * D > HalfLen - 18.f) Pl.Pos.X = D * (HalfLen - 18.5f);
		}
	}
}

// ------------------------------------------------------------------------------------------------ rendering
EPFDir FPFMatch::ScreenDir(FVector2D WorldDir) const
{
	const FVector2D S = Proj.DirToScreen(WorldDir);
	// angle from screen-down (south), clockwise toward screen-left?  PixelLab: south-east = down-right.
	float A = FMath::RadiansToDegrees(FMath::Atan2(S.X, S.Y));   // 0 = down, +90 = right
	// directions clockwise S(0) SE(45 right-down) E(90 right) NE(135) N(180) NW(-135) W(-90) SW(-45)
	int32 Idx = FMath::RoundToInt(A / 45.f);
	Idx = (Idx % 8 + 8) % 8;
	static const EPFDir Map[8] = { EPFDir::S, EPFDir::SE, EPFDir::E, EPFDir::NE, EPFDir::N, EPFDir::NW, EPFDir::W, EPFDir::SW };
	return Map[Idx];
}

FName FPFMatch::PoseFor(const FPFPlayer& Pl) const
{
	if (Pl.Act != EPFAct::None)
	{
		const FPFClip& C = PFClips::Get(Pl.Clip);
		if (Pl.Act == EPFAct::Celebrate && Pl.Clip == TEXT("celebrate"))
			return C.At(FMath::Fmod(Pl.ActT, C.Length()));
		return C.At(Pl.ActT * Pl.A.AnimRate);
	}
	if (Pl.bKeeper && Ball.Owner == Pl.Index) return TEXT("gk_hold_ball");
	const float S = Pl.Vel.Size();
	if (S < 0.35f)
	{
		if (Pl.bKeeper && FVector2D::Distance(Pl.Pos, V2(Ball.Pos)) < 30.f) return TEXT("gk_ready");
		return FMath::Fmod(Pl.Stride * 0.f + (Pl.Index * 0.37f + MatchClock * 60.f), 1.6f) < 0.8f ? FName(TEXT("idle")) : FName(TEXT("idle_breathe"));
	}
	if (Pl.bCharging) return TEXT("shot_plant");
	// distance-driven cycles: the feet stay planted because frames advance with ground covered
	if (S < 2.2f)
	{
		static const FName W[] = { TEXT("walk_contactR"), TEXT("idle"), TEXT("walk_contactL"), TEXT("idle") };
		return W[int32(Pl.Stride / 0.42f) % 4];
	}
	const bool bSprint = S > Pl.A.MaxSpeed * 1.04f && !Pl.bKeeper;
	const float Cycle = bSprint ? 3.6f : 2.3f + S * 0.12f;   // metres per full cycle (two strides)
	const int32 F = int32(Pl.Stride / (Cycle / 6.f)) % 6;
	if (Ball.Owner == Pl.Index && S < 5.5f)
	{
		static const FName D[] = { TEXT("dribble_touchR"), TEXT("run_downR"), TEXT("run_passR"), TEXT("dribble_touchL"), TEXT("run_downL"), TEXT("run_passL") };
		return D[F];
	}
	if (bSprint)
	{
		static const FName Sp[] = { TEXT("sprint_contactR"), TEXT("run_downR"), TEXT("sprint_passR"), TEXT("sprint_contactL"), TEXT("run_downL"), TEXT("sprint_passL") };
		return Sp[F];
	}
	static const FName R[] = { TEXT("run_contactR"), TEXT("run_downR"), TEXT("run_passR"), TEXT("run_contactL"), TEXT("run_downL"), TEXT("run_passL") };
	return R[F];
}

UMaterialInstanceDynamic* FPFMatch::MidFor(UMaterialInterface* M, int32 Set, const FPFKit* Kit)
{
	const uint64 Key = (uint64(Set) << 32) | uint64(Kit ? (Kit == &Setup.Home.Kit ? 1 : Kit == &Setup.Away.Kit ? 2 : Kit == &Setup.Home.KeeperKit ? 3 : 4) : 0);
	if (TObjectPtr<UMaterialInstanceDynamic>* F = Mids.Find(Key)) return *F;
	UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(M, GetTransientPackage());
	const FPFSpriteSet& S = Sets[Set];
	D->SetTextureParameterValue(TEXT("Atlas"), S.Atlas);
	D->SetScalarParameterValue(TEXT("KitOn"), (S.bKitKeyed && Kit) ? 1.f : 0.f);
	D->SetVectorParameterValue(TEXT("AtlasCell"), FLinearColor(S.AtlasPx.X, S.AtlasPx.Y, S.Cell, 0));
	if (Kit)
	{
		D->SetVectorParameterValue(TEXT("ShirtA"), Kit->Shirt);
		D->SetVectorParameterValue(TEXT("ShirtB"), Kit->Trim);
		D->SetVectorParameterValue(TEXT("Shorts"), Kit->Shorts);
		D->SetVectorParameterValue(TEXT("Socks"), Kit->Socks);
		D->SetScalarParameterValue(TEXT("Pattern"), Kit->Pattern);
	}
	Mids.Add(Key, D);
	return D;
}

void FPFMatch::Draw(UCanvas* C, UMaterialInterface* SpriteMat)
{
	if (!C) return;
	const float SW = C->ClipX, SH = C->ClipY;
	// ISS-close camera pulled out a touch: ~40 m of pitch across. 6 screen px per stadium pixel and 2 per sprite pixel at 1080p.
	const float SpriteScale = FMath::Max(1.f, FMath::RoundToFloat(SH / 1080.f * 2.f));
	const float Zoom = SpriteScale;
	const FVector2D View(SW / Zoom, SH / Zoom);
	FVector2D Cam = CamCenter;
	Cam.X = FMath::Clamp(Cam.X, View.X * 0.5f, Proj.AtlasSize.X - View.X * 0.5f);
	Cam.Y = FMath::Clamp(Cam.Y, View.Y * 0.5f, Proj.AtlasSize.Y - View.Y * 0.5f);
	FVector2D TL = Cam - View * 0.5f;
	// snap so each stadium pixel lands on whole screen pixels
	TL.X = FMath::RoundToFloat(TL.X * Zoom) / Zoom; TL.Y = FMath::RoundToFloat(TL.Y * Zoom) / Zoom;
	auto ToScreen = [&](FVector2D A) { return FVector2D(FMath::RoundToFloat((A.X - TL.X) * Zoom), FMath::RoundToFloat((A.Y - TL.Y) * Zoom)); };

	if (Stadium)
	{
		FCanvasTileItem T(FVector2D(0, 0), Stadium->GetResource(), FVector2D(SW, SH),
			FVector2D(TL.X / Proj.AtlasSize.X, TL.Y / Proj.AtlasSize.Y), FVector2D((TL.X + View.X) / Proj.AtlasSize.X, (TL.Y + View.Y) / Proj.AtlasSize.Y), FLinearColor::White);
		T.BlendMode = SE_BLEND_Opaque;
		C->DrawItem(T);
	}
	auto DrawTex = [&](UTexture2D* Tex, FVector2D AtlasXY, FVector2D AtlasWH)
	{
		if (!Tex) return;
		const FVector2D P0 = ToScreen(AtlasXY);
		FCanvasTileItem T(P0, Tex->GetResource(), AtlasWH * Zoom, FLinearColor::White);
		T.BlendMode = SE_BLEND_Translucent;
		C->DrawItem(T);
	};
	for (int32 g = 0; g < 2; ++g) { DrawTex(GoalShadow[g].Tex, GoalShadow[g].XY, GoalShadow[g].WH); DrawTex(GoalRear[g].Tex, GoalRear[g].XY, GoalRear[g].WH); }

	// shadows
	auto DrawShadow = [&](FVector2D Ground, float Scale)
	{
		const FVector2D S = ToScreen(Proj.GroundToAtlas(Ground));
		const FVector2D Sz = FVector2D(24, 10) * SpriteScale * Scale;
		FCanvasTileItem T(S - Sz * 0.5f, ShadowTex->GetResource(), Sz, FLinearColor::White);
		T.BlendMode = SE_BLEND_Translucent;
		C->DrawItem(T);
	};
	for (const FPFPlayer& Pl : P) if (!Pl.bSentOff) DrawShadow(Pl.Pos, 1.f);
	for (const FPFPlayer& Of : Officials) DrawShadow(Of.Pos, 1.f);
	for (const FPFPlayer& Mg : Managers) DrawShadow(Mg.Pos, 1.f);
	// sideline crew (static, behind the far touchline)
	for (const FCrew& Cr : Crew)
	{
		if (!Cr.Tex) continue;
		const FVector2D Feet = ToScreen(Proj.GroundToAtlas(Cr.Pos));
		const FVector2D Sz(Cr.Tex->GetSizeX() * SpriteScale, Cr.Tex->GetSizeY() * SpriteScale);
		FCanvasTileItem T(FVector2D(FMath::RoundToFloat(Feet.X - Sz.X * 0.5f), FMath::RoundToFloat(Feet.Y - Sz.Y)), Cr.Tex->GetResource(), Sz, FLinearColor::White);
		T.BlendMode = SE_BLEND_Translucent;
		C->DrawItem(T);
	}
	DrawShadow(V2(Ball.Pos), 0.45f);

	// controlled-player ring (ISS)
	if (Human >= 0 && !Setup.bWatchOnly)
	{
		const FPFPlayer& H = P[Human];
		const FVector2D S = ToScreen(Proj.GroundToAtlas(H.Pos));
		const FVector2D Sz = FVector2D(30, 12) * SpriteScale;
		FCanvasTileItem T(S - Sz * 0.5f, RingTex->GetResource(), Sz, H.Team == 0 ? FLinearColor(0.25f, 0.45f, 1.f) : FLinearColor(1.f, 0.35f, 0.2f));
		T.BlendMode = SE_BLEND_Translucent;
		C->DrawItem(T);
	}

	// sort sprites by ground depth (atlas y of the feet)
	struct FDrawItem { float Depth; int32 Kind; int32 Idx; };
	TArray<FDrawItem> Items;
	for (const FPFPlayer& Pl : P) if (!Pl.bSentOff) Items.Add({ (float)Proj.GroundToAtlas(Pl.Pos).Y, 0, Pl.Index });
	Items.Add({ (float)Proj.GroundToAtlas(V2(Ball.Pos)).Y, 1, 0 });
	for (const FPFPlayer& Of : Officials) Items.Add({ (float)Proj.GroundToAtlas(Of.Pos).Y, 2, Of.Index });
	for (const FPFPlayer& Mg : Managers) Items.Add({ (float)Proj.GroundToAtlas(Mg.Pos).Y, 3, Mg.Index });
	Items.Sort([](const FDrawItem& A, const FDrawItem& B) { return A.Depth < B.Depth; });
	for (const FDrawItem& It : Items)
	{
		if (It.Kind == 1)
		{
			if (!BallTex) continue;
			const FVector2D S = ToScreen(Proj.ToAtlas(Ball.Pos));
			const int32 Frame = int32(Ball.Roll / 0.35f) % 5;
			const float Px = 16.f * SpriteScale * 0.75f;
			FCanvasTileItem T(FVector2D(FMath::RoundToFloat(S.X - Px * 0.5f), FMath::RoundToFloat(S.Y - Px * 0.5f)), BallTex->GetResource(), FVector2D(Px, Px),
				FVector2D(Frame / 5.f, 0), FVector2D((Frame + 1) / 5.f, 1), FLinearColor::White);
			T.BlendMode = SE_BLEND_Masked;
			C->DrawItem(T);
			continue;
		}
		const FPFPlayer& Pl = It.Kind == 2 ? Officials[It.Idx] : It.Kind == 3 ? Managers[It.Idx] : P[It.Idx];
		if (!Sets.IsValidIndex(Pl.SpriteSet) || !SpriteMat) continue;
		const FPFSpriteSet& Set = Sets[Pl.SpriteSet];
		const FPFKit* Kit = It.Kind >= 2 ? nullptr : Pl.bKeeper ? (Pl.Team == 0 ? &Setup.Home.KeeperKit : &Setup.Away.KeeperKit) : (Pl.Team == 0 ? &Setup.Home.Kit : &Setup.Away.Kit);
		if (It.Kind == 2 && !Set.Id.StartsWith(TEXT("R"))) continue;   // officials' sprites not built yet
		if (It.Kind == 3 && !Set.Id.StartsWith(TEXT("C"))) continue;   // managers' sprites not built yet
		FName Pose = PoseFor(Pl);
		if (!Set.Has(Pose))
		{
			const FString PS = Pose.ToString();
			Pose = (PS.StartsWith(TEXT("run")) || PS.StartsWith(TEXT("sprint")) || PS.StartsWith(TEXT("dribble"))) ? (Set.Has(TEXT("run_contactR")) ? FName(TEXT("run_contactR")) : FName(TEXT("jog_contactR"))) : FName(TEXT("idle"));
			if (PS.EndsWith(TEXT("L")) && Set.Has(TEXT("run_contactL"))) Pose = TEXT("run_contactL");
		}
		const FPFFrame& F = Set.Get(ScreenDir(Pl.Facing), Pose);
		if (!F.bValid) continue;
		const FVector2D Feet = ToScreen(Proj.GroundToAtlas(Pl.Pos));
		const FVector2D Size(Set.Cell * SpriteScale, Set.Cell * SpriteScale);
		const FVector2D Pos(Feet.X - Set.Pivot.X * SpriteScale, Feet.Y - Set.Pivot.Y * SpriteScale);
		UMaterialInstanceDynamic* Mid = MidFor(SpriteMat, Pl.SpriteSet, Kit);
		FVector2D UV0 = F.UV0, UV1 = F.UV1;
		if (F.bFlip) Swap(UV0.X, UV1.X);
		FCanvasTileItem T(Pos, Mid->GetRenderProxy(), Size, UV0, UV1);
		C->DrawItem(T);
	}
	for (int32 g = 0; g < 2; ++g) DrawTex(GoalFront[g].Tex, GoalFront[g].XY, GoalFront[g].WH);
}
