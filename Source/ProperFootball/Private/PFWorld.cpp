#include "PFWorld.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

namespace PFWorld
{
	FLinearColor Hex(const FString& H)
	{
		const FColor C = FColor::FromHex(H);
		// sRGB code values in 0..1: the sprite material linearises kit colours itself
		return FLinearColor(C.R / 255.f, C.G / 255.f, C.B / 255.f, 1.f);
	}

	static int32 PatternOf(const FString& S)
	{
		if (S == TEXT("vertical") || S == TEXT("stripes")) return 1;
		if (S == TEXT("hoops") || S == TEXT("horizontal")) return 2;
		if (S == TEXT("halves")) return 3;
		return 0;
	}

	static FPFKit KitFrom(const TSharedPtr<FJsonObject>& O)
	{
		FPFKit K;
		if (!O.IsValid()) return K;
		K.Shirt = Hex(O->GetStringField(TEXT("shirt")));
		K.Trim = Hex(O->GetStringField(TEXT("trim")));
		K.Shorts = Hex(O->GetStringField(TEXT("shorts")));
		K.Socks = Hex(O->GetStringField(TEXT("socks")));
		K.Pattern = PatternOf(O->GetStringField(TEXT("pattern")));
		return K;
	}

	const TArray<FPFClub>& Clubs()
	{
		static TArray<FPFClub> C;
		if (C.Num()) return C;
		TSharedPtr<FJsonObject> J = PF::LoadJson(TEXT("GameData/Clubs/clubs.json"));
		if (!J.IsValid()) return C;
		for (const TSharedPtr<FJsonValue>& V : J->GetArrayField(TEXT("items")))
		{
			const TSharedPtr<FJsonObject>& O = V->AsObject();
			FPFClub Cl;
			Cl.Id = O->GetStringField(TEXT("id")); Cl.Slug = O->GetStringField(TEXT("slug"));
			Cl.Name = O->GetStringField(TEXT("name")); Cl.Short = O->GetStringField(TEXT("short"));
			Cl.Tier = O->GetIntegerField(TEXT("tier")); Cl.OrigDivision = O->GetStringField(TEXT("division_2005_06"));
			const TSharedPtr<FJsonObject>& K = O->GetObjectField(TEXT("kits"));
			Cl.Kits.Home = KitFrom(K->GetObjectField(TEXT("home"))); Cl.Kits.Away = KitFrom(K->GetObjectField(TEXT("away")));
			Cl.Kits.Keeper1 = KitFrom(K->GetObjectField(TEXT("keeper1"))); Cl.Kits.Keeper2 = KitFrom(K->GetObjectField(TEXT("keeper2")));
			Cl.Badge = O->GetStringField(TEXT("badge")); Cl.BadgeSmall = O->GetStringField(TEXT("badge_small"));
			Cl.JerseyHome = O->GetStringField(TEXT("jersey_home")); Cl.JerseyAway = O->GetStringField(TEXT("jersey_away"));
			Cl.Stadium = O->GetStringField(TEXT("stadium"));
			C.Add(Cl);
		}
		UE_LOG(LogPF, Log, TEXT("PF clubs loaded: %d"), C.Num());
		return C;
	}

	const FPFClub* FindClub(const FString& Id)
	{
		for (const FPFClub& C : Clubs()) if (C.Id == Id) return &C;
		return nullptr;
	}

	static TArray<FString> ListSprites(const TCHAR* Prefix)
	{
		TArray<FString> Files, Out;
		IFileManager::Get().FindFiles(Files, *PF::DataPath(TEXT("GameData/Sprites/*.json")), true, false);
		for (const FString& F : Files) if (F.StartsWith(Prefix)) Out.Add(FPaths::GetBaseFilename(F));
		Out.Sort();
		return Out;
	}
	const TArray<FString>& OutfieldSprites() { static TArray<FString> S = ListSprites(TEXT("O")); return S; }
	const TArray<FString>& KeeperSprites() { static TArray<FString> S = ListSprites(TEXT("K")); return S; }

	FString RandomName(PF::FRng& R, bool bFull)
	{
		static const TCHAR* First[] = { TEXT("Alan"), TEXT("Barry"), TEXT("Craig"), TEXT("Darren"), TEXT("Danny"), TEXT("Gary"), TEXT("Gareth"), TEXT("Jamie"), TEXT("Jermaine"), TEXT("Kieran"),
			TEXT("Lee"), TEXT("Marcus"), TEXT("Neil"), TEXT("Paul"), TEXT("Rob"), TEXT("Ryan"), TEXT("Scott"), TEXT("Shaun"), TEXT("Steve"), TEXT("Tom"), TEXT("Wayne"), TEXT("Ade"),
			TEXT("Kofi"), TEXT("Luis"), TEXT("Marek"), TEXT("Nico"), TEXT("Olly"), TEXT("Rio"), TEXT("Sol"), TEXT("Tony"), TEXT("Zak"), TEXT("Callum"), TEXT("Dean"), TEXT("Ellis"), TEXT("Femi"),
			TEXT("Harvey"), TEXT("Ian"), TEXT("Jonjo"), TEXT("Kev"), TEXT("Lenny"), TEXT("Mo"), TEXT("Nathan"), TEXT("Phil"), TEXT("Reece"), TEXT("Stuart"), TEXT("Theo"), TEXT("Vince"), TEXT("Wes") };
		static const TCHAR* Last[] = { TEXT("Ashworth"), TEXT("Bramley"), TEXT("Calloway"), TEXT("Danbury"), TEXT("Ellison"), TEXT("Fenwick"), TEXT("Gristwood"), TEXT("Hartigan"), TEXT("Ingleby"),
			TEXT("Jessop"), TEXT("Kettering"), TEXT("Lomax"), TEXT("Marlow"), TEXT("Netherton"), TEXT("Okafor"), TEXT("Pemberton"), TEXT("Quarmby"), TEXT("Rowntree"), TEXT("Sowerby"), TEXT("Thwaite"),
			TEXT("Underhill"), TEXT("Varley"), TEXT("Whitlock"), TEXT("Yardley"), TEXT("Ashcroft"), TEXT("Bellamy"), TEXT("Crowther"), TEXT("Dunmore"), TEXT("Eastwood"), TEXT("Fairclough"),
			TEXT("Garside"), TEXT("Holroyd"), TEXT("Illingworth"), TEXT("Kenyon"), TEXT("Lister"), TEXT("Mensah"), TEXT("Norcross"), TEXT("Oduya"), TEXT("Pickering"), TEXT("Radcliffe"),
			TEXT("Shacklock"), TEXT("Tranter"), TEXT("Walmsley"), TEXT("Adeyemi"), TEXT("Brennan"), TEXT("Costa"), TEXT("Duffy"), TEXT("Haddock"), TEXT("Kowalski"), TEXT("Lindqvist"),
			TEXT("Mbeki"), TEXT("Novak"), TEXT("O'Rourke"), TEXT("Petrov"), TEXT("Rossi"), TEXT("Sandoval"), TEXT("Tiernan"), TEXT("Vidic"), TEXT("Wainwright"), TEXT("Akinola"), TEXT("Byrne") };
		const FString L = Last[R.Int(UE_ARRAY_COUNT(Last))];
		const FString F = First[R.Int(UE_ARRAY_COUNT(First))];
		return bFull ? F + TEXT(" ") + L : FString::Printf(TEXT("%c. %s"), F[0], *L);
	}

	FPFStats RollStats(PF::FRng& R, int32 Ovr, EPFRole Role)
	{
		auto S = [&](float Bias) { return (uint8)FMath::Clamp(FMath::RoundToInt(Ovr + Bias + R.Gauss() * 8.f), 1, 99); };
		FPFStats St;
		const bool bGK = Role == EPFRole::GK, bDef = Role == EPFRole::DEF, bFwd = Role == EPFRole::FWD;
		St.Pace = S(bFwd ? 5 : bGK ? -15 : 0); St.Accel = S(bFwd ? 5 : bGK ? -10 : 0); St.Agility = S(0); St.Control = S(bDef ? -6 : 3);
		St.Passing = S(Role == EPFRole::MID ? 6 : 0); St.Shooting = S(bFwd ? 9 : bDef ? -12 : bGK ? -30 : 0); St.Heading = S(bDef ? 6 : 0);
		St.Tackling = S(bDef ? 10 : bFwd ? -14 : bGK ? -25 : 0); St.Positioning = S(0); St.Reactions = S(0); St.Stamina = S(0);
		St.Keeping = bGK ? S(8) : (uint8)FMath::Clamp(R.Int(15) + 1, 1, 99);
		return St;
	}

	TArray<FVector2D> FormationSlots(const FString& F)
	{
		// x: 0 own goal .. 1 opposition goal (formation spot depth), y: -1..1 across
		if (F == TEXT("4-3-3"))
			return { {0.0, 0}, {0.18, -0.85}, {0.15, -0.3}, {0.15, 0.3}, {0.18, 0.85}, {0.42, -0.5}, {0.38, 0}, {0.42, 0.5}, {0.68, -0.8}, {0.74, 0}, {0.68, 0.8} };
		if (F == TEXT("5-3-2"))
			return { {0.0, 0}, {0.22, -0.95}, {0.14, -0.45}, {0.12, 0}, {0.14, 0.45}, {0.22, 0.95}, {0.42, -0.5}, {0.40, 0}, {0.42, 0.5}, {0.72, -0.25}, {0.72, 0.25} };
		// 4-4-2
		return { {0.0, 0}, {0.18, -0.85}, {0.15, -0.3}, {0.15, 0.3}, {0.18, 0.85}, {0.44, -0.85}, {0.40, -0.3}, {0.40, 0.3}, {0.44, 0.85}, {0.72, -0.25}, {0.72, 0.25} };
	}

	TArray<EPFRole> FormationRoles(const FString& F)
	{
		if (F == TEXT("4-3-3")) return { EPFRole::GK, EPFRole::DEF, EPFRole::DEF, EPFRole::DEF, EPFRole::DEF, EPFRole::MID, EPFRole::MID, EPFRole::MID, EPFRole::FWD, EPFRole::FWD, EPFRole::FWD };
		if (F == TEXT("5-3-2")) return { EPFRole::GK, EPFRole::DEF, EPFRole::DEF, EPFRole::DEF, EPFRole::DEF, EPFRole::DEF, EPFRole::MID, EPFRole::MID, EPFRole::MID, EPFRole::FWD, EPFRole::FWD };
		return { EPFRole::GK, EPFRole::DEF, EPFRole::DEF, EPFRole::DEF, EPFRole::DEF, EPFRole::MID, EPFRole::MID, EPFRole::MID, EPFRole::MID, EPFRole::FWD, EPFRole::FWD };
	}

	bool KitsClash(const FPFKit& A, const FPFKit& B)
	{
		// v0.9 bug fix: a striped (hooped, halved) shirt reads as both its colours, so black-with-white-stripes against
		// white-with-black-stripes clashes even though the base shirt colours are far apart
		auto D = [](const FLinearColor& X, const FLinearColor& Y) { return FMath::Abs(X.R - Y.R) + FMath::Abs(X.G - Y.G) + FMath::Abs(X.B - Y.B); };
		if (D(A.Shirt, B.Shirt) < 0.45f) return true;
		if (A.Pattern != 0 && B.Pattern != 0 && D(A.Shirt, B.Trim) < 0.45f && D(A.Trim, B.Shirt) < 0.45f) return true;
		if (A.Pattern != 0 && D(A.Trim, B.Shirt) < 0.45f && D(A.Shorts, B.Shorts) < 0.45f) return true;
		if (B.Pattern != 0 && D(B.Trim, A.Shirt) < 0.45f && D(A.Shorts, B.Shorts) < 0.45f) return true;
		return false;
	}

	FPFKit AwayKitFor(const FPFKit& HomeKit, const FPFKitSet& AwayKits, bool& bAwayKit)
	{
		bAwayKit = false;
		if (!KitsClash(HomeKit, AwayKits.Home)) return AwayKits.Home;
		bAwayKit = true;
		if (!KitsClash(HomeKit, AwayKits.Away)) return AwayKits.Away;
		auto D = [](const FLinearColor& X, const FLinearColor& Y) { return FMath::Abs(X.R - Y.R) + FMath::Abs(X.G - Y.G) + FMath::Abs(X.B - Y.B); };
		static const FLinearColor Cand[] = { FLinearColor(0.95f, 0.95f, 0.93f), FLinearColor(0.98f, 0.82f, 0.12f), FLinearColor(0.80f, 0.10f, 0.12f),
			FLinearColor(0.45f, 0.72f, 0.95f), FLinearColor(0.08f, 0.08f, 0.10f), FLinearColor(0.10f, 0.55f, 0.25f), FLinearColor(0.95f, 0.45f, 0.10f) };
		FLinearColor Best = Cand[0]; float BestD = -1.f;
		for (const FLinearColor& Cc : Cand)
		{
			const float Dd = FMath::Min(D(Cc, HomeKit.Shirt), HomeKit.Pattern != 0 ? D(Cc, HomeKit.Trim) : 9.f);
			if (Dd > BestD) { BestD = Dd; Best = Cc; }
		}
		FPFKit K;
		K.Shirt = Best; K.Pattern = 0;
		K.Trim = D(Best, FLinearColor::White) < 0.6f ? FLinearColor(0.08f, 0.08f, 0.10f) : FLinearColor(0.95f, 0.95f, 0.93f);
		K.Shorts = D(Best, HomeKit.Shorts) < 0.45f ? K.Trim : Best;
		K.Socks = K.Shorts;
		return K;
	}

	FPFTeamSetup FriendlyTeam(const FPFClub& C, bool bAwayKit, uint64 Seed)
	{
		PF::FRng R(Seed);
		FPFTeamSetup T;
		T.Name = C.Name; T.Short = C.Short; T.BadgePath = C.Badge;
		T.Kit = bAwayKit ? C.Kits.Away : C.Kits.Home;
		T.KeeperKit = C.Kits.Keeper1;
		T.ClubSlug = C.Slug; T.bAwayKit = bAwayKit;
		const TArray<FVector2D> Slots = FormationSlots(TEXT("4-4-2"));
		const TArray<EPFRole> Roles = FormationRoles(TEXT("4-4-2"));
		const int32 Base = 46 + C.Tier * 6;    // tier 1 ~52, tier 5 ~76
		const TArray<FString>& OS = OutfieldSprites();
		const TArray<FString>& KS = KeeperSprites();
		for (int32 i = 0; i < 11; ++i)
		{
			FPFPlayerSetup P;
			P.Role = Roles[i]; P.Slot = Slots[i]; P.Number = i + 1;
			P.Name = RandomName(R);
			P.Stats = RollStats(R, Base + R.Int(9) - 4, P.Role);
			P.Sprite = P.Role == EPFRole::GK ? (KS.Num() ? KS[R.Int(KS.Num())] : (OS.Num() ? OS[0] : TEXT("O01"))) : (OS.Num() ? OS[R.Int(OS.Num())] : TEXT("O01"));
			T.XI.Add(P);
		}
		return T;
	}
}
