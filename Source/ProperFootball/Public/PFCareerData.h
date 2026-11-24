// Proper Football - career world model and simulation (no UI). Endless seasons across a 5-tier English pyramid.
#pragma once

#include "CoreMinimal.h"
#include "PFMatch.h"
#include "PFWorld.h"

// ---------------------------------------------------------------- people
enum class EPFRegion : uint8 { England, Celtic, WesternEurope, SouthernEurope, EasternEurope, Scandinavia, WestAfrica, NorthAfrica, SouthAmerica, Americas, Asia, Oceania, Count };
const TCHAR* PFRegionName(EPFRegion R);

struct FPFPerson
{
	int32 Id = 0;
	FString Name;
	int32 Age = 20;
	EPFRegion Region = EPFRegion::England;
	EPFRole Role = EPFRole::MID;
	FPFStats Stats;
	int32 Potential = 60;      // hidden ceiling (overall)
	int32 Club = -1;           // club index; -1 free agent / unsigned youth
	bool bAcademy = false;     // in a club's youth academy
	bool bScoutedOnly = false; // a prospect found by scouts (not signed): stats are never shown
	int32 Wage = 1000;         // per week
	int32 ContractYears = 2;
	int32 Morale = 70;
	int32 InjuryWeeks = 0;
	int32 SeasonApps = 0, SeasonGoals = 0;
	int32 CareerApps = 0, CareerGoals = 0;
	FString Sprite = TEXT("O01");
	int32 Number = 0;
	bool bListed = false;      // transfer-listed by its club
	bool bDropped = false;     // left out of the XI by the user
	int32 Ovr() const { return Stats.Overall(Role == EPFRole::GK); }
};

// ---------------------------------------------------------------- clubs
struct FPFCareerClub
{
	FString ClubId;            // PFWorld club id (art) or "G###" for generated non-league clubs
	FString Name, Short;
	int32 Prestige = 1;        // 1..5 (grows/shrinks slowly with success)
	int32 Division = 3;        // 0 = Premier .. 4 = National League
	FPFKitSet Kits;
	FString Badge, Stadium;
	int64 Balance = 1000000;   // bank (pounds)
	int64 TransferBudget = 200000;
	int32 WageBudget = 30000;  // per week
	FString Formation = TEXT("4-4-2");
	TArray<int32> Squad;       // person ids (first team)
	TArray<int32> Academy;     // person ids (youth)
	int32 FacilityLevel = 1;   // academy quality 1..5
	float Strength = 50;       // cached team rating
};

// ---------------------------------------------------------------- competitions
struct FPFFixture
{
	int32 Home = -1, Away = -1;
	int32 HG = -1, AG = -1;    // -1 = not played
	int32 Matchday = 0;
	int32 Comp = 0;            // 0..4 league division, 10 = FA-style cup, 11 = league cup
	int32 Round = 0;
	bool bPens = false;        // cup tie decided on penalties
	int32 PenWinner = -1;
	bool Played() const { return HG >= 0; }
};

struct FPFTableRow { int32 Club = -1, P = 0, W = 0, D = 0, L = 0, GF = 0, GA = 0, Pts = 0; int32 GD() const { return GF - GA; } };

struct FPFCup
{
	FString Name;
	int32 Comp = 10;
	TArray<int32> Alive;       // clubs still in
	int32 Round = 0;
	TArray<int32> RoundMatchdays;
	int32 Winner = -1;
};

struct FPFScout { FString Name; int32 Quality = 2; EPFRegion Region = EPFRegion::England; int32 WeeksLeft = 0; int32 Wage = 400; };

struct FPFOffer { int32 Club = -1; FString Reason; };

struct FPFBid
{
	int32 Person = -1;
	int32 FromClub = -1;       // buyer
	int64 Fee = 0;
	int32 Wage = 0;
	int32 Status = 0;          // 0 pending, 1 accepted by club, 2 rejected, 3 completed, 4 player refused
	bool bByUser = false;
	FString Note;
};

struct FPFNews { int32 Season = 0, Matchday = 0; FString Text; };

// ---------------------------------------------------------------- the save
struct FPFCareerState
{
	int32 Version = 2;
	uint64 Seed = 1;
	int32 Season = 2006;            // season starting year
	int32 Matchday = 0;             // next matchday to play (0-based)
	int32 MaxMatchday = 46;
	FString ManagerName = TEXT("The Gaffer");
	int32 UserClub = -1;
	int32 Reputation = 20;          // 0..100
	int32 BoardConfidence = 60;     // 0..100
	int32 SeasonsAtClub = 0, TotalSeasons = 0;
	int32 Trophies = 0;
	bool bUnemployed = false;
	TArray<FPFOffer> Offers;
	TArray<FPFCareerClub> Clubs;
	TArray<FPFPerson> People;
	TArray<FPFFixture> Fixtures;
	TArray<FPFCup> Cups;
	TArray<FPFScout> Scouts;
	TArray<int32> Prospects;        // scouted youth (person ids, unsigned)
	TArray<FPFBid> Bids;
	TArray<FPFNews> News;
	TArray<FString> History;        // one line per finished season
	FString Training = TEXT("Balanced");
	int32 NextPersonId = 1;
	bool bWindowOpen = true;
	int32 LastSeasonFinish = 0;
	FString Phase = TEXT("Season");  // Season / Offers (end-of-season job market)
};

// ---------------------------------------------------------------- the simulation
class PROPERFOOTBALL_API FPFCareerSim
{
public:
	FPFCareerState S;
	PF::FRng Rng;

	void NewCareer(uint64 Seed, const FString& Manager);
	bool Save(const FString& Slot) const;
	bool Load(const FString& Slot);
	static bool SaveExists(const FString& Slot);

	FPFPerson* Person(int32 Id);
	const FPFPerson* Person(int32 Id) const;
	FPFCareerClub& UserClub() { return S.Clubs[S.UserClub]; }

	// season flow
	TArray<int32> FixturesOn(int32 Matchday, bool bUserOnly = false) const;
	int32 UserFixture() const;                         // index of the user's fixture on the current matchday (-1 = none)
	void PlayMatchday(const TFunction<void(FPFFixture&)>& UserResult = nullptr);   // sims every fixture (user fixture via callback if already played)
	void ApplyResult(FPFFixture& F, int32 HG, int32 AG, int32 PenWinner = -1);
	void QuickSim(FPFFixture& F);
	TArray<FPFTableRow> Table(int32 Division) const;
	bool SeasonOver() const { return S.Matchday >= S.MaxMatchday; }
	void EndSeason();                                  // promotion/relegation, ageing, retirements, regens, board verdict, job offers
	void AcceptOffer(int32 OfferIndex);
	void StartSeason();

	// match setup for the user's (or any) fixture
	FPFMatchSetup MatchSetup(const FPFFixture& F, bool bUserPlays) const;
	TArray<int32> BestXI(int32 Club) const;
	float TeamStrength(int32 Club) const;

	// transfers
	int64 Value(const FPFPerson& P) const;
	int32 WageDemand(const FPFPerson& P, int32 ToClub) const;
	FString MakeBid(int32 Person, int64 Fee, int32 Wage);       // user bid; resolves over following matchdays
	void ProcessBids();                                          // AI responses, rival bids, completions
	void SellToBest(int32 Person);                               // user lists/sells
	void AIWindowActivity();
	int32 SquadWages(int32 Club) const;

	// youth
	void SendScout(int32 Scout, EPFRegion Region);
	void HireScout();
	void ScoutTick();
	TArray<int32> YouthMatchSquad(EPFRegion Region, int32 Count, uint64 Seed);   // prospects + local kids for a youth match
	FString SignProspect(int32 Person);
	void PromoteYouth(int32 Person);
	void ReleasePlayer(int32 Person);
	FString OfferContract(int32 Person);
	FPFMatchSetup YouthMatchSetup(EPFRegion Region, uint64 Seed) const;

	void AddNews(const FString& T);
	FString DivisionName(int32 D) const;
	FString CompName(int32 Comp) const;
	int32 Expectation(int32 Club) const;   // expected league position

private:
	int32 NewPerson(int32 Club, EPFRole Role, int32 Ovr, int32 Age, EPFRegion Reg);
	FPFPerson MakePerson(EPFRole Role, int32 Ovr, int32 Age, EPFRegion Reg);
	void BuildFixtures();
	void BuildCups();
	void CupDraw(FPFCup& C, int32 Matchday);
	void Develop(FPFPerson& P, float Weeks);
	void RecalcStrength(int32 Club);
	void AutoPick(int32 Club);
	void Regens();
	void EnsureSquad(int32 Club);
	FString CupWinnerNote;
	void BoardAfterMatch(int32 GF, int32 GA, bool bHome);
};
