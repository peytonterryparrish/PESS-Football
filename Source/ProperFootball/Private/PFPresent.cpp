// Proper Football v0.10 - presentation: how the players look between and around their actions, and the goal show.
//
// Owner (v0.9.9 notes): "after an action ends, they need to rest for half a second or a second, and then have an
// animation getting back up, or a transition animation ... so things don't look extremely choppy"; "the shooting
// animation, I can't even tell if there's a shooting animation"; "celebration cutscenes ... saying GOAL when we score".
// Design: fable_iss_advice_v2 sections 2 (readable kicks), 4 (rests), 5 (keeper with the ball) and 7 (goal show).
//
// Everything here is data-driven so the transition takes the lead is generating slot in by name: a body that has an
// approved animation called in a table below plays it; one that has not falls back to the frames there are today.
#include "PFPresent.h"
#include "PFMatch.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "CanvasItem.h"
#include "UnrealClient.h"
#include "Misc/CommandLine.h"

namespace PFPresentNS
{
	constexpr float PHalfLen = 52.5f, PHalfWid = 34.f;
	inline FVector2D PV2(const FVector& V) { return FVector2D(V.X, V.Y); }
	inline FVector2D PSafeN(FVector2D V, FVector2D Def = FVector2D(1, 0)) { const double L = V.Size(); return L > 1e-4 ? V / L : Def; }
	inline float PDiveFlight(const FPFPlayer& Pl) { return Pl.Clip.ToString().Contains(TEXT("high")) ? 0.45f : 0.29f; }   // = DiveFlightFor (PFMatch.cpp)

	// ---- kicks (section 2): wind-up frames, the contact frame held, then the rest of the take at its own pace.
	// Pre/PreT: the wind-up, seconds each (clip time); they must add up to the clip's event time in PFClips (the ball
	// leaves on contact; checked once at start-up). Hold: the contact frame (the take's Ev). PostDt: each later frame.
	// The follow-through keeps drawing after the clip's lock ends (FPFPlayer::PresClip) while he stands or jogs.
	struct FKickMap { const TCHAR* Clip; const TCHAR* Action; int32 NPre; int32 Pre[4]; float PreT[4]; float Hold; float PostDt; };
	static const FKickMap Kicks[] = {
		{ TEXT("pass"),   TEXT("pass"),   2, { 3, 4 },    { 0.03f, 0.03f },        0.07f, 0.07f },   // tap: 0.06 to the ball, 0.34 visible
		{ TEXT("shot"),   TEXT("shot"),   2, { 3, 4 },    { 0.06f, 0.06f },        0.08f, 0.07f },   // the charge already held f03; 0.60 visible
		{ TEXT("lob"),    TEXT("shot"),   2, { 3, 4 },    { 0.03f, 0.03f },        0.07f, 0.08f },
		{ TEXT("volley"), TEXT("volley"), 0, { 0 },       { 0.f },                 0.08f, 0.08f },   // struck on arrival (no time for a wind-up)
		{ TEXT("header"), TEXT("header"), 3, { 1, 2, 3 }, { 0.03f, 0.03f, 0.04f }, 0.10f, 0.07f },
	};
	const FKickMap* FindKick(FName Clip) { for (const FKickMap& K : Kicks) if (Clip == FName(K.Clip)) return &K; return nullptr; }

	int32 KickFrame(const FKickMap& K, const FPFAnim& A, float T, bool& bDone)
	{
		const int32 Ev = A.Ev >= 0 ? FMath::Min(A.Ev, A.N - 1) : A.N / 2;
		float Acc = 0.f;
		for (int32 i = 0; i < K.NPre; ++i) { Acc += K.PreT[i]; if (T < Acc) return FMath::Clamp(K.Pre[i], 0, Ev); }
		const float U = T - Acc;
		if (U < K.Hold) return Ev;
		const int32 F = Ev + 1 + int32((U - K.Hold) / FMath::Max(0.03f, K.PostDt));
		if (F >= A.N) { bDone = true; return A.N - 1; }
		return F;
	}

	// ---- going to ground and getting up (section 4). Start..Land plays over the flight; Land..GetUp-1 is the time down
	// (the rest); GetUp..N-1 is the get-up; then a settle pose. Frame numbers are per approved take.
	struct FGround { const TCHAR* Anim; int32 Start, Land, GetUp; };
	static const FGround Grounds[] = {
		{ TEXT("gk_dive_low"), 1, 8, 12 }, { TEXT("gk_dive_high_00"), 3, 10, 13 }, { TEXT("gk_dive_high_07"), 3, 9, 13 },
		{ TEXT("gk_dive_high_08"), 2, 10, 13 }, { TEXT("gk_dive_high_12"), 2, 10, 13 }, { TEXT("fall"), 0, 4, 8 } };
	const FGround* FindGround(FName Anim) { for (const FGround& G : Grounds) if (Anim == FName(G.Anim)) return &G; return nullptr; }
	// the rests: seconds down, getting up (the take's last frames), and settling (a stand pose) - Fable's table, owner "about 1 s"
	struct FRest { float Lie, GetUp, Stand; };
	static const FRest DiveMissed = { 0.70f, 0.40f, 0.15f }, DiveParried = { 0.35f, 0.25f, 0.15f }, DiveHeld = { 0.60f, 0.25f, 0.20f }, FallRest = { 0.80f, 0.40f, 0.10f };
	static constexpr float FallFlight = 0.25f;
	static constexpr float SlideRestAt = 0.44f, SlideRest = 0.25f;   // a missed slide lies flat this much longer (clip time)

	// ---- transitions between locomotion and actions: a dedicated approved take first (when the lead's round lands), else
	// the still that exists today
	struct FTrans { const TCHAR* When; const TCHAR* Anims[2]; const TCHAR* Still; float T; };
	static const FTrans Trans[] = {
		{ TEXT("stop"),  { TEXT("trans_stop"), TEXT("stop") }, TEXT("turn_plant"), 0.12f },   // a fast run brakes to a stand
		{ TEXT("carry"), { TEXT("gk_carry"), nullptr },        TEXT("gk_hold_ball"), 0.f },   // a keeper walking with the ball
	};
	const FTrans& FindTrans(const TCHAR* When) { for (const FTrans& T : Trans) if (FCString::Strcmp(T.When, When) == 0) return T; return Trans[0]; }

	int32 Spread(int32 F0, int32 F1, float U) { return F1 <= F0 ? F0 : FMath::Clamp(F0 + int32(FMath::Clamp(U, 0.f, 0.999f) * (F1 - F0 + 1)), F0, F1); }

	void CheckTables()
	{
		static bool bDone = false;
		if (bDone) return;
		bDone = true;
		for (const FKickMap& K : Kicks)
		{
			const FPFClip& C = PFClips::Get(K.Clip);
			float Ev = 0.f; for (int32 i = 0; i < C.EventIndex && i < C.Times.Num(); ++i) Ev += C.Times[i];
			float Pre = 0.f; for (int32 i = 0; i < K.NPre; ++i) Pre += K.PreT[i];
			if (FMath::Abs(Ev - Pre) > 0.005f) UE_LOG(LogPF, Warning, TEXT("PF present: the %s wind-up (%.3f s) does not meet the clip's contact (%.3f s)"), K.Clip, Pre, Ev);
		}
	}
}
using namespace PFPresentNS;

// ------------------------------------------------------------------------------------------------ poses
FName FPFMatch::PresentPose(const FPFPlayer& Pl, const FPFSpriteSet& S, bool& bHandled) const
{
	bHandled = false;
	CheckTables();
	const float Spd = Pl.Vel.Size();
	auto Frame = [&](FName Anim, int32 I) { bHandled = true; return FPFSpriteSet::AnimFrame(Anim, I); };
	// the goal show: run off, then the approved celebration takes (each man his own); the beaten side's heads go down
	if (Pl.Act == EPFAct::Celebrate)
	{
		if (Pl.Clip == TEXT("celeb_run") || (Pl.Clip == TEXT("celeb_wait") && Spd > 0.5f))
			if (const FPFAnim* A = S.Anims.Find(Spd > 6.5f && S.Anims.Contains(TEXT("sprint")) ? TEXT("sprint") : TEXT("run")))
				return Frame(Spd > 6.5f && S.Anims.Contains(TEXT("sprint")) ? FName(TEXT("sprint")) : FName(TEXT("run")), FMath::Min(A->N - 1, int32(FMath::Frac(Pl.Gait) * A->N)));
		if (Pl.Clip == TEXT("celeb_wait"))
			if (const FPFAnim* A = S.Anims.Find(TEXT("idle"))) return Frame(TEXT("idle"), int32(FMath::Max(0.f, Pl.ActT + 2.f) / FMath::Max(0.05f, A->Dt)) % A->N);
		if (Pl.Clip == TEXT("celebrate"))
		{
			FName Take;
			if (const FPFAnim* A = S.FindAnim(TEXT("celebrate"), Pl.Index + GoalCeleb, &Take))
			{
				// once through, then the last third again and again (the arms stay up; no drop back to a stand mid-party)
				const int32 I = int32(FMath::Max(0.f, Pl.ActT) / FMath::Max(0.05f, A->Dt));
				const int32 Loop0 = A->N * 2 / 3;
				return Frame(Take, I < A->N ? I : Loop0 + (I - A->N) % FMath::Max(1, A->N - Loop0));
			}
		}
		return NAME_None;
	}
	// kicks: wind-up, contact held, follow-through at the take's pace
	if (Pl.Act == EPFAct::Clip)
		if (const FKickMap* K = FindKick(Pl.Clip))
		{
			FName Take;
			if (const FPFAnim* A = S.FindAnim(K->Action, Pl.Index, &Take))
			{
				bool bDone = false;
				return Frame(Take, KickFrame(*K, *A, Pl.ActT * Pl.A.AnimRate, bDone));
			}
		}
	// a keeper's dive and an outfielder's fall: flight, down, get-up, settle
	if ((Pl.Act == EPFAct::Dive && !Pl.DiveDir.IsNearlyZero()) || Pl.Act == EPFAct::Fall)
	{
		const bool bDive = Pl.Act == EPFAct::Dive;
		static const TMap<FName, FName> GroundAnim = { { TEXT("gk_dive_low_R"), TEXT("gk_dive_low") }, { TEXT("gk_dive_low_L"), TEXT("gk_dive_low") },
			{ TEXT("gk_dive_high_R"), TEXT("gk_dive_high") }, { TEXT("gk_dive_high_L"), TEXT("gk_dive_high") }, { TEXT("fall"), TEXT("fall") } };
		if (const FName* AN = GroundAnim.Find(Pl.Clip))
		{
			const int32 Seed = bDive ? int32(FMath::Abs(Pl.DiveDir.X * 977.f + Pl.DiveDir.Y * 613.f)) + Pl.Index : Pl.Index;   // = PoseFor's take
			FName Take;
			const FPFAnim* A = S.FindAnim(*AN, Seed, &Take);
			const FGround* G = A ? FindGround(Take) : nullptr;
			if (A && G)
			{
				const bool bHeld = bDive && Ball.Owner == Pl.Index && bKeeperHands;
				const FRest& R = !bDive ? FallRest : bHeld ? DiveHeld : Pl.bRestAdded ? DiveParried : DiveMissed;
				const float T = Pl.ActT, Len = Pl.ActLen, Fl = bDive ? PDiveFlight(Pl) : FallFlight;
				const float GetUp0 = Len - R.GetUp - R.Stand, Stand0 = Len - R.Stand;
				if (T < Fl) return Frame(Take, Spread(G->Start, G->Land, T / Fl));
				if (T < GetUp0) return Frame(Take, Spread(G->Land, G->GetUp - 1, (T - Fl) / FMath::Max(0.05f, GetUp0 - Fl)));
				if (T < Stand0) return Frame(Take, Spread(G->GetUp, A->N - 1, (T - GetUp0) / FMath::Max(0.05f, R.GetUp)));
				// settle: a keeper with the ball hugs it (the catch take's last frame), else his ready stance; a fallen man stands
				if (bHeld) if (const FPFAnim* C = S.Anims.Find(TEXT("gk_catch"))) return Frame(TEXT("gk_catch"), C->N - 1);
				if (bDive && S.Anims.Contains(TEXT("gk_ready"))) return Frame(TEXT("gk_ready"), 0);
				if (S.Anims.Contains(TEXT("idle"))) return Frame(TEXT("idle"), 0);
			}
		}
	}
	// a missed slide lies flat a little longer (the clip's time is held at the end of the flat slide)
	if (Pl.Act == EPFAct::Slide && Pl.bRestAdded)
	{
		const FPFClip& C = PFClips::Get(Pl.Clip);
		float Tc = Pl.ActT * Pl.A.AnimRate;
		Tc = Tc < SlideRestAt ? Tc : Tc < SlideRestAt + SlideRest ? SlideRestAt - 0.01f : Tc - SlideRest;
		bHandled = true;
		return C.At(Tc);
	}
	if (Pl.Act != EPFAct::None) return NAME_None;
	// ---- between actions
	// a keeper with the ball in his hands: hugged at the chest the first moments, then the hold; walking, the carry
	if (Pl.bKeeper && Ball.Owner == Pl.Index && bKeeperHands)
	{
		const FTrans& Tr = FindTrans(TEXT("carry"));
		if (Spd > 0.35f)
		{
			for (const TCHAR* An : Tr.Anims) if (An) if (const FPFAnim* A = S.Anims.Find(An)) return Frame(An, FMath::Min(A->N - 1, int32(FMath::Frac(Pl.Gait) * A->N)));
			bHandled = true; return FName(Tr.Still);
		}
		if (HoldT < 0.6f) if (const FPFAnim* C = S.Anims.Find(TEXT("gk_catch"))) return Frame(TEXT("gk_catch"), C->N - 1);
		bHandled = true; return FName(Tr.Still);
	}
	// the pad's pass being held: the leg winds up with the bar (f02 after 0.08 s, f03 after 0.20 s)
	if (bPassCharging && ChargeOwner == Pl.Index && Ball.Owner == Pl.Index && Spd < 3.f && PassHoldT >= 0.08f)
		if (const FPFAnim* A = S.Anims.Find(TEXT("pass")))
			return Frame(TEXT("pass"), FMath::Min(A->N - 1, PassHoldT >= 0.20f ? 3 : 2));
	// a kick's follow-through finishing while he stands or jogs
	if (!Pl.PresClip.IsNone() && Spd < 2.5f && !Pl.bCharging)
		if (const FKickMap* K = FindKick(Pl.PresClip))
		{
			FName Take;
			if (const FPFAnim* A = S.FindAnim(K->Action, Pl.Index, &Take))
			{
				bool bDone = false;
				const int32 F = KickFrame(*K, *A, Pl.PresT, bDone);
				if (!bDone) return Frame(Take, F);
			}
		}
	// a fast run braking to a stand: the plant
	if (Pl.PlantT > 0.f && Spd < 2.2f)
	{
		const FTrans& Tr = FindTrans(TEXT("stop"));
		for (const TCHAR* An : Tr.Anims)
			if (An) if (const FPFAnim* A = S.Anims.Find(An)) return Frame(An, Spread(0, A->N - 1, 1.f - Pl.PlantT / FMath::Max(0.01f, Tr.T)));
		if (S.Has(Tr.Still)) { bHandled = true; return FName(Tr.Still); }
	}
	return NAME_None;
}

// ------------------------------------------------------------------------------------------------ rests
void FPFMatch::OnActEnd(FPFPlayer& Pl)
{
	// the follow-through keeps drawing from where the clip's lock ended
	if (Pl.Act == EPFAct::Clip) { Pl.PresClip = Pl.Clip; Pl.PresT = Pl.ActT * Pl.A.AnimRate; }
	else Pl.PresClip = NAME_None;
	Pl.bRestAdded = false;
}

void FPFMatch::OnDiveTouch(FPFPlayer& K, bool bHeld)
{
	// held: down on the ball 0.60, kneel 0.25, stand hugging it 0.20; parried: down 0.35, scramble 0.25, up 0.15
	// (missed: the clip's own length, 0.70 down and a 0.55 get-up after the flight)
	const FRest& R = bHeld ? DiveHeld : DiveParried;
	K.ActLen = FMath::Max(K.ActT, PDiveFlight(K)) + R.Lie + R.GetUp + R.Stand;
	K.bRestAdded = !bHeld;
}

// ------------------------------------------------------------------------------------------------ per drawn frame
void FPFMatch::PresentTick(float Dt)
{
	const float Ds = Dt * GameSpeed;
	for (FPFPlayer& Pl : P)
	{
		const float S = Pl.Vel.Size();
		Pl.PresT += Ds * Pl.A.AnimRate;
		Pl.PlantT = FMath::Max(0.f, Pl.PlantT - Ds);
		if (Pl.Act == EPFAct::None && Pl.PrevSpeed > 5.f && S < 1.2f && Ball.Owner != Pl.Index) Pl.PlantT = FindTrans(TEXT("stop")).T;
		if (Pl.Act != EPFAct::None) Pl.PlantT = 0.f;
		Pl.PrevSpeed = S;
	}
	if (Phase != EPFPhase::Goal) { GoalT = -1.f; ReplayIdx = -1; FadeA = FMath::Max(0.f, FadeA - Dt * 4.f); }
	// test hook: -pfkeeperdrill=S every S seconds of open play a shot from the edge of the box at a corner of the home goal
	// (dives, catches, parries and goals on demand for -pfpresshots captures)
	{
		static float Every = -1.f, Acc = 0.f; static int32 N = 0;
		if (Every < 0.f) { Every = 0.f; FParse::Value(FCommandLine::Get(), TEXT("pfkeeperdrill="), Every); }
		if (Every > 0.f && Phase == EPFPhase::Play)
		{
			Acc += Ds;
			bool bBusy = false; for (const FPFPlayer& K : P) if (K.bKeeper && K.Act != EPFAct::None) bBusy = true;
			if (Acc > Every && !bBusy && !KeeperHolding())
			{
				Acc = 0.f; ++N;
				int32 Shooter = -1; for (const FPFPlayer& Q : P) if (Q.Team == 1 && !Q.bKeeper && !Q.bSentOff) { Shooter = Q.Index; break; }
				const float Gx = GoalCentre(0, false).X, In = Gx < 0.f ? 1.f : -1.f, Sy = (N % 2 ? 1.f : -1.f) * (8.f + (N % 3) * 3.f);
				const FVector From(Gx + 17.f * In, Sy, 0.25f);
				const FVector To(Gx, (N % 2 ? -1.f : 1.f) * (1.6f + (N % 4) * 0.55f), (N % 3 == 0) ? 1.9f : 0.4f);
				const float Spd = 17.f + (N % 3) * 3.f, Tt = FVector::Dist(From, To) / Spd;
				FVector V = (To - From) / Tt; V.Z = (To.Z - From.Z) / Tt + 0.5f * 13.f * Tt;
				if (P.IsValidIndex(Shooter)) { P[Shooter].Pos = FVector2D(From.X + 0.6f * In, From.Y); P[Shooter].Vel = FVector2D::ZeroVector; }
				Ball.Owner = -1; Ball.Pos = From; Ball.Vel = V; Ball.LastKind = int32(EPFKick::Shot); Ball.LastTeam = 1; Ball.LastTouch = Shooter; Ball.bBeatsKeeper = N % 2 == 0; Ball.Guide = -1;   // every other one beats him (a missed dive, a goal)
				CamCenter = Proj.ToAtlas(FVector(Gx + 8.f * In, 0.f, 0.f));
			}
		}
	}
	// test hook: -pfpresshots=N captures N keeper dives (a frame every 0.1 s from take-off to standing, then two of him
	// walking / holding) and N goal shows (letters, score, run, celebration, replay) as ps_dive_* / ps_goal_* png
	{
		static int32 Want = -1, ND = 0, NG = 0, DiveK = -1, Fr = 0; static float FrT = 0.f, GoalShotT = -1.f;
		if (Want < 0) { Want = 0; FParse::Value(FCommandLine::Get(), TEXT("pfpresshots="), Want); }
		if (Want > 0)
		{
			FrT -= Dt;
			if (DiveK < 0 && ND < Want && Phase == EPFPhase::Play)
				for (const FPFPlayer& K : P) if (K.bKeeper && K.Act == EPFAct::Dive && !K.DiveDir.IsNearlyZero() && K.ActT < 0.1f) { DiveK = K.Index; Fr = 0; FrT = 0.f; break; }
			if (DiveK >= 0 && FrT <= 0.f)
			{
				const FPFPlayer& K = P[DiveK];
				FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("ps_dive_%02d_%02d_%s.png"), ND, Fr, (Ball.Owner == DiveK && bKeeperHands) ? TEXT("held") : TEXT("free")), false, false);
				++Fr; FrT = K.Act == EPFAct::Dive ? 0.1f : 0.35f;
				if (K.Act != EPFAct::Dive && (Fr > 24 || !(Ball.Owner == DiveK && bKeeperHands) || K.Act == EPFAct::Clip)) { DiveK = -1; ++ND; }
				if (Fr > 40) { DiveK = -1; ++ND; }
			}
			// kicks: a burst every 0.04 s from the first frame of a pass or shot clip (ps_kick_<n>_<k>_<clip>)
			{
				static int32 NK = 0, KK = -1, KF = 0; static float KT = 0.f; static FName KC;
				KT -= Dt;
				if (KK < 0 && NK < Want * 2 && Phase == EPFPhase::Play && DiveK < 0)
					for (const FPFPlayer& Q : P)
						if (Q.Act == EPFAct::Clip && Q.ActT < 0.03f && (Q.Clip == TEXT("shot") || Q.Clip == TEXT("pass")) && (NK % 2 == 0) == (Q.Clip == TEXT("shot"))) { KK = Q.Index; KF = 0; KT = 0.f; KC = Q.Clip; break; }
				if (KK >= 0 && KT <= 0.f)
				{
					FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("ps_kick_%02d_%02d_%s.png"), NK, KF, *KC.ToString()), false, false);
					KT = 0.04f;
					if (++KF >= 18) { KK = -1; ++NK; }
				}
			}
			if (Phase == EPFPhase::Goal && NG < Want)
			{
				static const float At[] = { 0.35f, 0.6f, 1.0f, 1.6f, 2.0f, 2.7f, 3.4f, 4.5f, 5.5f, 6.5f, 7.6f, 8.6f, 9.5f };
				for (int32 k = 0; k < UE_ARRAY_COUNT(At); ++k)
					if (GoalShotT < At[k] && PhaseT >= At[k]) FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("ps_goal_%02d_%02d.png"), NG, k), false, false);
				GoalShotT = PhaseT;
			}
			else if (GoalShotT >= 0.f) { GoalShotT = -1.f; ++NG; }
		}
	}
	// the replay recorder: 40 snapshots a second of open play (and the first moments after the ball goes in)
	static const float RecHz = 40.f;
	static const int32 Cap = 170;
	ReplayClock += Ds;
	const bool bRec = Phase == EPFPhase::Play || (Phase == EPFPhase::Goal && PhaseT < 0.6f);
	if (!bRec) { if (Phase != EPFPhase::Goal) ReplayN = 0; return; }
	if (HeadScreen.Num() == 0) return;   // never drawn (sim and feel tests): nothing to replay
	RecAcc -= Ds;
	if (RecAcc > 0.f) return;
	RecAcc = 1.f / RecHz;
	if (Replay.Num() < Cap) Replay.SetNum(Cap);
	FSnap& Sn = Replay[ReplayHead];
	Sn.P = P; Sn.Ball = Ball; Sn.bKeeperHands = bKeeperHands; Sn.T = ReplayClock;
	ReplayHead = (ReplayHead + 1) % Cap; ReplayN = FMath::Min(ReplayN + 1, Cap);
}

bool FPFMatch::DrawReplaySwap(bool bIn)
{
	if (bIn)
	{
		if (bInReplayDraw || !Replay.IsValidIndex(ReplayIdx)) return false;
		bInReplayDraw = true;
	}
	else bInReplayDraw = false;
	FSnap& Sn = Replay[ReplayIdx];
	Swap(P, Sn.P); Swap(Ball, Sn.Ball); Swap(bKeeperHands, Sn.bKeeperHands);
	return true;
}

// ------------------------------------------------------------------------------------------------ the goal show
void FPFMatch::GoalStart(int32 ScoringTeam, bool bOwnGoal)
{
	GoalT = 0.f; GoalTeam = ScoringTeam;
	GoalScoreWas[0] = Score[0]; GoalScoreWas[1] = Score[1]; GoalScoreWas[ScoringTeam]--;
	GoalScorer = (!bOwnGoal && P.IsValidIndex(Ball.LastTouch) && P[Ball.LastTouch].Team == ScoringTeam) ? Ball.LastTouch : -1;
	GoalMinute = FMath::Max(1, FMath::CeilToInt(MatchClock));
	FString Sur = GoalScorer >= 0 ? P[GoalScorer].Setup.Name : FString(TEXT("OWN GOAL"));
	{ FString L, R; if (GoalScorer >= 0 && Sur.Split(TEXT(" "), &L, &R, ESearchCase::IgnoreCase, ESearchDir::FromEnd)) Sur = R; }
	GoalWho = FString::Printf(TEXT("%d'  %s"), GoalMinute, *Sur.ToUpper());
	GoalCeleb = int32(Score[0] * 7 + Score[1] * 13 + GoalMinute * 3);
	HitStop = FMath::Max(HitStop, 0.12f);   // real seconds: the net bulges, everything holds a beat
	bGoalSkippable = false; ReplayIdx = -1; FadeA = 0.f;
	for (FPFPlayer& Pl : P) { Pl.PresClip = NAME_None; Pl.PlantT = 0.f; }
	// the scorer runs off toward the near corner of the pitch (12 m or so), arms up, and celebrates there
	if (GoalScorer >= 0)
	{
		FPFPlayer& S = P[GoalScorer];
		const FVector2D Corner(FMath::Sign(S.Pos.X) * (PHalfLen - 2.f), -(PHalfWid - 2.f));   // the touchline nearer the camera
		const FVector2D Dir = PSafeN(Corner - S.Pos, FVector2D(0, -1));
		GoalRunTo = S.Pos + Dir * FMath::Min(12.f, (float)FVector2D::Distance(Corner, S.Pos));
		GoalRunTo.X = FMath::Clamp(GoalRunTo.X, -PHalfLen + 1.f, PHalfLen - 1.f); GoalRunTo.Y = FMath::Clamp(GoalRunTo.Y, -PHalfWid + 1.f, PHalfWid - 1.f);
		S.Act = EPFAct::Celebrate; S.Clip = TEXT("celeb_run"); S.ActT = 0.f; S.ActLen = 99.f; S.PendingKick = EPFKick::None; S.bCharging = false;
		// the four nearest team-mates chase him and gather round
		TArray<int32> Mates;
		for (const FPFPlayer& Q : P) if (Q.Team == ScoringTeam && Q.Index != GoalScorer && !Q.bKeeper && !Q.bSentOff) Mates.Add(Q.Index);
		Mates.Sort([&](int32 A, int32 B) { return FVector2D::DistSquared(P[A].Pos, S.Pos) < FVector2D::DistSquared(P[B].Pos, S.Pos); });
		for (int32 k = 0; k < Mates.Num() && k < 4; ++k)
		{
			FPFPlayer& Q = P[Mates[k]];
			Q.Act = EPFAct::Celebrate; Q.Clip = TEXT("celeb_wait"); Q.ActT = -(0.35f + 0.2f * k); Q.ActLen = 99.f; Q.PendingKick = EPFKick::None; Q.bCharging = false;
		}
	}
	// the beaten side: the keeper and the two nearest defenders hold their heads
	TArray<int32> Beat;
	for (const FPFPlayer& Q : P) if (Q.Team != ScoringTeam && !Q.bSentOff && !Q.bKeeper) Beat.Add(Q.Index);
	Beat.Sort([&](int32 A, int32 B) { return FVector2D::DistSquared(P[A].Pos, PV2(Ball.Pos)) < FVector2D::DistSquared(P[B].Pos, PV2(Ball.Pos)); });
	for (FPFPlayer& Q : P)
		if (Q.Team != ScoringTeam && !Q.bSentOff && Q.Act != EPFAct::Dive && Q.Act != EPFAct::Fall && (Q.bKeeper || (Beat.Num() > 0 && Q.Index == Beat[0]) || (Beat.Num() > 1 && Q.Index == Beat[1])))   // a beaten keeper stays down first
		{ Q.Act = EPFAct::Celebrate; Q.Clip = TEXT("dejected"); Q.ActT = 0.f; Q.ActLen = 99.f; Q.Vel = FVector2D::ZeroVector; Q.PendingKick = EPFKick::None; }
}

void FPFMatch::GoalScene(float Dt, const FPFPadInput& Pad)
{
	const float T0 = GoalT;
	GoalT = PhaseT;
	Highlight = FMath::Max(0.f, Highlight - Dt);
	if (PhaseT > 1.5f) bGoalSkippable = true;
	// any button skips to the kick-off (after 1.5 s, so the press that scored can't)
	if (bGoalSkippable && PhaseT < GoalFade0 && (Pad.bPass || Pad.bShootPressed || Pad.bLob || Pad.bThrough || Pad.bSwitch)) { PhaseT = GoalFade0; GoalT = PhaseT; ReplayIdx = -1; }
	// each letter lands with a little screen shake
	for (int32 i = 0; i < 4; ++i) { const float L = GoalLetters0 + 0.12f * i + 0.15f; if (T0 < L && GoalT >= L) { ShakeT = 0.f; ShakeAmp = 4.f; } }
	if (T0 < GoalScore0 + 0.3f && GoalT >= GoalScore0 + 0.3f) Sfx.Add(TEXT("crowd_goal"));   // the roar swells as the score ticks up
	BallPhysics(Dt);
	const FVector2D ScorerPos = P.IsValidIndex(GoalScorer) ? P[GoalScorer].Pos : PV2(Ball.Pos);
	for (FPFPlayer& Pl : P)
	{
		if (Pl.bSentOff) continue;
		if (Pl.Act != EPFAct::Celebrate)
		{
			Pl.Vel *= FMath::Exp(-Dt * 6.f); Pl.Pos += Pl.Vel * Dt;
			if (Pl.Act != EPFAct::None)
			{
				Pl.ActT += Dt;
				if (Pl.ActT > Pl.ActLen)
				{
					OnActEnd(Pl); Pl.Act = EPFAct::None;
					// the beaten keeper, up from his dive, holds his head
					if (Pl.bKeeper && Pl.Team != GoalTeam) { Pl.Act = EPFAct::Celebrate; Pl.Clip = TEXT("dejected"); Pl.ActT = 0.f; Pl.ActLen = 99.f; }
				}
			}
			continue;
		}
		Pl.ActT += Dt;
		const bool bScorer = Pl.Index == GoalScorer;
		if (Pl.Clip == TEXT("celeb_wait") && Pl.ActT >= 0.f) Pl.Clip = TEXT("celeb_run");
		if (Pl.Clip == TEXT("celeb_run"))
		{
			FVector2D To = GoalRunTo;
			if (!bScorer)
			{
				const float Ang = Pl.Index * 2.4f;   // each mate his own place round the scorer
				To = ScorerPos + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang) * 0.6f) * 1.1f;
			}
			const FVector2D D = To - Pl.Pos; const float Dist = D.Size();
			const float Want = (bScorer ? 6.2f : 7.f) * FMath::Clamp(Dist / 2.f, 0.25f, 1.f);
			if ((bScorer && (Dist < 0.5f || Pl.ActT > 2.8f)) || (!bScorer && Dist < 0.45f))
			{
				Pl.Clip = TEXT("celebrate"); Pl.ActT = bScorer ? 0.f : 0.1f * (Pl.Index % 5);
				if (!bScorer) Pl.Facing = PSafeN(ScorerPos - Pl.Pos, Pl.Facing);
				else Pl.Facing = FVector2D(0, -1);   // to the camera
				continue;
			}
			const FVector2D Dir = PSafeN(D);
			const float Cur = Pl.Vel.Size();
			const float Sp = FMath::Min(Want, Cur + 14.f * Dt);
			Pl.Vel = Dir * Sp; Pl.Pos += Pl.Vel * Dt; Pl.Facing = Dir;
			Pl.Gait += (Sp < 2.2f ? 1.2f : FMath::Clamp(Sp / 3.f, 1.3f, 2.3f)) * Dt;
		}
		else
		{
			Pl.Vel *= FMath::Exp(-Dt * 8.f); Pl.Pos += Pl.Vel * Dt;
			if (!bScorer && Pl.Team == GoalTeam && P.IsValidIndex(GoalScorer)) { Pl.Facing = PSafeN(ScorerPos - Pl.Pos, Pl.Facing); }
		}
	}
	// the replay: the last 2.4 s of play at 0.7x, from the recorder (drawn only; the sim never reads it)
	ReplayIdx = -1;
	if (PhaseT >= GoalReplay0 && PhaseT < GoalFade0 && ReplayN > 4 && Replay.Num())
	{
		const int32 Cap = Replay.Num();
		auto At = [&](int32 i) -> FSnap& { return Replay[((ReplayHead - ReplayN + i) % Cap + Cap) % Cap]; };
		const float TEnd = At(ReplayN - 1).T, TStart = FMath::Max(At(0).T, TEnd - 2.4f);
		const float Clock = TStart + (PhaseT - GoalReplay0) * 0.7f;
		if (Clock > TEnd) PhaseT = GoalFade0;
		else
		{
			int32 Best = 0;
			for (int32 i = 0; i < ReplayN; ++i) if (At(i).T <= Clock) Best = i;
			ReplayIdx = ((ReplayHead - ReplayN + Best) % Cap + Cap) % Cap;
		}
	}
	FadeA = PhaseT >= GoalFade0 ? FMath::Clamp((PhaseT - GoalFade0) / (GoalEnd - GoalFade0), 0.f, 1.f) : 0.f;
	if (PhaseT >= GoalEnd)
	{
		GoalT = -1.f; ReplayIdx = -1; ReplayN = 0; FadeA = 1.f;
		for (FPFPlayer& Pl : P) if (Pl.Act == EPFAct::Celebrate) { Pl.Act = EPFAct::None; Pl.Vel = FVector2D::ZeroVector; }
		PlaceForKickoff(KickoffTeam);
		CamCenter = Proj.ToAtlas(FVector(Ball.Pos.X, Ball.Pos.Y, 0.f));
	}
}

// ------------------------------------------------------------------------------------------------ the HUD's part
namespace PFPresentNS
{
	// 5 x 7 chunky font (our own)
	struct FGlyph { TCHAR Ch; const char* Rows[7]; };
	static const FGlyph Glyphs[] = {
		{ 'G', { ".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".###." } },
		{ 'O', { ".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###." } },
		{ 'A', { ".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#" } },
		{ 'L', { "#....", "#....", "#....", "#....", "#....", "#....", "#####" } },
		{ 'R', { "####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#" } },
		{ 'E', { "#####", "#....", "#....", "####.", "#....", "#....", "#####" } },
		{ 'P', { "####.", "#...#", "#...#", "####.", "#....", "#....", "#...." } },
		{ 'Y', { "#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#.." } },
		{ '!', { ".##..", ".##..", ".##..", ".##..", ".##..", ".....", ".##.." } },
		{ '-', { ".....", ".....", ".....", "####.", ".....", ".....", "....." } },
		{ '0', { ".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###." } },
		{ '1', { "..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###." } },
		{ '2', { ".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####" } },
		{ '3', { "#####", "...#.", "..#..", "...#.", "....#", "#...#", ".###." } },
		{ '4', { "...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#." } },
		{ '5', { "#####", "#....", "####.", "....#", "....#", "#...#", ".###." } },
		{ '6', { "..##.", ".#...", "#....", "####.", "#...#", "#...#", ".###." } },
		{ '7', { "#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..." } },
		{ '8', { ".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###." } },
		{ '9', { ".###.", "#...#", "#...#", ".####", "....#", "...#.", ".##.." } } };
	const FGlyph* FindGlyph(TCHAR Ch) { for (const FGlyph& G : Glyphs) if (G.Ch == Ch) return &G; return nullptr; }

	void PRect(UCanvas* C, float X, float Y, float W, float H, FLinearColor Col)
	{
		FCanvasTileItem It(FVector2D(FMath::RoundToFloat(X), FMath::RoundToFloat(Y)), GWhiteTexture, FVector2D(FMath::Max(1.f, FMath::RoundToFloat(W)), FMath::Max(1.f, FMath::RoundToFloat(H))), Col);
		It.BlendMode = SE_BLEND_Translucent;
		C->DrawItem(It);
	}

	// one glyph, top-left at X, Y; layers: dark shadow, the orange side (a block 3D look), the face
	void Glyph(UCanvas* C, const FGlyph& G, float X, float Y, float Px, float Sx, float Sy, FLinearColor Face, FLinearColor Side, FLinearColor Shadow)
	{
		const float Cw = Px * Sx, Ch = Px * Sy;
		const float Dd = FMath::Max(2.f, Px * 0.3f);
		for (int32 Layer = 0; Layer < 3; ++Layer)
		{
			const FLinearColor Col = Layer == 0 ? Shadow : Layer == 1 ? Side : Face;
			const FVector2D Off = Layer == 0 ? FVector2D(Dd * 2.f, Dd * 2.f) : Layer == 1 ? FVector2D(Dd, Dd) : FVector2D::ZeroVector;
			if (Col.A <= 0.f) continue;
			for (int32 r = 0; r < 7; ++r)
				for (int32 c = 0; c < 5; ++c)
					if (G.Rows[r][c] == '#') PRect(C, X + c * Cw + Off.X, Y + r * Ch + Off.Y, Cw + 0.5f, Ch + 0.5f, Col);
		}
	}
}

float PFPresent::PixelTextWidth(const FString& S, float Px)
{
	float W = 0.f;
	for (TCHAR Ch : S) W += (Ch == ' ' ? 3.f : 6.f) * Px;
	return FMath::Max(0.f, W - Px);
}

void PFPresent::PixelText(UCanvas* C, const FString& S, float CX, float Y, float Px, FLinearColor Face, FLinearColor Side, FLinearColor Shadow, float ScaleX, float ScaleY)
{
	float X = CX - PixelTextWidth(S, Px) * ScaleX * 0.5f;
	for (TCHAR Ch : S)
	{
		if (Ch == ' ') { X += 3.f * Px * ScaleX; continue; }
		if (const FGlyph* G = FindGlyph(FChar::ToUpper(Ch))) Glyph(C, *G, X, Y, Px, ScaleX, ScaleY, Face, Side, Shadow);
		X += 6.f * Px * ScaleX;
	}
}

bool PFPresent::DrawGoalOverlay(UCanvas* C, const FPFMatch& M, float U, float& OutNameY)
{
	if (!C) return false;
	const float W = C->ClipX, H = C->ClipY, T = M.GoalT;
	const FLinearColor Gold(1.f, 0.82f, 0.16f), Orange(0.92f, 0.42f, 0.06f), Navy(0.03f, 0.06f, 0.3f, 0.9f), White(1.f, 1.f, 1.f), Blue(0.12f, 0.3f, 0.85f);
	bool bCard = false;
	if (M.InReplay())
	{
		// REPLAY, top-left, blinking at 2 Hz
		if (FMath::Fmod(T, 0.5f) < 0.32f) PixelText(C, TEXT("REPLAY"), 40 * U + PixelTextWidth(TEXT("REPLAY"), 7 * U) * 0.5f, 40 * U, 7 * U, FLinearColor(0.85f, 0.85f, 0.88f), FLinearColor(0.35f, 0.35f, 0.4f), FLinearColor(0, 0, 0, 0.6f));
	}
	else if (T >= 0.f)
	{
		// G-O-A-L: each letter drops in from 3x size 0.12 s apart, a squash as it lands; it flashes white twice and flies off
		static const TCHAR* L[] = { TEXT("G"), TEXT("O"), TEXT("A"), TEXT("L") };
		const float Px = FMath::RoundToFloat(H * 0.22f / 7.f);
		const float Step = 6.f * Px, Total = 4.f * Step - Px;
		const float Y0 = H * 0.40f - 3.5f * Px;
		if (T >= FPFMatch::GoalLetters0 && T < FPFMatch::GoalLettersEnd)
		{
			const bool bFlash = (T > 1.90f && T < 1.97f) || (T > 2.04f && T < 2.11f);
			const float Fly = T > 2.11f ? FMath::Square((T - 2.11f) / (FPFMatch::GoalLettersEnd - 2.11f)) * (W * 0.75f) : 0.f;
			for (int32 i = 0; i < 4; ++i)
			{
				const float A = (T - FPFMatch::GoalLetters0 - 0.12f * i) / 0.15f;
				if (A < 0.f) continue;
				float Sc = A < 1.f ? 3.f - 2.f * FMath::Sin(A * HALF_PI) : 1.f;
				float Sx = Sc, Sy = Sc;
				if (A >= 1.f && A < 1.35f) { Sx = 1.15f; Sy = 0.85f; }   // squash on landing
				const float Cx = W * 0.5f - Total * 0.5f + i * Step + 2.5f * Px + Fly * (1.f + 0.25f * i);
				const float Cy = Y0 + 3.5f * Px;
				const FLinearColor Face = bFlash ? White : Gold;
				const float Al = FMath::Clamp(A * 1.5f, 0.f, 1.f);
				PixelText(C, L[i], Cx, Cy - 3.5f * Px * Sy, Px, FLinearColor(Face.R, Face.G, Face.B, Al), FLinearColor(Orange.R, Orange.G, Orange.B, Al), FLinearColor(Navy.R, Navy.G, Navy.B, Navy.A * Al), Sx, Sy);
			}
		}
		// the score, the scorers' digit ticking up with a pop
		if (T >= FPFMatch::GoalScore0 && T < FPFMatch::GoalScoreEnd && M.GoalTeam >= 0)
		{
			const float SPx = FMath::RoundToFloat(H * 0.16f / 7.f);
			const bool bNew = T >= FPFMatch::GoalScore0 + 0.3f;
			const int32 S0 = bNew ? M.Score[0] : M.GoalScoreWas[0], S1 = bNew ? M.Score[1] : M.GoalScoreWas[1];
			const float Pop = bNew ? 1.f + 0.4f * FMath::Clamp(1.f - (T - FPFMatch::GoalScore0 - 0.3f) / 0.18f, 0.f, 1.f) : 1.f;
			const float In = FMath::Clamp((T - FPFMatch::GoalScore0) / 0.12f, 0.f, 1.f);
			const float Yc = H * 0.40f;
			const FLinearColor Face(1, 1, 1, In), Side(Blue.R, Blue.G, Blue.B, In), Sh(0, 0, 0.1f, 0.7f * In);
			const float Gap = 9.f * SPx;
			PixelText(C, TEXT("-"), W * 0.5f + SPx * 0.5f, Yc - 3.5f * SPx, SPx, Face, Side, Sh);
			const float P0 = M.GoalTeam == 0 ? Pop : 1.f, P1 = M.GoalTeam == 1 ? Pop : 1.f;
			PixelText(C, FString::FromInt(S0), W * 0.5f - Gap, Yc - 3.5f * SPx * P0, SPx, Face, Side, Sh, P0, P0);
			PixelText(C, FString::FromInt(S1), W * 0.5f + Gap, Yc - 3.5f * SPx * P1, SPx, Face, Side, Sh, P1, P1);
			OutNameY = Yc + 5.f * SPx;
			bCard = true;
		}
	}
	if (M.FadeA > 0.f) PRect(C, 0, 0, W, H, FLinearColor(0, 0, 0, FMath::Clamp(M.FadeA, 0.f, 1.f)));
	return bCard;
}
