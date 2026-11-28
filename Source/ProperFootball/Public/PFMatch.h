// Proper Football - the arcade match: 11v11 simulation in world metres, AI, the human controller, and the 2D renderer.
#pragma once

#include "CoreMinimal.h"
#include "PFCore.h"
#include "PFSprites.h"
#include "PFStadium3D.h"

class UCanvas;
class UTexture2D;
class UMaterialInterface;
class UMaterialInstanceDynamic;

// ---------------------------------------------------------------- stats -> visible ability
// The key design rule: attributes produce EXTREME, obvious differences on the pitch. A 20-rated player is slow, heavy-touched
// and clumsy; a 90-rated one is visibly fast, sharp and accurate. Mapping curves live in PFAbility (PFMatch.cpp) only.
struct FPFStats
{
	uint8 Pace = 50, Accel = 50, Agility = 50, Control = 50, Passing = 50, Shooting = 50, Heading = 50,
		Tackling = 50, Positioning = 50, Reactions = 50, Stamina = 50, Keeping = 20;
	int32 Overall(bool bKeeper) const;
};

struct FPFAbility
{
	float MaxSpeed = 6.f;      // m/s running (sprint adds SprintMul)
	float SprintMul = 1.2f;
	float Accel = 12.f;        // m/s^2
	float TurnRate = 540.f;    // deg/s
	float TurnKeep = 0.6f;     // fraction of speed kept through a sharp cut
	float TouchDist = 1.2f;    // how far ahead each dribble touch pushes the ball (heavy touch = big)
	float FirstTouchErr = 0.3f;// chance-scale of a miscontrol on receiving
	float PassErrDeg = 6.f;
	float ShotErrDeg = 8.f;
	float ShotPower = 26.f;    // m/s at full charge
	float PassPower = 1.f;
	float ReactSec = 0.3f;
	float AnimRate = 1.f;      // clip playback speed (pace/agility)
	float TackleReach = 1.3f;
	float TackleWin = 0.5f;
	float HeadReach = 2.3f;
	float KeeperReach = 2.5f;  // dive reach
	float KeeperReact = 0.25f;
};
FPFAbility PFAbility(const FPFStats& S, bool bKeeper);

enum class EPFRole : uint8 { GK, DEF, MID, FWD };

struct FPFPlayerSetup
{
	FString Name = TEXT("Player");
	int32 Number = 1;
	EPFRole Role = EPFRole::MID;
	FPFStats Stats;
	FString Sprite = TEXT("O01");     // base character sprite set id
	FVector2D Slot = FVector2D(0.5, 0); // formation slot: x 0 own goal .. 1 opponents' goal, y -1 (left) .. 1 (right) of the attacking direction
};

struct FPFKit
{
	FLinearColor Shirt = FLinearColor::White, Trim = FLinearColor::Black, Shorts = FLinearColor::White, Socks = FLinearColor::White;
	int32 Pattern = 0;   // 0 solid, 1 vertical stripes, 2 hoops, 3 halves
};

struct FPFTeamSetup
{
	FString Name = TEXT("Home"), Short = TEXT("HOM");
	FPFKit Kit;
	FPFKit KeeperKit;
	// the club's own animated kit art (v0.7, GameData/Sprites/Clubs/<slug>_<kit>.json); empty for generated clubs
	FString ClubSlug;
	bool bAwayKit = false, bKeeper2 = false;
	TArray<FPFPlayerSetup> XI;
	bool bHuman = false;
	FString BadgePath;
};

struct FPFMatchSetup
{
	FPFTeamSetup Home, Away;
	FString StadiumSlug = TEXT("arsenal");
	FString Stage = TEXT("Daylight");   // Daylight / Sunset / Floodlit_Night
	float HalfSeconds = 150.f;         // real seconds per half
	uint64 Seed = 1;
	bool bWatchOnly = false;           // manager mode watch: both teams AI
	bool bTraining = false;            // v0.1 sandbox: one player, an empty pitch and two keepers
};

enum class EPFAct : uint8 { None, Clip, Slide, Fall, Dive, Celebrate };
enum class EPFKick : uint8 { None, Pass, Lob, Through, Shot, Clear, Volley, Header, Punt, Throw, Chip };

struct FPFPlayer
{
	FPFPlayerSetup Setup;
	FPFAbility A;
	int32 Team = 0;
	int32 Index = 0;
	bool bKeeper = false;
	FVector2D Pos = FVector2D::ZeroVector, Vel = FVector2D::ZeroVector;
	FVector2D Facing = FVector2D(1, 0);
	FVector2D Wish = FVector2D::ZeroVector;   // desired move direction * (0..1)
	FVector2D Home = FVector2D::ZeroVector;   // managers: centre of the technical area
	bool bSprint = false;
	EPFAct Act = EPFAct::None;
	FName Clip;
	float ActT = 0, ActLen = 0;
	bool bEventFired = false;
	EPFKick PendingKick = EPFKick::None;
	FVector2D KickAim = FVector2D::ZeroVector;
	float KickPower = 0;
	int32 KickTarget = -1;
	float Charge = 0;      // shot charge 0..1
	bool bCharging = false;
	float Stride = 0;      // metres travelled (officials/managers)
	float Gait = 0;        // run-cycle phase in cycles (1 cycle = two steps); advanced by cadence, not raw distance
	float TouchPhase = 0;  // dribble: phase of the current touch (0 = on the foot)
	FVector2D DribbleDir = FVector2D(1, 0);
	FVector2D AIDir = FVector2D::ZeroVector;   // AI carrier's committed running line (changes at human-like speed)
	float NoTouch = 0;     // seconds before this player may touch the ball again
	float ReactWait = 0;
	float Stamina = 1;
	FVector2D DiveDir = FVector2D::ZeroVector;
	int32 SpriteSet = 0;
	bool bBooked = false;
	bool bSentOff = false;
	bool bWonBallThisSlide = false;
	// v0.10 presentation (PFPresent.cpp): what is drawn between actions. PresClip/PresT/PlantT/PrevSpeed are drawing only
	// (advanced per frame, never read by the sim); bRestAdded marks an action whose rest hold was added (sim)
	FName PresClip; float PresT = 9.f;   // the kick that just ended: its follow-through finishes drawing while he stands or jogs
	float PlantT = 0.f, PrevSpeed = 0.f; // a brake / plant pose after a fast run stops
	bool bRestAdded = false;
	float AIThink = 0;
	float FeintT = 0;          // skill move in progress (s left)
	FVector2D FeintDir = FVector2D::ZeroVector;
	float FooledT = 0;         // a defender who bought a feint: committed the wrong way for a moment
	bool bSettled = false;     // v0.9.2: reached his spot; stays put until it moves well away (no stop-start jitter)
	uint8 DrawDir = 255;       // v0.9.2: the view drawn last frame (changes only past a margin: no flicker between views)
	float RunT = 0;            // v0.9.2 LB call: seconds left on a forward run the pad's player asked for
	FVector2D RunTo = FVector2D::ZeroVector;
	float BoostT = 0;          // v0.9.4: seconds left on the burst a through ball's runner gets (faster for better players)
	float BoostMul = 1.f;      // top-speed multiplier while it lasts (acceleration gets twice the gain)
	float CarryT = 0;          // v0.9.8: a through ball taken in stride: his first touch carries him on (s left, stick idle)
	FVector2D CarryDir = FVector2D(1, 0);
	float RunSeenT = 9.f;      // v0.10 attack stats: seconds since he was last seen on a forward run off the ball (sim measure)
	// v0.10 attacking runs (TeamAI): the run he is on (0 none, 1 in behind, 2 checking short, 3 channel, 4 overlap, 5 third man),
	// its target, time left, and the cool-down before he may start another
	uint8 AtkRun = 0; FVector2D AtkRunTo = FVector2D::ZeroVector; float AtkRunT = 0.f, AtkRunCool = 0.f, AtkRunAge = 0.f;
	float CrossPeak = -1.f, CrossZa = 1.7f;   // v0.10: the cross being struck: its peak height and the height it reaches the target at (-1 = an ordinary lob)
};

struct FPFBall
{
	FVector Pos = FVector::ZeroVector, Vel = FVector::ZeroVector;
	int32 Owner = -1;
	int32 LastTouch = -1;
	int32 LastKind = 0;    // last EPFKick that moved it (16 = poked away, 17 = blocked, 18 = saved)
	int32 LastTeam = 0;
	float Roll = 0;   // distance rolled (sprite rotation)
	float Curl = 0;
	float GuideV = 0;   // its steady speed while guided (m/s)
	int32 Guide = -1; // v0.8 assisted pass: the team-mate it is being steered onto (-1 = free ball)
	FVector2D GuideDir0 = FVector2D(1, 0);   // v0.9.8: the line it was struck on (the steering stays within 6 deg of it)
	bool bBeatsKeeper = false;   // v0.9.6: a well-struck pad shot the keeper will not get a hand to (rolled when struck)
	FVector2D ThroughAim = FVector2D::ZeroVector;   // v0.9.4: the space a through ball was played into
	float ThroughT = 0;                             // seconds since it was played
};

enum class EPFPhase : uint8 { PreKick, Play, Goal, Restart, HalfTime, FullTime };
enum class EPFRestart : uint8 { Kickoff, ThrowIn, GoalKick, Corner, FreeKick, Penalty };

struct FPFPadInput
{
	FVector2D Move = FVector2D::ZeroVector;   // screen space, y down
	bool bSprint = false;
	bool bPass = false, bLob = false, bThrough = false, bSwitch = false;  // pressed this frame
	bool bPassHeld = false, bThroughHeld = false;                         // v0.9.8 power bar: A / Y still held (a press with neither held is a tap)
	bool bLobHeld = false;                                                // v0.10 cross bar: X still held
	bool bShootHeld = false, bShootReleased = false, bShootPressed = false;
	bool bChipMod = false;                    // LB held: B becomes a chip shot
	bool bFeintL = false, bFeintR = false;   // right stick flicked left / right this frame
	FVector2D Look = FVector2D::ZeroVector;  // v0.10: the right stick (screen space, y down): set-piece height up / down
};

struct FPFMatchEvent { float Minute; int32 Team; FString Text; };

class PROPERFOOTBALL_API FPFMatch
{
public:
	void Init(const FPFMatchSetup& InSetup);
	void Tick(float Dt, const FPFPadInput& Pad);
	// -pfautopad: a scripted player drives the human side through the real pad input (long playtests, owner v0.7)
	FPFPadInput AutoPad(float Dt);
	bool bAutoPad = false; float AutoT = 0, AutoHold = 0; bool bAutoChip = false;
	float AutoPassHold = 0; bool bAutoPassY = false, bAutoPassX = false;   // v0.9.8: the bot holds A / Y for the power bar
	// playtest log (-pfautopad): what the pad's side did and what went wrong
	int32 PT_Passes = 0, PT_PassOK = 0, PT_Shots = 0, PT_Chips = 0, PT_Crosses = 0, PT_Feints = 0, PT_FeintBeat = 0, PT_KeeperThrows = 0, PT_ThrowIns = 0;
	int32 AnyPassFrom = -1, AnyPassTo = -1; float AnyPassT = 0, AnyPassMin = 99; FVector2D AnyPassFromPos;
	int32 PT_ShortN = 0, PT_ShortOK = 0, PT_ShortToTarget = 0, PT_ShortLost = 0, PT_PassKind = 0, PT_PassTarget = -1; FVector2D PT_PassFromPos = FVector2D::ZeroVector; float PT_PassLen = 0;   // v0.8: short (A) passes only
	int32 PT_ThroughN = 0, PT_ThroughOK = 0; float PT_ThroughLen = 0;   // v0.9.4
	int32 PT_KeeperCrowded = 0; float PT_MaxRoll = 0, PT_RollT = 0; int32 PT_LongRolls = 0; int32 PT_PassFrom = -1; float PT_PassT = 0;
	void Draw(UCanvas* C, UMaterialInterface* SpriteMat);
	void AddReferencedObjects(FReferenceCollector& Collector);

	// read-only state for HUD / career
	int32 Score[2] = { 0, 0 };
	float MatchClock = 0;     // game minutes 0..90
	EPFPhase Phase = EPFPhase::PreKick;
	TArray<FPFMatchEvent> Events;
	int32 Shots[2] = { 0, 0 };
	int32 Fouls[2] = { 0, 0 }, Yellows[2] = { 0, 0 }, Reds[2] = { 0, 0 };
	float PossT[2] = { 0, 0 };
	int32 Corners[2] = { 0, 0 };
	int32 Human = -1;          // controlled player index
	FPFMatchSetup Setup;
	TArray<FPFPlayer> P;
	TArray<FPFPlayer> Officials;   // [0] referee, [1] [2] assistant referees (drawn, never touch the ball)
	TArray<FPFPlayer> Managers;    // [0] home, [1] away: in their technical areas
	TArray<FVector2D> HeadScreen;  // v0.9.2: each player's head on screen last frame (HUD ratings); X < -1e8 = not drawn
	float DrawSpriteScale = 1.f;   // v0.10 bug fix: screen pixels per sprite pixel last frame (the HUD stacks its tags above the cursor)
	int32 TeamOverall(int32 Team) const;
	// test helpers (feel test): the stick direction that points along a world direction, and the goal a team attacks
	FVector2D StickFor(FVector2D WorldDir) const { return Proj.DirToScreen(WorldDir).GetSafeNormal(); }
	FVector2D AttackGoal(int32 Team) const { return GoalCentre(Team, true); }
	int32 AttackSign(int32 Team) const { return AttackDir(Team); }
	// v0.9.4 test helper: everyone but the man on the ball at his formation spot, standing (a side in shape round him)
	void TestTakeShape() { for (FPFPlayer& Pl : P) if (!Pl.bSentOff && Pl.Index != Ball.Owner) { Pl.Pos = FormationSpot(Pl); Pl.Vel = FVector2D::ZeroVector; Pl.bSettled = true; } }
	// v0.9.4 test helper: the AI's through ball from one player, along a direction (returns the receiver, -1 = none)
	int32 TestThroughBall(int32 From, FVector2D Dir)
	{
		FPFPlayer& Pl = P[From];
		const int32 R = BestPassTarget(Pl, Dir, true);
		if (R < 0) return -1;
		Pl.PendingKick = EPFKick::Through; Pl.KickTarget = R; Pl.KickPower = 0.75f; Pl.KickAim = P[R].Pos;
		FireKick(Pl);
		return R;
	}
	struct FCrew { TObjectPtr<UTexture2D> Tex = nullptr; FVector2D Pos; };
	TArray<FCrew> Crew;            // camera operators and ball boys (static sideline sprites from the owner's pack)
	FPFBall Ball;
	FString Banner;            // "GOAL!", "CORNER KICK" ...
	float BannerT = 0;
	// ---- v0.10 presentation (PFPresent.cpp): the goal sequence, the replay, rests and transitions
	float GoalT = -1.f;              // sim seconds into the goal sequence (-1: none); the HUD draws the letters and score off it
	int32 GoalTeam = -1, GoalScorer = -1, GoalMinute = 0, GoalCeleb = 0;
	int32 GoalScoreWas[2] = { 0, 0 };   // the score before this goal (the HUD ticks the scorers' digit up)
	FString GoalWho;                 // "23' SURNAME"
	FVector2D GoalRunTo = FVector2D::ZeroVector;
	float FadeA = 0.f;               // black over the pitch (the cut back to the kick-off)
	bool bGoalSkippable = false;
	// goal sequence timeline (sim seconds): letters, score card, replay, fade to the kick-off
	static constexpr float GoalLetters0 = 0.15f;
	static constexpr float GoalLettersEnd = 2.3f;
	static constexpr float GoalScore0 = 2.3f;
	static constexpr float GoalScoreEnd = 4.3f;
	static constexpr float GoalReplay0 = 6.0f;
	static constexpr float GoalFade0 = 9.4f;
	static constexpr float GoalEnd = 9.7f;
	bool InReplay() const { return ReplayIdx >= 0; }
	void PresentTick(float Dt);      // per drawn frame: brake poses, follow-through clocks, the replay recorder
	struct FSnap { TArray<FPFPlayer> P; FPFBall Ball; bool bKeeperHands = false; float T = 0; };
	TArray<FSnap> Replay;            // ring of snapshots (30 Hz, about 4 s), recorded while drawing open play
	int32 ReplayHead = 0, ReplayN = 0, ReplayIdx = -1; float RecAcc = 0, ReplayClock = 0;
	bool bInReplayDraw = false;
	bool bFinished = false;
	bool bQuit = false;        // left from the pause menu (career: the rest is simulated)
	FVector2D CamCenter;       // atlas px
	static float CamZoom;
	// test diagnostics: 0 shots, 1 shot dist sum (dm), 2 saves (dive+standing), 3 AI tackles tried, 4 won, 5 slides, 6 lost on cut, 7 passes, 8 intercept/loose pickups, 9 shots inside 12 m
	static int64 Diag[64];   // (v0.10 bug pass: 48 own goals, 49 goals last touched by a keeper - they used to be added into 6 and 7 as x1000 / x100000, which made the per-match "lost on cut" and "passes" read 150 and 725)
	// v0.10 attack stats (sim): 0 possession ticks, 1 sum of the side's mean line height (cm, + = toward their goal), 2 sum of
	// width (cm), 3 runs seen (off-ball forward sprints near their line), 4 completed passes, 5 completed passes to a man on
	// a run, 6 passes received beyond the defence's line, 7 crosses, 8 crosses won by the crossing side, 9 sum of the
	// deepest-to-highest spread (cm), 10 AI runs started (in behind), 11 checking short, 12 channel, 13 overlap, 14 third man,
	// 15 recycled (passed back after no run was found), 16 ticks a team-mate was beyond the ball in the final third
	static int64 AtkDiag[24];
	int32 AtkPrevOwner = -1; bool bAtkFlight = false; int32 AtkCrossTeam = -1;
	void AttackStats(int32 Team, float Dt);      // stadium pixels -> screen pixels at 1080p (options: near / medium / far)
	static constexpr float BallDrawK = 0.56f;      // v0.10: drawn ball = 16 x this sprite pixels (was 0.72)
	static constexpr float PlayerHeightM = 1.82f;   // the sprite figure (64 px tall) is drawn at this real height
	static inline bool bOldAnims = false;   // v0.9.9: -pfoldanims draws the v0.9.7 poses instead of the approved animations
	static float HeightScale;
	static float GameSpeed;
	static constexpr float NormalSpeed = 0.89f;   // v0.10: the default GameSpeed (Options GAME SPEED: SLOW 0.80, NORMAL, FAST 1.05 = v0.9.9); tests always run at it     // sim seconds per real second: 0.85 matches ISS Deluxe's on-screen pace at half-size players (v0.6 footage study)   // drawn heights (people, ball flight, goals) vs the stadium's projection: 0.5 = owner's "half the size" (v0.4)
	float Highlight = 0;
	float HitStop = 0, ShakeT = 9.f, ShakeAmp = 0;
	// v0.9.8 pass power bar (owner: "if I tap it, it's a very short pass to the closest player. But the longer I hold it,
	// the further the ball will travel to a different player"): A / Y held fill it; the charge picks the distance
	// (ISS study numbers: it fills in 0.45 s like the shot; under 0.12 s is a tap = the nearest man that way; then
	// 12 .. 42 m; the ball goes on release, or by itself when the bar is full)
	static constexpr float PassTapT = 0.12f;    // released sooner: a tap
	static constexpr float PassFullT = 0.45f;   // seconds from the press to a full bar
	static constexpr float PassMinD = 12.f;     // A: the distance the bar asks for just past a tap ..
	static constexpr float PassMaxD = 42.f;     // .. and full
	static constexpr float ThroughMinLead = 5.f;   // Y: how far ahead of the runner the ball is played, just past a tap ..
	static constexpr float ThroughMaxLead = 24.f;  // .. and full (a tap: 5.5 m, slipped in)
	float PassCharge = 0.f;          // the bar, 0..1 (HUD)
	bool bPassCharging = false;      // A or Y held with the ball (or one on its way)
	bool bPassChargeThrough = false; // it is Y's bar (through ball)
	bool bPassChargeLob = false;     // v0.10: it is X's bar (cross / lofted ball): fills in CrossFullT
	// v0.10 cross bar (owner: "the longer you hold it, the higher the ball goes and the further it goes to a player. If
	// you just tap it, it'll go to the nearest player"): a tap is a low ball to the nearest man on the aim line, then head
	// height to the middle man, and a full bar a high hanging ball to the farthest man (far post or the far winger)
	static constexpr float CrossFullT = 0.5f;
	static constexpr float CrossMidT = 0.30f;   // the bar's second tick (head-height balls below it, high ones above)
	float PendLobHoldT = 0.25f;                 // the released X hold, buffered with BufLob
	float BarFullT() const { return bPassChargeLob ? CrossFullT : PassFullT; }
	float CrossChargeFor(float Held) const { return FMath::Clamp((Held - PassTapT) / (CrossFullT - PassTapT), 0.f, 1.f); }
	int32 PadCrossPick(const FPFPlayer& From, FVector2D Aim, float Held, FVector2D& OutPt) const;
	void SetCross(FPFPlayer& Pl, FVector2D Target, float C, bool bTap);
	float PassHoldT = 0.f;           // seconds held
	float PendHoldT = 0.f;           // the released hold, buffered with BufPass / BufThrough until the ball is at his feet
	int32 ChargeOwner = -1;          // who was controlled when it started (a switch cancels it)
	int32 PassPick = -1;             // live: who the bar and the stick pick right now (HUD ring), -1 = into space
	FVector2D PassPickPt = FVector2D::ZeroVector;   // live: where a ball into space / a through ball would go
	float PassPickT = 0.f;           // through: seconds until the next re-pick (it is costly)
	float PassDistFor(float Held) const { return PassMinD + (PassMaxD - PassMinD) * FMath::Clamp((Held - PassTapT) / (PassFullT - PassTapT), 0.f, 1.f); }
	float ThroughLeadFor(float Held) const { return Held < PassTapT ? 5.5f : ThroughMinLead + (ThroughMaxLead - ThroughMinLead) * FMath::Clamp((Held - PassTapT) / (PassFullT - PassTapT), 0.f, 1.f); }
	// v0.9.8 the pad's choices (tests read them too): the receiver for a pass along a world direction with A held HoldT
	// seconds (-1 = none near the line: OutSpace is where it is played), and the runner and space for a through ball
	int32 PadPassTarget(const FPFPlayer& From, FVector2D Aim, float Held, FVector2D& OutSpace) const;
	int32 PadThroughPick(const FPFPlayer& From, FVector2D Aim, float Held, bool bLoftOnly, FVector2D& OutPt, float& OutT) const;
	float PadShotQ = -1.f;   // v0.9.6: the quality of the pad's shot being struck (0 scuffed .. 1 perfect), -1 none
	FString LastShotNote;    // v0.9.6: "far post, good" etc. (tests, captures)
	// v0.9 turf and dust kicked up by slides and keeper landings (owner: "it's really hard to see how the slide tackles are looking")
	struct FDust { FVector Pos, Vel; float T = 0, Life = 0.5f; int32 Kind = 0; };
	TArray<FDust> Dust;
	void SpawnDust(FVector2D At, FVector2D Dir, int32 N, float Speed);
	// v0.9 crowd reactions: +1 cheering (home goal / home fans), -1 "no!" (a goal against, a near miss), seconds left
	int32 CrowdMood = 0; int32 CrowdTeam = 0; float CrowdT = 0;   // v0.9 impact: a few frames' freeze and a camera kick when a shot is struck
	TArray<FName> Sfx;         // sound cues raised this frame (the flow plays and clears them)
	FLinearColor CardColor = FLinearColor::Yellow;   // the card the referee is holding up (while his ref_card clip plays)       // >0 while something worth watching happens (manage mode slows down and shows sprites)

	// ---- v0.10 set pieces (PFSetPiece.cpp). Owner: "an orange arrow on the ball ... left of the field, right of the field,
	// middle ... how high do we want the kick, high, medium, low, and then how much power is up to us" and, for throw-ins,
	// "arrows and lines on the ground to see where we're about to throw the ball". The pad's taker turns a small arrow at the
	// ball with the left stick, picks the height with Y (higher) / X (lower) or the right stick, holds B (or A) to fill the
	// bar and lets go to strike. A tap of A plays it short to the ringed man. AI takers keep their own routine.
	enum : int32 { SPLow = 0, SPMed = 1, SPHigh = 2 };
	static constexpr float SPFullT = 0.8f;   // seconds from the press to a full set-piece bar
	bool bSPAim = false;                     // the pad's man is lining one up (drawing, HUD)
	FVector2D SPDir = FVector2D(1, 0);       // the arrow (world, unit)
	int32 SPHeight = SPMed;
	float SPCharge = 0.f;                    // the bar 0..1
	bool bSPCharging = false;
	float SPHoldT = 0.f;
	int32 SPPick = -1;                       // the team-mate it will reach (ring), -1 none
	FVector2D SPLand = FVector2D::ZeroVector;   // where it comes down (throw-in line end, landing mark while charging)
	bool bSPShot = false;                    // free kick / penalty: the arrow is on goal (a shot)
	EPFRestart SetPieceKind() const { return Restart; }
	// the last set piece the pad's man struck (tests): kind, arrow, height step, power, launch velocity, receiver, landing
	struct FSPLast { int32 Kind = -1; FVector2D Dir = FVector2D::ZeroVector; int32 Height = 0; float Power = 0; FVector Vel = FVector::ZeroVector; FVector From = FVector::ZeroVector; int32 Receiver = -1; FVector2D Land = FVector2D::ZeroVector; bool bShot = false, bShort = false; int32 N = 0; } SPLast;
	// test hook: a restart of this kind for Team at Spot (-1 kinds: none), as a foul / the ball going out would place it
	void TestRestart(EPFRestart Kind, int32 Team, FVector2D Spot);
	FVector2D TestRestartSpot(EPFRestart Kind, int32 Team, int32 Variant) const;

private:
	bool bSPReleased = false, bSPByA = false, bSPShort = false; float SPIdleT = 0.f, SPPower = 0.f; int32 SPLookStep = 0;
	int32 SPDemoN = 0; float SPDemoT = 0.f; int32 SPShotsTaken = 0;
	FVector2D SPTarget = FVector2D::ZeroVector;   // throw-in: the point the throw is aimed at when released
	bool bSPLaunch = false;                       // FireKick: the pad's set piece is the kick being struck (SetPieceLaunch sets the flight)
	bool bSPHandsDone = false;
	void SetPieceBegin(FPFPlayer& T);
	bool SetPieceHuman(FPFPlayer& T, const FPFPadInput& Pad, float Dt, bool bGKGo);
	void SetPieceGo(FPFPlayer& T);
	void SetPieceLaunch(FPFPlayer& T);
	void SetPiecePreview(const FPFPlayer& T);
	void SetPieceMates(float Dt);
	bool SetPieceAutoPad(float Dt, FPFPadInput& In);
	void SetPieceDemo(float Dt, FPFPadInput& In);
	FVector2D SPClampDir(FVector2D D) const;
	int32 SPMateNear(FVector2D At, float MaxD, int32 Team, int32 Except) const;
	int32 SPShortPick(const FPFPlayer& T) const;
	FVector SPPlanVel(const FPFPlayer& T, FVector From, int32 Height, float Power, bool bShot, FVector2D Dir, FVector2D* OutLand) const;
	FName SetPiecePose(const FPFPlayer& Pl) const;
	void SetPieceHands();
	void DrawSetPiece(UCanvas* C, TFunctionRef<FVector2D(float, float, float)> W2S, float SpriteScale);

private:
	FPFProjection Proj;
	PF::FRng Rng;
	TArray<FPFSpriteSet> Sets;
	TMap<FString, int32> SetIndex;
	TObjectPtr<UTexture2D> Stadium = nullptr;
	TObjectPtr<UTexture2D> BallTex = nullptr;
	TObjectPtr<UTexture2D> ShadowTex = nullptr, RingTex = nullptr, CursorTex = nullptr;
	struct FGoalLayer { TObjectPtr<UTexture2D> Tex = nullptr; FVector2D XY, WH; };
	FGoalLayer GoalShadow[2], GoalRear[2], GoalFront[2];
	TObjectPtr<UMaterialInstanceDynamic> PitchMid = nullptr;
	TObjectPtr<UMaterialInstanceDynamic> StadiumMid = nullptr;   // M_PFStadium: the owner's stadium art, sharp-bilinear (even pixels at 6.24x)
	FPFStadium3D Stadium3D;                    // v0.9.8: the 3D ground round the 2D pitch (PFStadium3D.cpp)
	bool bStadium3D = true;                    // false with -pfoldstadium: the v0.9.7 painted plate
	TObjectPtr<UTexture2D> FanTex = nullptr;   // the owner's ISS Deluxe supporters (GameData/Fans/fans.png: 20 fans, seated row + standing row)
	TArray<FIntVector> Fans;                   // this stadium's seats: atlas x, y (the hips) and which fan (Tools/pipeline/fans.py)   // M_PFPitch: dithered grass + markings, drawn over the plate's grass
	FLinearColor PitchTint = FLinearColor::White;
	float PitchX0 = 57.f, PitchY0 = 34.5f;
	TMap<uint64, TObjectPtr<UMaterialInstanceDynamic>> Mids;

	int32 Half = 1;
	float HalfT = 0;            // real seconds into the half
	float PhaseT = 0;
	EPFRestart Restart = EPFRestart::Kickoff;
	int32 RestartTeam = 0;
	FVector2D RestartSpot;
	int32 RestartTaker = -1;
	int32 KickoffTeam = 0;
	int32 FirstKickoffTeam = 0;
	float Accum = 0;
	FPFPadInput PendPad;          // presses seen on frames that ran no 120 Hz step (high frame rates), kept for the next step
	float BufPass = 0, BufLob = 0, BufThrough = 0, BufShoot = 0;   // input buffer (s left): a press counts if the player can act soon after
	float StuckT = 0;
	// ---- Laws of the Game (v0.7)
	bool bKeeperHands = false;    // the ball's owner is a keeper holding it in his hands (only inside his own penalty area)
	float HoldT = 0;              // seconds he has held it: over 8 = corner kick to the opponents (Law 12, 2025/26)
	uint32 OffsideMask = 0;       // players caught offside when a team-mate last played the ball (bit = player index)
	int32 OffsideTeam = -1;
	bool bNoOffsideKick = false;  // the next kick is a throw-in, corner or goal kick (no offside from those)
	bool bIndirectFK = false;     // the pending free kick is indirect (offside, back-pass, impeding the keeper)
	bool bIndirectLive = false; int32 IndirectTaker = -1;   // an indirect free kick nobody else has touched yet
	TArray<int32> Wall;           // defenders lined up 9.15 m from a free kick
	TArray<int32> ThrowCands;     // throw-in: the three nearest team-mates the human can pick from
	int32 ThrowPick = 0;
	float NetHitT[2] = { 9.f, 9.f }, NetHitV[2] = { 0.f, 0.f };
	float AfterT = 9.f;            // aftertouch (ISS): seconds since the pad's player struck a shot, cross or set piece
	int32 LastShotTeam = -1;

	int32 SpriteFor(const FString& Id);
	int32 ClubSpriteFor(const FString& Id);   // no stand-in: -1 when that club set isn't built
	int32 AttackDir(int32 Team) const;    // +1 attacks +x
	FVector2D GoalCentre(int32 Team, bool bOpponents) const;
	void PlaceForKickoff(int32 KickTeam);
	void PlaceForRestart();
	void Step(float Dt, const FPFPadInput& Pad);
	void HumanControl(float Dt, const FPFPadInput& Pad);
	void TeamAI(int32 Team, float Dt);
	void KeeperAI(FPFPlayer& K, float Dt);
	void CarrierAI(FPFPlayer& Pl, float Dt);
	void MovePlayers(float Dt);
	void MoveOfficials(float Dt);
	void MoveManagers(float Dt);
	void BallPhysics(float Dt);
	void Possession(float Dt);
	void CheckOut();
	void StartClip(FPFPlayer& Pl, FName Clip, EPFKick Kick = EPFKick::None);
	void FireKick(FPFPlayer& Pl);
	void Kick(FPFPlayer& Pl, EPFKick Type, FVector2D Target, float Power, int32 Receiver = -1);
	int32 BestPassTarget(const FPFPlayer& From, FVector2D Dir, bool bThrough) const;
	int32 BestPassTargetCone(const FPFPlayer& From, FVector2D Dir, bool bThrough, float MinCos) const;
	// v0.9.4 through balls: the space to play it into (the longest lead the runner wins against every defender), the
	// runner's time to get there, his burst, and the point on the ball's path he runs to
	FVector2D ThroughSpace(const FPFPlayer& From, const FPFPlayer& R, float& OutRunT) const;
	float RunnerSpeed(const FPFPlayer& R, bool bWithBoost) const;
	float ThroughBoostFor(const FPFPlayer& R) const;
	FVector2D ThroughMeet(const FPFPlayer& R, float* OutT = nullptr) const;
	float ThroughPace(const FPFPlayer& R, float Dist, float T) const;
	// v0.9.8: the race for one point (the first moment a defender or keeper can reach the ball against the runner);
	// false when a lofted ball would be blocked at the passer's feet
	bool ThroughRace(const FPFPlayer& R, FVector2D BallP, FVector2D Pt, bool bLoft, float& OutMargin, float& OutTr, float& OutTf, float* OutWait = nullptr) const;
	float PendThroughT = 0.f;
	mutable bool bRaceShape = false;   // v0.9.8: ThroughRace for the pad's pick (only the two nearest defenders chase)
	bool bPendSpace = false;          // v0.9.8: the pad's pass goes into space along the stick (no guide)
	bool bPadThroughFixed = false;    // v0.9.8: the pad chose the through ball's space already (PadThroughT is its timing)
	float PadThroughT = 0.f;
	float PendPassCharge = -1.f;      // v0.9.8: the pad's bar for the pass being struck (pace), -1 none
	// v0.9.8 goal kick (owner: "the keeper steps back about four steps, then runs up and kicks when the button is
	// pressed"): -1 off, else seconds into the routine; the queued long kick and where the stick aimed it
	float GKT = -1.f, GKRunT = -1.f; bool bGKQueued = false; FVector2D GKLine = FVector2D(1, 0), GKAim = FVector2D::ZeroVector;
	// v0.10 bug pass: the penalty just struck - the side the keeper guessed (-1 / 0 stay / +1, world y) and the side it went;
	// PenShotT >= 0 while it is on its way (KeeperAI dives where he guessed)
	int32 PenGuess = 0, PenWent = 0; float PenShotT = -1.f;
public:
	mutable float DbgThroughMargin = 0, DbgThroughRun = 0, DbgMaxM = 0;   // test: the chosen space's predicted margin over the defence and the runner's time
private:
	int32 NearestToBall(int32 Team, bool bOutfieldOnly) const;
	// v0.10 attacking play: the side's shape on the ball (by role, round the ball and their back line), the runs off it
	enum EAtkKind : uint8 { AK_GK, AK_CB, AK_FB, AK_DM, AK_CM, AK_WM, AK_ST, AK_WF };
	enum EAtkRun : uint8 { AR_None, AR_Behind, AR_Short, AR_Channel, AR_Overlap, AR_Third, AR_PassGo };
	EAtkKind AtkKindOf(const FPFPlayer& Pl) const;
	FVector2D AttackSpot(const FPFPlayer& Pl, float& OutUrgency) const;
	void PlanRuns(int32 Team, float Dt);
	int32 AtkRunCount(int32 Team) const;
	void StartAtkRun(FPFPlayer& Pl, uint8 Kind, FVector2D To, float T);
	int32 BestRunner(const FPFPlayer& From) const;
	bool OurPassInFlight(int32 Team) const;
	float AtkFreeT[2] = { 0.f, 0.f }, AtkPlanT[2] = { 0.f, 0.f };
	FVector2D FormationSpot(const FPFPlayer& Pl) const;
	void Goal(int32 ScoringTeam);
	void Foul(FPFPlayer& Offender, FPFPlayer& Victim, bool bIndirect = false);
	bool InOwnBox(int32 Team, FVector2D Pos) const;          // inside that team's own penalty area
	bool CanHandle(const FPFPlayer& K) const;                 // a keeper may use his hands on the ball right now
	bool KeeperHolding() const { return bKeeperHands && Ball.Owner >= 0; }
	float SecondLastX(int32 DefTeam, int32 AttackD) const;    // offside line (along the attack), second-last defender
	void MarkOffside(const FPFPlayer& Kicker);
	void Offside(FPFPlayer& Pl);
	void IndirectFreeKick(int32 Team, FVector2D Spot, const TCHAR* Msg);
	void EnforceRestartDistances();
	void GiveKeeperHands(FPFPlayer& K);
	void SwitchHuman(int32 NewIdx);
	EPFDir ScreenDir(FVector2D WorldDir) const;
	FName PoseFor(const FPFPlayer& Pl) const;
	// v0.10 presentation (PFPresent.cpp)
	FName PresentPose(const FPFPlayer& Pl, const FPFSpriteSet& S, bool& bHandled) const;   // timelines, follow-throughs, rests, transitions
	void GoalStart(int32 ScoringTeam, bool bOwnGoal);   // from Goal(): the celebration cast and the sequence clock
	void GoalScene(float Dt, const FPFPadInput& Pad);  // EPFPhase::Goal, every step
	void OnActEnd(FPFPlayer& Pl);                       // an action's clip ran out: rest holds, follow-through
	void OnDiveTouch(FPFPlayer& K, bool bHeld);         // a diving keeper got to the ball (held or parried): his time down
	bool DrawReplaySwap(bool bIn);                      // Draw: swap the replay frame in (and back out)
	UMaterialInstanceDynamic* MidFor(UMaterialInterface* M, int32 Set, const FPFKit* Kit);
	void SetBanner(const FString& S, float T = 1.6f)
	{
		Banner = S; BannerT = T;
		// v0.9 crowd: a save or the woodwork makes the shooting side's fans clutch their heads (not over a goal cheer)
		if ((S == TEXT("SAVE!") || S == TEXT("POST!") || S == TEXT("CROSSBAR!")) && !(CrowdMood != 0 && CrowdT > 1.f && Phase == EPFPhase::Goal))
		{ CrowdMood = -1; CrowdTeam = LastShotTeam >= 0 ? LastShotTeam : 0; CrowdT = 2.f; }
	}
	void Log(int32 Team, const FString& S);
};

// v0.10 -pfsetpiecetest: the pad's set pieces (PFSetPiece.cpp), PFSETPIECE lines in the log
void PFSetPieceTests();
