// Sprite sets: one atlas per base character, built offline from PixelLab static poses (Tools/pipeline/build_atlas.py).
// The animation itself is assembled here in code from those stills.
#pragma once

#include "CoreMinimal.h"

class UTexture2D;

// Screen-space facing in 24 slots of 15 degrees, clockwise from "south" (facing the camera). v0.9.8: atlases may be drawn
// in 8 directions (south, south-east, ... every 45 degrees) or 12 (south, a030, a060, east, a120, ... every 30); a sprite
// set resolves any slot to the nearest direction it actually has for that pose.
enum class EPFDir : uint8 { S = 0, SE = 3, E = 6, NE = 9, N = 12, NW = 15, W = 18, SW = 21, Count = 24 };

struct FPFFrame
{
	FVector2D UV0 = FVector2D::ZeroVector;   // atlas UV of the cell's top-left
	FVector2D UV1 = FVector2D::ZeroVector;   // bottom-right
	bool bFlip = false;
	bool bValid = false;
};

struct FPFSpriteSet
{
	FString Id;
	FString Kind;                 // outfield / keeper / official / coach
	TObjectPtr<UTexture2D> Atlas = nullptr;
	int32 Cell = 96;
	FVector2D Pivot = FVector2D(48, 88);   // feet, in cell pixels
	FVector2D AtlasPx = FVector2D(1, 1);
	bool bKitKeyed = false;       // kit pixels carry key colours that the sprite material recolours
	TMap<FName, FPFFrame> Frames; // "dir/pose"
	TSet<FName> Poses;
	TMap<FName, FVector2D> Hands; // v0.8: "dir/pose" -> ball centre in cell pixels for ball-in-hands frames (<id>_hands.json)
	TMap<FName, TArray<FName>> Resolved;   // v0.9.8: pose -> the "dir/pose" key drawn for each of the 24 slots (nearest direction it has)
	TMap<FName, TArray<uint8>> ResolvedSlot; // pose -> the slot that key's direction sits in
	float DirStep = 45.f;         // degrees between the directions this set is drawn in (45 or 30)

	bool Load(const FString& InId);
	// Frame for a direction + pose; falls back to the same pose in the nearest direction, then the idle.
	const FPFFrame& Get(EPFDir Dir, FName Pose, uint8* OutSlot = nullptr) const;
	bool Has(FName Pose) const { return Poses.Contains(Pose); }
	// Where the hands are (cell pixels) for the frame Get() would return; null if not measured.
	const FVector2D* HandAt(EPFDir Dir, FName Pose) const;
	static const TCHAR* DirName(EPFDir D);
	static int32 SlotOf(const FString& DirName);   // -1 if unknown
};

// A timed clip of still poses (shots, passes, tackles, dives...). Locomotion cycles are distance-driven instead (PFMatch).
struct FPFClip
{
	TArray<FName> Poses;
	TArray<float> Times;   // seconds each pose is held (scaled by the player's animation rate)
	int32 EventIndex = -1; // pose index where the contact happens (ball struck)
	float Length() const { float L = 0; for (float T : Times) L += T; return L; }
	// Pose at time T; returns the index too.
	FName At(float T, int32* OutIndex = nullptr) const;
};

namespace PFClips
{
	const FPFClip& Get(FName Name);
}
