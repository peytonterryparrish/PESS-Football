#include "PFCareerData.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"

// ------------------------------------------------------------------------------------------------ names & regions
const TCHAR* PFRegionName(EPFRegion R)
{
	static const TCHAR* N[] = { TEXT("England"), TEXT("Scotland & Ireland"), TEXT("Western Europe"), TEXT("Southern Europe"), TEXT("Eastern Europe"), TEXT("Scandinavia"),
		TEXT("West Africa"), TEXT("North Africa"), TEXT("South America"), TEXT("North America"), TEXT("Asia"), TEXT("Oceania") };
	return N[FMath::Clamp((int32)R, 0, (int32)EPFRegion::Count - 1)];
}

static const TCHAR* GDivNames[] = { TEXT("Premier Division"), TEXT("The Championship"), TEXT("League One"), TEXT("League Two"), TEXT("National League") };
FString FPFCareerSim::DivisionName(int32 D) const { return GDivNames[FMath::Clamp(D, 0, 4)]; }
FString FPFCareerSim::CompName(int32 Comp) const
{
	if (Comp < 5) return DivisionName(Comp);
	for (const FPFCup& C : S.Cups) if (C.Comp == Comp) return C.Name;
	return TEXT("Cup");
}

static const int32 GDivSize[] = { 20, 24, 24, 24, 24 };
static const int32 GDivBaseOvr[] = { 71, 61, 54, 48, 42 };
static const int64 GDivRevenue[] = { 60000000, 12000000, 4500000, 2200000, 800000 };

// ------------------------------------------------------------------------------------------------ serialization
static FArchive& operator<<(FArchive& Ar, FPFStats& S)
{
	Ar << S.Pace << S.Accel << S.Agility << S.Control << S.Passing << S.Shooting << S.Heading << S.Tackling << S.Positioning << S.Reactions << S.Stamina << S.Keeping;
	return Ar;
}
static FArchive& operator<<(FArchive& Ar, FPFKit& K) { Ar << K.Shirt << K.Trim << K.Shorts << K.Socks << K.Pattern; return Ar; }
static FArchive& operator<<(FArchive& Ar, FPFKitSet& K) { Ar << K.Home << K.Away << K.Keeper1 << K.Keeper2; return Ar; }
static FArchive& operator<<(FArchive& Ar, FPFPerson& P)
{
	uint8 Reg = (uint8)P.Region, Role = (uint8)P.Role;
	Ar << P.Id << P.Name << P.Age << Reg << Role << P.Stats << P.Potential << P.Club << P.bAcademy << P.bScoutedOnly << P.Wage << P.ContractYears
		<< P.Morale << P.InjuryWeeks << P.SeasonApps << P.SeasonGoals << P.CareerApps << P.CareerGoals << P.Sprite << P.Number << P.bListed << P.bDropped;
	P.Region = (EPFRegion)Reg; P.Role = (EPFRole)Role;
	return Ar;
}
static FArchive& operator<<(FArchive& Ar, FPFCareerClub& C)
{
	Ar << C.ClubId << C.Name << C.Short << C.Prestige << C.Division << C.Kits << C.Badge << C.Stadium << C.Balance << C.TransferBudget << C.WageBudget
		<< C.Formation << C.Squad << C.Academy << C.FacilityLevel << C.Strength;
	return Ar;
}
static FArchive& operator<<(FArchive& Ar, FPFFixture& F) { Ar << F.Home << F.Away << F.HG << F.AG << F.Matchday << F.Comp << F.Round << F.bPens << F.PenWinner; return Ar; }
static FArchive& operator<<(FArchive& Ar, FPFCup& C) { Ar << C.Name << C.Comp << C.Alive << C.Round << C.RoundMatchdays << C.Winner; return Ar; }
static FArchive& operator<<(FArchive& Ar, FPFScout& S) { uint8 R = (uint8)S.Region; Ar << S.Name << S.Quality << R << S.WeeksLeft << S.Wage; S.Region = (EPFRegion)R; return Ar; }
static FArchive& operator<<(FArchive& Ar, FPFOffer& O) { Ar << O.Club << O.Reason; return Ar; }
static FArchive& operator<<(FArchive& Ar, FPFBid& B) { Ar << B.Person << B.FromClub << B.Fee << B.Wage << B.Status << B.bByUser << B.Note; return Ar; }
static FArchive& operator<<(FArchive& Ar, FPFNews& N) { Ar << N.Season << N.Matchday << N.Text; return Ar; }
static FArchive& operator<<(FArchive& Ar, FPFCareerState& S)
{
	Ar << S.Version << S.Seed << S.Season << S.Matchday << S.MaxMatchday << S.ManagerName << S.UserClub << S.Reputation << S.BoardConfidence
		<< S.SeasonsAtClub << S.TotalSeasons << S.Trophies << S.bUnemployed << S.Offers << S.Clubs << S.People << S.Fixtures << S.Cups << S.Scouts
		<< S.Prospects << S.Bids << S.News << S.History << S.Training << S.NextPersonId << S.bWindowOpen << S.LastSeasonFinish << S.Phase;
	return Ar;
}

static FString SavePath(const FString& Slot) { return FPaths::ProjectSavedDir() / TEXT("SaveGames") / (Slot + TEXT(".pfsave")); }

bool FPFCareerSim::Save(const FString& Slot) const
{
	TArray<uint8> Bytes;
	FMemoryWriter W(Bytes);
	uint32 Magic = 0x50465356;   // "PFSV"
	W << Magic;
	W << const_cast<FPFCareerState&>(S);
	uint64 RS = Rng.S; W << RS;
	return FFileHelper::SaveArrayToFile(Bytes, *SavePath(Slot));
}

bool FPFCareerSim::Load(const FString& Slot)
{
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *SavePath(Slot))) return false;
	FMemoryReader R(Bytes);
	uint32 Magic = 0; R << Magic;
	if (Magic != 0x50465356) return false;
	R << S;
	uint64 RS = 0; R << RS; Rng.S = RS ? RS : 1;
	return !R.IsError();
}

bool FPFCareerSim::SaveExists(const FString& Slot) { return IFileManager::Get().FileExists(*SavePath(Slot)); }

// ------------------------------------------------------------------------------------------------ people
FPFPerson* FPFCareerSim::Person(int32 Id)
{
	// ids are dense-ish; binary search over the sorted array
	int32 Lo = 0, Hi = S.People.Num() - 1;
	while (Lo <= Hi) { const int32 M = (Lo + Hi) / 2; if (S.People[M].Id == Id) return &S.People[M]; if (S.People[M].Id < Id) Lo = M + 1; else Hi = M - 1; }
	return nullptr;
}
const FPFPerson* FPFCareerSim::Person(int32 Id) const { return const_cast<FPFCareerSim*>(this)->Person(Id); }

static FString SpriteForPerson(PF::FRng& R, EPFRole Role)
{
	if (Role == EPFRole::GK) return FString::Printf(TEXT("K%02d"), 1 + R.Int(8));
	return FString::Printf(TEXT("O%02d"), 1 + R.Int(32));
}

static FString NameForRegion(PF::FRng& R, EPFRegion Reg)
{
	if (Reg == EPFRegion::England || Reg == EPFRegion::Celtic || Reg == EPFRegion::Oceania || Reg == EPFRegion::Americas) return PFWorld::RandomName(R);
	static const TCHAR* F[] = { TEXT("Luca"), TEXT("Jonas"), TEXT("Pieter"), TEXT("Mateo"), TEXT("Andrei"), TEXT("Sven"), TEXT("Kwame"), TEXT("Youssef"), TEXT("Thiago"), TEXT("Hiro"), TEXT("Ibrahim"), TEXT("Nikola"), TEXT("Emil"), TEXT("Rafael"), TEXT("Tomas"), TEXT("Moussa") };
	static const TCHAR* LastByReg[][6] = {
		{}, {},
		{ TEXT("Van Damme"), TEXT("Lefevre"), TEXT("Brandt"), TEXT("De Groot"), TEXT("Moreau"), TEXT("Keller") },
		{ TEXT("Romero"), TEXT("Bianchi"), TEXT("Ferreira"), TEXT("Navarro"), TEXT("Esposito"), TEXT("Duarte") },
		{ TEXT("Kowal"), TEXT("Popescu"), TEXT("Horvat"), TEXT("Dvorak"), TEXT("Ivanov"), TEXT("Szabo") },
		{ TEXT("Lindgren"), TEXT("Haugen"), TEXT("Nielsen"), TEXT("Virtanen"), TEXT("Berg"), TEXT("Dahl") },
		{ TEXT("Mensah"), TEXT("Diallo"), TEXT("Okonkwo"), TEXT("Traore"), TEXT("Asante"), TEXT("Adebayo") },
		{ TEXT("Benali"), TEXT("Haddad"), TEXT("Mansour"), TEXT("El Amrani"), TEXT("Saidi"), TEXT("Brahimi") },
		{ TEXT("Oliveira"), TEXT("Gomez"), TEXT("Alvarez"), TEXT("Santos"), TEXT("Rojas"), TEXT("Cabrera") },
		{}, { TEXT("Tanaka"), TEXT("Kim"), TEXT("Park"), TEXT("Nakamura"), TEXT("Lee"), TEXT("Sato") }, {} };
	const int32 RI = FMath::Clamp((int32)Reg, 0, 11);
	const TCHAR* L = LastByReg[RI][R.Int(6)];
	if (!L) return PFWorld::RandomName(R);
	return FString::Printf(TEXT("%c. %s"), F[R.Int(16)][0], L);
}

FPFPerson FPFCareerSim::MakePerson(EPFRole Role, int32 Ovr, int32 Age, EPFRegion Reg)
{
	FPFPerson P;
	P.Id = S.NextPersonId++;
	P.Name = NameForRegion(Rng, Reg);
	P.Age = Age; P.Region = Reg; P.Role = Role;
	P.Stats = PFWorld::RollStats(Rng, Ovr, Role);
	const int32 O = P.Ovr();
	// potential: young players have headroom; rare wonderkids have a lot
	int32 Head = 0;
	if (Age <= 18) Head = 6 + Rng.Int(18) + (Rng.Unit() < 0.06f ? 18 + Rng.Int(14) : 0);
	else if (Age <= 21) Head = 3 + Rng.Int(14) + (Rng.Unit() < 0.04f ? 12 : 0);
	else if (Age <= 24) Head = Rng.Int(8);
	P.Potential = FMath::Clamp(O + Head, O, 97);
	if (Age <= 17)
	{
		// youngsters: one hidden talent number drives both the ceiling and what you SEE now. Raw athletic and ball skills
		// show talent early (pace, acceleration, agility, touch); technique comes later. That is what scouts judge by eye.
		const float T = FMath::Clamp(46.f + Rng.Gauss() * 10.f + (Rng.Unit() < 0.06f ? 26.f : 0.f), 28.f, 96.f);
		const int32 Cur = FMath::Clamp(FMath::RoundToInt(T * 0.55f + (Age - 15) * 3 + Rng.Gauss() * 3.f), 18, 70);
		P.Stats = PFWorld::RollStats(Rng, Cur, Role);
		auto Innate = [&](uint8& V) { V = (uint8)FMath::Clamp(FMath::RoundToInt(V * 0.6f + Cur * 0.4f + (T - 50.f) * 0.65f), 1, 99); };
		Innate(P.Stats.Pace); Innate(P.Stats.Accel); Innate(P.Stats.Agility); Innate(P.Stats.Control); Innate(P.Stats.Reactions);
		if (Role == EPFRole::GK) Innate(P.Stats.Keeping);
		P.Potential = FMath::Clamp(FMath::RoundToInt(T), P.Ovr() + 3, 97);
	}
	P.Sprite = SpriteForPerson(Rng, Role);
	P.Wage = FMath::Max(100, int32(300.f * FMath::Exp((O - 45) * 0.1f)));
	P.ContractYears = 1 + Rng.Int(4);
	return P;
}

int32 FPFCareerSim::NewPerson(int32 Club, EPFRole Role, int32 Ovr, int32 Age, EPFRegion Reg)
{
	FPFPerson P = MakePerson(Role, Ovr, Age, Reg);
	P.Club = Club;
	S.People.Add(P);   // ids increase monotonically: the array stays sorted
	if (Club >= 0) S.Clubs[Club].Squad.Add(P.Id);
	return P.Id;
}

// ------------------------------------------------------------------------------------------------ world creation
void FPFCareerSim::NewCareer(uint64 Seed, const FString& Manager)
{
	S = FPFCareerState();
	S.Seed = Seed; Rng = PF::FRng(Seed);
	S.ManagerName = Manager;
	const TArray<FPFClub>& Art = PFWorld::Clubs();
	// clubs from the owner's art, by their 2005-06 division
	auto DivOf = [](const FString& D) { return D.Contains(TEXT("Premier")) ? 0 : D.Contains(TEXT("Championship")) ? 1 : D.Contains(TEXT("One")) ? 2 : D.Contains(TEXT("Two")) ? 3 : 4; };
	TArray<FString> T1Stadiums;
	for (const FPFClub& A : Art)
	{
		FPFCareerClub C;
		C.ClubId = A.Id; C.Name = A.Name; C.Short = A.Short; C.Prestige = A.Tier; C.Division = DivOf(A.OrigDivision);
		C.Kits = A.Kits; C.Badge = A.Badge; C.Stadium = A.Stadium;
		if (A.Tier <= 2) T1Stadiums.Add(A.Stadium);
		S.Clubs.Add(C);
	}
	// National League: the heritage club plus generated non-league sides
	static const TCHAR* Towns[] = { TEXT("Ashbury"), TEXT("Bramford"), TEXT("Cottesmore"), TEXT("Dunholme"), TEXT("Elmswell"), TEXT("Fernley"), TEXT("Grimsdale"), TEXT("Hallowmere"),
		TEXT("Ivybridge Vale"), TEXT("Kirkby Moor"), TEXT("Lowther"), TEXT("Marshfield"), TEXT("Northop"), TEXT("Oakhurst"), TEXT("Pendlebury"), TEXT("Quarrydale"), TEXT("Redmoor"),
		TEXT("Saltcote"), TEXT("Thornaby Moor"), TEXT("Upton Weir"), TEXT("Wexcombe"), TEXT("Yarrowby"), TEXT("Brackenfield"), TEXT("Coldharbour"), TEXT("Drayholt") };
	static const TCHAR* Suffix[] = { TEXT("Town"), TEXT("United"), TEXT("Rovers"), TEXT("Athletic"), TEXT("Borough"), TEXT("Wanderers"), TEXT("Albion"), TEXT("FC") };
	static const TCHAR* Cols[] = { TEXT("#C8102E"), TEXT("#1D428A"), TEXT("#F2F3EE"), TEXT("#FFD100"), TEXT("#00843D"), TEXT("#6CACE4"), TEXT("#7A263A"), TEXT("#F57F20"), TEXT("#111111"), TEXT("#5C2D91") };
	int32 NL = 0;
	for (const FPFCareerClub& C : S.Clubs) if (C.Division == 4) NL++;
	int32 T = 0;
	while (NL < 24)
	{
		FPFCareerClub C;
		C.ClubId = FString::Printf(TEXT("G%03d"), T);
		C.Name = FString::Printf(TEXT("%s %s"), Towns[T % UE_ARRAY_COUNT(Towns)], Suffix[Rng.Int(UE_ARRAY_COUNT(Suffix))]);
		C.Short = FString(Towns[T % UE_ARRAY_COUNT(Towns)]).Left(3).ToUpper();
		C.Prestige = 1; C.Division = 4;
		const FString A = Cols[Rng.Int(10)]; FString B = Cols[Rng.Int(10)]; if (B == A) B = TEXT("#F2F3EE");
		auto Kit = [](const FString& Sh, const FString& Tr, const FString& Sho, int32 Pat) { FPFKit K; K.Shirt = PFWorld::Hex(Sh); K.Trim = PFWorld::Hex(Tr); K.Shorts = PFWorld::Hex(Sho); K.Socks = PFWorld::Hex(Sh); K.Pattern = Pat; return K; };
		C.Kits.Home = Kit(A, B, Rng.Unit() < 0.5f ? A : B, Rng.Unit() < 0.25f ? 1 : Rng.Unit() < 0.15f ? 2 : 0);
		C.Kits.Away = Kit(B, A, A, 0);
		C.Kits.Keeper1 = Kit(TEXT("#2E7D32"), TEXT("#111111"), TEXT("#111111"), 0);
		C.Kits.Keeper2 = Kit(TEXT("#F9A825"), TEXT("#111111"), TEXT("#111111"), 0);
		C.Stadium = T1Stadiums.Num() ? T1Stadiums[Rng.Int(T1Stadiums.Num())] : TEXT("arsenal");
		S.Clubs.Add(C);
		NL++; T++;
	}
	// finances and squads
	for (int32 i = 0; i < S.Clubs.Num(); ++i)
	{
		FPFCareerClub& C = S.Clubs[i];
		const float PF = 0.7f + 0.15f * C.Prestige;
		const int64 Rev = int64(GDivRevenue[C.Division] * PF);
		C.Balance = Rev / 3; C.TransferBudget = Rev / 5; C.WageBudget = int32(Rev * 0.55 / 52);
		C.FacilityLevel = FMath::Clamp(C.Prestige, 1, 5);
		const int32 Base = GDivBaseOvr[C.Division] + (C.Prestige - 3) * 3;
		const EPFRole Roles[] = { EPFRole::GK, EPFRole::GK, EPFRole::GK, EPFRole::DEF, EPFRole::DEF, EPFRole::DEF, EPFRole::DEF, EPFRole::DEF, EPFRole::DEF, EPFRole::DEF, EPFRole::DEF,
			EPFRole::MID, EPFRole::MID, EPFRole::MID, EPFRole::MID, EPFRole::MID, EPFRole::MID, EPFRole::MID, EPFRole::MID, EPFRole::FWD, EPFRole::FWD, EPFRole::FWD, EPFRole::FWD, EPFRole::FWD };
		for (int32 k = 0; k < UE_ARRAY_COUNT(Roles); ++k)
		{
			const int32 Age = FMath::Clamp(int32(17 + FMath::Abs(Rng.Gauss()) * 6 + Rng.Int(8)), 17, 36);
			const EPFRegion Reg = Rng.Unit() < (C.Division <= 1 ? 0.5f : 0.8f) ? (Rng.Unit() < 0.85f ? EPFRegion::England : EPFRegion::Celtic) : (EPFRegion)Rng.Int((int32)EPFRegion::Count);
			const int32 Ovr = Base + int32(Rng.Gauss() * 5.f) + (k < 1 || (k >= 3 && k < 7) || (k >= 11 && k < 15) || (k >= 19 && k < 21) ? 3 : -3);
			NewPerson(i, Roles[k], Ovr, Age, Reg);
		}
		// academy
		for (int32 k = 0; k < 5; ++k)
		{
			const int32 Id = NewPerson(i, (EPFRole)(k == 0 ? 0 : 1 + Rng.Int(3)), 28 + Rng.Int(14) + C.FacilityLevel * 2, 15 + Rng.Int(3), EPFRegion::England);
			C.Squad.Remove(Id); C.Academy.Add(Id);
			Person(Id)->bAcademy = true;
		}
		RecalcStrength(i);
	}
	// world youth pool per region (what scouts find)
	for (int32 r = 0; r < (int32)EPFRegion::Count; ++r)
		for (int32 k = 0; k < 30; ++k)
		{
			const int32 Id = NewPerson(-1, (EPFRole)(Rng.Unit() < 0.1f ? 0 : 1 + Rng.Int(3)), 25 + Rng.Int(22), 15 + Rng.Int(3), (EPFRegion)r);
			Person(Id)->bScoutedOnly = true;
		}
	// a random League Two club for the user
	TArray<int32> L2;
	for (int32 i = 0; i < S.Clubs.Num(); ++i) if (S.Clubs[i].Division == 3) L2.Add(i);
	S.UserClub = L2[Rng.Int(L2.Num())];
	S.Reputation = 15; S.BoardConfidence = 60;
	S.Scouts.Add({ PFWorld::RandomName(Rng), 2, EPFRegion::England, 0, 400 });
	StartSeason();
	AddNews(FString::Printf(TEXT("%s appointed manager of %s."), *S.ManagerName, *UserClub().Name));
}

void FPFCareerSim::AddNews(const FString& T)
{
	S.News.Insert({ S.Season, S.Matchday, T }, 0);
	if (S.News.Num() > 60) S.News.SetNum(60);
}

// ------------------------------------------------------------------------------------------------ fixtures
void FPFCareerSim::BuildFixtures()
{
	S.Fixtures.Reset();
	S.MaxMatchday = 46;
	for (int32 D = 0; D < 5; ++D)
	{
		TArray<int32> Teams;
		for (int32 i = 0; i < S.Clubs.Num(); ++i) if (S.Clubs[i].Division == D) Teams.Add(i);
		// shuffle
		for (int32 i = Teams.Num() - 1; i > 0; --i) Teams.Swap(i, Rng.Int(i + 1));
		if (Teams.Num() % 2) Teams.Add(-1);
		const int32 N = Teams.Num(), Rounds = (N - 1) * 2;
		TArray<int32> Rot = Teams;
		for (int32 R = 0; R < N - 1; ++R)
		{
			for (int32 k = 0; k < N / 2; ++k)
			{
				int32 A = Rot[k], B = Rot[N - 1 - k];
				if (A < 0 || B < 0) continue;
				if ((R + k) % 2) Swap(A, B);
				FPFFixture F1; F1.Home = A; F1.Away = B; F1.Comp = D; F1.Round = R; F1.Matchday = FMath::FloorToInt(R * 46.f / Rounds);
				FPFFixture F2; F2.Home = B; F2.Away = A; F2.Comp = D; F2.Round = R + N - 1; F2.Matchday = FMath::FloorToInt((R + N - 1) * 46.f / Rounds);
				S.Fixtures.Add(F1); S.Fixtures.Add(F2);
			}
			// circle method rotation (first stays)
			const int32 Last = Rot.Last();
			for (int32 i = N - 1; i > 1; --i) Rot[i] = Rot[i - 1];
			Rot[1] = Last;
		}
	}
}

void FPFCareerSim::BuildCups()
{
	S.Cups.Reset();
	FPFCup FA; FA.Name = TEXT("The Association Cup"); FA.Comp = 10; FA.RoundMatchdays = { 6, 12, 18, 24, 30, 36, 44 };
	FPFCup LC; LC.Name = TEXT("The League Trophy"); LC.Comp = 11; LC.RoundMatchdays = { 3, 9, 15, 21, 27, 33, 40 };
	for (int32 i = 0; i < S.Clubs.Num(); ++i) { FA.Alive.Add(i); if (S.Clubs[i].Division <= 3) LC.Alive.Add(i); }
	S.Cups.Add(FA); S.Cups.Add(LC);
}

void FPFCareerSim::CupDraw(FPFCup& C, int32 Matchday)
{
	TArray<int32> A = C.Alive;
	for (int32 i = A.Num() - 1; i > 0; --i) A.Swap(i, Rng.Int(i + 1));
	for (int32 i = 0; i + 1 < A.Num(); i += 2)
	{
		FPFFixture F; F.Home = A[i]; F.Away = A[i + 1]; F.Comp = C.Comp; F.Round = C.Round; F.Matchday = Matchday;
		S.Fixtures.Add(F);
	}
	// odd one out: a bye (stays alive)
}

void FPFCareerSim::StartSeason()
{
	S.Matchday = 0;
	S.Phase = TEXT("Season");
	S.bWindowOpen = true;
	S.BoardConfidence = FMath::Clamp(40 + S.BoardConfidence / 3, 40, 75);   // a fresh season starts with patience
	BuildFixtures();
	BuildCups();
	for (FPFCup& C : S.Cups) { C.Round = 0; CupDraw(C, C.RoundMatchdays[0]); }
	for (FPFPerson& P : S.People) { P.SeasonApps = 0; P.SeasonGoals = 0; }
	for (int32 i = 0; i < S.Clubs.Num(); ++i) { EnsureSquad(i); RecalcStrength(i); }
}

TArray<int32> FPFCareerSim::FixturesOn(int32 Matchday, bool bUserOnly) const
{
	TArray<int32> Out;
	for (int32 i = 0; i < S.Fixtures.Num(); ++i)
	{
		const FPFFixture& F = S.Fixtures[i];
		if (F.Matchday != Matchday) continue;
		if (bUserOnly && F.Home != S.UserClub && F.Away != S.UserClub) continue;
		Out.Add(i);
	}
	return Out;
}

int32 FPFCareerSim::UserFixture() const
{
	if (S.bUnemployed) return -1;
	for (int32 i : FixturesOn(S.Matchday, true)) if (!S.Fixtures[i].Played()) return i;
	return -1;
}

// ------------------------------------------------------------------------------------------------ team strength / selection
TArray<int32> FPFCareerSim::BestXI(int32 Club) const
{
	const FPFCareerClub& C = S.Clubs[Club];
	const TArray<EPFRole> Roles = PFWorld::FormationRoles(C.Formation);
	TArray<int32> Pool;
	for (int32 Id : C.Squad) if (const FPFPerson* P = Person(Id)) if (P->InjuryWeeks <= 0 && !P->bDropped) Pool.Add(Id);
	if (Pool.Num() < 11) for (int32 Id : C.Squad) if (const FPFPerson* P = Person(Id)) if (P->InjuryWeeks <= 0 && P->bDropped) Pool.Add(Id);
	TArray<int32> XI;
	for (EPFRole R : Roles)
	{
		int32 Best = -1; int32 BO = -1;
		for (int32 Id : Pool)
		{
			const FPFPerson* P = Person(Id);
			int32 Score = P->Ovr() - (P->Role == R ? 0 : (R == EPFRole::GK || P->Role == EPFRole::GK) ? 60 : 12);
			if (Score > BO) { BO = Score; Best = Id; }
		}
		if (Best >= 0) { XI.Add(Best); Pool.Remove(Best); }
	}
	return XI;
}

void FPFCareerSim::RecalcStrength(int32 Club)
{
	const TArray<int32> XI = BestXI(Club);
	float Sum = 0; int32 N = 0;
	for (int32 Id : XI) if (const FPFPerson* P = Person(Id)) { Sum += P->Ovr(); N++; }
	S.Clubs[Club].Strength = N ? Sum / FMath::Max(N, 11) * (N / 11.f) + (N < 11 ? -(11 - N) * 3.f : 0.f) : 20.f;
}

float FPFCareerSim::TeamStrength(int32 Club) const { return S.Clubs[Club].Strength; }

int32 FPFCareerSim::Expectation(int32 Club) const
{
	const int32 D = S.Clubs[Club].Division;
	int32 Rank = 1;
	for (int32 i = 0; i < S.Clubs.Num(); ++i) if (i != Club && S.Clubs[i].Division == D && S.Clubs[i].Strength + S.Clubs[i].Prestige * 0.5f > S.Clubs[Club].Strength + S.Clubs[Club].Prestige * 0.5f) Rank++;
	return Rank;
}

// ------------------------------------------------------------------------------------------------ results
static int32 Poisson(PF::FRng& R, float L)
{
	const float E = FMath::Exp(-L); float P = 1; int32 K = 0;
	do { K++; P *= R.Unit(); } while (P > E && K < 12);
	return K - 1;
}

void FPFCareerSim::QuickSim(FPFFixture& F)
{
	const float SH = TeamStrength(F.Home), SA = TeamStrength(F.Away);
	const float LH = 1.30f * FMath::Exp((SH - SA) / 16.f) + 0.18f, LA = 1.05f * FMath::Exp((SA - SH) / 16.f);
	int32 HG = Poisson(Rng, LH), AG = Poisson(Rng, LA);
	int32 PW = -1;
	if (F.Comp >= 10 && HG == AG) PW = Rng.Unit() < 0.5f + (SH - SA) * 0.01f ? F.Home : F.Away;
	ApplyResult(F, HG, AG, PW);
}

void FPFCareerSim::ApplyResult(FPFFixture& F, int32 HG, int32 AG, int32 PenWinner)
{
	F.HG = HG; F.AG = AG;
	if (F.Comp >= 10 && HG == AG) { F.bPens = true; F.PenWinner = PenWinner >= 0 ? PenWinner : (Rng.Unit() < 0.5f ? F.Home : F.Away); }
	// appearances and scorers (weighted to forwards)
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const int32 Club = Side == 0 ? F.Home : F.Away;
		const int32 Goals = Side == 0 ? HG : AG;
		const TArray<int32> XI = BestXI(Club);
		for (int32 Id : XI) if (FPFPerson* P = Person(Id)) { P->SeasonApps++; P->CareerApps++; }
		for (int32 g = 0; g < Goals && XI.Num(); ++g)
		{
			float Tot = 0; for (int32 Id : XI) { const FPFPerson* P = Person(Id); Tot += P->Role == EPFRole::FWD ? 6 : P->Role == EPFRole::MID ? 3 : P->Role == EPFRole::DEF ? 1 : 0.05f; }
			float R = Rng.Unit() * Tot;
			for (int32 Id : XI) { FPFPerson* P = Person(Id); R -= P->Role == EPFRole::FWD ? 6 : P->Role == EPFRole::MID ? 3 : P->Role == EPFRole::DEF ? 1 : 0.05f; if (R <= 0) { P->SeasonGoals++; P->CareerGoals++; break; } }
		}
		// knocks
		if (Rng.Unit() < 0.25f && XI.Num()) if (FPFPerson* P = Person(XI[Rng.Int(XI.Num())])) P->InjuryWeeks = 1 + Rng.Int(4);
	}
	if (F.Home == S.UserClub || F.Away == S.UserClub)
	{
		const bool bHome = F.Home == S.UserClub;
		BoardAfterMatch(bHome ? HG : AG, bHome ? AG : HG, bHome);
	}
}

void FPFCareerSim::BoardAfterMatch(int32 GF, int32 GA, bool bHome)
{
	const FPFCareerClub& U = UserClub();
	const int32 Exp = Expectation(S.UserClub);
	const int32 N = GDivSize[U.Division];
	// expected points per game from expected position: top ~2.1, bottom ~0.8
	const float ExpPts = 1.8f - 0.9f * (Exp - 1) / float(N - 1);
	const int32 Pts = GF > GA ? 3 : GF == GA ? 1 : 0;
	S.BoardConfidence = FMath::Clamp(S.BoardConfidence + FMath::RoundToInt((Pts - ExpPts) * 1.6f), 0, 100);
}

void FPFCareerSim::PlayMatchday(const TFunction<void(FPFFixture&)>& UserResult)
{
	const int32 MD = S.Matchday;
	for (int32 i : FixturesOn(MD))
	{
		FPFFixture& F = S.Fixtures[i];
		if (F.Played()) continue;
		QuickSim(F);
	}
	// cups: advance rounds played today
	for (FPFCup& C : S.Cups)
	{
		if (C.Winner >= 0 || !C.RoundMatchdays.IsValidIndex(C.Round) || C.RoundMatchdays[C.Round] != MD) continue;
		TSet<int32> Out;
		for (const FPFFixture& F : S.Fixtures)
			if (F.Comp == C.Comp && F.Round == C.Round && F.Played())
			{
				const int32 W = F.bPens ? F.PenWinner : (F.HG > F.AG ? F.Home : F.Away);
				Out.Add(W == F.Home ? F.Away : F.Home);
			}
		C.Alive.RemoveAll([&Out](int32 X) { return Out.Contains(X); });
		C.Round++;
		if (C.Alive.Num() == 1)
		{
			C.Winner = C.Alive[0];
			AddNews(FString::Printf(TEXT("%s win %s!"), *S.Clubs[C.Winner].Name, *C.Name));
			if (C.Winner == S.UserClub) { S.Trophies++; S.Reputation = FMath::Min(100, S.Reputation + 8); S.BoardConfidence = FMath::Min(100, S.BoardConfidence + 20); }
		}
		else if (C.RoundMatchdays.IsValidIndex(C.Round)) CupDraw(C, C.RoundMatchdays[C.Round]);
		else
		{
			// out of scheduled rounds: play a final on the last matchday
			C.RoundMatchdays.Add(FMath::Min(S.MaxMatchday - 1, MD + 2));
			CupDraw(C, C.RoundMatchdays.Last());
		}
	}
	// weekly upkeep
	for (FPFPerson& P : S.People) if (P.InjuryWeeks > 0) P.InjuryWeeks--;
	for (int32 i = 0; i < S.Clubs.Num(); ++i)
	{
		FPFCareerClub& C = S.Clubs[i];
		const float PF = 0.7f + 0.15f * C.Prestige;
		C.Balance += int64(GDivRevenue[C.Division] * PF / 46) - SquadWages(i);
	}
	S.bWindowOpen = (MD + 1) < 4 || ((MD + 1) >= 22 && (MD + 1) <= 25);
	ScoutTick();
	ProcessBids();
	if (S.bWindowOpen) AIWindowActivity();
	for (int32 i = 0; i < S.Clubs.Num(); ++i) RecalcStrength(i);
	// mid-season sacking
	if (!S.bUnemployed && MD >= 15 && S.BoardConfidence < 10)
	{
		AddNews(FString::Printf(TEXT("%s have sacked %s."), *UserClub().Name, *S.ManagerName));
		S.bUnemployed = true;
		S.Reputation = FMath::Max(1, S.Reputation - 6);
		S.Offers.Reset();
		// clubs a little below the manager's standing
		TArray<int32> Cand;
		for (int32 i = 0; i < S.Clubs.Num(); ++i) if (i != S.UserClub && S.Clubs[i].Division >= FMath::Min(4, UserClub().Division)) Cand.Add(i);
		for (int32 k = 0; k < 3 && Cand.Num(); ++k) { const int32 Pick = Cand[Rng.Int(Cand.Num())]; Cand.Remove(Pick); S.Offers.Add({ Pick, TEXT("Looking for a new manager") }); }
	}
	if (MD == 36 && !S.bUnemployed)
	{
		int32 N = 0;
		for (int32 Id : UserClub().Squad) if (const FPFPerson* P = Person(Id)) if (P->ContractYears <= 1) N++;
		if (N) AddNews(FString::Printf(TEXT("%d of your players are out of contract this summer. Offer new deals from the squad screen."), N));
	}
	S.Matchday++;
	if (SeasonOver()) EndSeason();
}

TArray<FPFTableRow> FPFCareerSim::Table(int32 Division) const
{
	TMap<int32, FPFTableRow> M;
	for (int32 i = 0; i < S.Clubs.Num(); ++i) if (S.Clubs[i].Division == Division) { FPFTableRow R; R.Club = i; M.Add(i, R); }
	for (const FPFFixture& F : S.Fixtures)
	{
		if (F.Comp != Division || !F.Played()) continue;
		FPFTableRow* H = M.Find(F.Home); FPFTableRow* A = M.Find(F.Away);
		if (!H || !A) continue;
		H->P++; A->P++; H->GF += F.HG; H->GA += F.AG; A->GF += F.AG; A->GA += F.HG;
		if (F.HG > F.AG) { H->W++; A->L++; H->Pts += 3; }
		else if (F.HG < F.AG) { A->W++; H->L++; A->Pts += 3; }
		else { H->D++; A->D++; H->Pts++; A->Pts++; }
	}
	TArray<FPFTableRow> Out;
	M.GenerateValueArray(Out);
	Out.Sort([](const FPFTableRow& A, const FPFTableRow& B) { if (A.Pts != B.Pts) return A.Pts > B.Pts; if (A.GD() != B.GD()) return A.GD() > B.GD(); if (A.GF != B.GF) return A.GF > B.GF; return A.Club < B.Club; });
	return Out;
}

// ------------------------------------------------------------------------------------------------ end of season
void FPFCareerSim::Develop(FPFPerson& P, float Weeks)
{
	const int32 O = P.Ovr();
	float Delta = 0;
	const float Gap = float(P.Potential - O);
	const float Train = S.Training == TEXT("Youth") && P.bAcademy ? 1.3f : 1.f;
	if (P.Age <= 21) Delta = FMath::Max(0.f, Gap) * (0.30f + Rng.Unit() * 0.25f) * Train + (P.SeasonApps > 15 ? 1.f : 0.f);
	else if (P.Age <= 25) Delta = FMath::Max(0.f, Gap) * (0.25f + Rng.Unit() * 0.2f);
	else if (P.Age <= 29) Delta = Rng.Range(-1.f, 1.5f);
	else if (P.Age <= 32) Delta = -Rng.Range(1.f, 4.f);
	else Delta = -Rng.Range(3.f, 7.f);
	Delta *= Weeks / 46.f;
	auto Bump = [&](uint8& V, float Extra) { V = (uint8)FMath::Clamp(FMath::RoundToInt(V + Delta + Extra + Rng.Gauss() * 1.2f), 1, 99); };
	const float PaceExtra = P.Age >= 30 ? -1.5f : 0.f;
	Bump(P.Stats.Pace, PaceExtra); Bump(P.Stats.Accel, PaceExtra); Bump(P.Stats.Agility, PaceExtra * 0.5f); Bump(P.Stats.Control, 0); Bump(P.Stats.Passing, 0); Bump(P.Stats.Shooting, 0);
	Bump(P.Stats.Heading, 0); Bump(P.Stats.Tackling, 0); Bump(P.Stats.Positioning, P.Age >= 28 ? 0.5f : 0); Bump(P.Stats.Reactions, 0); Bump(P.Stats.Stamina, PaceExtra);
	if (P.Role == EPFRole::GK) Bump(P.Stats.Keeping, P.Age >= 30 ? 1.f : 0.f);
}

void FPFCareerSim::Regens()
{
	// academy intakes
	for (int32 i = 0; i < S.Clubs.Num(); ++i)
	{
		FPFCareerClub& C = S.Clubs[i];
		const int32 N = 2 + Rng.Int(2) + (C.FacilityLevel >= 4 ? 1 : 0);
		for (int32 k = 0; k < N; ++k)
		{
			const int32 Id = NewPerson(i, (EPFRole)(Rng.Unit() < 0.1f ? 0 : 1 + Rng.Int(3)), 26 + Rng.Int(14) + C.FacilityLevel * 2, 15 + Rng.Int(2), Rng.Unit() < 0.85f ? EPFRegion::England : EPFRegion::Celtic);
			C.Squad.Remove(Id); C.Academy.Add(Id);
			Person(Id)->bAcademy = true;
		}
	}
	// world youth pool refresh
	for (int32 r = 0; r < (int32)EPFRegion::Count; ++r)
		for (int32 k = 0; k < 18; ++k)
		{
			const int32 Id = NewPerson(-1, (EPFRole)(Rng.Unit() < 0.1f ? 0 : 1 + Rng.Int(3)), 25 + Rng.Int(22), 15 + Rng.Int(2), (EPFRegion)r);
			Person(Id)->bScoutedOnly = true;
		}
}

void FPFCareerSim::EnsureSquad(int32 Club)
{
	FPFCareerClub& C = S.Clubs[Club];
	int32 Counts[4] = { 0, 0, 0, 0 };
	for (int32 Id : C.Squad) if (const FPFPerson* P = Person(Id)) Counts[(int32)P->Role]++;
	const int32 Need[4] = { 2, 7, 7, 4 };
	const int32 Base = GDivBaseOvr[C.Division] + (C.Prestige - 3) * 3;
	for (int32 r = 0; r < 4; ++r)
		while (Counts[r] < Need[r])
		{
			// promote an academy player of that role if any, else sign a free agent
			int32 Promote = -1;
			for (int32 Id : C.Academy) { const FPFPerson* P = Person(Id); if (P && (int32)P->Role == r && P->Age >= 17) { Promote = Id; break; } }
			if (Promote >= 0) { C.Academy.Remove(Promote); C.Squad.Add(Promote); Person(Promote)->bAcademy = false; }
			else NewPerson(Club, (EPFRole)r, Base - 4 + Rng.Int(6), 21 + Rng.Int(12), EPFRegion::England);
			Counts[r]++;
		}
	int32 No = 1;
	for (int32 Id : C.Squad) if (FPFPerson* P = Person(Id)) P->Number = No++;
}

void FPFCareerSim::EndSeason()
{
	// final tables
	TArray<TArray<FPFTableRow>> Tables;
	for (int32 D = 0; D < 5; ++D) Tables.Add(Table(D));
	const int32 UDiv = S.bUnemployed ? -1 : UserClub().Division;
	int32 UPos = 0;
	if (UDiv >= 0) for (int32 i = 0; i < Tables[UDiv].Num(); ++i) if (Tables[UDiv][i].Club == S.UserClub) UPos = i + 1;
	const int32 Exp = S.bUnemployed ? 0 : Expectation(S.UserClub);
	S.LastSeasonFinish = UPos;
	// promotion / relegation: 3 up/down between each tier (Premier has 20)
	const int32 Move[4] = { 3, 3, 4, 2 };   // places exchanged between D and D+1
	TArray<int32> NewDiv; NewDiv.SetNum(S.Clubs.Num());
	for (int32 i = 0; i < S.Clubs.Num(); ++i) NewDiv[i] = S.Clubs[i].Division;
	for (int32 D = 0; D < 4; ++D)
	{
		const TArray<FPFTableRow>& Up = Tables[D];
		const TArray<FPFTableRow>& Lo = Tables[D + 1];
		for (int32 k = 0; k < Move[D]; ++k)
		{
			if (Up.Num() - 1 - k >= 0) NewDiv[Up[Up.Num() - 1 - k].Club] = D + 1;
			if (k < Lo.Num()) NewDiv[Lo[k].Club] = D;
		}
	}
	// champions
	for (int32 D = 0; D < 5; ++D) if (Tables[D].Num()) AddNews(FString::Printf(TEXT("%s are %s champions."), *S.Clubs[Tables[D][0].Club].Name, *DivisionName(D)));
	FString Line = S.bUnemployed ? FString::Printf(TEXT("%d/%02d  out of work"), S.Season, (S.Season + 1) % 100)
		: FString::Printf(TEXT("%d/%02d  %s  %s  %d%s"), S.Season, (S.Season + 1) % 100, *UserClub().Name, *DivisionName(UDiv), UPos, (UPos % 100 >= 11 && UPos % 100 <= 13) ? TEXT("th") : UPos % 10 == 1 ? TEXT("st") : UPos % 10 == 2 ? TEXT("nd") : UPos % 10 == 3 ? TEXT("rd") : TEXT("th"));
	bool bRelegated = false, bPromoted = false;
	if (!S.bUnemployed)
	{
		bRelegated = NewDiv[S.UserClub] > UDiv;
		bPromoted = NewDiv[S.UserClub] < UDiv;
		if (bPromoted) Line += TEXT("  PROMOTED");
		if (bRelegated) Line += TEXT("  RELEGATED");
		// reputation and board
		S.Reputation = FMath::Clamp(S.Reputation + FMath::RoundToInt((Exp - UPos) * 1.5f) + (bPromoted ? 6 : 0) - (bRelegated ? 8 : 0) + (UPos == 1 ? 6 : 0), 1, 100);
		S.BoardConfidence = FMath::Clamp(S.BoardConfidence + (Exp - UPos) * 2 + (bPromoted ? 25 : 0), 0, 100);
	}
	for (const FPFCup& C : S.Cups) if (C.Winner == S.UserClub) Line += FString::Printf(TEXT("  %s WINNERS"), *C.Name.ToUpper());
	S.History.Add(Line);
	for (int32 i = 0; i < S.Clubs.Num(); ++i)
	{
		FPFCareerClub& C = S.Clubs[i];
		if (NewDiv[i] < C.Division) C.Prestige = FMath::Min(5, C.Prestige + (Rng.Unit() < 0.35f ? 1 : 0));
		if (NewDiv[i] > C.Division) C.Prestige = FMath::Max(1, C.Prestige - (Rng.Unit() < 0.25f ? 1 : 0));
		C.Division = NewDiv[i];
	}
	// people: ageing, development, contracts, retirements
	TArray<int32> Retire;
	for (FPFPerson& P : S.People)
	{
		Develop(P, 46);
		P.Age++;
		if (P.Club >= 0 && !P.bAcademy) P.ContractYears--;
		if (P.bScoutedOnly && P.Club < 0 && P.Age > 18) Retire.Add(P.Id);          // unsigned youths drift out of football
		else if (P.Age >= 34 && Rng.Unit() < (P.Age - 33) * 0.3f) Retire.Add(P.Id);
		else if (P.Club < 0 && !P.bScoutedOnly && Rng.Unit() < 0.4f) Retire.Add(P.Id); // unattached veterans
	}
	for (int32 i = 0; i < S.Clubs.Num(); ++i)
	{
		FPFCareerClub& C = S.Clubs[i];
		// AI clubs renew most contracts; expired AI players leave
		for (int32 k = C.Squad.Num() - 1; k >= 0; --k)
		{
			FPFPerson* P = Person(C.Squad[k]);
			if (!P) { C.Squad.RemoveAt(k); continue; }
			if (Retire.Contains(P->Id)) { C.Squad.RemoveAt(k); continue; }
			if (P->ContractYears <= 0)
			{
				if (i != S.UserClub && Rng.Unit() < 0.75f) P->ContractYears = 1 + Rng.Int(3);
				else if (i == S.UserClub) { AddNews(FString::Printf(TEXT("%s leaves %s as his contract runs out."), *P->Name, *C.Name)); P->Club = -1; P->bDropped = false; C.Squad.RemoveAt(k); }
				else { P->Club = -1; C.Squad.RemoveAt(k); }
			}
		}
		for (int32 k = C.Academy.Num() - 1; k >= 0; --k)
		{
			FPFPerson* P = Person(C.Academy[k]);
			if (!P) { C.Academy.RemoveAt(k); continue; }
			if (P->Age >= 19) { P->bAcademy = false; C.Academy.RemoveAt(k); if (P->Potential >= 50 || Rng.Unit() < 0.3f) { C.Squad.Add(P->Id); P->ContractYears = 2; } else P->Club = -1; }
		}
		// trim bloated squads (keep 28)
		while (C.Squad.Num() > 28)
		{
			int32 Worst = -1, WO = 999;
			for (int32 Id : C.Squad) { const FPFPerson* P = Person(Id); if (P && P->Ovr() < WO) { WO = P->Ovr(); Worst = Id; } }
			if (Worst < 0) break;
			C.Squad.Remove(Worst); Person(Worst)->Club = -1;
		}
		// finances for next season
		const float PF = 0.7f + 0.15f * C.Prestige;
		const int64 Rev = int64(GDivRevenue[C.Division] * PF);
		C.TransferBudget = FMath::Max<int64>(Rev / 6, C.Balance / 4);
		C.WageBudget = int32(Rev * 0.55 / 52);
	}
	S.People.RemoveAll([&Retire](const FPFPerson& P) { return Retire.Contains(P.Id); });
	S.Prospects.RemoveAll([this](int32 Id) { return Person(Id) == nullptr; });
	Regens();
	S.TotalSeasons++;
	// the manager's fate
	S.Offers.Reset();
	if (!S.bUnemployed)
	{
		S.SeasonsAtClub++;
		if (bRelegated)
		{
			AddNews(FString::Printf(TEXT("Relegation costs %s his job at %s."), *S.ManagerName, *UserClub().Name));
			S.bUnemployed = true;
		}
		else if (S.BoardConfidence < 20)
		{
			AddNews(FString::Printf(TEXT("The %s board have lost faith. %s is dismissed."), *UserClub().Name, *S.ManagerName));
			S.bUnemployed = true;
		}
	}
	// job offers (always at least one if out of work)
	const int32 Standing = S.Reputation;
	TArray<TPair<float, int32>> Scored;
	for (int32 i = 0; i < S.Clubs.Num(); ++i)
	{
		if (i == S.UserClub) continue;
		const float ClubScore = (4 - S.Clubs[i].Division) * 20.f + S.Clubs[i].Prestige * 4.f;
		const float Target = S.bUnemployed ? Standing - 12.f : Standing + 4.f;
		Scored.Add({ FMath::Abs(ClubScore - Target) + Rng.Range(0.f, 10.f), i });
	}
	Scored.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
	const int32 NOff = S.bUnemployed ? 3 : (Exp - UPos >= 3 || bPromoted ? 2 + Rng.Int(2) : (Rng.Unit() < 0.35f ? 1 : 0));
	for (int32 k = 0; k < NOff && k < Scored.Num(); ++k) S.Offers.Add({ Scored[k].Value, S.bUnemployed ? TEXT("Needs a new manager") : TEXT("Wants to talk to you") });
	S.Phase = TEXT("Offers");
	S.Season++;
	S.Matchday = 0;
}

void FPFCareerSim::AcceptOffer(int32 OfferIndex)
{
	if (S.Offers.IsValidIndex(OfferIndex))
	{
		const int32 Club = S.Offers[OfferIndex].Club;
		if (Club != S.UserClub) { S.SeasonsAtClub = 0; }
		S.UserClub = Club;
		S.bUnemployed = false;
		S.BoardConfidence = 60;
		AddNews(FString::Printf(TEXT("%s takes charge of %s."), *S.ManagerName, *UserClub().Name));
	}
	S.Offers.Reset();
	if (S.Phase == TEXT("Offers")) StartSeason();
}

// ------------------------------------------------------------------------------------------------ match setup
FPFMatchSetup FPFCareerSim::MatchSetup(const FPFFixture& F, bool bUserPlays) const
{
	FPFMatchSetup M;
	M.Seed = S.Seed ^ (uint64(S.Season) << 20) ^ (uint64(S.Matchday) << 8) ^ uint64(F.Home * 131 + F.Away);
	auto Team = [&](int32 Club, bool bAway) -> FPFTeamSetup
	{
		const FPFCareerClub& C = S.Clubs[Club];
		FPFTeamSetup T;
		T.Name = C.Name; T.Short = C.Short; T.BadgePath = C.Badge;
		T.Kit = bAway ? C.Kits.Away : C.Kits.Home;
		T.KeeperKit = C.Kits.Keeper1;
		const TArray<int32> XI = BestXI(Club);
		const TArray<FVector2D> Slots = PFWorld::FormationSlots(C.Formation);
		const TArray<EPFRole> Roles = PFWorld::FormationRoles(C.Formation);
		for (int32 i = 0; i < XI.Num() && i < 11; ++i)
		{
			const FPFPerson* P = Person(XI[i]);
			FPFPlayerSetup PS;
			PS.Name = P->Name; PS.Number = P->Number ? P->Number : i + 1; PS.Role = Roles[i]; PS.Stats = P->Stats; PS.Sprite = P->Sprite; PS.Slot = Slots[i];
			T.XI.Add(PS);
		}
		return T;
	};
	M.Home = Team(F.Home, false);
	M.Away = Team(F.Away, false);
	if (PFWorld::KitsClash(M.Home.Kit, M.Away.Kit)) M.Away.Kit = S.Clubs[F.Away].Kits.Away;
	if (PFWorld::KitsClash(M.Home.KeeperKit, M.Away.KeeperKit)) M.Away.KeeperKit = S.Clubs[F.Away].Kits.Keeper2;
	M.Home.bHuman = bUserPlays && F.Home == S.UserClub;
	M.Away.bHuman = bUserPlays && F.Away == S.UserClub;
	M.bWatchOnly = !bUserPlays;
	M.StadiumSlug = S.Clubs[F.Home].Stadium;
	const float R = PF::FRng(M.Seed).Unit();
	M.Stage = R < 0.6f ? TEXT("Daylight") : R < 0.8f ? TEXT("Sunset") : TEXT("Floodlit_Night");
	return M;
}

// ------------------------------------------------------------------------------------------------ transfers
int64 FPFCareerSim::Value(const FPFPerson& P) const
{
	double V = 50000.0 * FMath::Exp((P.Ovr() - 45) * 0.154);
	if (P.Age <= 21) V *= 1.0 + FMath::Max(0, P.Potential - P.Ovr()) / 20.0;
	else if (P.Age >= 33) V *= 0.3;
	else if (P.Age >= 30) V *= 0.6;
	if (P.ContractYears <= 1) V *= 0.65;
	return int64(FMath::Max(5000.0, FMath::RoundToDouble(V / 5000.0) * 5000.0));
}

int32 FPFCareerSim::WageDemand(const FPFPerson& P, int32 ToClub) const
{
	const float Base = 300.f * FMath::Exp((P.Ovr() - 45) * 0.1f);
	const float Drop = ToClub >= 0 ? FMath::Max(0, S.Clubs[ToClub].Division - (P.Club >= 0 ? S.Clubs[P.Club].Division : 4)) * 0.25f : 0.f;
	return FMath::Max(100, int32(FMath::Max(Base, (float)P.Wage * 1.1f) * (1.f + Drop)));
}

int32 FPFCareerSim::SquadWages(int32 Club) const
{
	int32 W = 0;
	for (int32 Id : S.Clubs[Club].Squad) if (const FPFPerson* P = Person(Id)) W += P->Wage;
	if (Club == S.UserClub) for (const FPFScout& Sc : S.Scouts) W += Sc.Wage;
	return W;
}

FString FPFCareerSim::MakeBid(int32 Id, int64 Fee, int32 Wage)
{
	const FPFPerson* P = Person(Id);
	if (!P || S.bUnemployed) return TEXT("No.");
	if (!S.bWindowOpen && P->Club >= 0) return TEXT("The transfer window is closed.");
	if (P->Club == S.UserClub) return TEXT("He already plays for you.");
	if (Fee > UserClub().TransferBudget) return TEXT("Not enough in the transfer budget.");
	if (SquadWages(S.UserClub) + Wage > UserClub().WageBudget * 1.15f) return TEXT("The board won't sanction those wages.");
	for (const FPFBid& B : S.Bids) if (B.bByUser && B.Person == Id && B.Status == 0) return TEXT("You already have a bid in.");
	FPFBid B; B.Person = Id; B.FromClub = S.UserClub; B.Fee = Fee; B.Wage = Wage; B.bByUser = true;
	S.Bids.Add(B);
	// rival interest: clubs that can afford him may also bid
	const int32 O = P->Ovr();
	for (int32 i = 0; i < S.Clubs.Num(); ++i)
	{
		if (i == S.UserClub || i == P->Club) continue;
		const FPFCareerClub& C = S.Clubs[i];
		if (C.Strength + 4 < O && Rng.Unit() < 0.08f && C.TransferBudget > Value(*P))
		{
			FPFBid R; R.Person = Id; R.FromClub = i; R.Fee = int64(Value(*P) * Rng.Range(0.95f, 1.25f)); R.Wage = int32(WageDemand(*P, i) * Rng.Range(1.f, 1.2f));
			S.Bids.Add(R);
			AddNews(FString::Printf(TEXT("%s are also chasing %s."), *C.Name, *P->Name));
			break;
		}
	}
	return TEXT("Bid submitted. You'll hear back after the next match.");
}

void FPFCareerSim::ProcessBids()
{
	// group pending bids by player
	TMap<int32, TArray<int32>> ByPerson;
	for (int32 i = 0; i < S.Bids.Num(); ++i) if (S.Bids[i].Status == 0) ByPerson.FindOrAdd(S.Bids[i].Person).Add(i);
	for (auto& KV : ByPerson)
	{
		FPFPerson* P = Person(KV.Key);
		if (!P) { for (int32 i : KV.Value) S.Bids[i].Status = 2; continue; }
		const int64 V = Value(*P);
		// selling club decides on each bid (the user decides on bids for his own players in the UI)
		if (P->Club == S.UserClub) continue;
		TArray<int32> Accepted;
		for (int32 i : KV.Value)
		{
			FPFBid& B = S.Bids[i];
			float Need = P->Club < 0 ? 0.f : (P->bListed ? 0.8f : 1.0f);
			if (P->Club >= 0)
			{
				// key players cost more
				const TArray<int32> XI = BestXI(P->Club);
				if (XI.Contains(P->Id)) Need *= 1.35f;
			}
			if (B.Fee >= V * Need) Accepted.Add(i);
			else { B.Status = 2; B.Note = FString::Printf(TEXT("Rejected: they want about %lld."), int64(V * Need)); }
		}
		if (!Accepted.Num()) continue;
		// the player chooses: money weighted by club stature
		int32 Best = -1; float BestScore = -1;
		for (int32 i : Accepted)
		{
			const FPFBid& B = S.Bids[i];
			const FPFCareerClub& C = S.Clubs[B.FromClub];
			const float Demand = WageDemand(*P, B.FromClub);
			if (B.Wage < Demand * 0.9f) { S.Bids[i].Status = 4; S.Bids[i].Note = FString::Printf(TEXT("He wants %d a week."), int32(Demand)); continue; }
			const float Score = B.Wage * (1.f + (4 - C.Division) * 0.25f + C.Prestige * 0.05f) + Rng.Range(0.f, B.Wage * 0.15f);
			if (Score > BestScore) { BestScore = Score; Best = i; }
		}
		if (Best < 0) continue;
		FPFBid& W = S.Bids[Best];
		// complete
		const int32 From = P->Club;
		if (From >= 0) { S.Clubs[From].Squad.Remove(P->Id); S.Clubs[From].Academy.Remove(P->Id); S.Clubs[From].Balance += W.Fee; S.Clubs[From].TransferBudget += W.Fee / 2; }
		S.Clubs[W.FromClub].Squad.Add(P->Id);
		S.Clubs[W.FromClub].TransferBudget -= W.Fee; S.Clubs[W.FromClub].Balance -= W.Fee;
		P->Club = W.FromClub; P->Wage = W.Wage; P->ContractYears = 3; P->bListed = false; P->bAcademy = false; P->bScoutedOnly = false;
		W.Status = 3; W.Note = TEXT("Done deal.");
		for (int32 i : KV.Value) if (i != Best && S.Bids[i].Status == 0) { S.Bids[i].Status = 2; S.Bids[i].Note = FString::Printf(TEXT("He chose %s."), *S.Clubs[W.FromClub].Name); }
		for (int32 i : Accepted) if (i != Best && S.Bids[i].Status != 4) { S.Bids[i].Status = 2; S.Bids[i].Note = FString::Printf(TEXT("He chose %s."), *S.Clubs[W.FromClub].Name); }
		AddNews(FString::Printf(TEXT("%s joins %s from %s for %lld."), *P->Name, *S.Clubs[W.FromClub].Name, From >= 0 ? *S.Clubs[From].Name : TEXT("free agency"), W.Fee));
	}
	// AI offers for the user's listed players
	if (S.bWindowOpen && !S.bUnemployed)
		for (int32 Id : UserClub().Squad)
		{
			const FPFPerson* P = Person(Id);
			if (!P || !P->bListed || Rng.Unit() > 0.35f) continue;
			bool bHas = false; for (const FPFBid& B : S.Bids) if (B.Person == Id && B.Status == 0) bHas = true;
			if (bHas) continue;
			int32 Buyer = -1;
			for (int32 t = 0; t < 20 && Buyer < 0; ++t) { const int32 c = Rng.Int(S.Clubs.Num()); if (c != S.UserClub && S.Clubs[c].TransferBudget > Value(*P) * 0.8f) Buyer = c; }
			if (Buyer < 0) continue;
			FPFBid B; B.Person = Id; B.FromClub = Buyer; B.Fee = int64(Value(*P) * Rng.Range(0.75f, 1.1f)); B.Wage = WageDemand(*P, Buyer);
			S.Bids.Add(B);
			AddNews(FString::Printf(TEXT("%s bid %lld for %s."), *S.Clubs[Buyer].Name, B.Fee, *P->Name));
		}
	// keep the list short
	if (S.Bids.Num() > 80) S.Bids.RemoveAt(0, S.Bids.Num() - 80);
}

void FPFCareerSim::SellToBest(int32 Id)
{
	// the user accepts the best pending bid for one of his players
	FPFPerson* P = Person(Id);
	if (!P || P->Club != S.UserClub) return;
	int32 Best = -1;
	for (int32 i = 0; i < S.Bids.Num(); ++i) if (S.Bids[i].Person == Id && S.Bids[i].Status == 0 && !S.Bids[i].bByUser && (Best < 0 || S.Bids[i].Fee > S.Bids[Best].Fee)) Best = i;
	if (Best < 0) return;
	FPFBid& B = S.Bids[Best];
	UserClub().Squad.Remove(Id); UserClub().Balance += B.Fee; UserClub().TransferBudget += B.Fee;
	S.Clubs[B.FromClub].Squad.Add(Id); S.Clubs[B.FromClub].TransferBudget -= B.Fee;
	P->Club = B.FromClub; P->Wage = B.Wage; P->bListed = false; P->ContractYears = 3;
	B.Status = 3; B.Note = TEXT("Sold.");
	AddNews(FString::Printf(TEXT("%s sold to %s for %lld."), *P->Name, *S.Clubs[B.FromClub].Name, B.Fee));
}

void FPFCareerSim::AIWindowActivity()
{
	// a handful of AI moves per matchday so the world keeps changing
	for (int32 t = 0; t < 6; ++t)
	{
		const int32 Buyer = Rng.Int(S.Clubs.Num());
		if (Buyer == S.UserClub) continue;
		FPFCareerClub& C = S.Clubs[Buyer];
		// find a player from a lower club, better than the buyer's weakest in that role
		const int32 PIdx = Rng.Int(S.People.Num());
		FPFPerson& P = S.People[PIdx];
		if (P.Club == Buyer || P.Club == S.UserClub || P.bAcademy || P.bScoutedOnly || P.Age > 31) continue;
		if (P.Club >= 0 && S.Clubs[P.Club].Division <= C.Division) continue;
		if (P.Ovr() < C.Strength - 2) continue;
		const int64 Fee = P.Club >= 0 ? Value(P) : 0;
		if (Fee > C.TransferBudget) continue;
		const int32 From = P.Club;
		if (From >= 0) { S.Clubs[From].Squad.Remove(P.Id); S.Clubs[From].Balance += Fee; }
		C.Squad.Add(P.Id); C.TransferBudget -= Fee; C.Balance -= Fee;
		P.Club = Buyer; P.ContractYears = 2 + Rng.Int(3); P.Wage = WageDemand(P, Buyer);
		if (P.Ovr() >= 65) AddNews(FString::Printf(TEXT("%s sign %s%s."), *C.Name, *P.Name, From >= 0 ? *FString::Printf(TEXT(" from %s"), *S.Clubs[From].Name) : TEXT("")));
	}
}

// ------------------------------------------------------------------------------------------------ youth & scouting
void FPFCareerSim::HireScout()
{
	if (S.Scouts.Num() >= 4) return;
	S.Scouts.Add({ PFWorld::RandomName(Rng), 1 + Rng.Int(4), EPFRegion::England, 0, 300 + Rng.Int(5) * 150 });
}

void FPFCareerSim::SendScout(int32 Scout, EPFRegion Region)
{
	if (!S.Scouts.IsValidIndex(Scout)) return;
	S.Scouts[Scout].Region = Region;
	S.Scouts[Scout].WeeksLeft = 4;
	AddNews(FString::Printf(TEXT("Scout %s heads to %s."), *S.Scouts[Scout].Name, PFRegionName(Region)));
}

void FPFCareerSim::ScoutTick()
{
	for (FPFScout& Sc : S.Scouts)
	{
		if (Sc.WeeksLeft <= 0) continue;
		Sc.WeeksLeft--;
		// candidates in the region not yet found
		TArray<int32> Pool;
		for (const FPFPerson& P : S.People) if (P.bScoutedOnly && P.Club < 0 && P.Region == Sc.Region && !S.Prospects.Contains(P.Id)) Pool.Add(P.Id);
		if (!Pool.Num()) continue;
		// better scouts find higher-potential kids more often
		const int32 Finds = 1 + (Rng.Unit() < Sc.Quality * 0.15f ? 1 : 0);
		for (int32 f = 0; f < Finds && Pool.Num(); ++f)
		{
			int32 Best = Pool[Rng.Int(Pool.Num())];
			for (int32 k = 0; k < Sc.Quality; ++k) { const int32 C = Pool[Rng.Int(Pool.Num())]; if (Person(C)->Potential > Person(Best)->Potential) Best = C; }
			S.Prospects.Add(Best); Pool.Remove(Best);
		}
		if (Sc.WeeksLeft == 0) AddNews(FString::Printf(TEXT("%s is back from %s with a list of names."), *Sc.Name, PFRegionName(Sc.Region)));
	}
}

TArray<int32> FPFCareerSim::YouthMatchSquad(EPFRegion Region, int32 Count, uint64 Seed)
{
	// the region's found prospects first, then other local kids (they can be signed after the match too)
	TArray<int32> Out;
	for (int32 Id : S.Prospects) if (const FPFPerson* P = Person(Id)) if (P->Region == Region && P->Club < 0) Out.Add(Id);
	for (const FPFPerson& P : S.People) if (Out.Num() < Count && P.bScoutedOnly && P.Club < 0 && P.Region == Region && !Out.Contains(P.Id)) Out.Add(P.Id);
	while (Out.Num() < Count)
	{
		const int32 Id = NewPerson(-1, (EPFRole)(Rng.Unit() < 0.1f ? 0 : 1 + Rng.Int(3)), 25 + Rng.Int(22), 15 + Rng.Int(3), Region);
		Person(Id)->bScoutedOnly = true;
		Out.Add(Id);
	}
	Out.SetNum(Count);
	return Out;
}

FPFMatchSetup FPFCareerSim::YouthMatchSetup(EPFRegion Region, uint64 Seed) const
{
	FPFCareerSim* Self = const_cast<FPFCareerSim*>(this);
	TArray<int32> Kids = Self->YouthMatchSquad(Region, 22, Seed);
	FPFMatchSetup M;
	M.Seed = Seed;
	M.bWatchOnly = true;
	M.HalfSeconds = 75.f;
	M.Stage = TEXT("Daylight");
	TArray<FString> Grounds;
	for (const FPFCareerClub& C : S.Clubs) if (C.Prestige <= 2) Grounds.Add(C.Stadium);
	M.StadiumSlug = Grounds.Num() ? Grounds[int32(Seed % Grounds.Num())] : TEXT("arsenal");
	auto Kit = [](const TCHAR* A, const TCHAR* B, const TCHAR* C) { FPFKit K; K.Shirt = PFWorld::Hex(A); K.Trim = PFWorld::Hex(B); K.Shorts = PFWorld::Hex(C); K.Socks = PFWorld::Hex(A); return K; };
	const TArray<FVector2D> Slots = PFWorld::FormationSlots(TEXT("4-4-2"));
	const TArray<EPFRole> Roles = PFWorld::FormationRoles(TEXT("4-4-2"));
	for (int32 t = 0; t < 2; ++t)
	{
		FPFTeamSetup& T = t == 0 ? M.Home : M.Away;
		T.Name = FString::Printf(TEXT("%s %s"), PFRegionName(Region), t == 0 ? TEXT("Blues") : TEXT("Reds"));
		T.Short = t == 0 ? TEXT("BLU") : TEXT("RED");
		T.Kit = t == 0 ? Kit(TEXT("#1D428A"), TEXT("#F2F3EE"), TEXT("#F2F3EE")) : Kit(TEXT("#C8102E"), TEXT("#F2F3EE"), TEXT("#111111"));
		T.KeeperKit = t == 0 ? Kit(TEXT("#2E7D32"), TEXT("#111111"), TEXT("#111111")) : Kit(TEXT("#F9A825"), TEXT("#111111"), TEXT("#111111"));
		// keepers: pick GK-role kids if any
		TArray<int32> Side;
		for (int32 i = t; i < Kids.Num(); i += 2) Side.Add(Kids[i]);
		Side.Sort([this](int32 A, int32 B) { return (Person(A)->Role == EPFRole::GK) > (Person(B)->Role == EPFRole::GK); });
		for (int32 i = 0; i < 11 && i < Side.Num(); ++i)
		{
			const FPFPerson* P = Person(Side[i]);
			FPFPlayerSetup PS; PS.Name = P->Name; PS.Number = i + 1; PS.Role = Roles[i]; PS.Stats = P->Stats; PS.Sprite = Roles[i] == EPFRole::GK ? FString(TEXT("K01")) : P->Sprite; PS.Slot = Slots[i];
			T.XI.Add(PS);
		}
	}
	return M;
}

FString FPFCareerSim::SignProspect(int32 Id)
{
	FPFPerson* P = Person(Id);
	if (!P || P->Club >= 0 || S.bUnemployed) return TEXT("Not available.");
	const int64 Fee = 5000 + int64(P->Age <= 16 ? 0 : 10000);
	if (UserClub().TransferBudget < Fee) return TEXT("Not enough budget.");
	if (UserClub().Academy.Num() >= 16) return TEXT("Academy is full (16).");
	UserClub().TransferBudget -= Fee; UserClub().Balance -= Fee;
	P->Club = S.UserClub; P->bAcademy = true; P->bScoutedOnly = false; P->Wage = 150; P->ContractYears = 3;
	UserClub().Academy.Add(Id);
	S.Prospects.Remove(Id);
	AddNews(FString::Printf(TEXT("%s (%d, %s) joins the %s academy."), *P->Name, P->Age, PFRegionName(P->Region), *UserClub().Name));
	return FString::Printf(TEXT("%s signed for %lld."), *P->Name, Fee);
}

void FPFCareerSim::PromoteYouth(int32 Id)
{
	FPFPerson* P = Person(Id);
	if (!P || P->Club != S.UserClub || !P->bAcademy) return;
	UserClub().Academy.Remove(Id); UserClub().Squad.Add(Id); P->bAcademy = false; P->Wage = FMath::Max(P->Wage, 400);
	int32 No = 1; for (int32 X : UserClub().Squad) if (FPFPerson* Q = Person(X)) No = FMath::Max(No, Q->Number + 1);
	P->Number = No;
}

FString FPFCareerSim::OfferContract(int32 Id)
{
	FPFPerson* P = Person(Id);
	if (!P || P->Club != S.UserClub) return TEXT("No.");
	if (P->ContractYears >= 3) return FString::Printf(TEXT("%s is happy with his %d years left."), *P->Name, P->ContractYears);
	const int32 Demand = WageDemand(*P, S.UserClub);
	if (SquadWages(S.UserClub) - P->Wage + Demand > UserClub().WageBudget * 1.15f) return FString::Printf(TEXT("He wants %d a week. The board won't stretch to it."), Demand);
	P->Wage = Demand; P->ContractYears = 3; P->Morale = FMath::Min(100, P->Morale + 10);
	return FString::Printf(TEXT("%s signs a new 3-year deal at %d a week."), *P->Name, Demand);
}

void FPFCareerSim::ReleasePlayer(int32 Id)
{
	FPFPerson* P = Person(Id);
	if (!P || P->Club != S.UserClub) return;
	UserClub().Squad.Remove(Id); UserClub().Academy.Remove(Id);
	P->Club = -1; P->bAcademy = false;
}
