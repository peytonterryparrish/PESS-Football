// Proper Football - the arcade match: 11v11 simulation in world metres, AI, the human controller, and the 2D renderer.
#pragma once

#include "CoreMinimal.h"
#include "PFCore.h"
#include "PFSprites.h"

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
enum class EPFKick : uint8 { None, Pass, Lob, Through, Shot, Clear, Volley, Header, Punt, Throw };

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
	float AIThink = 0;
};

struct FPFBall
{
	FVector Pos = FVector::ZeroVector, Vel = FVector::ZeroVector;
	int32 Owner = -1;
	int32 LastTouch = -1;
	int32 LastKind = 0;    // last EPFKick that moved it (16 = poked away)
	int32 LastTeam = 0;
	float Roll = 0;   // distance rolled (sprite rotation)
	float Curl = 0;
};

enum class EPFPhase : uint8 { PreKick, Play, Goal, Restart, HalfTime, FullTime };
enum class EPFRestart : uint8 { Kickoff, ThrowIn, GoalKick, Corner, FreeKick, Penalty };

struct FPFPadInput
{
	FVector2D Move = FVector2D::ZeroVector;   // screen space, y down
	bool bSprint = false;
	bool bPass = false, bLob = false, bThrough = false, bSwitch = false;  // pressed this frame
	bool bShootHeld = false, bShootReleased = false, bShootPressed = false;
};

struct FPFMatchEvent { float Minute; int32 Team; FString Text; };

class PROPERFOOTBALL_API FPFMatch
{
public:
	void Init(const FPFMatchSetup& InSetup);
	void Tick(float Dt, const FPFPadInput& Pad);
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
	struct FCrew { TObjectPtr<UTexture2D> Tex = nullptr; FVector2D Pos; };
	TArray<FCrew> Crew;            // camera operators and ball boys (static sideline sprites from the owner's pack)
	FPFBall Ball;
	FString Banner;            // "GOAL!", "CORNER KICK" ...
	float BannerT = 0;
	bool bFinished = false;
	bool bQuit = false;        // left from the pause menu (career: the rest is simulated)
	FVector2D CamCenter;       // atlas px
	static float CamZoom;
	// test diagnostics: 0 shots, 1 shot dist sum (dm), 2 saves (dive+standing), 3 AI tackles tried, 4 won, 5 slides, 6 lost on cut, 7 passes, 8 intercept/loose pickups, 9 shots inside 12 m
	static int64 Diag[32];      // stadium pixels -> screen pixels at 1080p (options: near / medium / far)
	static constexpr float PlayerHeightM = 1.82f;   // the sprite figure (64 px tall) is drawn at this real height
	static float HeightScale;   // drawn heights (people, ball flight, goals) vs the stadium's projection: 0.5 = owner's "half the size" (v0.4)
	float Highlight = 0;
	TArray<FName> Sfx;         // sound cues raised this frame (the flow plays and clears them)
	FLinearColor CardColor = FLinearColor::Yellow;   // the card the referee is holding up (while his ref_card clip plays)       // >0 while something worth watching happens (manage mode slows down and shows sprites)

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
	TObjectPtr<UMaterialInstanceDynamic> PitchMid = nullptr;   // M_PFPitch: dithered grass + markings, drawn over the plate's grass
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
	float StuckT = 0;
	int32 LastShotTeam = -1;

	int32 SpriteFor(const FString& Id);
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
	int32 NearestToBall(int32 Team, bool bOutfieldOnly) const;
	FVector2D FormationSpot(const FPFPlayer& Pl) const;
	void Goal(int32 ScoringTeam);
	void Foul(FPFPlayer& Offender, FPFPlayer& Victim);
	void SwitchHuman(int32 NewIdx);
	EPFDir ScreenDir(FVector2D WorldDir) const;
	FName PoseFor(const FPFPlayer& Pl) const;
	UMaterialInstanceDynamic* MidFor(UMaterialInterface* M, int32 Set, const FPFKit* Kit);
	void SetBanner(const FString& S, float T = 1.6f) { Banner = S; BannerT = T; }
	void Log(int32 Team, const FString& S);
};
