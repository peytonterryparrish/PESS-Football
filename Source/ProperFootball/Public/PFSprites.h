// Sprite sets: one atlas per base character, built offline from PixelLab static poses (Tools/pipeline/build_atlas.py).
// The animation itself is assembled here in code from those stills.
#pragma once

#include "CoreMinimal.h"

class UTexture2D;

// 8 screen-space directions, clockwise from "south" (facing the camera), matching PixelLab's naming.
enum class EPFDir : uint8 { S, SE, E, NE, N, NW, W, SW, Count };

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

	bool Load(const FString& InId);
	// Frame for a direction + pose; falls back to the same pose in the nearest direction, then the idle.
	const FPFFrame& Get(EPFDir Dir, FName Pose) const;
	bool Has(FName Pose) const { return Poses.Contains(Pose); }
	// Where the hands are (cell pixels) for the frame Get() would return; null if not measured.
	const FVector2D* HandAt(EPFDir Dir, FName Pose) const;
	static const TCHAR* DirName(EPFDir D);
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
