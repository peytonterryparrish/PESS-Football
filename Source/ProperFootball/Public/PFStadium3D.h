// Proper Football - the 3D stadium (v0.9.8, textured v0.10.0): the ground round the 2D pitch modelled in world metres
// (A-frame boards, walls, tiered stands with real seat rows, roofs, tunnel, dugouts, floodlight pylons, a big screen, and
// the streets outside) and drawn through the pitch's own parallel projection as painter-ordered, pixel-art-textured
// triangles, so it lines up exactly with the grass and the players. The owner's 2D pixel fans sit on its seats, row by
// row (front views on the far side and the covered end, back views where the camera sees a stand from behind).
// -pfoldstadium keeps the v0.9.7 painted plate.
#pragma once

#include "CoreMinimal.h"
#include "PFCore.h"
#include "UObject/ObjectPtr.h"

class UCanvas;
class UTexture2D;
class FReferenceCollector;

// what the crowd is doing this frame (FPFMatch's CrowdMood / CrowdTeam / CrowdT), and what the big screen shows
struct FPFCrowdView
{
	int32 Mood = 0, Team = 0;
	float T = 0.f;
	bool bGoal = false;
	float Clock = 0.f;
	FLinearColor Light = FLinearColor::White;
	FString Screen1, Screen2;   // e.g. "CRA 1-0 MHF", "23:41"
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

	// test hooks for stadium captures (with -pfshots): -pfstadtour=S moves the camera round 16 ground points, S real
	// seconds each; -pfstadzooms=a,b,.. runs the whole tour once per zoom (CamZoom values, 0.5 = the whole ground);
	// -pfstadzoom=Z holds one zoom. TourCam returns true while touring (the camera is not clamped to the play area).
	static float TourZoom(float GameZoom, float ScreenH);
	static bool TourCam(FVector2D& Cam, const FPFProjection& Proj);

	// the pixel-art materials (PFStadium3DArt.cpp)
	static constexpr int32 NBoards = 12, NPhoto = 10;
	enum EMat : int32 { MWhite, MTread, MRiser, MAisle, MSeatBack, MSeatPan, MBrick, MWallGreen, MRoof, MFascia, MSteel, MLattice, MLamp, MTunnel,
		MTarmac, MPaving, MRunoff, MPerspex, MGate, MWindow, MBoardBack, MCoping, MSlate, MHouse, MTree, MCar, MScreen, MFlag, MBanner,
		MPhoto0, MBoard0 = MPhoto0 + NPhoto, MNum = MBoard0 + NBoards };
	static const TCHAR* BoardWord(int32 I);

private:
	enum EKind : uint8 { KFace, KFan, KScreen, KSprite };
	// one convex piece (a quad, or a triangle when N == 3) in atlas pixels with texel UVs; or a supporter, the big
	// screen's live face or a standing sprite (Idx into Fans / Screens / Sprites)
	struct FOp { FVector2f P[4]; FVector2f UV[4]; FVector2f Lo, Hi; FLinearColor Col; int32 Idx = -1; uint8 N = 4; uint8 Kind = KFace; };
	struct FTile { int32 X = 0, Y = 0, W = 1, H = 1; };
	struct FFan { FVector2f Hip, Feet; uint8 Fi = 0; uint8 bBack = 0; uint8 Side = 0; uint8 bFlag = 0; uint32 Hash = 0; };
	struct FScreen { FVector2f O, U, V; };          // the face: O + U * (col / cols) + V * (row / rows), atlas pixels
	struct FSprite { FVector2f Feet; int32 Tile = 0; bool bFlip = false; uint32 Hash = 0; };
	enum ELayer { LGround, LStands, LNear, LNum };
	TArray<FOp> Ops[LNum];
	TArray<FFan> Fans;
	TArray<FScreen> Screens;
	TArray<FSprite> Sprites;
	TObjectPtr<UTexture2D> Sheet = nullptr;
	TObjectPtr<UTexture2D> Green = nullptr;
	FVector2D BuiltAz = FVector2D::ZeroVector;
	int32 SheetW = 1, SheetH = 1, CellH = 80, FrontRows = 8;
	TArray<FTile> Tiles;                     // the materials' texel rectangles in the sheet (EMat order)
	float AtlasPerSpritePx = 0.4f;

	bool BuildSheet(const FString& FanSheetRel, const FColor& Seat, const FColor& SeatTrim, bool bNight);
	void PaintMaterials(TArray<FColor>& Px, const FColor& Seat, const FColor& SeatTrim, bool bNight);
	void DrawLayer(UCanvas* C, int32 Layer, const FVector2D& TL, float Zoom, float FanScale, const FPFCrowdView& Crowd) const;
};
