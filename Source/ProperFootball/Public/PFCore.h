// Proper Football - core helpers: logging, run-time PNG/JSON loading, the stadium projection.
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UTexture2D;
DECLARE_LOG_CATEGORY_EXTERN(LogPF, Log, All);

namespace PF
{
	// Absolute path of a project-relative data file (works in editor and packaged builds: files are staged NonUFS).
	FString DataPath(const FString& Rel);
	TSharedPtr<FJsonObject> LoadJson(const FString& Rel);
	bool LoadText(const FString& Rel, FString& Out);
	// Decodes a PNG into a transient, nearest-filtered, unmipped texture. bSRGB=false keeps raw 8-bit codes (kit key colours).
	UTexture2D* LoadTexture(const FString& Rel, bool bSRGB = true, bool bSmooth = false);   // bSmooth: bilinear (HD art), else nearest (pixel art)

	// Deterministic small RNG so matches can be simulated and replayed.
	struct FRng
	{
		uint64 S = 0x9E3779B97F4A7C15ull;
		explicit FRng(uint64 Seed = 1) : S(Seed * 0x9E3779B97F4A7C15ull + 1) {}
		uint32 Next() { S ^= S << 13; S ^= S >> 7; S ^= S << 17; return uint32(S >> 11); }
		float Unit() { return (Next() & 0xFFFFFF) / float(0x1000000); }
		float Range(float A, float B) { return A + (B - A) * Unit(); }
		int32 Int(int32 N) { return N <= 0 ? 0 : int32(Next() % uint32(N)); }
		float Gauss() { float U = FMath::Max(Unit(), 1e-6f), V = Unit(); return FMath::Sqrt(-2.f * FMath::Loge(U)) * FMath::Cos(2.f * PI * V); }
	};
}

// The owner's stadium atlases share one affine projection: world metres (x along the pitch, y across, z up) -> 4K atlas pixels.
struct FPFProjection
{
	FVector2D Origin = FVector2D(1920, 1080);
	FVector2D Ax = FVector2D(21.889, -1.9595);   // atlas delta per +1 m world x
	FVector2D Ay = FVector2D(-10.298, -8.840);   // per +1 m world y
	FVector2D Az = FVector2D(2.136, -22.533);    // per +1 m height
	FVector2D AtlasSize = FVector2D(3840, 2160);
	FVector2D KickoffView = FVector2D(716.8, 403.2);

	bool Load();
	FVector2D ToAtlas(const FVector& W) const { return Origin + Ax * W.X + Ay * W.Y + Az * W.Z; }
	FVector2D GroundToAtlas(const FVector2D& W) const { return Origin + Ax * W.X + Ay * W.Y; }
	// atlas pixel on the ground plane -> world metres
	FVector2D AtlasToGround(const FVector2D& A) const;
	// a world ground direction -> screen direction (atlas space, y down)
	FVector2D DirToScreen(const FVector2D& D) const { return Ax * D.X + Ay * D.Y; }
	FVector2D ScreenDirToWorld(const FVector2D& S) const;
};
