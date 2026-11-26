// Proper Football - the football world: clubs (from the owner's packs), players, squads, name generation.
#pragma once

#include "CoreMinimal.h"
#include "PFMatch.h"

struct FPFKitSet { FPFKit Home, Away, Keeper1, Keeper2; };

struct FPFClub
{
	FString Id, Slug, Name, Short;
	int32 Tier = 1;               // prestige 1..5
	FString OrigDivision;         // 2005-06 placement (sets the starting division)
	FPFKitSet Kits;
	FString Badge, BadgeSmall, JerseyHome, JerseyAway;
	FString Stadium;              // stadium art slug
};

namespace PFWorld
{
	const TArray<FPFClub>& Clubs();
	const FPFClub* FindClub(const FString& Id);
	FLinearColor Hex(const FString& H);
	// Generated names: English-inspired, fictional.
	FString RandomName(PF::FRng& R, bool bFull = false);
	// Sprite base ids actually built (GameData/Sprites/*.json).
	const TArray<FString>& OutfieldSprites();
	const TArray<FString>& KeeperSprites();
	// A quick friendly squad for a club (career uses PFCareer's persistent players).
	FPFTeamSetup FriendlyTeam(const FPFClub& C, bool bAwayKit, uint64 Seed);
	// Stats around a target overall with a wide spread (so talent shows).
	FPFStats RollStats(PF::FRng& R, int32 Overall, EPFRole Role);
	TArray<FVector2D> FormationSlots(const FString& Formation);   // 11 slots, GK first
	TArray<EPFRole> FormationRoles(const FString& Formation);
	bool KitsClash(const FPFKit& A, const FPFKit& B);
	// v0.9: the away side's outfield kit against the home kit: home kit, else away kit, else a plain change kit in the
	// colour furthest from the home kit (1.2% of pairings clash with both of the away club's kits). bAwayKit is set
	// when it isn't the home kit.
	FPFKit AwayKitFor(const FPFKit& HomeKit, const FPFKitSet& AwayKits, bool& bAwayKit);
}
