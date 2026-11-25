#include "PFMatch.h"
#include "UnrealClient.h"
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

float FPFMatch::CamZoom = 6.24f;   // NEAR: 2 * 64 / (22.533 * 0.5 * 1.82), so a sprite pixel is exactly 2x2 screen pixels at 1080p (ISS framing)
float FPFMatch::HeightScale = 0.5f;
float FPFMatch::GameSpeed = 0.85f;
int64 FPFMatch::Diag[48] = {};

FPFAbility PFAbility(const FPFStats& S, bool bKeeper)
{
	// Wide, non-linear spreads on purpose (docs/DESIGN.md, "Talent you can see").
	FPFAbility A;
	const float Pace = T01(S.Pace), Acc = T01(S.Accel), Agi = T01(S.Agility), Ctl = T01(S.Control), Pas = T01(S.Passing),
		Sho = T01(S.Shooting), Tac = T01(S.Tackling), Rea = T01(S.Reactions), Hea = T01(S.Heading), Kee = T01(S.Keeping);
	A.MaxSpeed = 3.4f + 5.9f * FMath::Pow(Pace, 1.35f);          // 3.4 m/s (plodder) .. 9.3 m/s (flyer)
	A.SprintMul = 1.12f + 0.16f * Pace;
	A.Accel = 11.f + 30.f * FMath::Pow(Acc, 1.5f);               // 0 -> 7 m/s in ~0.6 s (sluggish) .. ~0.17 s (explosive)
	A.TurnRate = 1100.f + 1700.f * FMath::Pow(Agi, 1.3f);        // deg/s: a full about-turn in 0.16 s .. 0.06 s (arcade)
	A.TurnKeep = 0.35f + 0.5f * FMath::Pow(Agi, 1.1f);           // speed kept through a sharp cut (AI; the pad's player keeps more, see MovePlayers)
	A.TouchDist = 2.9f - 2.35f * FMath::Pow(Ctl, 0.8f);          // 2.9 m heavy touch .. 0.55 m glued
	A.FirstTouchErr = 0.95f * FMath::Pow(1.f - Ctl, 1.4f);
	A.PassErrDeg = 15.f - 14.f * FMath::Pow(Pas, 0.9f);
	A.PassPower = 0.85f + 0.3f * Pas;
	A.ShotErrDeg = 19.f - 15.f * FMath::Pow(Sho, 0.9f);
	A.ShotPower = 17.f + 16.f * Sho;
	A.ReactSec = 0.65f - 0.55f * Rea;
	A.AnimRate = 0.95f + 0.4f * (Pace * 0.5f + Agi * 0.5f);
	A.TackleReach = 0.9f + 0.9f * Tac;
	A.TackleWin = 0.15f + 0.75f * Tac;
	A.HeadReach = 1.6f + 1.2f * Hea;
	A.KeeperReach = bKeeper ? 1.9f + 2.1f * FMath::Pow(Kee, 1.1f) : 0.8f;   // v0.7: a touch less (keepers no longer lose caught balls to attackers)
	A.KeeperReact = bKeeper ? 0.28f - 0.22f * Kee : 0.6f;
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

// ------------------------------------------------------------------------------------------------ laws of the game
static bool PFAB(int32 N) { static int32 V = -1; if (V < 0) { V = 0; FParse::Value(FCommandLine::Get(), TEXT("pfab="), V); } return V == N; }   // A/B test switches (simtest only)

bool FPFMatch::InOwnBox(int32 Team, FVector2D Pos) const
{
	if (PFAB(2)) return true;
	// penalty area: 16.5 m from the goal line, 40.32 m wide
	const float OwnX = -AttackDir(Team) * HalfLen;
	return FMath::Abs(Pos.X - OwnX) < 16.5f && FMath::Abs(Pos.X) <= HalfLen + 0.5f && FMath::Abs(Pos.Y) < 20.16f;
}

bool FPFMatch::CanHandle(const FPFPlayer& K) const
{
	if (!K.bKeeper || !InOwnBox(K.Team, K.Pos) || !InOwnBox(K.Team, V2(Ball.Pos))) return false;
	if (PFAB(4)) return true;
	// back-pass (Law 12): a ball deliberately kicked to him by a team-mate, or thrown in by one, is for his feet.
	// Headers, chests, deflections and blocks (LastKind Header / 17 / 18) are fine. His own release can't be re-handled.
	if (Ball.LastTeam == K.Team)
	{
		const EPFKick Kd = EPFKick(FMath::Clamp(Ball.LastKind, 0, 15));
		const bool bFootOrThrow = Ball.LastKind < 16 && (Kd == EPFKick::Pass || Kd == EPFKick::Lob || Kd == EPFKick::Through || Kd == EPFKick::Clear
			|| Kd == EPFKick::Shot || Kd == EPFKick::Volley || Kd == EPFKick::Throw || Kd == EPFKick::Punt);
		if (bFootOrThrow) return false;
	}
	return true;
}

void FPFMatch::GiveKeeperHands(FPFPlayer& K)
{
	Ball.Owner = K.Index; Ball.Vel = FVector::ZeroVector; Ball.LastTouch = K.Index; Ball.LastTeam = K.Team;
	bKeeperHands = true; HoldT = 0; Diag[34]++;
	K.AIThink = Rng.Range(1.4f, 2.6f);    // holds it a moment while his team spreads (never near the 8 s limit)
}

float FPFMatch::SecondLastX(int32 DefTeam, int32 AttackD) const
{
	// the two defenders nearest their own goal line (keeper included); the second of them sets the offside line
	float A = -1e9f, B = -1e9f;
	for (const FPFPlayer& O : P)
	{
		if (O.Team != DefTeam || O.bSentOff) continue;
		const float X = O.Pos.X * AttackD;
		if (X > A) { B = A; A = X; } else if (X > B) B = X;
	}
	return B;
}

void FPFMatch::MarkOffside(const FPFPlayer& Kicker)
{
	// Law 11: judged when a team-mate plays the ball. Offside position = in the opponents' half, nearer their goal
	// line than both the ball and the second-last opponent (level is onside). Being there is no offence on its own:
	// it becomes one when he plays the ball before an opponent does.
	OffsideMask = 0; OffsideTeam = Kicker.Team; Diag[36]++;
	if (bNoOffsideKick) { bNoOffsideKick = false; return; }
	const int32 D = AttackDir(Kicker.Team);
	const float Line = FMath::Max(SecondLastX(1 - Kicker.Team, D), Ball.Pos.X * D);
	for (const FPFPlayer& Q : P)
		if (Q.Team == Kicker.Team && Q.Index != Kicker.Index && !Q.bSentOff && !Q.bKeeper && Q.Pos.X * D > 0.f && Q.Pos.X * D > Line + 0.25f && Q.Index < 32)
			{ OffsideMask |= 1u << Q.Index; Diag[35]++; }
}

void FPFMatch::IndirectFreeKick(int32 Team, FVector2D Spot, const TCHAR* Msg)
{
	Sfx.Add(TEXT("whistle_short"));
	Restart = EPFRestart::FreeKick; RestartTeam = Team; bIndirectFK = true;
	RestartSpot = FVector2D(FMath::Clamp(Spot.X, -HalfLen + 2.f, HalfLen - 2.f), FMath::Clamp(Spot.Y, -HalfWid + 1.f, HalfWid - 1.f));
	// an indirect free kick inside the goal area is taken from its line (Law 13)
	if (FMath::Abs(RestartSpot.X) > HalfLen - 5.5f && FMath::Abs(RestartSpot.Y) < 9.16f) RestartSpot.X = FMath::Sign(RestartSpot.X) * (HalfLen - 5.5f);
	SetBanner(Msg, 1.8f);
	PlaceForRestart();
}

void FPFMatch::Offside(FPFPlayer& Pl)
{
	Diag[32]++;
	// the assistant on that half raises his flag
	const int32 L = Pl.Pos.X < 0 ? 1 : 2;
	if (Officials.IsValidIndex(L)) { Officials[L].Act = EPFAct::Clip; Officials[L].Clip = TEXT("lino_flag"); Officials[L].ActT = 0; Officials[L].ActLen = 1.6f; }
	OffsideMask = 0;
	IndirectFreeKick(1 - Pl.Team, Pl.Pos, TEXT("OFFSIDE"));
}

void FPFMatch::EnforceRestartDistances()
{
	if (PFAB(5)) return;
	// Law 13 / 15 / 16 / 17: opponents 9.15 m from a free kick or corner, 2 m from a throw-in, out of the penalty
	// area at a goal kick; at a penalty everyone but the taker and the keeper is outside the area and the arc.
	if (Phase != EPFPhase::Restart) return;
	const float Min = Restart == EPFRestart::ThrowIn ? 2.f : (Restart == EPFRestart::FreeKick || Restart == EPFRestart::Corner) ? 9.15f : 0.f;
	for (FPFPlayer& Pl : P)
	{
		if (Pl.Index == RestartTaker || Pl.bSentOff) continue;
		if (Restart == EPFRestart::Penalty)
		{
			const int32 D = FMath::Sign(RestartSpot.X);
			if (Pl.bKeeper && Pl.Team != RestartTeam) continue;
			if (Pl.Pos.X * D > HalfLen - 20.3f) Pl.Pos.X = D * (HalfLen - 20.3f);
			continue;
		}
		if (Pl.Team == RestartTeam) continue;
		if (Restart == EPFRestart::GoalKick)
		{
			if (InOwnBox(RestartTeam, Pl.Pos)) Pl.Pos.X = -AttackDir(RestartTeam) * (HalfLen - 16.8f);
			continue;
		}
		if (Min > 0)
		{
			const FVector2D Off = Pl.Pos - RestartSpot;
			const float Dd = Off.Size();
			if (Dd < Min) Pl.Pos = RestartSpot + SafeN(Off, SafeN(GoalCentre(RestartTeam, false) - RestartSpot)) * Min;
		}
	}
}

int32 FPFMatch::ClubSpriteFor(const FString& Id)
{
	if (const int32* I = SetIndex.Find(Id)) return *I;
	FPFSpriteSet S;
	if (!S.Load(Id)) { SetIndex.Add(Id, -1); return -1; }
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
			else if (r <= 1.f) C = FColor(0, 0, 0, 165);
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
	BallTex = PF::LoadTexture(TEXT("GameData/Ball/ball_sheet.png"), true, true);
	ShadowTex = MakeEllipse(26, 9, false);
	RingTex = MakeEllipse(30, 12, true);
	CursorTex = PF::LoadTexture(TEXT("GameData/UI/ico_cursor_south.png"));

	// HD pitch (the plate's own grass is 1280x720 art upscaled; goals are drawn in code at the same resolution)
	if (UMaterialInterface* PM = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/PF/M_PFPitch.M_PFPitch")))
		PitchMid = UMaterialInstanceDynamic::Create(PM, GetTransientPackage());
	if (UMaterialInterface* SM = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/PF/M_PFStands.M_PFStands")))
		StandsMid = UMaterialInstanceDynamic::Create(SM, GetTransientPackage());
	if (TSharedPtr<FJsonObject> PJ = PF::LoadJson(TEXT("GameData/Pitch/pitch.json")))
	{
		PitchX0 = PJ->GetNumberField(TEXT("x0")); PitchY0 = PJ->GetNumberField(TEXT("y0"));
		const TSharedPtr<FJsonObject>* Tn;
		const TArray<TSharedPtr<FJsonValue>>* Tv;
		if (PJ->TryGetObjectField(TEXT("tint"), Tn) && (*Tn)->TryGetArrayField(Setup.Stage, Tv) && Tv->Num() == 3)
			PitchTint = FLinearColor((*Tv)[0]->AsNumber(), (*Tv)[1]->AsNumber(), (*Tv)[2]->AsNumber(), 1.f);
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
			// v0.7: the club's own kit art, animated (one body per kit); the shared template bodies stand in until it's built
			if (!TS.ClubSlug.IsEmpty())
			{
				const FString K = Pl.bKeeper ? (TS.bKeeper2 ? TEXT("keeper2") : TEXT("keeper1")) : (TS.bAwayKit ? TEXT("away") : TEXT("home"));
				const int32 CS = ClubSpriteFor(FString::Printf(TEXT("Clubs/%s_%s"), *TS.ClubSlug, *K));
				if (CS >= 0) Pl.SpriteSet = CS;
			}
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
		Of.Pos = o == 0 ? FVector2D(-8, -6) : FVector2D(0, o == 1 ? -(HalfWid + 0.9f) : HalfWid - 0.35f);
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
		Mg.Pos = FVector2D(t == 0 ? -9.f : 9.f, -(HalfWid + 1.2f));   // technical areas on the near track (the far touchline backs onto the stand)
		Mg.Home = Mg.Pos;
		Mg.Facing = FVector2D(0, 1);
		Managers.Add(Mg);
	}
	Crew.Reset();
	{
		static const TCHAR* Cam[] = { TEXT("sid_camera_operator_001"), TEXT("sid_camera_operator_004"), TEXT("sid_camera_operator_007") };
		static const TCHAR* Boy[] = { TEXT("sid_ball_boy_002"), TEXT("sid_ball_boy_005"), TEXT("sid_ball_boy_008"), TEXT("sid_ball_boy_003") };
		const FVector2D CamPos[] = { FVector2D(-HalfLen - 3.4f, GoalHalf + 4.f), FVector2D(24.f, -(HalfWid + 1.4f)), FVector2D(HalfLen + 3.4f, -GoalHalf - 4.f) };   // behind each goal, one on the near track
		const FVector2D BoyPos[] = { FVector2D(-HalfLen - 3.f, HalfWid - 4.f), FVector2D(HalfLen + 3.f, HalfWid - 4.f), FVector2D(-HalfLen - 3.f, -HalfWid + 6.f), FVector2D(HalfLen + 3.f, -HalfWid + 6.f) };
		for (int32 i = 0; i < 3; ++i) Crew.Add({ PF::LoadTexture(FString::Printf(TEXT("GameData/UI/%s.png"), Cam[i]), true, true), CamPos[i] });
		for (int32 i = 0; i < 4; ++i) Crew.Add({ PF::LoadTexture(FString::Printf(TEXT("GameData/UI/%s.png"), Boy[i]), true, true), BoyPos[i] });
	}
	Score[0] = Score[1] = 0;
	Half = 1; HalfT = 0; MatchClock = 0;
	KickoffTeam = Rng.Int(2);
	FirstKickoffTeam = KickoffTeam;
	PlaceForKickoff(KickoffTeam);
	CamCenter = Proj.GroundToAtlas(FVector2D::ZeroVector);
}

void FPFMatch::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Stadium);
	Collector.AddReferencedObject(BallTex);
	Collector.AddReferencedObject(ShadowTex);
	Collector.AddReferencedObject(RingTex);
	Collector.AddReferencedObject(CursorTex);
	Collector.AddReferencedObject(PitchMid);
	Collector.AddReferencedObject(StandsMid);
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
		if (Pl.bSentOff) { Pl.Pos = FVector2D(Pl.Pos.X, -HalfWid - 6.f); Pl.Vel = FVector2D::ZeroVector; Pl.Act = EPFAct::None; continue; }
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
		if (Pl.Team == KickTeam && !Pl.bKeeper && !Pl.bSentOff)
		{
			if (Taker < 0 || Pl.Setup.Slot.X > P[Taker].Setup.Slot.X) { Mate = Taker; Taker = Pl.Index; }
			else if (Mate < 0 || Pl.Setup.Slot.X > P[Mate].Setup.Slot.X) Mate = Pl.Index;
		}
	if (Taker >= 0) { P[Taker].Pos = FVector2D(-0.6f * AttackDir(KickTeam), 0.f); Ball.Owner = -1; }
	if (Mate >= 0) P[Mate].Pos = FVector2D(-1.f * AttackDir(KickTeam), 6.f);
	Restart = EPFRestart::Kickoff; RestartTeam = KickTeam; RestartTaker = Taker;
	OffsideMask = 0; bKeeperHands = false; HoldT = 0; bIndirectFK = bIndirectLive = false; Wall.Reset();
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
		for (FPFPlayer& Pl : P) if (Pl.Team == RestartTeam && Pl.bKeeper && !Pl.bSentOff) Taker = Pl.Index;
	}
	if (Taker < 0)
	{
		float Best = 1e9;
		for (FPFPlayer& Pl : P)
			if (Pl.Team == RestartTeam && !Pl.bKeeper && !Pl.bSentOff)
			{
				const float Dd = FVector2D::DistSquared(Pl.Pos, RestartSpot);
				if (Dd < Best) { Best = Dd; Taker = Pl.Index; }
			}
	}
	for (FPFPlayer& Pl : P)
	{
		Pl.Act = EPFAct::None; Pl.PendingKick = EPFKick::None; Pl.bCharging = false;
		if (Restart == EPFRestart::Corner && !Pl.bKeeper && !Pl.bSentOff)
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
	OffsideMask = 0; bKeeperHands = false; HoldT = 0;
	if (Restart != EPFRestart::FreeKick) bIndirectFK = false;
	// a wall for a direct free kick within shooting range: up to 4 defenders 9.15 m out, on the line to goal
	Wall.Reset();
	if (Restart == EPFRestart::FreeKick)
	{
		const FVector2D Goal = GoalCentre(RestartTeam, true);
		const float Dg = FVector2D::Distance(RestartSpot, Goal);
		if (Dg < 32.f)
		{
			const int32 N = Dg < 20.f ? 4 : Dg < 26.f ? 3 : 2;
			const FVector2D Dir = SafeN(Goal - RestartSpot), Lat(-Dir.Y, Dir.X);
			TArray<int32> Cand;
			for (const FPFPlayer& Pl : P) if (Pl.Team != RestartTeam && !Pl.bKeeper && !Pl.bSentOff) Cand.Add(Pl.Index);
			Cand.Sort([&](int32 A, int32 B) { return FVector2D::DistSquared(P[A].Pos, RestartSpot) < FVector2D::DistSquared(P[B].Pos, RestartSpot); });
			for (int32 i = 0; i < N && i < Cand.Num(); ++i)
			{
				FPFPlayer& W = P[Cand[i]];
				W.Pos = RestartSpot + Dir * 9.2f + Lat * ((i - (N - 1) * 0.5f) * 0.62f);
				W.Facing = -Dir; W.Vel = FVector2D::ZeroVector; W.Wish = FVector2D::ZeroVector;
				Wall.Add(W.Index);
			}
		}
	}
	EnforceRestartDistances();
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
		if (Pl.Team != Team || Pl.bSentOff || (bOutfieldOnly && Pl.bKeeper)) continue;
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
	Accum += Dt * GameSpeed;
	// above 120 fps many frames run no step: a press on such a frame used to be dropped (v0.5 "passing is unresponsive")
	PendPad.bPass |= Pad.bPass; PendPad.bLob |= Pad.bLob; PendPad.bThrough |= Pad.bThrough; PendPad.bSwitch |= Pad.bSwitch;
	PendPad.bShootPressed |= Pad.bShootPressed; PendPad.bShootReleased |= Pad.bShootReleased;
	FPFPadInput In = Pad;
	In.bPass = PendPad.bPass; In.bLob = PendPad.bLob; In.bThrough = PendPad.bThrough; In.bSwitch = PendPad.bSwitch;
	In.bShootPressed = PendPad.bShootPressed; In.bShootReleased = PendPad.bShootReleased;
	while (Accum >= 1.f / 120.f)
	{
		Step(1.f / 120.f, In);
		In.bPass = In.bLob = In.bThrough = In.bSwitch = In.bShootPressed = In.bShootReleased = false;
		PendPad = FPFPadInput();
		Accum -= 1.f / 120.f;
	}
	// test hook: -pfdiveshots=N screenshots N keeper dives in mid-flight (checks the flat ISS dive)
	{
		static int32 DiveShots = -1; static float Cool = 0;
		if (DiveShots < 0) { DiveShots = 0; FParse::Value(FCommandLine::Get(), TEXT("pfdiveshots="), DiveShots); }
		Cool -= Dt;
		if (DiveShots > 0 && Cool <= 0)
			for (const FPFPlayer& K : P)
				if (K.bKeeper && K.Act == EPFAct::Dive && !K.DiveDir.IsNearlyZero() && K.ActT > 0.16f && K.ActT < 0.3f)
				{
					FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("dive_%02d.png"), DiveShots), false, false);
					--DiveShots; Cool = 2.f; break;
				}
	}
	// camera: follow the ball with a little lead, in atlas pixels
	FVector2D Target = Proj.ToAtlas(FVector(Ball.Pos.X + Ball.Vel.X * 0.25f, Ball.Pos.Y + Ball.Vel.Y * 0.25f, 0));
	{
		// test hook: -pfcamx=X -pfcamy=Y holds the camera on a ground point (screenshots of the stands)
		static float CX = 1e9f, CY = 1e9f; static bool bRead = false;
		if (!bRead) { bRead = true; FParse::Value(FCommandLine::Get(), TEXT("pfcamx="), CX); FParse::Value(FCommandLine::Get(), TEXT("pfcamy="), CY); }
		if (CX < 1e8f && CY < 1e8f) { Target = Proj.ToAtlas(FVector(CX, CY, 0)); CamCenter = Target; }
	}
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
		if (PhaseT > 3.f) { Half = 2; HalfT = 0; PlaceForKickoff(1 - FirstKickoffTeam); }
		return;
	case EPFPhase::FullTime:
		bFinished = PhaseT > 3.f;
		return;
	case EPFPhase::Restart:
	{
		// players drift to shape while the taker waits
		for (FPFPlayer& Pl : P) if (Pl.Index != RestartTaker && Pl.Index != Human && !Pl.bSentOff && !Wall.Contains(Pl.Index)) { const FVector2D S = FormationSpot(Pl); Pl.Wish = SafeN(S - Pl.Pos, FVector2D::ZeroVector) * FMath::Clamp(FVector2D::Distance(S, Pl.Pos) / 3.f, 0.f, 0.6f); }
		MovePlayers(Dt);
		EnforceRestartDistances();
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
				Wall.Reset();
				bNoOffsideKick = Restart == EPFRestart::ThrowIn || Restart == EPFRestart::Corner || Restart == EPFRestart::GoalKick;
				if (Restart == EPFRestart::FreeKick && bIndirectFK) { bIndirectLive = true; IndirectTaker = T.Index; }
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
				else if (Restart == EPFRestart::FreeKick && !bIndirectFK && FVector2D::Distance(T.Pos, GoalCentre(T.Team, true)) < 30.f)
				{
					StartClip(T, TEXT("shot"), EPFKick::Shot);
					T.KickAim = GoalCentre(T.Team, true) + FVector2D(0, Rng.Range(-GoalHalf + 0.5f, GoalHalf - 0.5f)); T.KickPower = 0.8f; T.KickTarget = -1;
				}
				else if (Restart == EPFRestart::GoalKick)
				{
					StartClip(T, TEXT("lob"), EPFKick::Clear);    // a goal kick is kicked off the ground, never punted from the hands
					T.KickAim = FVector2D(D * Rng.Range(0.f, 20.f), Rng.Range(-20.f, 20.f)); T.KickPower = 1.f; T.KickTarget = -1;
				}
				else
				{
					int32 R = BestPassTarget(T, T.Facing, false);
					StartClip(T, TEXT("pass"), EPFKick::Pass); T.KickTarget = R; T.KickAim = R >= 0 ? P[R].Pos : T.Pos + T.Facing * 15.f; T.KickPower = 0.6f;
				}
			}
		}
		else { Phase = EPFPhase::Play; PhaseT = 0; }
		return;
	}
	default: break;
	}

	// ---- open play
	HalfT += Dt / GameSpeed;      // halves keep their real-time length
	MatchClock = (Half - 1) * 45.f + FMath::Min(45.f, HalfT / Setup.HalfSeconds * 45.f);
	if (HalfT >= Setup.HalfSeconds && (Ball.Owner < 0 || FMath::Abs(Ball.Pos.X) < 30.f || HalfT > Setup.HalfSeconds + 8.f))
	{
		if (Half == 1) { Phase = EPFPhase::HalfTime; PhaseT = 0; SetBanner(TEXT("HALF TIME"), 3.f); Sfx.Add(TEXT("whistle_long")); return; }
		Phase = EPFPhase::FullTime; PhaseT = 0; SetBanner(TEXT("FULL TIME"), 3.f); Sfx.Add(TEXT("whistle_three")); return;
	}

	for (FPFPlayer& Pl : P) { Pl.NoTouch = FMath::Max(0.f, Pl.NoTouch - Dt); Pl.Wish = FVector2D::ZeroVector; Pl.bSprint = false; }
	// hands: only while a keeper owns it; the 8-second limit gives the opponents a corner (Law 12, 2025/26)
	if (Ball.Owner < 0 || !P[Ball.Owner].bKeeper) { bKeeperHands = false; HoldT = 0; }
	else if (bKeeperHands)
	{
		HoldT += Dt;
		if (HoldT > 8.f)
		{
			FPFPlayer& K = P[Ball.Owner];
			Diag[33]++;
			Restart = EPFRestart::Corner; RestartTeam = 1 - K.Team; Corners[RestartTeam]++;
			RestartSpot = FVector2D(-AttackDir(K.Team) * (HalfLen - 0.3f), (K.Pos.Y >= 0 ? 1.f : -1.f) * (HalfWid - 0.3f));
			SetBanner(TEXT("8 SECONDS - CORNER"), 2.f); Sfx.Add(TEXT("whistle_short"));
			PlaceForRestart();
			return;
		}
	}
	// offside positions lapse once an opponent has the ball
	if (Ball.Owner >= 0 && P[Ball.Owner].Team != OffsideTeam) OffsideMask = 0;
	if (bIndirectLive && Ball.LastTouch != IndirectTaker) bIndirectLive = false;
	Highlight = FMath::Max(0.f, Highlight - Dt);
	if (Ball.Owner >= 0 && FVector2D::Distance(P[Ball.Owner].Pos, GoalCentre(P[Ball.Owner].Team, true)) < 26.f) Highlight = FMath::Max(Highlight, 1.2f);
	TeamAI(0, Dt);
	TeamAI(1, Dt);
	if (Human >= 0 && !Setup.bWatchOnly) HumanControl(Dt, Pad);
	if (Ball.Owner >= 0) PossT[P[Ball.Owner].Team] += Dt; else if (Ball.LastTeam >= 0 && Ball.LastTeam < 2) PossT[Ball.LastTeam] += Dt;
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
			Target = FVector2D(X, Side < 0 ? -(HalfWid + 0.9f) : HalfWid - 0.35f);   // far side: no track, the stand starts at the line
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
	FVector2D Snapped = Stick;
	if (!Stick.IsZero())
	{
		const float Ang = FMath::RoundToFloat(FMath::Atan2(Stick.Y, Stick.X) / (PI / 4.f)) * (PI / 4.f);
		Snapped = FVector2D(FMath::Cos(Ang), FMath::Sin(Ang));
	}
	FVector2D WDir = Stick.IsZero() ? FVector2D::ZeroVector : SafeN(Proj.ScreenDirToWorld(Snapped));
	const float Mag = FMath::Clamp(Stick.Size(), 0.f, 1.f);
	if (H.Act == EPFAct::None || H.Act == EPFAct::Clip) { H.Wish = WDir * Mag; H.bSprint = Pad.bSprint; }
	const FVector2D AimDir = WDir.IsZero() ? H.Facing : WDir;
	const bool bHasBall = Ball.Owner == Human;
	if (Pad.bPass) BufPass = 0.22f;
	if (Pad.bLob) BufLob = 0.22f;
	if (Pad.bThrough) BufThrough = 0.22f;
	if (Pad.bShootPressed) BufShoot = 0.22f;
	ON_SCOPE_EXIT { BufPass = FMath::Max(0.f, BufPass - Dt); BufLob = FMath::Max(0.f, BufLob - Dt); BufThrough = FMath::Max(0.f, BufThrough - Dt); BufShoot = FMath::Max(0.f, BufShoot - Dt); };

	if (bHasBall || (Ball.Owner < 0 && Ball.LastTouch == Human && FVector2D::Distance(H.Pos, V2(Ball.Pos)) < 2.f))
	{
		// a kick in progress finishes; anything else (a turn, a trap) is cancelled by a kick press
		if (H.Act != EPFAct::None && (H.Act != EPFAct::Clip || H.PendingKick != EPFKick::None)) return;
		// the ball leaves the boot on the press (ISS): the kick fires now, the clip just plays out
		auto KickNow = [&](FName Clip, EPFKick Kind, int32 R, FVector2D Aim, float Pow)
		{
			StartClip(H, Clip, Kind); H.KickTarget = R; H.KickAim = Aim; H.KickPower = Pow;
			H.bEventFired = true; FireKick(H);
			BufPass = BufLob = BufThrough = BufShoot = 0;
		};
		if (BufPass > 0) { const int32 R = BestPassTarget(H, AimDir, false); KickNow(TEXT("pass"), EPFKick::Pass, R, R >= 0 ? P[R].Pos : H.Pos + AimDir * 14.f, 0.6f); return; }
		if (BufThrough > 0) { const int32 R = BestPassTarget(H, AimDir, true); KickNow(TEXT("pass"), EPFKick::Through, R, R >= 0 ? P[R].Pos : H.Pos + AimDir * 20.f, 0.75f); return; }
		if (BufLob > 0) { const int32 R = BestPassTarget(H, AimDir, false); KickNow(TEXT("lob"), EPFKick::Lob, R, R >= 0 ? P[R].Pos : H.Pos + AimDir * 28.f, 0.8f); return; }
		if (BufShoot > 0 && !H.bCharging) { H.bCharging = true; H.Charge = 0; BufShoot = 0; }
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
				H.bEventFired = true; FireKick(H);
			}
		}
		return;
	}

	// without the ball
	H.bCharging = false;
	// a pass pressed just before a ball arrives is a first-time pass, not a switch
	const bool bIncoming = Ball.Owner < 0 && Ball.LastTeam == H.Team && Ball.LastTouch != Human && FVector2D::Distance(H.Pos, V2(Ball.Pos)) < 4.f;
	if (Pad.bSwitch || (Pad.bPass && !bIncoming))
	{
		BufPass = BufLob = BufThrough = BufShoot = 0;
		// next-best teammate to the ball (ISS style)
		int32 Best = -1; float BD = 1e9;
		for (const FPFPlayer& Pl : P)
			if (Pl.Team == H.Team && !Pl.bKeeper && !Pl.bSentOff && Pl.Index != Human)
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
			if (Ball.Owner >= 0 && !KeeperHolding() && P[Ball.Owner].Team != H.Team && FVector2D::Distance(H.Pos, V2(Ball.Pos)) < H.A.TackleReach)
			{
				FPFPlayer& O = P[Ball.Owner];
				const float Win = H.A.TackleWin - O.A.TouchDist * 0.08f + (1.f - O.A.FirstTouchErr) * -0.15f;
				if (Rng.Unit() < FMath::Clamp(Win, 0.1f, 0.9f))
				{
					Ball.Owner = Human; Ball.Vel = FVector::ZeroVector; O.NoTouch = 0.5f; Ball.LastTouch = Human; Ball.LastTeam = H.Team; Ball.LastKind = 0;
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
		if (Pl.Team != From.Team || Pl.Index == From.Index || Pl.bSentOff) continue;
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
		// a team-mate in an offside position is not an option (the flag would go up)
		const int32 Ad = AttackDir(From.Team);
		const bool bOff = Pl.Pos.X * Ad > 0.f && Pl.Pos.X * Ad > FMath::Max(SecondLastX(1 - From.Team, Ad), Ball.Pos.X * Ad) + 0.25f;
		const float S = Cos * 30.f - D * 0.35f + FMath::Min(Open, 6.f) * 3.f + (bThrough ? Forward * 0.4f : Forward * 0.1f) - (Pl.bKeeper ? 25.f : 0.f) - (bOff ? 60.f * (0.35f + 0.6f * T01(From.Setup.Stats.Passing)) : 0.f);
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
		if (Pl.Team != Team || Pl.bKeeper || Pl.bSentOff) continue;
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
		float Urgency = 0.7f;
		// a pass is coming to me: go and meet it
		if (Ball.Owner < 0 && Ball.LastTeam == Team && P.IsValidIndex(Ball.LastTouch) && P[Ball.LastTouch].KickTarget == Pl.Index && Ball.Vel.SizeSquared() > 1.f)
		{
			Target = V2(Ball.Pos) + V2(Ball.Vel) * 0.3f; Urgency = 1.f;
		}
		else if (!bInPoss && Pl.Index == Press && KeeperHolding())
		{
			// the keeper has it in his hands: he may not be challenged, so drop off out of his area and take shape
			Target = FormationSpot(Pl);
			if (InOwnBox(1 - Team, Target)) Target.X = AttackDir(Team) * (HalfLen - 18.f);
			Urgency = 0.7f;
		}
		else if (!bInPoss && Pl.Index == Press)
		{
			Target = BallG; Urgency = 1.f; Pl.bSprint = FVector2D::Distance(Pl.Pos, BallG) > 6.f;
			// tackle attempts (reaction-gated so poor players are late)
			Pl.AIThink -= Dt;
			const float DB = FVector2D::Distance(Pl.Pos, V2(Ball.Pos));
			// in reach: go in once his reactions allow (time spent in reach), then a reaction-long recovery after each try
			if (Ball.Owner >= 0 && DB < Pl.A.TackleReach) Pl.ReactWait += Dt; else Pl.ReactWait = 0;
			if (Ball.Owner >= 0) { Diag[12]++; Diag[13] += int64(DB * 100); if (DB < Pl.A.TackleReach) Diag[14]++; Diag[15] += int64(Pl.Vel.Size() * 100); }
			if (Ball.Owner >= 0 && !KeeperHolding() && Pl.AIThink <= 0 && Pl.Act == EPFAct::None && (DB >= Pl.A.TackleReach ? DB < 3.2f : Pl.ReactWait > Pl.A.ReactSec))
			{
				Pl.AIThink = Pl.A.ReactSec * 1.5f + Rng.Range(0.4f, 1.0f);
				const float D = DB;
				if (D < Pl.A.TackleReach)
				{
					FPFPlayer& O = P[Ball.Owner];
					StartClip(Pl, TEXT("tackle")); Diag[3]++;
					if (Rng.Unit() < FMath::Clamp(Pl.A.TackleWin * 0.7f + O.A.TouchDist * 0.08f, 0.08f, 0.85f))
					{
						Ball.Owner = Pl.Index; Ball.Vel = FVector::ZeroVector; O.NoTouch = 0.45f; Ball.LastTouch = Pl.Index; Ball.LastTeam = Team; Ball.LastKind = 0; Diag[4]++;
					}
				}
				else if (D < 3.2f && Rng.Unit() < 0.18f)
				{
					Pl.Act = EPFAct::Slide; Pl.Clip = TEXT("slide"); Pl.ActT = 0; Pl.ActLen = PFClips::Get(Pl.Clip).Length() / Pl.A.AnimRate; Diag[5]++;
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
		else if (!bInPoss && Pl.Setup.Role != EPFRole::FWD && V2(Ball.Pos).X * AttackDir(Team) < 8.f)
		{
			// marking: pick up the nearest attacker in my zone and stay goal-side of him
			const FVector2D OwnGoal = GoalCentre(Team, false);
			int32 Mk = -1; float MB = 14.f * 14.f;
			for (const FPFPlayer& O : P)
				if (O.Team != Team && !O.bSentOff && !O.bKeeper && O.Index != Ball.Owner)
				{
					const float Dd = FVector2D::DistSquared(O.Pos, Target);
					if (Dd < MB) { MB = Dd; Mk = O.Index; }
				}
			if (Mk >= 0) { Target = P[Mk].Pos + SafeN(OwnGoal - P[Mk].Pos) * 1.6f; Urgency = 0.95f; }
		}
		else if (bInPoss && Pl.Setup.Role == EPFRole::FWD)
		{
			// forwards make runs beyond
			Target.X += AttackDir(Team) * 4.f;
			Urgency = 0.75f;
		}
		// runs are timed to stay onside: level with the second-last defender (or the ball) at most, until the pass
		static const bool bNoOnside = FParse::Param(FCommandLine::Get(), TEXT("pfab1"));
		if (!bNoOnside && bInPoss && Pl.Index != Ball.Owner && Urgency < 1.f)
		{
			const int32 Dd = AttackDir(Team);
			const float Line = FMath::Max(SecondLastX(1 - Team, Dd), Ball.Pos.X * Dd);
			const float Gamble = Pl.Setup.Role == EPFRole::FWD ? 1.0f * FMath::Sin(MatchClock * 2.3f + Pl.Index * 1.7f) : 0.f;
			if (Target.X * Dd > Line - 0.4f + Gamble && Target.X * Dd > 0.f) Target.X = FMath::Max(0.f, Line - 0.4f + Gamble) * Dd;
		}
		const FVector2D To = Target - Pl.Pos;
		const float D = To.Size();
		// chasers (presser, pass receiver) stay at full speed right onto the ball; everyone else eases into position
		const bool bChase = Urgency >= 1.f;
		Pl.Wish = (D > 0.6f || (bChase && D > 0.25f)) ? SafeN(To) * (bChase ? 1.f : FMath::Clamp(D / 3.f, 0.f, 1.f) * Urgency) : FVector2D::ZeroVector;
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
	// commit to a line and change it at a human rate (reactions), not instantly every tick
	if (Pl.AIDir.IsNearlyZero()) Pl.AIDir = Pl.Facing;
	const float Rea = T01(Pl.Setup.Stats.Reactions);
	Pl.AIDir = SafeN(FMath::Lerp(Pl.AIDir, Dir, FMath::Min(1.f, Dt * (1.5f + 3.5f * Rea))), Dir);
	Pl.Wish = Pl.AIDir; Pl.bSprint = Near > 3.f && DG > 25.f;
	Pl.AIThink -= Dt;
	if (Pl.Act != EPFAct::None) return;
	if (Pl.AIThink > 0 && DG > 8.5f) return;   // in close he shoots at once, never walks it in
	Pl.AIThink = Pl.A.ReactSec * 0.6f + Rng.Range(0.05f, 0.25f);
	const float Angle = FMath::Abs(FMath::Atan2(FMath::Abs(Pl.Pos.Y), FMath::Abs(Goal.X - Pl.Pos.X)));
	// a decent sight of goal, not a reflex: closer and better shooters pull the trigger sooner
	const float ShootWill = DG < 7.f ? 1.f : DG < 11.f ? 0.45f : DG < 18.f ? 0.22f * (0.6f + T01(Pl.Setup.Stats.Shooting)) : DG < 26.f ? 0.06f * (0.4f + T01(Pl.Setup.Stats.Shooting)) : 0.f;
	// a body in the shooting lane puts most players off (they look for a pass or keep going)
	int32 InLane = 0;
	for (const FPFPlayer& O : P)
		if (O.Team != Pl.Team && !O.bKeeper && !O.bSentOff)
		{
			const FVector2D Rel = O.Pos - Pl.Pos; const FVector2D L = SafeN(Goal - Pl.Pos);
			const float Along = FVector2D::DotProduct(Rel, L);
			if (Along > 0.5f && Along < DG && FMath::Abs(FVector2D::CrossProduct(L, Rel)) < 1.3f) InLane++;
		}
	// bodies in the lane put him off (v0.7): he works an angle or passes; point-blank he shoots anyway
	const float Will = DG < 6.f ? ShootWill : ShootWill * (InLane == 0 ? 1.f : InLane == 1 ? 0.3f : 0.1f);
	if (Angle < 0.85f && Rng.Unit() < Will)
	{
		StartClip(Pl, TEXT("shot"), EPFKick::Shot);
		// he picks the corner away from the keeper (better finishers more often, and tighter to the post)
		float KY = 0; for (const FPFPlayer& K : P) if (K.bKeeper && K.Team != Pl.Team && !K.bSentOff) KY = K.Pos.Y;
		const float Side = Rng.Unit() < 0.45f + 0.4f * T01(Pl.Setup.Stats.Shooting) ? (KY > 0 ? -1.f : 1.f) : (Rng.Unit() < 0.5f ? -1.f : 1.f);
		Pl.KickAim = Goal + FVector2D(0, Side * Rng.Range(1.2f, GoalHalf - 0.45f)); Pl.KickPower = Rng.Range(0.55f, 1.f); Pl.KickTarget = -1;
		return;
	}
	const bool bPressured = Near < 4.5f;
	if (bPressured || (DG > 30.f && Rng.Unit() < 0.4f) || (DG > 18.f && Rng.Unit() < 0.15f))
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
	if (Ball.Owner == K.Index && !bKeeperHands && !PFAB(6))
	{
		// at his feet (outside the area, or a back-pass): play it like an outfielder, quickly and safely
		K.Wish = FVector2D::ZeroVector;
		K.AIThink -= Dt;
		if (K.AIThink <= 0 && K.Act == EPFAct::None)
		{
			const int32 R = BestPassTarget(K, FVector2D(AttackDir(K.Team), 0), false);
			float Near = 99; for (const FPFPlayer& O : P) if (O.Team != K.Team && !O.bSentOff) Near = FMath::Min(Near, (float)FVector2D::Distance(O.Pos, K.Pos));
			if (R >= 0 && Near > 6.f && FVector2D::Distance(P[R].Pos, K.Pos) < 30.f) { StartClip(K, TEXT("pass"), EPFKick::Pass); K.KickTarget = R; K.KickAim = P[R].Pos; K.KickPower = 0.6f; }
			else { StartClip(K, TEXT("lob"), EPFKick::Clear); K.KickAim = FVector2D(AttackDir(K.Team) * Rng.Range(0.f, 20.f), Rng.Range(-22.f, 22.f)); K.KickPower = 1.f; K.KickTarget = -1; }
		}
		return;
	}
	if (Ball.Owner == K.Index)
	{
		// in his hands: hold a moment (opponents may not challenge him), then throw or kick it out of his hands
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
			if (FMath::Abs(Y) < GoalHalf + 1.f && K.ReactWait <= 0 && TimeTo < 1.2f && InOwnBox(K.Team, K.Pos))
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
						StartClip(K, C); K.Act = EPFAct::Dive; Diag[8]++; UE_LOG(LogPF, Verbose, TEXT("PF dive %s at %.1f"), *C.ToString(), MatchClock);
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
	if (Ball.Owner < 0 && Dist < 15.f && FMath::Abs(BallG.Y) < 18.f && Ball.Pos.Z < 1.f)
	{
		float Opp = 1e9;
		for (const FPFPlayer& O : P) if (O.Team != K.Team && !O.bSentOff) Opp = FMath::Min(Opp, (float)FVector2D::Distance(O.Pos, BallG));
		if (FVector2D::Distance(K.Pos, BallG) < Opp * 0.8f) Spot = BallG;
	}
	// an opponent running at goal with the ball: come off the line to narrow the angle, and smother at his feet
	bool bCharge = false;
	if (Ball.Owner >= 0 && P[Ball.Owner].Team != K.Team && Dist < 16.f)
	{
		Spot = Goal + SafeN(BallG - Goal) * FMath::Clamp(Dist - 1.2f, 1.f, Dist < 9.f ? 7.f : 4.f);
		bCharge = Dist < 9.f;
		K.AIThink -= Dt;
		if (FVector2D::Distance(K.Pos, BallG) < 1.4f && K.AIThink <= 0 && K.Act == EPFAct::None)
		{
			K.AIThink = 0.45f;
			const float Kee = T01(K.Setup.Stats.Keeping);
			FPFPlayer& O = P[Ball.Owner];
			StartClip(K, TEXT("gk_catch")); K.Act = EPFAct::Dive; K.DiveDir = SafeN(BallG - K.Pos) * 0.3f;
			if (InOwnBox(K.Team, BallG) && Rng.Unit() < 0.3f + 0.5f * Kee - O.A.TouchDist * 0.04f)
			{
				GiveKeeperHands(K); O.NoTouch = 0.6f;
				SetBanner(TEXT("SAVE!"), 0.8f); Sfx.Add(TEXT("catch")); Diag[2]++;
			}
			return;
		}
	}
	const FVector2D To = Spot - K.Pos;
	K.Wish = To.Size() > 0.3f ? SafeN(To) * (bCharge ? 1.f : FMath::Clamp(To.Size() / 3.f, 0.f, 1.f)) : FVector2D::ZeroVector;
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
		if (Pl.bSentOff) continue;
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
				if (Pl.ActT < Pl.ActLen * 0.55f && Ball.Pos.Z < 0.6f && !KeeperHolding() && FVector2D::Distance(Pl.Pos + Pl.Facing * 0.7f, V2(Ball.Pos)) < 0.95f && Ball.Owner != Pl.Index)
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
						// sliding into a keeper who holds the ball is always a foul (he may not be challenged)
						if (O.bKeeper && KeeperHolding() && Ball.Owner == O.Index && Phase == EPFPhase::Play) { O.Act = EPFAct::Fall; O.Clip = TEXT("fall"); O.ActT = 0; O.ActLen = PFClips::Get(O.Clip).Length(); Foul(Pl, O); return; }
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
					const bool bCatch = InOwnBox(Pl.Team, Body) && Speed < 16.f + 12.f * T01(Pl.Setup.Stats.Keeping) && Rng.Unit() < 0.45f + 0.5f * T01(Pl.Setup.Stats.Keeping);
						if (bCatch) { GiveKeeperHands(Pl); SetBanner(TEXT("SAVE!"), 1.0f); Sfx.Add(TEXT("catch")); Diag[10]++; }
						else
						{
							Ball.LastKind = 18;
							Ball.Vel = FVector(-Ball.Vel.X * 0.35f, Ball.Vel.Y * 0.4f + Pl.DiveDir.Y * 5.f, 3.f);
						SetBanner(TEXT("SAVE!"), 1.0f); Sfx.Add(TEXT("crowd_ooh")); Diag[10]++;
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
		const bool bOnBall = Ball.Owner == Pl.Index;
		if (bOnBall) MaxV *= 0.8f;
		if (bKicking) MaxV *= (Pl.PendingKick == EPFKick::Shot ? 0.55f : 0.8f);
		FVector2D Wish = Pl.Wish;
		if (Wish.SizeSquared() > 1.f) Wish = SafeN(Wish);
		// Arcade movement: the body goes where the stick points straight away (no drifting sideways). Talent shows in how
		// much speed survives a sharp cut, how fast the top speed comes, and how fast the body turns to face the new way.
		float Speed = Pl.Vel.Size();
		const float WantSpeed = Wish.IsNearlyZero() ? 0.f : MaxV * Wish.Size();
		if (!Wish.IsNearlyZero())
		{
			const FVector2D Want = SafeN(Wish);
			if (Speed > 0.05f)
			{
				const float Cos = FVector2D::DotProduct(Pl.Vel / Speed, Want);
				const float K = FMath::Clamp((Cos + 1.f) * 0.5f, 0.f, 1.f);
				const bool bPad = Pl.Index == Human && !Setup.bWatchOnly;   // arcade cuts for the controlled player: most of the speed survives
				const float Keep = bPad ? FMath::Max(Pl.A.TurnKeep, 0.72f + 0.25f * Pl.A.TurnKeep) * (bOnBall ? 0.92f : 1.f) : Pl.A.TurnKeep * (bOnBall ? 0.75f : 1.f);
				if (Cos < 0.92f) Speed *= FMath::Lerp(Keep, 1.f, K * K);
			}
			if (!bKicking)
			{
				const float Ang = FMath::RadiansToDegrees(FMath::Atan2(FVector2D::CrossProduct(Pl.Facing, Want), FVector2D::DotProduct(Pl.Facing, Want)));
				const float MaxTurn = Pl.Index == Human && !Setup.bWatchOnly ? 360.f : Pl.A.TurnRate * Dt;   // the pad's player snaps round
				Pl.Facing = SafeN(Rot(Pl.Facing, FMath::Clamp(Ang, -MaxTurn, MaxTurn)));
			}
			Speed = Speed < WantSpeed ? FMath::Min(WantSpeed, Speed + Pl.A.Accel * Dt) : FMath::Max(WantSpeed, Speed - 32.f * Dt);
			Pl.Vel = (bKicking ? SafeN(Pl.Vel, Want) : Want) * Speed;
		}
		else
		{
			Speed = FMath::Max(0.f, Speed - 30.f * Dt);   // stop in about a quarter of a second
			Pl.Vel = SafeN(Pl.Vel, Pl.Facing) * Speed;
		}
		Pl.Pos += Pl.Vel * Dt;
		Pl.Stride += Speed * Dt;
		// keepers shuffle facing the ball (they never turn their back on play), and face up the pitch with it in their hands
		if (Pl.bKeeper && Pl.Act == EPFAct::None)
			Pl.Facing = Ball.Owner == Pl.Index ? FVector2D(AttackDir(Pl.Team), 0) : SafeN(V2(Ball.Pos) - Pl.Pos, Pl.Facing);
		// run cycle: cadence from a realistic stride length (longer strides when faster), so feet never blur
		if (Speed > 0.35f)
		{
			// metres per two steps, scaled to the half-size bodies: about 0.3 s a cycle at a jog, as measured in ISS Deluxe footage
			const float CycleLen = 0.45f * (Speed < 2.2f ? 1.5f : FMath::Clamp(1.25f + 0.33f * Speed, 2.2f, 4.6f));
			Pl.Gait += Speed / CycleLen * Dt;
		}
		Pl.Pos.X = FMath::Clamp(Pl.Pos.X, -HalfLen - 4.f, HalfLen + 4.f);
		Pl.Pos.Y = FMath::Clamp(Pl.Pos.Y, -HalfWid - 3.f, HalfWid + 3.f);
	}
	// light separation so bodies do not stack
	for (int32 i = 0; i < P.Num(); ++i)
		for (int32 j = i + 1; j < P.Num(); ++j)
		{
			if (P[i].bSentOff || P[j].bSentOff) continue;
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
	MarkOffside(Pl);
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
		Diag[0]++; { const float Dg = FVector2D::Distance(Pl.Pos, GoalCentre(Pl.Team, true)); Diag[1] += int64(Dg * 10); if (Dg < 12.f) Diag[9]++; }
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
		Diag[7]++;
		Err = Pl.A.PassErrDeg * (Pl.bSprint ? 1.4f : 1.f);
		Dir = Rot(Dir, Rng.Gauss() * Err * 0.5f);
		const float Spd = FMath::Clamp((7.f + D * 0.75f) * Pl.A.PassPower, 8.f, 27.f);
		V = FVector(Dir * Spd, 0.4f);
		break;
	}
	}
	Ball.Owner = -1;
	Ball.Vel = V;
	Ball.LastKind = (int32)Type;
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
		if (O.bKeeper && bKeeperHands && !(O.Act == EPFAct::Clip && O.PendingKick != EPFKick::None))
		{
			// held in the gloves, never behind his own line
			FVector2D Hold = O.Pos + O.Facing * 0.3f;
			Hold.X = FMath::Clamp(Hold.X, -HalfLen + 0.35f, HalfLen - 0.35f);
			Ball.Pos = FVector(Hold, 1.0f); Ball.Vel = FVector::ZeroVector; return;
		}
		if (O.Act == EPFAct::Clip && (O.PendingKick == EPFKick::Throw || (O.bKeeper && O.PendingKick == EPFKick::Punt)))
		{
			// held in the hands until the release frame (throw-ins are taken from just outside the line)
			FVector2D Hold = O.Pos + O.Facing * 0.3f;
			Hold.Y = FMath::Clamp(Hold.Y, -HalfWid + 0.25f, HalfWid - 0.25f);
			Hold.X = FMath::Clamp(Hold.X, -HalfLen + 0.35f, HalfLen - 0.35f);
			Ball.Pos = FVector(Hold, O.bKeeper ? 1.0f : 1.9f); Ball.Vel = FVector::ZeroVector;
			return;
		}
		if (Spd < 1.2f)
		{
			// standing: ball settles at the feet
			const FVector2D NP = FMath::Lerp(BP, Foot, FMath::Min(1.f, Dt * 10.f));
			Ball.Vel = FVector(O.Vel, 0); Ball.Pos = FVector(NP, 0.11f);
			O.DribbleDir = O.Facing; O.TouchPhase = 0;
		}
		else
		{
			// Dribbling is scripted, not simulated: the ball is touched on the foot-contact frames of the run cycle
			// (every step for good close control, every other step when sprinting or for heavy-touch players), then
			// rolls out ahead and comes back to the next contact. Control sets how far it runs from the boots.
			const FVector2D Mv = SafeN(O.Vel, O.Facing);
			// a sharp cut at speed can lose a poorly controlled ball (the old direction keeps it)
			const bool bCut = FVector2D::DotProduct(Mv, O.DribbleDir) < -0.1f;
			if (bCut && Spd > 6.f && O.Index != Human && Rng.Unit() < O.A.FirstTouchErr * 0.25f)   // AI only, one roll per cut: the ball snaps round if he keeps it
			{
				Ball.Owner = -1; Ball.Vel = FVector(O.DribbleDir * Spd * 1.1f, 0); O.NoTouch = 0.3f; Ball.LastTouch = O.Index; Ball.LastTeam = O.Team; Diag[6]++;
				return;
			}
			O.DribbleDir = Mv;                                        // the ball swings round with the body at once (arcade)
			const float Steps = O.Gait * 2.f;                         // steps taken
			const float U = FMath::Frac(Steps);                       // a touch every step: the ball never leaves the boots (ISS)
			const float Amp = FMath::Clamp(O.A.TouchDist * 0.08f, 0.04f, 0.16f) * (O.bSprint ? 1.6f : 1.f);
			const float Ahead = 0.24f + Amp * FMath::Sin(U * PI);     // metres; players are drawn at half height, so this reads as at the feet
			const float Side = 0.04f * FMath::Cos(PI * Steps);        // swaps feet smoothly, one step to the next
			const FVector2D Lat(-O.DribbleDir.Y, O.DribbleDir.X);
			const FVector2D NP = O.Pos + O.DribbleDir * Ahead + Lat * Side;
			// the ball's velocity is the carrier's: AI prediction, the camera lead and anyone reading Ball.Vel see a smooth run,
			// not the in-out of the touches
			Ball.Vel = FVector(O.Vel, 0);
			Ball.Pos = FVector(NP, 0.11f);
			// a touch that runs out from the boots can be nicked by an opponent who gets a foot to it
			if (Ahead > 0.27f)
				for (FPFPlayer& D : P)
					if (D.Team != O.Team && !D.bSentOff && !D.bKeeper && D.NoTouch <= 0 && D.Act == EPFAct::None && FVector2D::Distance(D.Pos + D.Facing * 0.3f, NP) < 0.6f && Rng.Unit() < 0.05f)
					{
						Ball.Owner = -1; { const FVector2D Lt(-O.DribbleDir.Y, O.DribbleDir.X); const float Sg = FVector2D::DotProduct(D.Pos - O.Pos, Lt) > 0 ? -1.f : 1.f; Ball.Vel = FVector(SafeN(Lt * Sg - O.DribbleDir * 0.3f) * 3.5f, 0.f); Ball.LastKind = 16; } Ball.LastTouch = D.Index; Ball.LastTeam = D.Team;
						O.NoTouch = 0.4f; Diag[4]++;
						break;
					}
			O.TouchPhase = U;
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
	// shots can be blocked by a body in the way
	if (Ball.Owner < 0 && Ball.LastKind == (int32)EPFKick::Shot && Ball.Pos.Z < 1.4f && FVector2D(V.X, V.Y).SizeSquared() > 100.f)
		for (FPFPlayer& B : P)
		{
			if (B.Team == Ball.LastTeam || B.bKeeper || B.bSentOff || B.NoTouch > 0 || B.Act == EPFAct::Fall) continue;
			if (FVector2D::Distance(B.Pos, V2(Ball.Pos)) < 0.36f)
			{
				const float Sp = FVector2D(V.X, V.Y).Size() * Rng.Range(0.2f, 0.45f);
				const FVector2D Dir = SafeN(Rot(-FVector2D(V.X, V.Y), Rng.Range(-70.f, 70.f)));
				V = FVector(Dir * Sp, Rng.Range(0.5f, 3.f));
				Ball.LastTouch = B.Index; Ball.LastTeam = B.Team; Ball.LastKind = 17; B.NoTouch = 0.25f;
				Sfx.Add(TEXT("ball_bounce")); Diag[29]++;
				break;
			}
		}
	// keepers block shots that come at them (no dive needed)
	if (Ball.Owner < 0 && FVector2D(V.X, V.Y).SizeSquared() > 64.f)
		for (FPFPlayer& K : P)
		{
			if (!K.bKeeper || K.NoTouch > 0 || K.Act == EPFAct::Dive || Ball.LastTeam == K.Team) continue;
			const float Kee = T01(K.Setup.Stats.Keeping);
			if (FVector2D::Distance(K.Pos, V2(Ball.Pos)) < 0.6f + Kee * 0.45f && Ball.Pos.Z < 2.3f)
			{
				// outside his area he can only block it with his body
				const bool bCatch = InOwnBox(K.Team, V2(Ball.Pos)) && Rng.Unit() < 0.3f + 0.6f * Kee;
				if (bCatch) { GiveKeeperHands(K); StartClip(K, Ball.Pos.Z > 1.4f ? TEXT("gk_catch_high") : TEXT("gk_catch")); }
				else { V = FVector(-V.X * 0.3f, V.Y * 0.5f + Rng.Range(-4.f, 4.f), 2.f); Ball.LastKind = 18; }
				Ball.LastTouch = K.Index; Ball.LastTeam = K.Team; K.NoTouch = 0.3f;
				SetBanner(TEXT("SAVE!"), 0.8f); Diag[2]++;
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
			if (Pl.bKeeper || Pl.bSentOff || Pl.NoTouch > 0 || Pl.Act != EPFAct::None) continue;
			if (FVector2D::Distance(Pl.Pos, V2(Ball.Pos)) > 0.5f + Pl.A.HeadReach * 0.25f) continue;
			if (Pl.Team == OffsideTeam && Pl.Index < 32 && (OffsideMask & (1u << Pl.Index))) { Offside(Pl); return; }
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
	// keepers claim high balls in their own area with their hands (crosses, lobs)
	if (Ball.Pos.Z > 1.2f && Ball.Pos.Z < 2.9f && !PFAB(3))
		for (FPFPlayer& K : P)
			if (K.bKeeper && !K.bSentOff && K.NoTouch <= 0 && K.Act != EPFAct::Fall && FVector2D::Distance(K.Pos, V2(Ball.Pos)) < 1.3f && CanHandle(K)
				&& Rng.Unit() < 0.35f + 0.6f * T01(K.Setup.Stats.Keeping))
			{
				GiveKeeperHands(K); StartClip(K, TEXT("gk_catch_high")); Sfx.Add(TEXT("catch")); return;
			}
	if (Ball.Pos.Z > 1.2f) return;
	int32 Best = -1; float BD = 1e9;
	for (FPFPlayer& Pl : P)
	{
		if (Pl.bSentOff || Pl.NoTouch > 0 || Pl.Act == EPFAct::Fall || Pl.Act == EPFAct::Celebrate || Pl.Act == EPFAct::Dive) continue;
		// a struck shot is only stopped by a body right in its path (it isn't trapped from 0.85 m away)
		const bool bShotIn = Ball.LastKind == int32(EPFKick::Shot) && Pl.Team != Ball.LastTeam && Ball.Vel.Size() > 12.f;
		const float Reach = (Pl.bKeeper && CanHandle(Pl)) ? (bShotIn ? 0.6f : 1.1f) : bShotIn ? 0.32f : 0.85f;
		const float D = FVector2D::Distance(Pl.Pos + Pl.Facing * 0.3f, V2(Ball.Pos));
		if (D < Reach && D < BD) { BD = D; Best = Pl.Index; }
	}
	if (Best < 0) return;
	FPFPlayer& Pl = P[Best];
	if (Pl.Team == OffsideTeam && Pl.Index < 32 && (OffsideMask & (1u << Pl.Index))) { Offside(Pl); return; }
	const float BallSpeed = Ball.Vel.Size();
	if (Ball.LastKind == int32(EPFKick::Shot) && !Pl.bKeeper) Diag[Pl.Team == Ball.LastTeam ? 41 : 40]++;
	if (!Pl.bKeeper && Ball.LastKind == int32(EPFKick::Shot) && Pl.Team != Ball.LastTeam && BallSpeed > 12.f)
	{
		// blocked: it cannons off him (Law-free, but it's what a fast shot does to a body)
		const FVector2D In = SafeN(FVector2D(Ball.Vel.X, Ball.Vel.Y));
		Ball.Vel = FVector(SafeN(Rot(-In, Rng.Range(-75.f, 75.f))) * BallSpeed * Rng.Range(0.2f, 0.45f), Rng.Range(0.5f, 3.f));
		Ball.LastTouch = Pl.Index; Ball.LastTeam = Pl.Team; Ball.LastKind = 17; Pl.NoTouch = 0.25f;
		Sfx.Add(TEXT("ball_bounce")); Diag[29]++;
		return;
	}
	if (Pl.bKeeper && CanHandle(Pl))
	{
		// in his own area he gathers it into his arms: a scoop for a ball on the ground, a catch for one in the air
		if (Ball.LastKind == int32(EPFKick::Shot)) Diag[38]++;
		// a fast shot straight at him: held or spilled, by his handling
		if (Ball.LastKind == int32(EPFKick::Shot) && Pl.Team != Ball.LastTeam && BallSpeed > 12.f && Rng.Unit() > 0.5f + 0.4f * T01(Pl.Setup.Stats.Keeping))
		{
			Ball.Vel = FVector(-Ball.Vel.X * 0.3f, Ball.Vel.Y * 0.5f + Rng.Range(-4.f, 4.f), 1.5f); Ball.LastTouch = Pl.Index; Ball.LastTeam = Pl.Team; Ball.LastKind = 18;
			Pl.NoTouch = 0.35f; SetBanner(TEXT("SAVE!"), 0.8f); Sfx.Add(TEXT("crowd_ooh")); Diag[2]++;
			return;
		}
		GiveKeeperHands(Pl); StartClip(Pl, Ball.Pos.Z > 0.5f ? TEXT("gk_catch") : TEXT("gk_pickup"));
		return;
	}
	// first touch: fast balls and poor control = it bounces off
	const float MisP = Pl.A.FirstTouchErr * FMath::Clamp((BallSpeed - 6.f) / 18.f, 0.f, 1.f) + 0.03f;
	if (Rng.Unit() < MisP)
	{
		Ball.Vel = FVector(SafeN(FVector2D(Ball.Vel.X, Ball.Vel.Y)) * BallSpeed * 0.35f + FVector2D(Rng.Gauss(), Rng.Gauss()) * 2.5f, 0.6f);
		Pl.NoTouch = 0.35f; Ball.LastTouch = Pl.Index; Ball.LastTeam = Pl.Team;
		return;
	}
	const int32 PrevTeam = Ball.LastTeam;
	Ball.Owner = Best; Ball.LastTouch = Best; Ball.LastTeam = Pl.Team; Ball.LastKind = 0;
	Ball.Vel *= 0.3f;
	bKeeperHands = false;
	if (Pl.bKeeper) { Pl.AIThink = Rng.Range(0.3f, 0.7f); }   // outside his area (or a back-pass): it's at his feet
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
		const bool bDirectThrow = Ball.LastKind == int32(EPFKick::Throw) && P.IsValidIndex(Ball.LastTouch) && !P[Ball.LastTouch].bKeeper;
		if (FMath::Abs(B.Y) < GoalHalf && B.Z < GoalH && (bDirectThrow || bIndirectLive)) Diag[37]++;
		if (FMath::Abs(B.Y) < GoalHalf && B.Z < GoalH && !bDirectThrow && !bIndirectLive)
		{
			// which team attacks this goal?
			const int32 Scorer = AttackDir(0) == int32(Side) ? 0 : 1;
			Goal(Scorer);
			return;
		}
		const int32 Defending = AttackDir(0) == int32(Side) ? 1 : 0;
		if (Ball.LastKind == int32(EPFKick::Shot)) Diag[39]++;
		if (Ball.LastTeam == Defending)
		{
			Restart = EPFRestart::Corner; RestartTeam = 1 - Defending;
			Corners[RestartTeam]++;
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
	for (const FPFPlayer& K : P) if (K.bKeeper && K.Team != ScoringTeam) { if (FMath::Abs(K.Pos.X) < HalfLen - 3.f || FMath::Abs(K.Pos.Y) > GoalHalf + 1.f) Diag[11]++; }
	if (bOwnGoal) Diag[6] += 1000;
	Diag[16 + FMath::Clamp(Ball.LastKind, 0, 15)]++;
	Diag[26 + (Ball.Owner < 0 ? 0 : P[Ball.Owner].Team == ScoringTeam ? 1 : 2)]++;   // 26 loose, 27 carried in by scorer, 28 carried in by defender
	if (P.IsValidIndex(Ball.LastTouch) && P[Ball.LastTouch].bKeeper) Diag[7] += 100000;
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

void FPFMatch::Foul(FPFPlayer& Off, FPFPlayer& Vic, bool bIndirect)
{
	Fouls[Off.Team]++;
	Sfx.Add(TEXT("whistle_short"));
	// cards (Law 12): reckless = caution, excessive force = sending-off, a second caution = sending-off.
	// Denying an obvious goal-scoring opportunity outside the area (through on goal, nobody but the keeper to beat) = red.
	const float R = Rng.Unit();
	FString Card;
	const FVector2D VGoal = GoalCentre(Vic.Team, true);
	bool bClear = FVector2D::Distance(Vic.Pos, VGoal) < 25.f && !InOwnBox(Off.Team, Vic.Pos);
	if (bClear)
		for (const FPFPlayer& D : P)
			if (D.Team == Off.Team && !D.bKeeper && !D.bSentOff && D.Index != Off.Index && FVector2D::Distance(D.Pos, VGoal) < FVector2D::Distance(Vic.Pos, VGoal)) { bClear = false; break; }
	const bool bBehind = FVector2D::DotProduct(Vic.Facing, Off.Facing) > 0.5f;
	const float Book = bBehind ? 0.55f : 0.25f;
	if (R < 0.03f || bClear || (R < Book && Off.bBooked))
	{
		Off.bSentOff = true; Reds[Off.Team]++; Card = (R < Book && Off.bBooked && !bClear && R >= 0.03f) ? TEXT("SECOND YELLOW - RED CARD") : TEXT("RED CARD");
		Off.Pos = FVector2D(Off.Pos.X, -HalfWid - 6.f); Off.Vel = FVector2D::ZeroVector;
		Log(Off.Team, FString::Printf(TEXT("%d' %s sent off"), FMath::Max(1, FMath::CeilToInt(MatchClock)), *Off.Setup.Name));
		if (Off.Index == Human) { const int32 N = NearestToBall(Off.Team, true); if (N >= 0) SwitchHuman(N); }
	}
	else if (R < Book) { Off.bBooked = true; Yellows[Off.Team]++; Card = TEXT("YELLOW CARD"); }
	CardColor = Card == TEXT("RED CARD") ? FLinearColor(0.85f, 0.05f, 0.05f) : FLinearColor(1.f, 0.85f, 0.05f);
	if (Officials.Num())
	{
		FPFPlayer& Ref = Officials[0];
		Ref.Act = EPFAct::Clip; Ref.Clip = Card.IsEmpty() ? TEXT("ref_whistle") : TEXT("ref_card"); Ref.ActT = 0; Ref.ActLen = Card.IsEmpty() ? 0.8f : 1.9f;
		Ref.Facing = SafeN(Off.Pos - Ref.Pos);
	}
	// where: inside the box = penalty
	const FVector2D Spot = Vic.Pos;
	const int32 D = AttackDir(Vic.Team);
	const bool bInBox = !bIndirect && !Vic.bKeeper && (Spot.X * D > HalfLen - 16.5f) && FMath::Abs(Spot.Y) < 20.15f;
	RestartTeam = Vic.Team;
	bIndirectFK = bIndirect || (Vic.bKeeper && InOwnBox(Vic.Team, Vic.Pos));   // impeding / challenging the keeper
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
			if (Pl.Pos.X * D > HalfLen - 20.3f) Pl.Pos.X = D * (HalfLen - 20.3f);   // outside the area and the arc (9.15 m from the spot)
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
	if (Pl.bKeeper && Ball.Owner == Pl.Index && bKeeperHands) return TEXT("gk_hold_ball");
	const float S = Pl.Vel.Size();
	if (S < 0.35f)
	{
		if (Pl.bKeeper && FVector2D::Distance(Pl.Pos, V2(Ball.Pos)) < 30.f) return TEXT("gk_ready");
		return FMath::Fmod(Pl.Stride * 0.f + (Pl.Index * 0.37f + MatchClock * 60.f), 1.6f) < 0.8f ? FName(TEXT("idle")) : FName(TEXT("idle_breathe"));
	}
	if (Pl.bCharging) return TEXT("shot_plant");
	// distance-driven cycles: the feet stay planted because frames advance with ground covered
	const float Ph = FMath::Frac(Pl.Gait);
	if (S < 2.2f)
	{
		static const FName W[] = { TEXT("walk_contactR"), TEXT("idle"), TEXT("walk_contactL"), TEXT("idle") };
		return W[FMath::Min(3, int32(Ph * 4.f))];
	}
	const bool bSprint = Pl.bSprint && S > Pl.A.MaxSpeed * 0.9f && !Pl.bKeeper;
	const int32 F = FMath::Min(5, int32(Ph * 6.f));
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
	// Camera: CamZoom screen pixels per stadium pixel at 1080p (2.5 = about 35 m of pitch across, close to the owner's
	// kickoff reference framing). Sprites are drawn at true size: the 64 px figure is PlayerHeightM tall in the
	// stadium's own projection (Az = atlas pixels per metre of height), so players and pitch share one scale.
	const float Zoom = FMath::Max(1.f, SH / 1080.f * CamZoom);
	FPFProjection Pv = Proj;                 // drawing projection: heights scaled (HeightScale), ground unchanged
	Pv.Az *= HeightScale;
	float SpriteScale = Zoom * Pv.Az.Size() * PlayerHeightM / 64.f;
	if (FMath::Abs(SpriteScale - FMath::RoundToFloat(SpriteScale)) < 0.03f) SpriteScale = FMath::RoundToFloat(SpriteScale);   // crisp 1:1 / 2:1 when close
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
	// exact (unrounded) world -> screen, for geometry
	auto W2S = [&](float X, float Y, float Z) { const FVector2D A = Pv.ToAtlas(FVector(X, Y, Z)); return FVector2D((A.X - TL.X) * Zoom, (A.Y - TL.Y) * Zoom); };
	TArray<FCanvasUVTri> Tris;
	auto Quad = [&](FVector2D A, FVector2D B, FVector2D Cc, FVector2D Dd, FLinearColor Col)
	{
		FCanvasUVTri T0; T0.V0_Pos = A; T0.V1_Pos = B; T0.V2_Pos = Cc; T0.V0_Color = T0.V1_Color = T0.V2_Color = Col;
		FCanvasUVTri T1; T1.V0_Pos = A; T1.V1_Pos = Cc; T1.V2_Pos = Dd; T1.V0_Color = T1.V1_Color = T1.V2_Color = Col;
		Tris.Add(T0); Tris.Add(T1);
	};
	auto Line = [&](FVector2D A, FVector2D B, float Wd, FLinearColor Col)
	{
		FVector2D N = (B - A).GetSafeNormal(); N = FVector2D(-N.Y, N.X) * Wd * 0.5f;
		Quad(A + N, B + N, B - N, A - N, Col);
	};
	auto Flush = [&]()
	{
		if (!Tris.Num()) return;
		FCanvasTriangleItem TI(Tris, GWhiteTexture);
		TI.BlendMode = SE_BLEND_Translucent;
		C->DrawItem(TI);
		Tris.Reset();
	};
	// HD pitch over the plate's grass (grass runs to the touchlines at the sides, ~4 m past the goal lines at the ends)
	if (StandsMid)
	{
		// home supporters in the home colours, a few in the away side's
		const FVector2D G0 = Proj.AtlasToGround(TL);
		const FVector2D GA = Proj.AtlasToGround(TL + FVector2D(1.f / Zoom, 0)) - G0, GB = Proj.AtlasToGround(TL + FVector2D(0, 1.f / Zoom)) - G0;
		auto Srgb = [](const FLinearColor& L) { const FColor F = L.ToFColorSRGB(); return FLinearColor(F.R / 255.f, F.G / 255.f, F.B / 255.f, 1.f); };
		StandsMid->SetVectorParameterValue(TEXT("P1"), FLinearColor(SW, SH, FMath::Max(1.f, FMath::RoundToFloat(SpriteScale)), 0));
		StandsMid->SetVectorParameterValue(TEXT("G0"), FLinearColor(G0.X, G0.Y, 0, 0));
		StandsMid->SetVectorParameterValue(TEXT("GA"), FLinearColor(GA.X, GA.Y, 0, 0));
		StandsMid->SetVectorParameterValue(TEXT("GB"), FLinearColor(GB.X, GB.Y, 0, 0));
		StandsMid->SetVectorParameterValue(TEXT("Cam"), FLinearColor(FMath::RoundToFloat(TL.X * Zoom), FMath::RoundToFloat(TL.Y * Zoom), 0, 0));
		StandsMid->SetVectorParameterValue(TEXT("Tint"), PitchTint);
		StandsMid->SetVectorParameterValue(TEXT("Seat"), Srgb(Setup.Home.Kit.Shirt));
		StandsMid->SetVectorParameterValue(TEXT("Seat2"), Srgb(Setup.Away.Kit.Shirt));
		FCanvasTileItem T(FVector2D(0, 0), StandsMid->GetRenderProxy(), FVector2D(SW, SH), FVector2D(0, 0), FVector2D(1, 1));
		C->DrawItem(T);
	}
	if (PitchMid)
	{
		const FVector2D G0 = Proj.AtlasToGround(TL);
		const FVector2D GA = Proj.AtlasToGround(TL + FVector2D(1.f / Zoom, 0)) - G0, GB = Proj.AtlasToGround(TL + FVector2D(0, 1.f / Zoom)) - G0;
		PitchMid->SetVectorParameterValue(TEXT("P1"), FLinearColor(SW, SH, FMath::Max(1.f, FMath::RoundToFloat(SpriteScale)), 0));
		PitchMid->SetVectorParameterValue(TEXT("G0"), FLinearColor(G0.X, G0.Y, 0, 0));
		PitchMid->SetVectorParameterValue(TEXT("GA"), FLinearColor(GA.X, GA.Y, 0, 0));
		PitchMid->SetVectorParameterValue(TEXT("GB"), FLinearColor(GB.X, GB.Y, 0, 0));
		PitchMid->SetVectorParameterValue(TEXT("Cam"), FLinearColor(FMath::RoundToFloat(TL.X * Zoom), FMath::RoundToFloat(TL.Y * Zoom), 0, 0));
		PitchMid->SetVectorParameterValue(TEXT("Tint"), PitchTint);
		FCanvasTileItem T(FVector2D(0, 0), PitchMid->GetRenderProxy(), FVector2D(SW, SH), FVector2D(0, 0), FVector2D(1, 1));
		C->DrawItem(T);
	}
	// goals, drawn in code: nets and their shadow behind everyone; posts and crossbar join the depth sort below
	const float NetD = 2.f, BackH = 1.6f;
	const float LineW = FMath::Max(1.f, FMath::RoundToFloat(SpriteScale));   // net strands one art pixel wide
	for (int32 g = 0; g < 2; ++g)
	{
		const float S = g == 0 ? -1.f : 1.f, X0 = S * HalfLen, X1 = S * (HalfLen + NetD);
		const FLinearColor Shade(0, 0, 0, 0.16f), Fill(1, 1, 1, 0.06f), Mesh(1, 1, 1, 0.30f), Frame(0.82f, 0.84f, 0.86f, 0.9f);
		Quad(W2S(X0, -GoalHalf, 0), W2S(X1, -GoalHalf, 0), W2S(X1, GoalHalf, 0), W2S(X0, GoalHalf, 0), Shade);
		// panels: back, roof, two sides
		Quad(W2S(X1, -GoalHalf, 0), W2S(X1, GoalHalf, 0), W2S(X1, GoalHalf, BackH), W2S(X1, -GoalHalf, BackH), Fill);
		Quad(W2S(X0, -GoalHalf, GoalH), W2S(X0, GoalHalf, GoalH), W2S(X1, GoalHalf, BackH), W2S(X1, -GoalHalf, BackH), Fill);
		for (float Y : { -GoalHalf, GoalHalf })
			Quad(W2S(X0, Y, 0), W2S(X0, Y, GoalH), W2S(X1, Y, BackH), W2S(X1, Y, 0), Fill);
		// mesh, ~0.3 m squares
		for (float Y = -GoalHalf; Y <= GoalHalf + 0.01f; Y += GoalHalf / 12.f)
		{
			Line(W2S(X1, Y, 0), W2S(X1, Y, BackH), LineW, Mesh);
			Line(W2S(X0, Y, GoalH), W2S(X1, Y, BackH), LineW, Mesh);
		}
		for (float Z = 0.f; Z <= BackH + 0.01f; Z += BackH / 6.f) Line(W2S(X1, -GoalHalf, Z), W2S(X1, GoalHalf, Z), LineW, Mesh);
		for (int32 k = 1; k <= 6; ++k)
		{
			const float F = k / 7.f, Xr = FMath::Lerp(X0, X1, F), Zr = FMath::Lerp(GoalH, BackH, F);
			Line(W2S(Xr, -GoalHalf, Zr), W2S(Xr, GoalHalf, Zr), LineW, Mesh);
			for (float Y : { -GoalHalf, GoalHalf }) Line(W2S(Xr, Y, 0), W2S(Xr, Y, Zr), LineW, Mesh);
		}
		for (float Y : { -GoalHalf, GoalHalf }) for (float Z = 0.4f; Z < GoalH; Z += 0.4f)
			Line(W2S(X0, Y, Z), W2S(X1, Y, FMath::Min(Z, BackH)), LineW, Mesh);
		// back frame: ground bar and the rear stanchions
		Line(W2S(X1, -GoalHalf, 0), W2S(X1, GoalHalf, 0), LineW * 2.f, Frame);
		for (float Y : { -GoalHalf, GoalHalf }) { Line(W2S(X1, Y, 0), W2S(X1, Y, BackH), LineW * 2.f, Frame); Line(W2S(X0, Y, GoalH), W2S(X1, Y, BackH), LineW * 1.5f, Frame); }
	}
	Flush();
	const float PostW = FMath::Max(2.f, 0.12f * Proj.Ax.Size() * Zoom), BarW = FMath::Max(2.f, 0.12f * Pv.Az.Size() * Zoom);
	auto DrawGoalPiece = [&](int32 Piece)
	{
		const int32 g = Piece / 3, k = Piece % 3;
		const float X0 = (g == 0 ? -1.f : 1.f) * HalfLen;
		const FLinearColor White(0.97f, 0.97f, 0.95f, 1.f), Edge(0.62f, 0.64f, 0.66f, 1.f);
		if (k < 2)
		{
			const float Y = k == 0 ? GoalHalf : -GoalHalf;
			const FVector2D A = W2S(X0, Y, 0), B = W2S(X0, Y, GoalH + 0.06f);
			Line(A, B, PostW, White);
			Line(A + FVector2D(PostW * 0.35f, 0), B + FVector2D(PostW * 0.35f, 0), PostW * 0.3f, Edge);   // shaded side
		}
		else
		{
			Line(W2S(X0, -GoalHalf, GoalH), W2S(X0, GoalHalf, GoalH), BarW, White);
			Line(W2S(X0, -GoalHalf, GoalH) + FVector2D(0, BarW * 0.35f), W2S(X0, GoalHalf, GoalH) + FVector2D(0, BarW * 0.35f), BarW * 0.3f, Edge);
		}
		Flush();
	};

	// shadows
	auto DrawShadow = [&](FVector2D Ground, float Scale)
	{
		const FVector2D S = ToScreen(Proj.GroundToAtlas(Ground)) + FVector2D(FMath::RoundToFloat(5.f * SpriteScale * Scale), FMath::RoundToFloat(1.f * SpriteScale * Scale));   // cast toward the lower right (ISS)
		const FVector2D Sz = FVector2D(FMath::RoundToFloat(26 * SpriteScale * Scale), FMath::RoundToFloat(9 * SpriteScale * Scale));
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
	for (int32 g = 0; g < 2; ++g) for (int32 k = 0; k < 3; ++k)
		Items.Add({ (float)Proj.GroundToAtlas(FVector2D((g == 0 ? -1.f : 1.f) * HalfLen, k == 0 ? GoalHalf : k == 1 ? -GoalHalf : 0.f)).Y, 4, g * 3 + k });
	Items.Sort([](const FDrawItem& A, const FDrawItem& B) { return A.Depth < B.Depth; });
	for (const FDrawItem& It : Items)
	{
		if (It.Kind == 4) { DrawGoalPiece(It.Idx); continue; }
		if (It.Kind == 1)
		{
			if (!BallTex) continue;
			const FVector2D S = ToScreen(Pv.ToAtlas(Ball.Pos));
			const int32 Frame = int32(Ball.Roll / 0.35f) % 5;
			const float Px = FMath::Max(4.f, FMath::RoundToFloat(16.f * SpriteScale * 0.72f));
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
		if (It.Kind == 0 && Pl.Act == EPFAct::Dive && !Pl.DiveDir.IsNearlyZero())
		{
			const FString ClipPose = PFClips::Get(Pl.Clip).At(Pl.ActT * Pl.A.AnimRate).ToString();
			if (ClipPose.StartsWith(TEXT("gk_stretch")))
			{
				// ISS dive: the keeper flies flat. The front-on full-stretch still is turned so his hands lead along the
				// dive as the camera sees it, and lifted to the dive's height (high dives fly higher).
				FName SP = ClipPose.EndsWith(TEXT("R")) ? FName(TEXT("gk_stretch_R")) : FName(TEXT("gk_stretch_L"));
				if (!Set.Has(SP)) SP = Set.Has(TEXT("gk_catch_high")) ? FName(TEXT("gk_catch_high")) : FName(TEXT("idle"));
				const FVector2D DS = SafeN(Proj.DirToScreen(Pl.DiveDir), FVector2D(1, 0));
				const bool bHighDive = Pl.Clip.ToString().Contains(TEXT("high"));
				// the club's own flat dive (owner's R08 pack, audited): drawn as is, hands leading toward the dive on screen
				const FName Fly = DS.X < 0 ? (bHighDive ? FName(TEXT("gk_fly_high_L")) : FName(TEXT("gk_fly_low_L"))) : (bHighDive ? FName(TEXT("gk_fly_high_R")) : FName(TEXT("gk_fly_low_R")));
				if (Set.Has(Fly))
				{
					const FPFFrame& FF = Set.Get(EPFDir::S, Fly);
					const FVector2D Body = ToScreen(Pv.ToAtlas(FVector(Pl.Pos.X, Pl.Pos.Y, bHighDive ? 0.75f : 0.1f)));
					FCanvasTileItem DT(FVector2D(Body.X - Set.Pivot.X * SpriteScale, Body.Y - Set.Pivot.Y * SpriteScale), Mid->GetRenderProxy(), Size, FF.UV0, FF.UV1);
					C->DrawItem(DT);
					continue;
				}
				const FPFFrame& SF = Set.Get(EPFDir::S, SP);
				if (SF.bValid)
				{
					const float Ang = FMath::RadiansToDegrees(FMath::Atan2(DS.X, -DS.Y));      // clockwise from "head up"
					const float Z = Pl.Clip.ToString().Contains(TEXT("high")) ? 1.15f : 0.45f;
					const FVector2D Body = ToScreen(Pv.ToAtlas(FVector(Pl.Pos.X, Pl.Pos.Y, Z)));
					const FVector2D Piv(0.5f, 56.f / 96.f);
					FVector2D SU0 = SF.UV0, SU1 = SF.UV1;
					if (SF.bFlip) Swap(SU0.X, SU1.X);
					FCanvasTileItem DT(Body - Size * Piv, Mid->GetRenderProxy(), Size, SU0, SU1);
					DT.Rotation = FRotator(0, Ang, 0); DT.PivotPoint = Piv;
					C->DrawItem(DT);
					continue;
				}
			}
		}
		FCanvasTileItem T(Pos, Mid->GetRenderProxy(), Size, UV0, UV1);
		C->DrawItem(T);
		// the card in the referee's raised hand
		if (It.Kind == 2 && It.Idx == 0 && Pl.Act == EPFAct::Clip && Pl.Clip == TEXT("ref_card") && Pose == TEXT("ref_card"))
		{
			const FVector2D Hand(Feet.X + (F.bFlip ? -6.f : 6.f) * SpriteScale, Feet.Y - 74.f * SpriteScale);
			FCanvasTileItem Cd(FVector2D(FMath::RoundToFloat(Hand.X - 3 * SpriteScale), FMath::RoundToFloat(Hand.Y - 5 * SpriteScale)), GWhiteTexture, FVector2D(6, 8) * SpriteScale, CardColor);
			C->DrawItem(Cd);
		}
	}
	// cursor over the controlled player (bobs gently)
	if (Human >= 0 && !Setup.bWatchOnly && CursorTex && P.IsValidIndex(Human) && !P[Human].bSentOff)
	{
		const FPFPlayer& H = P[Human];
		const FVector2D Feet = ToScreen(Proj.GroundToAtlas(H.Pos));
		const float Bob = FMath::RoundToFloat(FMath::Sin(Accum * 0.f + MatchClock * 40.f) * 2.f) * SpriteScale;
		const FVector2D Sz(16 * SpriteScale, 16 * SpriteScale);
		FCanvasTileItem T(FVector2D(FMath::RoundToFloat(Feet.X - Sz.X * 0.5f), FMath::RoundToFloat(Feet.Y - 84 * SpriteScale + Bob)), CursorTex->GetResource(), Sz,
			H.Team == 0 ? FLinearColor(0.45f, 0.65f, 1.f) : FLinearColor(1.f, 0.5f, 0.35f));
		T.BlendMode = SE_BLEND_Translucent;
		C->DrawItem(T);
	}
}
