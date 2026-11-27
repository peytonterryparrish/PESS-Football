// Proper Football - v0.9.8 3D stadium: the ground round the 2D pitch modelled in world metres (boards, walls, tiered stands
// with real seat rows, roofs, tunnel, dugouts, floodlight pylons) and drawn through the pitch's own parallel projection as
// flat-shaded, painter-ordered triangles, so it lines up exactly with the grass and the players. The owner's 2D pixel fans
// sit on its seats, row by row (front views on the far side and the covered end, back views where the camera sees a stand
// from behind). -pfoldstadium keeps the v0.9.7 painted plate.
#pragma once

#include "CoreMinimal.h"
#include "PFCore.h"
#include "UObject/ObjectPtr.h"

class UCanvas;
class UTexture2D;
class FReferenceCollector;

// what the crowd is doing this frame (FPFMatch's CrowdMood / CrowdTeam / CrowdT)
struct FPFCrowdView
{
	int32 Mood = 0, Team = 0;
	float T = 0.f;
	bool bGoal = false;
	float Clock = 0.f;
	FLinearColor Light = FLinearColor::White;
};

class FPFStadium3D
{
public:
	// Pv: the drawing projection (heights already scaled by HeightScale). FanSheetRel: the home club's supporter sheet.
	void Build(const FPFProjection& Pv, const FString& FanSheetRel, const FLinearColor& HomeShirt, const FLinearColor& HomeTrim, const FString& Stage, uint32 Seed);
	bool IsReady() const { return Sheet != nullptr; }
	bool BuiltFor(const FPFProjection& Pv) const { return Sheet != nullptr && BuiltAz.Equals(Pv.Az, 1e-4); }

	// back to front: the ground outside the grass (before the pitch), the stands (after the pitch, before the players),
	// the near side (after the players: it is nearer the camera than anyone on the pitch)
	void DrawGround(UCanvas* C, const FVector2D& TL, float Zoom) const;
	void DrawStands(UCanvas* C, const FVector2D& TL, float Zoom, float FanScale, const FPFCrowdView& Crowd) const;
	void DrawNear(UCanvas* C, const FVector2D& TL, float Zoom, float FanScale, const FPFCrowdView& Crowd) const;

	// the lowest ground y (metres) the bottom of the view may show: the back of the near stand
	static constexpr float NearViewLimitY = -47.5f;
	// the pitch material's "is this grass" texture: solid grass, so the turf runs to the boards all round
	UTexture2D* GrassMask() const { return Green; }
	void AddReferencedObjects(FReferenceCollector& Collector);

private:
	struct FOp { FVector2f P[4]; FVector2f Lo, Hi; FLinearColor Col; int32 Fan = -1; };
	struct FFan { FVector2f Hip, Feet; uint8 Fi = 0; uint8 bBack = 0; uint8 Side = 0; uint32 Hash = 0; };
	enum ELayer { LGround, LStands, LNear, LNum };
	TArray<FOp> Ops[LNum];
	TArray<FFan> Fans;
	TObjectPtr<UTexture2D> Sheet = nullptr;
	TObjectPtr<UTexture2D> Green = nullptr;
	FVector2D BuiltAz = FVector2D::ZeroVector;
	int32 SheetW = 1, SheetH = 1, CellH = 80, FrontRows = 8;
	FVector2f WhiteUV = FVector2f(0, 0);
	float AtlasPerSpritePx = 0.4f;

	bool BuildSheet(const FString& FanSheetRel);
	void DrawLayer(UCanvas* C, int32 Layer, const FVector2D& TL, float Zoom, float FanScale, const FPFCrowdView& Crowd) const;
};
