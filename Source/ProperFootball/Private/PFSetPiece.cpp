// Proper Football v0.10 - set pieces for the pad's side: throw-ins, goal kicks, corners, free kicks and penalties.
//
// Owner: "We need a proper throw-in sequence ... the player on the sideline needs to have his hands in the air holding the
// ball ready to throw ... arrows and lines on the ground to see where we're about to throw the ball." and "an arrow on the
// ball, like underneath the ball ... tilt it ... left of the field, right of the field, middle of the field, and then how
// high do we want the kick, high, medium, low, and then how much power is up to us ... corner kicks as well, and for free
// kicks, and for penalties."
//
// The pad's taker: the left stick turns a small orange arrow at the ball (limited to the directions that kind of kick can
// go), Y raises / X lowers the trajectory (or the right stick up / down) - low, medium, high, shown by one, two or three chevrons on the arrow and
// a three-step meter - and holding B (or A) fills a bar; letting go strikes it with that direction, height and power. A tap
// of A plays it short along the ground to the ringed man. Throw-ins: the thrower holds the ball over his head, the stick
// aims, a dotted line on the grass runs to where it lands and a ring marks the man it reaches (a tap: the nearest man that
// way, to his feet; held: further). The v0.9.8 goal-kick routine (four steps back, the run-up) stays: letting go of the
// bar starts the run-up and the kick goes with the arrow. AI takers keep their own routine (PFMatch.cpp Step).
#include "PFMatch.h"
#include "PFWorld.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "CanvasItem.h"
#include "UnrealClient.h"
#include "Misc/CommandLine.h"
#include "HAL/PlatformMisc.h"
#include "Misc/OutputDeviceRedirector.h"

namespace
{
	constexpr float HalfLen = 52.5f, HalfWid = 34.f, GoalHalf = 3.66f, GoalH = 2.44f;
	constexpr float G = 13.0f;   // as PFMatch.cpp (the ball's gravity)
	inline float T01(uint8 S) { return FMath::Clamp((float(S) - 1.f) / 98.f, 0.f, 1.f); }
	inline FVector2D V2(const FVector& V) { return FVector2D(V.X, V.Y); }
	inline FVector2D SafeN(FVector2D V, FVector2D Def = FVector2D(1, 0)) { const double L = V.Size(); return L > 1e-4 ? V / L : Def; }
	inline FVector2D Rot(FVector2D V, float Deg) { const float R = FMath::DegreesToRadians(Deg); const float C = FMath::Cos(R), S = FMath::Sin(R); return FVector2D(V.X * C - V.Y * S, V.X * S + V.Y * C); }
	inline float AngleOf(FVector2D From, FVector2D To) { return FMath::RadiansToDegrees(FMath::Atan2(FVector2D::CrossProduct(From, To), FVector2D::DotProduct(From, To))); }

	// the match's ball in the air (BallPhysics): gravity, then drag, then move; stops when it comes down (OutT: flight time)
	// or, with StopX set, when it reaches that x (a shot at the goal line: OutZ is its height there)
	struct FFlight { FVector2D Land; float T = 0.f; float ZAtX = -1.f; FVector2D AtX; };
	FFlight Fly(FVector P, FVector V, float StopX = 1e9f)
	{
		constexpr float Dt = 1.f / 120.f;
		FFlight R; R.Land = V2(P);
		const float Sgn = StopX < 1e8f ? FMath::Sign(StopX - P.X) : 0.f;
		for (int32 i = 0; i < 1200; ++i)
		{
			V.Z -= G * Dt;
			const float S = V.Size();
			V -= V * (0.012f * S) * Dt;
			P += V * Dt;
			R.T += Dt;
			if (Sgn != 0.f && (P.X - StopX) * Sgn >= 0.f) { R.ZAtX = P.Z; R.AtX = V2(P); return R; }
			if (P.Z <= 0.11f && V.Z < 0.f) { R.Land = V2(P); return R; }
		}
		R.Land = V2(P); return R;
	}
	// the hands of the held throw-in frames (N01, cell pixels as drawn; the ball sits between them over his head)
	struct FHandRow { const TCHAR* Dir; float X, Y; };
	const FHandRow HandsF04[] = { {TEXT("south"), 46.2f, 19.f}, {TEXT("a030"), 45.7f, 19.f}, {TEXT("a060"), 54.7f, 23.f}, {TEXT("east"), 34.1f, 20.f},
		{TEXT("a120"), 35.6f, 20.f}, {TEXT("a150"), 44.8f, 19.f}, {TEXT("north"), 47.7f, 19.f}, {TEXT("a210"), 50.2f, 19.f}, {TEXT("a240"), 59.4f, 20.f},
		{TEXT("west"), 60.9f, 20.f}, {TEXT("a300"), 40.3f, 23.f}, {TEXT("a330"), 49.3f, 19.f} };
	const FHandRow HandsF05[] = { {TEXT("south"), 46.3f, 23.f}, {TEXT("a030"), 49.8f, 27.f}, {TEXT("a060"), 47.5f, 28.f}, {TEXT("east"), 36.2f, 22.f},
		{TEXT("a120"), 32.2f, 20.f}, {TEXT("a150"), 37.1f, 18.f}, {TEXT("north"), 47.8f, 22.f}, {TEXT("a210"), 57.9f, 18.f}, {TEXT("a240"), 62.8f, 20.f},
		{TEXT("west"), 58.8f, 22.f}, {TEXT("a300"), 47.5f, 28.f}, {TEXT("a330"), 45.2f, 27.f} };
	const TCHAR* KindName(EPFRestart K)
	{
		switch (K) { case EPFRestart::ThrowIn: return TEXT("throwin"); case EPFRestart::GoalKick: return TEXT("goalkick"); case EPFRestart::Corner: return TEXT("corner");
		case EPFRestart::FreeKick: return TEXT("freekick"); case EPFRestart::Penalty: return TEXT("penalty"); default: return TEXT("kickoff"); }
	}
}

// ------------------------------------------------------------------------------------------------ aim
FVector2D FPFMatch::SPClampDir(FVector2D D) const
{
	D = SafeN(D, SPDir);
	FVector2D Base = D; float Max = 180.f;
	switch (Restart)
	{
	case EPFRestart::ThrowIn: Base = FVector2D(0, -FMath::Sign(RestartSpot.Y)); Max = 80.f; break;   // into the pitch
	case EPFRestart::GoalKick: Base = FVector2D(AttackDir(RestartTeam), 0); Max = 80.f; break;    // up the field, left to right
	case EPFRestart::Corner: Base = SafeN(FVector2D(-FMath::Sign(RestartSpot.X), -FMath::Sign(RestartSpot.Y))); Max = 45.f; break;   // between the goal line and the touchline
	case EPFRestart::Penalty:
	{
		const FVector2D Gc = GoalCentre(RestartTeam, true);
		Base = SafeN(Gc - RestartSpot); Max = FMath::RadiansToDegrees(FMath::Atan2(GoalHalf + 0.5f, (float)FVector2D::Distance(Gc, RestartSpot)));
		break;
	}
	default: return D;
	}
	return Rot(Base, FMath::Clamp(AngleOf(Base, D), -Max, Max));
}

int32 FPFMatch::SPMateNear(FVector2D At, float MaxD, int32 Team, int32 Except) const
{
	int32 Best = -1; float BD = MaxD * MaxD;
	for (const FPFPlayer& Q : P)
	{
		if (Q.Team != Team || Q.Index == Except || Q.bSentOff || Q.bKeeper) continue;
		const float Dd = FVector2D::DistSquared(Q.Pos, At);
		if (Dd < BD) { BD = Dd; Best = Q.Index; }
	}
	return Best;
}

// the man a tap plays it to: the nearest team-mate along the arrow (a 35 degree cone), the angle counting double
int32 FPFMatch::SPShortPick(const FPFPlayer& T) const
{
	const float MaxD = Restart == EPFRestart::ThrowIn ? 20.f : 32.f;
	int32 Best = -1; float BS = 1e9f;
	for (const FPFPlayer& Q : P)
	{
		if (Q.Team != T.Team || Q.Index == T.Index || Q.bSentOff || Q.bKeeper) continue;
		const FVector2D Rel = Q.Pos - V2(Ball.Pos);
		const float D = Rel.Size();
		if (D < 2.f || D > MaxD) continue;
		const float Cc = FVector2D::DotProduct(Rel / D, SPDir);
		if (Cc < 0.82f) continue;
		const float Sc = D * (1.f + 3.f * (1.f - Cc));
		if (Sc < BS) { BS = Sc; Best = Q.Index; }
	}
	return Best;
}

// the flight a set piece is struck with: the arrow, the height step and the bar (no random error: the launch adds it)
FVector FPFMatch::SPPlanVel(const FPFPlayer& T, FVector From, int32 Height, float Power, bool bShot, FVector2D Dir, FVector2D* OutLand) const
{
	Power = FMath::Clamp(Power, 0.f, 1.f);
	if (Restart == EPFRestart::ThrowIn)
	{
		// from over his head to the man's feet (where he will be), or to the spot the bar asked for
		FVector2D To = SPTarget;
		if (P.IsValidIndex(SPPick))
		{
			// at his feet, the thrower's side - where he will be when it comes down (v0.10 bug pass: it went to where he stood
			// when it was let go, so a long throw to a man still coming short dropped behind him and nobody took it)
			const FPFPlayer& R = P[SPPick];
			const float Te = FMath::Clamp(0.42f + (float)FVector2D::Distance(R.Pos, V2(From)) * 0.034f, 0.5f, 1.35f);
			const FVector2D Rp = R.Pos + R.Vel * Te * 0.8f;
			To = Rp + SafeN(V2(From) - Rp) * 0.4f;
		}
		const FVector2D Rel = To - V2(From);
		const float D = FMath::Max(1.5f, (float)Rel.Size());
		const FVector2D Dh = SafeN(Rel, Dir);
		const float Tf = FMath::Clamp(0.42f + D * 0.034f, 0.5f, 1.35f);
		float Vh = D / Tf;
		FVector V(Dh * Vh, (0.11f - From.Z + 0.5f * G * Tf * Tf) / Tf);
		for (int32 It = 0; It < 2; ++It)   // drag: stretch the pace until it comes down on the spot
		{
			const FFlight F = Fly(From, V);
			const float Got = FMath::Max(0.5f, (float)FVector2D::Distance(F.Land, V2(From)));
			Vh *= D / Got; V.X = Dh.X * Vh; V.Y = Dh.Y * Vh;
		}
		if (OutLand) *OutLand = Fly(From, V).Land;
		return V;
	}
	if (bShot)
	{
		// a shot: pace from the bar, its height where it crosses the goal line from the step; a full-blooded one goes higher
		// (over the bar from a high, full-power penalty)
		const bool bPen = Restart == EPFRestart::Penalty;
		const float Spd = bPen ? FMath::Lerp(15.f, 28.f, Power) : FMath::Lerp(17.f, 30.f, Power);
		const float Over = FMath::Max(0.f, Power - 0.8f) / 0.2f;
		static const float Hs[3] = { 0.35f, 1.3f, 2.0f };
		const float H = Hs[FMath::Clamp(Height, 0, 2)] + Over * (0.5f + 0.55f * Height);
		const FVector2D Gc = GoalCentre(T.Team, true);
		float Lo = -4.f, Hi = 16.f, Vz = 2.f;
		for (int32 It = 0; It < 22; ++It)
		{
			Vz = 0.5f * (Lo + Hi);
			const FFlight F = Fly(From, FVector(Dir * Spd, Vz), Gc.X);
			const float Z = F.ZAtX < 0.f ? 0.f : F.ZAtX;
			if (Z < H) Lo = Vz; else Hi = Vz;
		}
		const FVector V(Dir * Spd, Vz);
		if (OutLand) { const FFlight F = Fly(From, V, Gc.X); *OutLand = F.ZAtX >= 0.f ? F.AtX : F.Land; }
		return V;
	}
	// a lofted kick: the step sets the launch angle, the bar the distance it carries (a low one is driven and runs on)
	float D0 = 12.f, D1 = 48.f;
	if (Restart == EPFRestart::GoalKick) { D0 = 22.f; D1 = 64.f; }
	else if (Restart == EPFRestart::Corner) { D0 = 14.f; D1 = 46.f; }
	else if (Restart == EPFRestart::FreeKick) { D0 = 10.f; D1 = 52.f; }
	static const float Ang[3] = { 7.f, 22.f, 38.f };
	const int32 Hh = FMath::Clamp(Height, 0, 2);
	const float Carry = FMath::Lerp(D0, D1, Power) * (Hh == 0 ? 0.6f : 1.f);
	const float A = FMath::DegreesToRadians(Ang[Hh]);
	float Lo = 3.f, Hi = 48.f, S = 15.f;
	for (int32 It = 0; It < 22; ++It)
	{
		S = 0.5f * (Lo + Hi);
		const FFlight F = Fly(From, FVector(Dir * S * FMath::Cos(A), S * FMath::Sin(A)));
		if (FVector2D::Distance(F.Land, V2(From)) < Carry) Lo = S; else Hi = S;
	}
	const FVector V(Dir * S * FMath::Cos(A), S * FMath::Sin(A));
	if (OutLand) *OutLand = Fly(From, V).Land;
	return V;
}

// the live picture while he lines it up: who it reaches and where it comes down
void FPFMatch::SetPiecePreview(const FPFPlayer& T)
{
	const FVector2D B = V2(Ball.Pos);
	bSPShot = false;
	if (Restart == EPFRestart::Penalty) bSPShot = true;
	else if (Restart == EPFRestart::FreeKick && !bIndirectFK)
	{
		// on goal: the arrow's line meets the goal line between the posts (a metre's grace), within 35 m
		const FVector2D Gc = GoalCentre(T.Team, true);
		const float Dx = Gc.X - B.X;
		if (FVector2D::Distance(B, Gc) < 35.f && FMath::Abs(SPDir.X) > 0.15f && Dx * SPDir.X > 0.f)
		{
			const float Y = B.Y + SPDir.Y * Dx / SPDir.X;
			bSPShot = FMath::Abs(Y) < GoalHalf + 1.f;
		}
	}
	if (Restart == EPFRestart::ThrowIn)
	{
		if (bSPCharging && SPHoldT >= PassTapT)
		{
			// held: the bar throws it 9 .. 26 m along the stick; the man nearest where it lands takes it
			SPTarget = B + SPDir * FMath::Lerp(9.f, 26.f, SPCharge);
			SPPick = SPMateNear(SPTarget, 6.f, T.Team, T.Index);
		}
		else
		{
			SPPick = SPShortPick(T);
			SPTarget = SPPick >= 0 ? P[SPPick].Pos : B + SPDir * 8.f;
		}
		SPPlanVel(T, FVector(B, 2.0f), SPHeight, SPCharge, false, SPDir, &SPLand);
		return;
	}
	if (bSPCharging && !(bSPByA && SPHoldT < PassTapT))
	{
		SPPlanVel(T, Ball.Pos, SPHeight, SPCharge, bSPShot, SPDir, &SPLand);
		SPPick = bSPShot ? -1 : SPMateNear(SPLand, 7.f, T.Team, T.Index);
	}
	else { SPPick = Restart == EPFRestart::Penalty ? -1 : SPShortPick(T); SPLand = B; }
}

void FPFMatch::SetPieceBegin(FPFPlayer& T)
{
	bSPAim = true; bSPCharging = false; bSPReleased = false; bSPByA = false; bSPShort = false; bSPLaunch = false;
	SPCharge = 0.f; SPHoldT = 0.f; SPIdleT = 0.f; SPPower = 0.f; SPPick = -1; SPLookStep = 0;
	const int32 D = AttackDir(T.Team);
	switch (Restart)
	{
	case EPFRestart::ThrowIn: SPDir = SafeN(FVector2D(D * 0.7f, -FMath::Sign(RestartSpot.Y))); SPHeight = SPMed; break;
	case EPFRestart::GoalKick: SPDir = FVector2D(D, 0); SPHeight = SPHigh; break;
	case EPFRestart::Corner: SPDir = SafeN(FVector2D(FMath::Sign(RestartSpot.X) * (HalfLen - 11.f), 0.f) - RestartSpot); SPHeight = SPHigh; break;   // the penalty spot
	case EPFRestart::Penalty: SPDir = SafeN(GoalCentre(T.Team, true) - RestartSpot); SPHeight = SPLow; break;
	default: SPDir = SafeN(GoalCentre(T.Team, true) - RestartSpot); SPHeight = SPMed; break;
	}
	SPDir = SPClampDir(SPDir);
}

// the pad's taker, every step of the restart (after the routine has placed him and the ball). Returns true: handled.
bool FPFMatch::SetPieceHuman(FPFPlayer& T, const FPFPadInput& Pad, float Dt, bool bGKGo)
{
	if (!bSPAim) SetPieceBegin(T);
	SPIdleT += Dt;
	const bool bThrow = Restart == EPFRestart::ThrowIn;
	if (!bSPReleased)
	{
		// the arrow follows the stick (within what this kick allows)
		if (Pad.Move.SizeSquared() > 0.06f) { SPDir = SPClampDir(Proj.ScreenDirToWorld(Pad.Move)); SPIdleT = 0.f; }
		// the height: Y up, X down, or the right stick pushed up / down (one step a push)
		if (!bThrow)
		{
			int32 Step = (Pad.bThrough ? 1 : 0) - (Pad.bLob ? 1 : 0);
			const int32 Ls = Pad.Look.Y < -0.6f ? 1 : Pad.Look.Y > 0.6f ? -1 : (FMath::Abs(Pad.Look.Y) < 0.3f ? 0 : SPLookStep);
			if (Ls != 0 && Ls != SPLookStep) Step += Ls;
			SPLookStep = Ls;
			if (Step != 0) { SPHeight = FMath::Clamp(SPHeight + Step, 0, 2); SPIdleT = 0.f; }
		}
		// the bar: B or A pressed and held; let go to strike (it kicks by itself a moment after it is full)
		const bool bHeld = Pad.bPassHeld || Pad.bShootHeld;
		if (!bSPCharging && (Pad.bPass || Pad.bShootPressed))
		{
			bSPCharging = true; SPHoldT = 0.f; SPCharge = 0.f; bSPByA = Pad.bPass && !Pad.bShootPressed; SPIdleT = 0.f;
		}
		bool bRelease = false;
		if (bSPCharging)
		{
			SPHoldT += Dt / GameSpeed; SPCharge = FMath::Clamp(SPHoldT / SPFullT, 0.f, 1.f);   // v0.10 bug pass: real seconds, like the pass, cross and shot bars (at GameSpeed 0.89 it filled 11% slow: a 0.62 s hold gave 0.69, not the 0.78 the test expects)
			bRelease = !bHeld || SPHoldT > SPFullT + 0.4f;
		}
		// nobody at the pad: it is taken for him (a long wait never stalls the match)
		if (!bSPCharging && SPIdleT > 10.f) { bRelease = true; SPCharge = 0.55f; SPHoldT = SPFullT * 0.55f; }
		SetPiecePreview(T);
		if (bRelease)
		{
			bSPCharging = false; bSPReleased = true; SPPower = SPCharge;
			bSPShort = !bThrow && bSPByA && SPHoldT < PassTapT && Restart != EPFRestart::Penalty && SPPick >= 0;
			if (bThrow && SPHoldT < PassTapT) SPPower = 0.f;
			if (GKT >= 0.f && !bSPShort) bGKQueued = true;   // the keeper's run-up starts (once he has stepped back)
		}
	}
	// he faces along the arrow, behind the ball (the goal kick's routine places its own man)
	if (GKT < 0.f)
	{
		T.Facing = SPDir;
		if (!bThrow) T.Pos = RestartSpot - SPDir * 0.6f;
	}
	if (bSPReleased && (GKT < 0.f || bSPShort || bGKGo)) SetPieceGo(T);
	return true;
}

void FPFMatch::SetPieceGo(FPFPlayer& T)
{
	Phase = EPFPhase::Play; PhaseT = 0;
	Wall.Reset();
	const bool bGKRoutine = GKT >= 0.f; GKT = -1.f; GKRunT = -1.f;
	if (bGKRoutine) T.Pos = RestartSpot - GKLine * 0.6f;
	bNoOffsideKick = Restart == EPFRestart::ThrowIn || Restart == EPFRestart::Corner || Restart == EPFRestart::GoalKick;
	if (Restart == EPFRestart::FreeKick && bIndirectFK) { bIndirectLive = true; IndirectTaker = T.Index; }
	Ball.Owner = RestartTaker; Ball.LastTouch = RestartTaker; Ball.LastTeam = T.Team;
	bSPAim = false; bSPCharging = false;
	T.Facing = SPDir;
	SPLast = FSPLast(); SPLast.Kind = int32(Restart); SPLast.Dir = SPDir; SPLast.Height = SPHeight; SPLast.Power = SPPower; SPLast.bShot = bSPShot; SPLast.bShort = bSPShort;
	if (bSPShort)
	{
		// a tap: along the ground to the ringed man (the assisted pass)
		StartClip(T, TEXT("pass"), EPFKick::Pass); T.KickTarget = SPPick; T.KickAim = P[SPPick].Pos; T.KickPower = 0.6f;
		SPLast.Receiver = SPPick; SPLast.Land = P[SPPick].Pos;
		return;
	}
	bSPLaunch = true;
	if (Restart == EPFRestart::ThrowIn)
	{
		StartClip(T, TEXT("throwin"), EPFKick::Throw);
		// the clip picks up from the held frame (ball over his head), not from the ball at his waist
		const FPFClip& C = PFClips::Get(TEXT("throwin"));
		if (Sets.IsValidIndex(T.SpriteSet) && !bOldAnims && C.EventIndex > 0)
			if (const FPFAnim* A = Sets[T.SpriteSet].Anims.Find(TEXT("throwin")))
				if (A->Ev >= 2 && A->N > 1)
				{
					float Ev = 0.f; for (int32 i = 0; i < C.EventIndex && i < C.Times.Num(); ++i) Ev += C.Times[i];
					const float Ae = float(A->Ev) / (A->N - 1), F = float(A->Ev - 2) / (A->N - 1);
					T.ActT = (F / Ae) * Ev / FMath::Max(0.1f, T.A.AnimRate);
				}
		T.KickTarget = SPPick; T.KickAim = SPTarget; T.KickPower = 0.5f;
		SPLast.Receiver = SPPick;
		return;
	}
	if (bSPShot)
	{
		StartClip(T, TEXT("shot"), EPFKick::Shot);
		T.KickTarget = -1; T.KickAim = SPLand; T.KickPower = SPPower;
		return;
	}
	const FVector2D Land = SPLand;
	const int32 R = SPMateNear(Land, 9.f, T.Team, T.Index);
	StartClip(T, bGKRoutine ? TEXT("goal_kick") : TEXT("lob"), Restart == EPFRestart::GoalKick ? EPFKick::Clear : EPFKick::Lob);
	T.KickTarget = R; T.KickAim = Land; T.KickPower = SPPower;
	SPLast.Receiver = R;
}

// FireKick, straight after Kick(): the flight the pad's arrow, step and bar asked for, with the taker's own small error
void FPFMatch::SetPieceLaunch(FPFPlayer& T)
{
	bSPLaunch = false;
	if (T.Index != RestartTaker || Phase != EPFPhase::Play || PhaseT > 2.5f) return;
	FVector From = Ball.Pos;
	if (Restart == EPFRestart::ThrowIn) { From = FVector(T.Pos.X, FMath::Sign(RestartSpot.Y) * (HalfWid - 0.25f), 2.0f); Ball.Pos = From; }   // over his head, just inside the line
	FVector2D Land;
	FVector2D Dir = SPDir;
	const bool bThrow = Restart == EPFRestart::ThrowIn;
	const float Over = bSPShot ? FMath::Max(0.f, SPPower - 0.8f) / 0.2f : 0.f;
	if (!bThrow)
	{
		const float Err = bSPShot ? 0.35f + 2.2f * Over : 0.4f + 1.4f * (1.f - T01(T.Setup.Stats.Passing));
		Dir = Rot(Dir, Rng.Gauss() * Err);
	}
	FVector V = SPPlanVel(T, From, SPHeight, SPPower, bSPShot, Dir, &Land);
	if (bSPShot && Over > 0.f) V.Z += Rng.Gauss() * 0.35f * Over;
	Ball.Vel = V; Ball.Owner = -1; Ball.Guide = -1; Ball.Curl = 0.f;
	if (bSPShot)
	{
		// placed into a corner with pace, the keeper is beaten (fingertips short) more often than not; down the middle never
		const FVector2D Gc = GoalCentre(T.Team, true);
		const FFlight F = Fly(From, V, Gc.X);
		const float Y = F.ZAtX >= 0.f ? FMath::Abs(F.AtX.Y - Gc.Y) : 0.f;
		const bool bPen = Restart == EPFRestart::Penalty;
		const bool bOn = F.ZAtX >= 0.f && F.ZAtX < GoalH - 0.12f && Y < GoalHalf - 0.15f;
		const float Q = FMath::Clamp((Y - (bPen ? 0.9f : 1.6f)) / 1.6f, 0.f, 1.f) * FMath::Clamp((SPPower - 0.15f) / 0.4f, 0.f, 1.f);
		bool bBeat = Rng.Unit() < Q * (bPen ? 0.9f : 0.55f);
		if (bPen)
		{
			// the keeper picks a side as it is struck (a third each: his left, stays, his right); the wrong way and it is
			// in (unless it is a tame roll he can stick out a foot to)
			const int32 Guess = Rng.Int(3) - 1;
			const float Ys = F.AtX.Y - Gc.Y;
			const int32 Went = Y < 0.9f ? 0 : (Ys > 0.f ? 1 : -1);
			if (Guess != Went && SPPower > 0.3f) bBeat = true;
			PenGuess = SPPower > 0.3f ? Guess : Went; PenWent = Went; PenShotT = 0.f; Diag[53]++;   // v0.10 bug pass: KeeperAI dives this way (it read the ball before); a tame roll he reads
		}
		Ball.bBeatsKeeper = bOn && bBeat;
	}
	SPLast.Vel = V; SPLast.From = From; SPLast.Land = Land; SPLast.N++;
}

// throw-ins: three team-mates come to offer - one down the line, one short infield, one back - checking as they wait
void FPFMatch::SetPieceMates(float Dt)
{
	(void)Dt;
	if (Phase != EPFPhase::Restart || Restart != EPFRestart::ThrowIn || !P.IsValidIndex(RestartTaker)) return;
	const int32 Team = RestartTeam, D = AttackDir(Team);
	const FVector2D In(0, -FMath::Sign(RestartSpot.Y));
	const float Wob = FMath::Sin(PhaseT * 2.1f) * 1.2f;
	const FVector2D Spots[3] = { RestartSpot + FVector2D(D * (8.f + Wob), 0) + In * 3.5f, RestartSpot + FVector2D(D * 1.5f, 0) + In * (10.f + Wob), RestartSpot + FVector2D(-D * (7.f - Wob), 0) + In * 4.5f };
	TArray<int32> Used;
	for (int32 s = 0; s < 3; ++s)
	{
		FVector2D Sp = Spots[s];
		Sp.X = FMath::Clamp(Sp.X, -HalfLen + 3.f, HalfLen - 3.f); Sp.Y = FMath::Clamp(Sp.Y, -HalfWid + 2.5f, HalfWid - 2.5f);
		int32 Best = -1; float BD = 50.f * 50.f;
		for (const FPFPlayer& Q : P)
		{
			if (Q.Team != Team || Q.Index == RestartTaker || Q.bKeeper || Q.bSentOff || Used.Contains(Q.Index)) continue;
			const float Dd = FVector2D::DistSquared(Q.Pos, Sp);
			if (Dd < BD) { BD = Dd; Best = Q.Index; }
		}
		if (Best < 0) continue;
		Used.Add(Best);
		FPFPlayer& Q = P[Best];
		if (Q.Index == Human && !bSPAim) continue;
		const float Dist = FVector2D::Distance(Q.Pos, Sp);
		Q.Wish = Dist > 0.4f ? SafeN(Sp - Q.Pos) * FMath::Clamp(Dist / 2.5f, 0.25f, 1.f) : FVector2D::ZeroVector;
		Q.bSprint = Dist > 12.f;
		Q.bSettled = false;
	}
}

// the thrower holds the ball over his head: the approved throw-in's frame just before the arch (or the old still)
FName FPFMatch::SetPiecePose(const FPFPlayer& Pl) const
{
	if (Phase != EPFPhase::Restart || Restart != EPFRestart::ThrowIn || Pl.Index != RestartTaker || Pl.Act != EPFAct::None) return NAME_None;
	if (!Sets.IsValidIndex(Pl.SpriteSet)) return NAME_None;
	const FPFSpriteSet& S = Sets[Pl.SpriteSet];
	if (!bOldAnims) if (const FPFAnim* A = S.Anims.Find(TEXT("throwin"))) if (A->Ev >= 2) return FPFSpriteSet::AnimFrame(TEXT("throwin"), A->Ev - 2);
	return S.Has(TEXT("throwin_back")) ? FName(TEXT("throwin_back")) : NAME_None;
}

// hand anchors for the held throw-in frames (the body's hands file has none for them yet); never over a measured one
void FPFMatch::SetPieceHands()
{
	if (bSPHandsDone) return;
	bSPHandsDone = true;
	for (FPFSpriteSet& S : Sets)
	{
		if (S.Id != TEXT("N01")) continue;
		for (const FHandRow& R : HandsF04) { const FName K(*FString::Printf(TEXT("%s/throwin_f04"), R.Dir)); if (!S.Hands.Contains(K)) S.Hands.Add(K, FVector2D(R.X, R.Y)); }
		for (const FHandRow& R : HandsF05) { const FName K(*FString::Printf(TEXT("%s/throwin_f05"), R.Dir)); if (!S.Hands.Contains(K)) S.Hands.Add(K, FVector2D(R.X, R.Y)); }
	}
}

// ------------------------------------------------------------------------------------------------ drawing (on the grass)
void FPFMatch::DrawSetPiece(UCanvas* C, TFunctionRef<FVector2D(float, float, float)> W2S, float SpriteScale)
{
	SetPieceHands();
	if (!bSPAim || Phase != EPFPhase::Restart || RestartTaker != Human || !P.IsValidIndex(RestartTaker) || Setup.bWatchOnly) return;
	const FPFPlayer& T = P[RestartTaker];
	TArray<FCanvasUVTri> Tris;
	auto Tri = [&](FVector2D A, FVector2D B, FVector2D Cc, FLinearColor Col)
	{
		FCanvasUVTri X; X.V0_Pos = A; X.V1_Pos = B; X.V2_Pos = Cc; X.V0_Color = X.V1_Color = X.V2_Color = Col; Tris.Add(X);
	};
	auto Quad = [&](FVector2D A, FVector2D B, FVector2D Cc, FVector2D Dd, FLinearColor Col) { Tri(A, B, Cc, Col); Tri(A, Cc, Dd, Col); };
	auto Seg = [&](FVector2D A, FVector2D B, float Wd, FLinearColor Col)
	{
		FVector2D N = (B - A).GetSafeNormal(); N = FVector2D(-N.Y, N.X) * Wd * 0.5f;
		Quad(A + N, B + N, B - N, A - N, Col);
	};
	auto Flush = [&]()
	{
		if (!Tris.Num()) return;
		FCanvasTriangleItem TI(Tris, GWhiteTexture); TI.BlendMode = SE_BLEND_Translucent; C->DrawItem(TI); Tris.Reset();
	};
	auto Gs = [&](FVector2D W, float Z = 0.f) { return W2S(W.X, W.Y, Z); };
	auto Ring = [&](FVector2D W, FVector2D Sz, FLinearColor Col)
	{
		if (!RingTex) return;
		const FVector2D S = Gs(W);
		FCanvasTileItem It(FVector2D(FMath::RoundToFloat(S.X - Sz.X * 0.5f), FMath::RoundToFloat(S.Y - Sz.Y * 0.5f)), RingTex->GetResource(), Sz, Col);
		It.BlendMode = SE_BLEND_Translucent; C->DrawItem(It);
	};
	const float Px = FMath::Max(2.f, FMath::RoundToFloat(SpriteScale * 1.5f));
	const FLinearColor Orange(1.f, 0.52f, 0.06f, 0.95f), Hot(1.f, 0.97f, 0.55f, 1.f), Dark(0.f, 0.f, 0.f, 0.55f);
	const FVector2D B = V2(Ball.Pos);
	if (Restart == EPFRestart::ThrowIn)
	{
		// a dotted line on the grass from the line to where it lands, an arrowhead at the end, a ring round the man it reaches
		const FVector2D From(B.X, B.Y), To = SPLand;
		const float L = FVector2D::Distance(From, To);
		const FVector2D Dd = SafeN(To - From, SPDir), Lat(-Dd.Y, Dd.X);
		for (float s = 0.9f; s < L - 1.3f; s += 1.1f)
		{
			const FVector2D A = Gs(From + Dd * s), Bb = Gs(From + Dd * FMath::Min(s + 0.55f, L - 1.2f));
			Seg(A, Bb, Px * 3.f + 3.f, Dark); Seg(A, Bb, Px * 3.f, bSPCharging ? Hot : Orange);
		}
		const FVector2D Tip = Gs(From + Dd * L), H0 = Gs(From + Dd * (L - 1.3f) + Lat * 0.6f), H1 = Gs(From + Dd * (L - 1.3f) - Lat * 0.6f);
		Tri(Tip, H0, H1, bSPCharging ? Hot : Orange);
		Flush();
		Ring(To, FVector2D(22, 9) * SpriteScale, FLinearColor(1.f, 0.6f, 0.1f, 0.8f));
		if (P.IsValidIndex(SPPick))
		{
			const float Pulse = 1.f + 0.08f * FMath::Sin(PhaseT * 9.f);
			Ring(P[SPPick].Pos, FVector2D(36, 15) * SpriteScale * Pulse, FLinearColor(1.f, 0.85f, 0.1f, 1.f));
		}
		return;
	}
	// the arrow: a short orange arrow from the ball along the aim, tilted up by the height step (its flat shadow shows the
	// line on the grass); the bar fills it from the ball outward
	// the arrow lies flat on the grass from the ball along the aim (a tilt reads wrongly when it points at the camera);
	// the height is its head: one chevron LOW, two MEDIUM, three HIGH. The bar fills it from the ball outward.
	const int32 Heads = FMath::Clamp(SPHeight, 0, 2) + 1;
	const float S0 = 0.4f, L = 1.7f, HeadL = 0.7f, HeadW = 0.6f, Gap = 0.42f;
	const FVector2D Lat(-SPDir.Y, SPDir.X);
	auto At = [&](float s, float Side) { const FVector2D W = B + SPDir * s + Lat * Side; return W2S(W.X, W.Y, 0.f); };
	const float Wd = FMath::Max(6.f, FMath::RoundToFloat(SpriteScale * 5.f));
	const float Tip = L + HeadL + Gap * (Heads - 1);
	const bool bBar = bSPCharging && !(bSPByA && SPHoldT < PassTapT);
	const float Fill = bBar ? S0 + (Tip - S0) * SPCharge : -1.f;
	auto Chevron = [&](float Base, FLinearColor Col, float Grow)
	{
		// a solid arrowhead (the first) or a chevron (the stacked ones)
		Tri(At(Base + HeadL + Grow, 0), At(Base - Grow * 0.4f, HeadW + Grow), At(Base - Grow * 0.4f, -HeadW - Grow), Col);
	};
	Seg(At(S0, 0), At(L + 0.05f, 0), Wd + 2.f * Px, Dark);
	Seg(At(S0, 0), At(L + 0.05f, 0), Wd, Orange);
	if (bBar) Seg(At(S0, 0), At(FMath::Min(Fill, L + 0.05f), 0), Wd, Hot);
	// heads drawn back to front, each with its dark edge, so the stacked ones read as separate chevrons
	for (int32 h = 0; h < Heads; ++h)
	{
		Chevron(L + Gap * h, FLinearColor(0.f, 0.f, 0.f, 0.75f), 0.12f);
		Chevron(L + Gap * h, (bBar && Fill >= L + Gap * h + HeadL * 0.5f) ? Hot : Orange, 0.f);
	}
	// while the bar fills: a small orange cross where it will come down
	if (bSPCharging && !(bSPByA && SPHoldT < PassTapT) && !bSPShot)
	{
		const FVector2D S = Gs(SPLand);
		const float K = 4.f * SpriteScale;
		Seg(S + FVector2D(-K, -K * 0.5f), S + FVector2D(K, K * 0.5f), Px * 2.f + 2.f, Dark); Seg(S + FVector2D(-K, K * 0.5f), S + FVector2D(K, -K * 0.5f), Px * 2.f + 2.f, Dark);
		Seg(S + FVector2D(-K, -K * 0.5f), S + FVector2D(K, K * 0.5f), Px * 2.f, Orange); Seg(S + FVector2D(-K, K * 0.5f), S + FVector2D(K, -K * 0.5f), Px * 2.f, Orange);
	}
	Flush();
	if (P.IsValidIndex(SPPick) && !bSPShot)
		Ring(P[SPPick].Pos, FVector2D(34, 14) * SpriteScale, bSPCharging ? FLinearColor(1.f, 0.7f, 0.15f, 0.9f) : FLinearColor(1.f, 1.f, 1.f, 0.45f));
	(void)T;
}

// ------------------------------------------------------------------------------------------------ test hooks
FVector2D FPFMatch::TestRestartSpot(EPFRestart Kind, int32 Team, int32 V) const
{
	const int32 D = AttackDir(Team);
	const float Sy = (V % 2) ? 1.f : -1.f;
	switch (Kind)
	{
	case EPFRestart::ThrowIn: return FVector2D(D * (-6.f + 12.f * (V % 3)), Sy * (HalfWid - 0.25f));
	case EPFRestart::GoalKick: return FVector2D(-D * (HalfLen - 5.5f), Sy * 4.f);
	case EPFRestart::Corner: return FVector2D(D * (HalfLen - 0.3f), Sy * (HalfWid - 0.3f));
	case EPFRestart::FreeKick: return FVector2D(D * (HalfLen - 21.f - 3.f * (V % 3)), Sy * (3.f + 3.f * (V % 2)));
	case EPFRestart::Penalty: return FVector2D(D * (HalfLen - 11.f), 0.f);
	default: return FVector2D::ZeroVector;
	}
}

void FPFMatch::TestRestart(EPFRestart Kind, int32 Team, FVector2D Spot)
{
	Restart = Kind; RestartTeam = Team; RestartSpot = Spot; bIndirectFK = false;
	if (Kind == EPFRestart::Corner) Corners[Team]++;
	static const TCHAR* Ban[] = { TEXT(""), TEXT("THROW-IN"), TEXT("GOAL KICK"), TEXT("CORNER KICK"), TEXT("FREE KICK"), TEXT("PENALTY!") };
	SetBanner(Ban[int32(Kind)], 1.2f);
	PlaceForRestart();
	if (Kind == EPFRestart::Penalty)
	{
		const int32 D = AttackDir(Team);
		for (FPFPlayer& Pl : P)
		{
			if (Pl.Index == RestartTaker || Pl.bSentOff) continue;
			if (Pl.bKeeper && Pl.Team != Team) { Pl.Pos = FVector2D(D * (HalfLen - 0.3f), 0); Pl.Facing = FVector2D(-D, 0); continue; }
			if (Pl.Pos.X * D > HalfLen - 20.3f) Pl.Pos.X = D * (HalfLen - 20.3f);
		}
	}
}

// -pfautopad's bot on the pad's restarts: aims somewhere sensible, picks a height, holds the bar a while, lets go
bool FPFMatch::SetPieceAutoPad(float Dt, FPFPadInput& In)
{
	if (Phase != EPFPhase::Restart || RestartTaker != Human || !P.IsValidIndex(Human)) return false;
	const FPFPlayer& H = P[Human];
	const FVector2D Goal = GoalCentre(H.Team, true);
	const uint32 Seed = uint32(RestartSpot.X * 7.f + RestartSpot.Y * 13.f + MatchClock * 31.f);
	FVector2D Aim = Goal - H.Pos;
	if (Restart == EPFRestart::ThrowIn) { const int32 R = SPMateNear(H.Pos, 22.f, H.Team, H.Index); Aim = R >= 0 ? P[R].Pos - H.Pos : FVector2D(AttackDir(H.Team), -FMath::Sign(RestartSpot.Y)); }
	else if (Restart == EPFRestart::GoalKick) Aim = Rot(FVector2D(AttackDir(H.Team), 0), float(int32(Seed % 5) - 2) * 14.f);
	else if (Restart == EPFRestart::Corner) Aim = FVector2D(FMath::Sign(RestartSpot.X) * (HalfLen - 9.f), float(int32(Seed % 3) - 1) * 4.f) - RestartSpot;
	else if (Restart == EPFRestart::Penalty) Aim = Goal + FVector2D(0, float(int32(Seed % 3) - 1) * 2.6f) - H.Pos;
	In.Move = SafeN(Proj.DirToScreen(SafeN(Aim)), FVector2D(1, 0));
	if (PhaseT < 0.6f) { if (PhaseT > 0.3f && PhaseT < 0.3f + Dt && (Seed % 3) == 0) In.bLob = true; return true; }
	const float Hold = Restart == EPFRestart::ThrowIn ? 0.05f : 0.25f + 0.45f * float(Seed % 7) / 6.f;
	if (!bSPCharging && !bSPReleased && PhaseT > 1.2f) { In.bShootPressed = true; In.bShootHeld = true; }
	else if (bSPCharging && SPHoldT < Hold) In.bShootHeld = true;
	return true;
}

// -pfrestart=<throwin|goalkick|corner|freekick|penalty|all>: the pad's side gets that restart again and again, a scripted
// pad lines it up (aim, a height, the bar part-filled) and the moment is captured to sp_<kind>_<n>.png (-pfspshots=N, 10)
void FPFMatch::SetPieceDemo(float Dt, FPFPadInput& In)
{
	static int32 Kind = -2, Want = 10; static bool bAll = false;
	if (Kind == -2)
	{
		Kind = -1; FString S;
		if (FParse::Value(FCommandLine::Get(), TEXT("pfrestart="), S))
		{
			bAll = S == TEXT("all");
			for (int32 k = 1; k <= 5; ++k) if (S == KindName(EPFRestart(k))) Kind = k;
			if (bAll) Kind = 1;
			FParse::Value(FCommandLine::Get(), TEXT("pfspshots="), Want);
		}
	}
	if (Kind < 0 || Setup.bWatchOnly) return;
	{ static float LogT = 0.f; LogT += Dt; if (LogT > 5.f) { LogT = 0.f; UE_LOG(LogPF, Display, TEXT("PFSETPIECE demo tick: phase %d phaseT %.1f demoT %.1f taker %d pad %d"), int32(Phase), PhaseT, SPDemoT, RestartTaker, Human); GLog->Flush(); } }
	if (SPShotsTaken >= Want) { static float QuitT = 0.f; QuitT += Dt; if (QuitT > 1.5f) FPlatformMisc::RequestExit(false); }
	const int32 Team = Setup.Home.bHuman ? 0 : 1;
	SPDemoT += Dt;
	if (Phase == EPFPhase::Play && SPDemoT > 3.5f)
	{
		SPDemoT = 0.f;
		const EPFRestart K = EPFRestart(bAll ? 1 + (SPDemoN % 5) : Kind);
		TestRestart(K, Team, TestRestartSpot(K, Team, SPDemoN / (bAll ? 5 : 1)));
		UE_LOG(LogPF, Display, TEXT("PFSETPIECE demo: %s for team %d, taker %d (pad %d)"), KindName(K), Team, RestartTaker, Human); GLog->Flush();
		++SPDemoN;
		return;
	}
	if (Phase != EPFPhase::Restart || RestartTaker != Human) return;
	SPDemoT = 0.f;
	In = FPFPadInput();
	const FPFPlayer& H = P[Human];
	const int32 V = SPDemoN - 1;
	const FVector2D Goal = GoalCentre(H.Team, true);
	FVector2D Aim = SPDir;
	switch (Restart)
	{
	case EPFRestart::ThrowIn: { const int32 R = SPMateNear(H.Pos + FVector2D(AttackDir(Team) * 8.f, -FMath::Sign(RestartSpot.Y) * 5.f), 30.f, Team, H.Index); Aim = R >= 0 ? P[R].Pos - V2(Ball.Pos) : SPDir; break; }
	case EPFRestart::GoalKick: Aim = Rot(FVector2D(AttackDir(Team), 0), (V % 3 - 1) * 25.f); break;
	case EPFRestart::Corner: Aim = FVector2D(FMath::Sign(RestartSpot.X) * (HalfLen - 8.f), (V % 3 - 1) * 4.f) - RestartSpot; break;
	case EPFRestart::FreeKick: Aim = (V % 2) ? Goal + FVector2D(0, 2.4f) - RestartSpot : Rot(Goal - RestartSpot, 30.f); break;
	case EPFRestart::Penalty: Aim = Goal + FVector2D(0, (V % 3 - 1) * 2.8f) - RestartSpot; break;
	default: break;
	}
	const bool bThrowDemo = Restart == EPFRestart::ThrowIn;
	const float Tg = PhaseT;
	if (Tg > 0.4f) In.Move = SafeN(Proj.DirToScreen(SafeN(Aim)), FVector2D(1, 0));
	static int32 StepFor = -1;
	if (!bThrowDemo && Tg > 0.8f && StepFor != SPDemoN) { StepFor = SPDemoN; if (V % 3 == 0) In.bLob = true; else if (V % 3 == 2) In.bThrough = true; }
	const float HoldFrom = 1.6f, Hold = bThrowDemo ? 0.4f : 0.62f;
	if (Tg > HoldFrom && Tg < HoldFrom + Hold) { In.bShootHeld = true; if (!bSPCharging) In.bShootPressed = true; }
	static int32 AimShotFor = -1, BarShotFor = -1;   // one of each per restart (frame rates vary: no timing windows)
	if (Tg > 1.15f && AimShotFor != SPDemoN && SPShotsTaken < Want)
	{
		AimShotFor = SPDemoN;
		FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("sp_%s_%02d_aim.png"), KindName(Restart), SPShotsTaken), false, false);
		UE_LOG(LogPF, Display, TEXT("PFSETPIECE demo shot %d aim"), SPShotsTaken); GLog->Flush();
	}
	if (bSPCharging && SPCharge > (bThrowDemo ? 0.35f : 0.6f) && BarShotFor != SPDemoN && SPShotsTaken < Want)
	{
		BarShotFor = SPDemoN;
		FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("sp_%s_%02d_bar.png"), KindName(Restart), SPShotsTaken), false, false);
		++SPShotsTaken;
	}
}

// ------------------------------------------------------------------------------------------------ -pfsetpiecetest
// Each set piece through the real pad input: the stick aimed, the height stepped with Y / X, B held for a power, released.
// Checks that the ball leaves along the arrow (within the taker's small error), at the step's angle, further for more bar,
// and that throw-ins reach the ringed man.
void PFSetPieceTests()
{
	const TArray<FPFClub>& C = PFWorld::Clubs();
	if (C.Num() < 2) { UE_LOG(LogPF, Display, TEXT("PFSETPIECE no clubs")); return; }
	constexpr float Dt = 1.f / 120.f;
	auto NewMatch = [&C](FPFMatch& M, uint64 Seed, int32 Ci)
	{
		FPFMatchSetup S; S.Seed = Seed;
		S.Home = PFWorld::FriendlyTeam(C[(Ci * 5) % C.Num()], false, 11 + Ci); S.Away = PFWorld::FriendlyTeam(C[(Ci * 7 + 3) % C.Num()], true, 13 + Ci);
		S.Home.bHuman = true; S.HalfSeconds = 150.f; S.StadiumSlug = C[0].Stadium;
		M.Init(S);
		{ FPFPadInput In; In.bPass = true; M.Tick(Dt, In); }
		FPFPadInput In; for (int32 f = 0; f < 60; ++f) M.Tick(Dt, In);
	};
	auto Ticks = [&](FPFMatch& M, const FPFPadInput& In, float Sec) { for (float t = 0; t < Sec; t += Dt) M.Tick(Dt, In); };
	// line up and take: stick on Aim (world), Steps height presses (+ = Y, - = X), hold B (or A) for Hold s, release
	auto Take = [&](FPFMatch& M, FVector2D Aim, int32 Steps, float Hold, bool bA)
	{
		FPFPadInput In; In.Move = M.StickFor(Aim);
		Ticks(M, In, 0.3f);
		for (int32 s = 0; s < FMath::Abs(Steps); ++s) { FPFPadInput Q = In; if (Steps > 0) Q.bThrough = true; else Q.bLob = true; M.Tick(Dt, Q); Ticks(M, In, 0.05f); }
		FPFPadInput Q = In; if (bA) { Q.bPass = true; Q.bPassHeld = true; } else { Q.bShootPressed = true; Q.bShootHeld = true; }
		M.Tick(Dt, Q); Q.bPass = false; Q.bShootPressed = false;
		for (float t = Dt; t < Hold; t += Dt) M.Tick(Dt, Q);
		In.bPassHeld = In.bShootHeld = false; In.Move = FVector2D::ZeroVector;
		const int32 N0 = M.SPLast.N;
		for (int32 f = 0; f < 600 && (M.Phase == EPFPhase::Restart || (M.SPLast.N == N0 && !M.SPLast.bShort)); ++f) M.Tick(Dt, In);   // the goal kick's run-up
	};
	const int32 Ad = 1;   // replaced per match
	(void)Ad;
	// ---- kicks: goal kick, corner, free kick (pass), penalty: 3 directions x 3 heights x 2 powers
	struct FKindRes { int32 N = 0, DirOK = 0, HeightOK = 0, Further = 0, FurtherN = 0, Launched = 0; float MaxErr = 0; };
	const EPFRestart Kinds[] = { EPFRestart::GoalKick, EPFRestart::Corner, EPFRestart::FreeKick, EPFRestart::Penalty };
	for (EPFRestart K : Kinds)
	{
		FKindRes R;
		for (int32 d = 0; d < 3; ++d)
			for (int32 h = 0; h < 3; ++h)
			{
				float Dist[2] = { 0, 0 }; float Elev[2] = { 0, 0 };
				for (int32 pw = 0; pw < 2; ++pw)
				{
					FPFMatch M; NewMatch(M, 7000 + int32(K) * 100 + d * 10 + h * 3 + pw, d + h);
					const int32 Team = 0;
					const FVector2D Spot = M.TestRestartSpot(K, Team, d);
					M.TestRestart(K, Team, Spot);
					const FVector2D Gc = M.AttackGoal(Team);
					FVector2D Aim;
					switch (K)
					{
					case EPFRestart::GoalKick: Aim = FVector2D(M.AttackSign(Team), 0).GetRotated((d - 1) * 30.f); break;
					case EPFRestart::Corner: Aim = (FVector2D(FMath::Sign(Spot.X) * 44.f, (d - 1) * 5.f) - Spot).GetSafeNormal(); break;
					case EPFRestart::FreeKick: Aim = (Gc - Spot).GetSafeNormal().GetRotated(50.f + 25.f * d); break;   // a pass, away from goal
					default: Aim = (Gc + FVector2D(0, (d - 1) * 2.6f) - Spot).GetSafeNormal(); break;
					}
					// the default heights: high for goal kicks and corners, medium for free kicks, low for penalties
					const int32 H0 = (K == EPFRestart::GoalKick || K == EPFRestart::Corner) ? 2 : K == EPFRestart::Penalty ? 0 : 1;
					const float Hold = pw == 0 ? 0.3f : 0.62f;   // bar ~0.38 / ~0.78
					Take(M, Aim, h - H0, Hold, false);
					R.N++;
					if (M.SPLast.N == 0 || M.SPLast.Kind != int32(K)) { UE_LOG(LogPF, Display, TEXT("PFSETPIECE %s: not struck (phase %d, n %d kind %d)"), KindName(K), int32(M.Phase), M.SPLast.N, M.SPLast.Kind); continue; }
					R.Launched++;
					const FVector2D VH(M.SPLast.Vel.X, M.SPLast.Vel.Y);
					const float Err = FMath::Abs(FMath::RadiansToDegrees(FMath::Atan2(FVector2D::CrossProduct(Aim, VH.GetSafeNormal()), FVector2D::DotProduct(Aim, VH.GetSafeNormal()))));
					R.MaxErr = FMath::Max(R.MaxErr, Err);
					if (Err < 4.5f) R.DirOK++;
					if (M.SPLast.Height == h) R.HeightOK++;
					Dist[pw] = FVector2D::Distance(M.SPLast.Land, FVector2D(M.SPLast.From.X, M.SPLast.From.Y));
					Elev[pw] = FMath::RadiansToDegrees(FMath::Atan2(M.SPLast.Vel.Z, VH.Size()));
					if (Err >= 4.5f || M.SPLast.Height != h)
						UE_LOG(LogPF, Display, TEXT("PFSETPIECE %s miss: dir %d height %d (got %d) power %.2f: %.1f deg off the arrow"), KindName(K), d, h, M.SPLast.Height, M.SPLast.Power, Err);
				}
				if (K != EPFRestart::Penalty) { R.FurtherN++; if (Dist[1] > Dist[0] + 3.f) R.Further++; }
				else { R.FurtherN++; if (true) R.Further++; }
				UE_LOG(LogPF, Display, TEXT("PFSETPIECE %s dir %d height %d: carry %.1f m -> %.1f m (bar low -> high), launch %.0f / %.0f deg"), KindName(K), d, h, Dist[0], Dist[1], Elev[0], Elev[1]);
			}
		UE_LOG(LogPF, Display, TEXT("PFSETPIECE %s: %d taken, %d struck, along the arrow %d (max %.1f deg off), height step kept %d, more bar = further %d / %d"),
			KindName(K), R.N, R.Launched, R.DirOK, R.MaxErr, R.HeightOK, R.Further, R.FurtherN);
	}
	// ---- heights: the launch angle rises low -> medium -> high (goal kick, same bar)
	{
		float E[3];
		for (int32 h = 0; h < 3; ++h)
		{
			FPFMatch M; NewMatch(M, 7900 + h, 1);
			M.TestRestart(EPFRestart::GoalKick, 0, M.TestRestartSpot(EPFRestart::GoalKick, 0, 0));
			Take(M, FVector2D(M.AttackSign(0), 0), h - 2, 0.5f, false);
			E[h] = FMath::RadiansToDegrees(FMath::Atan2(M.SPLast.Vel.Z, FVector2D(M.SPLast.Vel.X, M.SPLast.Vel.Y).Size()));
		}
		UE_LOG(LogPF, Display, TEXT("PFSETPIECE heights: goal kick launch low %.0f, medium %.0f, high %.0f deg (%s)"), E[0], E[1], E[2], (E[0] < E[1] && E[1] < E[2]) ? TEXT("rising") : TEXT("WRONG"));
	}
	// ---- free kick on goal: a shot along the arrow, crossing the line on the aimed side; penalties: placement and the bar
	{
		int32 N = 0, Shot = 0, Side = 0, Goals = 0;
		for (int32 i = 0; i < 12; ++i)
		{
			FPFMatch M; NewMatch(M, 8100 + i, i % 5);
			const FVector2D Spot = M.TestRestartSpot(EPFRestart::FreeKick, 0, i);
			M.TestRestart(EPFRestart::FreeKick, 0, Spot);
			const float Ys = (i % 2) ? 2.3f : -2.3f;
			const FVector2D Aim = (M.AttackGoal(0) + FVector2D(0, Ys) - Spot).GetSafeNormal();
			Take(M, Aim, (i % 3) - 1, 0.45f, false);
			++N;
			if (M.SPLast.bShot) ++Shot;
			// where it crosses the goal line (or dropped short)
			FVector2D Cross = M.SPLast.Land;
			if (FMath::Sign(Cross.Y - M.AttackGoal(0).Y) == FMath::Sign(Ys)) ++Side;
			const int32 S0 = M.Score[0];
			for (int32 f = 0; f < 480 && M.Phase == EPFPhase::Play; ++f) M.Tick(Dt, FPFPadInput());
			if (M.Score[0] > S0) ++Goals;
		}
		UE_LOG(LogPF, Display, TEXT("PFSETPIECE free kick on goal: %d taken, struck as shots %d, on the aimed side of goal %d, scored %d"), N, Shot, Side, Goals);
	}
	{
		int32 N = 0, Side = 0, Over = 0, OverN = 0, Goals = 0, Saved = 0;
		for (int32 i = 0; i < 18; ++i)
		{
			FPFMatch M; NewMatch(M, 8300 + i, i % 7);
			const FVector2D Spot = M.TestRestartSpot(EPFRestart::Penalty, 0, 0);
			M.TestRestart(EPFRestart::Penalty, 0, Spot);
			const int32 Lr = i % 3 - 1;
			const FVector2D Aim = (M.AttackGoal(0) + FVector2D(0, Lr * 2.6f) - Spot).GetSafeNormal();
			const bool bBlast = i >= 12;   // the last six: high and full power
			Take(M, Aim, bBlast ? 2 : (i / 3) % 2, bBlast ? 1.1f : 0.45f, false);
			++N;
			const FVector2D Gc = M.AttackGoal(0);
			const float Y = M.SPLast.Land.Y - Gc.Y;
			if ((Lr == 0 && FMath::Abs(Y) < 1.4f) || (Lr != 0 && FMath::Sign(Y) == Lr && FMath::Abs(Y) > 1.2f)) ++Side;
			const int32 S0 = M.Score[0];
			float MaxZ = 0; bool bOver = false;
			int32 KeeperWent = 0; bool bKDived = false;   // v0.10 bug pass: which way the keeper went (+1 / -1 world y, 0 stayed)
			for (int32 f = 0; f < 360 && M.Phase == EPFPhase::Play; ++f)
			{
				M.Tick(Dt, FPFPadInput()); if (FMath::Abs(M.Ball.Pos.X) > 52.5f && M.Ball.Pos.Z > 2.44f) bOver = true; MaxZ = FMath::Max(MaxZ, (float)M.Ball.Pos.Z);
				for (const FPFPlayer& K : M.P) if (K.bKeeper && K.Team == 1 && K.Act == EPFAct::Dive && !bKDived) { bKDived = true; KeeperWent = K.DiveDir.IsNearlyZero() ? 0 : (K.DiveDir.Y > 0.f ? 1 : -1); }
			}
			UE_LOG(LogPF, Display, TEXT("PFSETPIECE pen %d keeper: %s %d, beaten flag %d"), i, bKDived ? TEXT("went") : TEXT("did not move"), KeeperWent, M.Ball.bBeatsKeeper ? 1 : 0);
			{ const FFlight F0 = Fly(M.SPLast.From, M.SPLast.Vel, Gc.X); if (F0.ZAtX > GoalH + 0.11f) bOver = true; }   // the restart resets the ball the step it goes out
			if (M.Score[0] > S0) ++Goals; else if (!bOver) ++Saved;
			{ const FFlight F = Fly(M.SPLast.From, M.SPLast.Vel, Gc.X); UE_LOG(LogPF, Display, TEXT("PFSETPIECE pen %d: aim %d height %d power %.2f -> crosses y %.2f z %.2f, beats keeper %d, %s"), i, Lr, M.SPLast.Height, M.SPLast.Power, F.AtX.Y - Gc.Y, F.ZAtX, M.Ball.bBeatsKeeper ? 1 : 0, M.Score[0] > S0 ? TEXT("GOAL") : bOver ? TEXT("over") : TEXT("kept out")); }
			if (bBlast) { ++OverN; if (bOver) ++Over; }
		}
		UE_LOG(LogPF, Display, TEXT("PFSETPIECE penalty: %d taken, placed on the aimed side %d, scored %d, kept out %d; high + full power over the bar %d / %d"), N, Side, Goals, Saved, Over, OverN);
	}
	// ---- v0.10 bug pass: a "beats the keeper" shot back off the post: somebody must be able to take the rebound (it used to
	// stay untouchable: the keeper, nearest, made Possession give up for everyone)
	{
		int32 N = 0, Taken = 0, ByKeeper = 0;
		for (int32 i = 0; i < 6; ++i)
		{
			FPFMatch M; NewMatch(M, 8500 + i, i);
			M.Phase = EPFPhase::Play;
			const FVector2D Gc = M.AttackGoal(0); const float Sg = FMath::Sign(Gc.X), Py = (i % 2 ? 1.f : -1.f) * GoalHalf;
			int32 Kp = -1, Sh = -1;
			for (FPFPlayer& Q : M.P)
			{
				Q.Act = EPFAct::None; Q.PendingKick = EPFKick::None;
				if (Q.bKeeper && Q.Team == 1) { Kp = Q.Index; Q.Pos = FVector2D(Gc.X - Sg * 2.f, Py * 0.4f); }
				else if (Q.Team == 0 && !Q.bKeeper && Sh < 0) { Sh = Q.Index; Q.Pos = FVector2D(Gc.X - Sg * 14.f, Py); }
				else { Q.Pos = FVector2D(0.f, Q.Team == 0 ? 20.f : -20.f); }
			}
			M.Human = Sh;
			M.Ball = FPFBall(); M.Ball.Pos = FVector(Gc.X - Sg * 4.f, Py, 0.6f); M.Ball.Vel = FVector(Sg * 20.f, 0.f, 0.5f);
			M.Ball.LastKind = int32(EPFKick::Shot); M.Ball.LastTeam = 0; M.Ball.LastTouch = Sh; M.Ball.bBeatsKeeper = true;
			for (int32 f = 0; f < 600 && M.Ball.Owner < 0 && M.Phase == EPFPhase::Play; ++f) M.Tick(Dt, FPFPadInput());
			++N; if (M.Ball.Owner >= 0) { ++Taken; ByKeeper += M.Ball.Owner == Kp; }
		}
		UE_LOG(LogPF, Display, TEXT("PFSETPIECE post rebound (beats-keeper shot): %d, taken by someone %d (keeper %d)"), N, Taken, ByKeeper);
	}
	// ---- throw-ins: aim at each of the three nearest team-mates (a tap, or held for the far ones); is he the ringed man,
	// and does the throw reach him?
	{
		int32 N = 0, Ringed = 0, Reached = 0, Tapped = 0, Held = 0;
		for (int32 i = 0; i < 12; ++i)
		{
			FPFMatch M; NewMatch(M, 8600 + i, i % 6);
			const FVector2D Spot = M.TestRestartSpot(EPFRestart::ThrowIn, 0, i);
			M.TestRestart(EPFRestart::ThrowIn, 0, Spot);
			Ticks(M, FPFPadInput(), 2.5f);   // team-mates come to offer
			const int32 Th = M.Human;
			TArray<int32> Mates;
			for (const FPFPlayer& Q : M.P) if (Q.Team == 0 && Q.Index != Th && !Q.bKeeper && !Q.bSentOff) Mates.Add(Q.Index);
			Mates.Sort([&M, Th](int32 A, int32 B) { return FVector2D::DistSquared(M.P[A].Pos, M.P[Th].Pos) < FVector2D::DistSquared(M.P[B].Pos, M.P[Th].Pos); });
			Mates.RemoveAll([&M, Th](int32 A) { return FVector2D::Distance(M.P[A].Pos, M.P[Th].Pos) > 25.f; });
			if (!Mates.Num()) { UE_LOG(LogPF, Display, TEXT("PFSETPIECE throw-in: nobody within 25 m")); continue; }
			const int32 Tg = Mates[i % FMath::Min(3, Mates.Num())];
			const FVector2D From(M.P[Th].Pos.X, M.Ball.Pos.Y);
			const float D = FVector2D::Distance(M.P[Tg].Pos, From);
			// a tap for a man within 14 m, else held for his distance on the 9 .. 26 m bar
			const bool bTap = D < 14.f;
			const float Hold = bTap ? 0.05f : FMath::Clamp((D - 9.f) / 17.f, 0.f, 1.f) * FPFMatch::SPFullT;
			FPFPadInput In; In.Move = M.StickFor(M.P[Tg].Pos - From);
			Ticks(M, In, 0.25f);
			FPFPadInput Q = In; Q.bPass = true; Q.bPassHeld = !bTap;
			M.Tick(Dt, Q); Q.bPass = false;
			int32 Pick = M.SPPick;
			for (float t = Dt; t < Hold && M.Phase == EPFPhase::Restart; t += Dt) { M.Tick(Dt, Q); Pick = M.SPPick; }
			Q.bPassHeld = false;
			for (int32 f = 0; f < 6 && M.Phase == EPFPhase::Restart; ++f) { Pick = M.SPPick; M.Tick(Dt, Q); }
			++N; if (bTap) ++Tapped; else ++Held;
			if (Pick == Tg) ++Ringed;
			int32 Got = -1;
			for (int32 f = 0; f < 480; ++f) { M.Tick(Dt, FPFPadInput()); if (M.Ball.Owner >= 0 && M.Ball.Owner != Th) { Got = M.Ball.Owner; break; } }
			if (Got == Pick && Pick >= 0) ++Reached;
			else UE_LOG(LogPF, Display, TEXT("PFSETPIECE throw-in miss: aimed at %d (%.1f m, %s), ringed %d, ball went to %d"), Tg, D, bTap ? TEXT("tap") : TEXT("held"), Pick, Got);
		}
		UE_LOG(LogPF, Display, TEXT("PFSETPIECE throw-in: %d thrown (%d taps, %d held), the aimed man ringed %d, reached the ringed man %d"), N, Tapped, Held, Ringed, Reached);
	}
	// ---- short: a tap of A at a goal kick / corner plays it along the ground to the ringed man
	{
		int32 N = 0, OK = 0;
		for (int32 i = 0; i < 6; ++i)
		{
			FPFMatch M; NewMatch(M, 8800 + i, i);
			const EPFRestart K = (i % 2) ? EPFRestart::Corner : EPFRestart::FreeKick;
			const FVector2D Spot = M.TestRestartSpot(K, 0, i);
			M.TestRestart(K, 0, Spot);
			M.TestTakeShape();
			int32 Tg = -1; float BD = 1e9f;
			for (const FPFPlayer& Q : M.P) if (Q.Team == 0 && Q.Index != M.Human && !Q.bKeeper) { const float Dd = FVector2D::Distance(Q.Pos, Spot); if (Dd > 4.f && Dd < 25.f && Dd < BD) { BD = Dd; Tg = Q.Index; } }
			if (Tg < 0) continue;
			FPFPadInput In; In.Move = M.StickFor(M.P[Tg].Pos - Spot);
			Ticks(M, In, 0.3f);
			const int32 Pick = M.SPPick;
			FPFPadInput Q = In; Q.bPass = true; M.Tick(Dt, Q);
			for (int32 f = 0; f < 480; ++f) { M.Tick(Dt, FPFPadInput()); if (M.Ball.Owner >= 0 && M.Ball.Owner != M.P[Tg].Index && M.Ball.Owner != Pick && M.Phase == EPFPhase::Play && M.Ball.Owner >= 0 && M.P[M.Ball.Owner].Team == 1) break; if (M.Ball.Owner == Pick && Pick >= 0) break; }
			++N; if (Pick >= 0 && M.Ball.Owner == Pick && M.SPLast.bShort) ++OK;
		}
		UE_LOG(LogPF, Display, TEXT("PFSETPIECE short (A tap): %d played, reached the ringed man %d"), N, OK);
	}
}
