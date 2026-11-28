// Proper Football - the 3D stadium (see PFStadium3D.h; the pixel-art materials are painted in PFStadium3DArt.cpp).
//
// One ground for every match: the grass and its markings, the goals and everyone on the pitch stay 2D; everything round
// them is a model in world metres (x along the pitch, y across, z up) drawn through the same parallel projection as the
// pitch (FPFProjection with heights scaled by HeightScale, exactly as the players and goals are drawn). The camera sits
// south and a little west of the pitch, looking down: the far (north) stand and the east end face it, the near (south)
// paddock and the west end are seen from behind.
//
//   pitch edge  A-frame advertising boards in 6 m units with gaps, photographers behind each goal
//   far stand   two tiers (20 + 12 rows), fascia, roof on columns with a lamp gantry, players' tunnel, vomitories,
//               aisles every 14 seats, banners on the wall, a brick end screen up to the roof
//   east end    18 rows under a roof with the big screen on it (score and clock, live), the away fans in its north half
//   west end    18 rows, an open kop seen from behind, finished with a brick side wall and coping that follow the rake
//   near side   the backs of the boards, dugouts and a shallow 8-row paddock seen from behind, kept low so it never
//               hides play
//   outside     floodlight pylons behind the far corners, a paved concourse, streets with terraced houses, trees and a
//               car park
//
// Every face is culled against the view direction, lit flat from one sun and textured in texels locked to the world
// (48 per metre along x, 32 along y and z, so a texel is about one sprite pixel on screen in every direction); faces,
// fans and sprites are emitted in painter's order by construction (rows back to front for the stands facing the camera,
// front to back for those seen from behind; along a row from the far end), so one nearest-sampled triangle list per
// layer draws the whole ground. The projected geometry is cached in atlas pixels; a frame only offsets, scales and culls.
#include "PFStadium3D.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Misc/FileHelper.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "UnrealClient.h"
#include "HAL/PlatformMisc.h"

namespace PFArt { const TCHAR* Glyph57(TCHAR Ch); uint32 Hash(int32 X, int32 Y, int32 S); }

namespace
{
	FLinearColor Lin(int32 R, int32 G, int32 B) { return FLinearColor::FromSRGBColor(FColor(uint8(R), uint8(G), uint8(B))); }

	// a stand's local frame: world = U * u + W * w + Z * z (w grows away from the pitch)
	struct FFr { FVector U, W; FVector At(const FVector& L) const { return U * L.X + W * L.Y + FVector(0, 0, L.Z); } FVector Dir(const FVector& L) const { return U * L.X + W * L.Y + FVector(0, 0, L.Z); } };
	const FFr FarF{ FVector(1, 0, 0), FVector(0, 1, 0) };
	const FFr NearF{ FVector(1, 0, 0), FVector(0, -1, 0) };
	const FFr EastF{ FVector(0, 1, 0), FVector(1, 0, 0) };
	const FFr WestF{ FVector(0, 1, 0), FVector(-1, 0, 0) };
	const FVector NToPitch(0, -1, 0), NOut(0, 1, 0), NUp(0, 0, 1), NUNeg(-1, 0, 0), NUPos(1, 0, 0);

	struct FRow { float W = 0, D = 0.8f, Z = 0, Rise = 0.45f, Shade = 1.f; bool bFascia = false; };
	FString GTourStage = TEXT("Daylight");   // for the tour's screenshot names

	// texels per metre along a world direction: x is drawn wider on screen than y and z
	float Dens(const FVector& D) { return FMath::Abs(D.X) > 0.7f ? 48.f : 32.f; }

	// convex polygon clip against s or t >= / <= a value (Sutherland-Hodgman)
	void Clip(TArray<FVector2D>& P, int32 Axis, double V, bool bKeepGreater)
	{
		if (P.Num() < 3) return;
		TArray<FVector2D> O;
		for (int32 i = 0; i < P.Num(); ++i)
		{
			const FVector2D A = P[i], B = P[(i + 1) % P.Num()];
			const double Da = (Axis ? A.Y : A.X) - V, Db = (Axis ? B.Y : B.X) - V;
			const bool Ia = bKeepGreater ? Da >= 0 : Da <= 0, Ib = bKeepGreater ? Db >= 0 : Db <= 0;
			if (Ia) O.Add(A);
			if (Ia != Ib) { const double T = Da / (Da - Db); O.Add(A + (B - A) * T); }
		}
		P = MoveTemp(O);
	}
}

// ------------------------------------------------------------------------------------------------ the sheet
// The home club's supporter sheet (GameData/Fans/<slug>.png: 20 fans x 8 rows of 64 x 80 cells - seated, standing, three
// cheer and three "no!" frames) plus the same 8 rows seen from behind, derived here: the head becomes the back of the
// head (hair, hat or bald crown), and a seated fan loses what is below the hips (his legs go away from the camera, under
// the seat). The stadium's materials are painted beside and below them (PaintMaterials).
bool FPFStadium3D::BuildSheet(const FString& Rel, const FColor& Seat, const FColor& SeatTrim, bool bNight)
{
	TArray<uint8> Bytes;
	FString Use = Rel;
	if (!FFileHelper::LoadFileToArray(Bytes, *PF::DataPath(Use))) { Use = TEXT("GameData/Fans/fans.png"); if (!FFileHelper::LoadFileToArray(Bytes, *PF::DataPath(Use))) return false; }
	IImageWrapperModule& IWM = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TSharedPtr<IImageWrapper> Wr = IWM.CreateImageWrapper(EImageFormat::PNG);
	TArray<uint8> Raw;
	if (!Wr.IsValid() || !Wr->SetCompressed(Bytes.GetData(), Bytes.Num()) || !Wr->GetRaw(ERGBFormat::BGRA, 8, Raw)) return false;
	const int32 SW = Wr->GetWidth(), SH = Wr->GetHeight();
	const int32 SrcCH = (SH % 80 == 0 && SH / 80 >= 2) ? 80 : 64;
	FrontRows = FMath::Clamp(SH / SrcCH, 1, 8);
	CellH = 80;
	SheetW = 2048;
	SheetH = 2048;
	TArray<FColor> Px; Px.SetNumZeroed(SheetW * SheetH);
	const FColor* Src = reinterpret_cast<const FColor*>(Raw.GetData());
	for (int32 r = 0; r < FrontRows; ++r)
		for (int32 y = 0; y < SrcCH; ++y)
			for (int32 x = 0; x < FMath::Min(SW, 64 * 20); ++x)
				Px[(r * CellH + (CellH - SrcCH) + y) * SheetW + x] = Src[(r * SrcCH + y) * SW + x];
	for (int32 r = 0; r < FrontRows; ++r)
		for (int32 f = 0; f < 20; ++f)
		{
			const int32 X0 = f * 64, Y0 = (FrontRows + r) * CellH, S0 = r * CellH;
			auto At = [&](int32 x, int32 y) -> FColor& { return Px[(Y0 + y) * SheetW + X0 + FMath::Clamp(x, 0, 63)]; };
			for (int32 y = 0; y < CellH; ++y) for (int32 x = 0; x < 64; ++x) At(x, y) = Px[(S0 + y) * SheetW + X0 + x];
			if (r == 0) for (int32 y = 66; y < CellH; ++y) for (int32 x = 0; x < 64; ++x) At(x, y) = FColor(0, 0, 0, 0);   // seated: no legs
			int32 Top = -1, Sh = -1;
			for (int32 y = 0; y < CellH && Sh < 0; ++y)
			{
				int32 N = 0; for (int32 x = 24; x < 40; ++x) N += At(x, y).A > 128;
				if (N > 0 && Top < 0) Top = y;
				if (Top >= 0 && N >= 14) Sh = y;
			}
			if (Top < 0 || Sh < 0 || Sh - Top > 18) continue;
			FColor Hair(60, 40, 30, 255); int32 Best = 1 << 30;
			for (int32 y = Top; y < FMath::Min(Top + 3, Sh); ++y)
				for (int32 x = 24; x < 40; ++x)
				{
					const FColor& Cc = At(x, y);
					const int32 L = Cc.R * 3 + Cc.G * 6 + Cc.B;
					if (Cc.A > 128 && L < Best) { Best = L; Hair = Cc; }
				}
			for (int32 y = Top; y < Sh - 1; ++y)
				for (int32 x = 22; x < 42; ++x)
				{
					FColor& Cc = At(x, y);
					if (Cc.A <= 128) continue;
					const bool bEdge = At(x - 1, y).A <= 128 || At(x + 1, y).A <= 128 || (y > 0 && At(x, y - 1).A <= 128);
					const float K = bEdge ? 0.72f : (x >= 32 ? 0.9f : 1.f);
					Cc = FColor(uint8(Hair.R * K), uint8(Hair.G * K), uint8(Hair.B * K), 255);
				}
		}
	PaintMaterials(Px, Seat, SeatTrim, bNight);

	UTexture2D* T = UTexture2D::CreateTransient(SheetW, SheetH, PF_B8G8R8A8);
	if (!T) return false;
	T->Filter = TF_Nearest; T->SRGB = true; T->NeverStream = true; T->LODGroup = TEXTUREGROUP_Pixels2D;
	T->AddressX = TA_Clamp; T->AddressY = TA_Clamp;
	FMemory::Memcpy(T->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE), Px.GetData(), Px.Num() * 4);
	T->GetPlatformData()->Mips[0].BulkData.Unlock();
	T->UpdateResource();
	Sheet = T;

	UTexture2D* G = UTexture2D::CreateTransient(1, 1, PF_B8G8R8A8);
	if (G)
	{
		const FColor Gc(60, 160, 60, 255);
		FMemory::Memcpy(G->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE), &Gc, 4);
		G->GetPlatformData()->Mips[0].BulkData.Unlock();
		G->Filter = TF_Nearest; G->NeverStream = true; G->UpdateResource();
	}
	Green = G;
	UE_LOG(LogPF, Log, TEXT("PF stadium3d sheet %s: %d front rows + back views, %d materials"), *Use, FrontRows, Tiles.Num());
	return true;
}

void FPFStadium3D::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Sheet);
	Collector.AddReferencedObject(Green);
}

// ------------------------------------------------------------------------------------------------ the model
void FPFStadium3D::Build(const FPFProjection& Pv, const FString& FanSheetRel, const FLinearColor& HomeShirt, const FLinearColor& HomeTrim, const FString& Stage, uint32 Seed)
{
	const bool bNight = Stage.StartsWith(TEXT("Floodlit")), bSunset = Stage == TEXT("Sunset");
	// the seats and fascia take the home colour (kit colours are sRGB codes in 0..1); a white or pale kit seats its
	// fans in its trim, or a slate blue
	auto Code = [](const FLinearColor& C) { return FColor(uint8(FMath::Clamp(C.R, 0.f, 1.f) * 255), uint8(FMath::Clamp(C.G, 0.f, 1.f) * 255), uint8(FMath::Clamp(C.B, 0.f, 1.f) * 255), 255); };
	auto Luma = [](const FColor& C) { return (C.R * 0.3f + C.G * 0.59f + C.B * 0.11f) / 255.f; };
	FColor Seat = Code(HomeShirt), Trim = Code(HomeTrim);
	if (Luma(Seat) > 0.7f) Seat = (Luma(Trim) < 0.55f && Luma(Trim) > 0.08f) ? Trim : FColor(64, 76, 104);
	if (Luma(Seat) < 0.08f) Seat = FColor(58, 60, 70);
	{ const float L = Luma(Seat) * 255.f; Seat = FColor(uint8(FMath::Lerp(L, float(Seat.R), 0.85f)), uint8(FMath::Lerp(L, float(Seat.G), 0.85f)), uint8(FMath::Lerp(L, float(Seat.B), 0.85f)), 255); }
	if (FMath::Abs(Luma(Trim) - Luma(Seat)) < 0.15f) Trim = Luma(Seat) > 0.5f ? FColor(30, 30, 36) : FColor(236, 236, 230);
	GTourStage = Stage;
	if (!Sheet && !BuildSheet(FanSheetRel, Seat, Trim, bNight)) { UE_LOG(LogPF, Warning, TEXT("PF stadium3d: no fan sheet")); return; }
	for (TArray<FOp>& L : Ops) L.Reset();
	Fans.Reset(); Screens.Reset(); Sprites.Reset();
	BuiltAz = Pv.Az;
	AtlasPerSpritePx = Pv.Az.Size() * 1.82f / 64.f;
	PF::FRng Rng(Seed ^ 0x5EA75u);

	// the view direction (into the scene): Ax*vx + Ay*vy - Az = 0 with vz = -1
	const double Det = Pv.Ax.X * Pv.Ay.Y - Pv.Ay.X * Pv.Ax.Y;
	const FVector V((Pv.Az.X * Pv.Ay.Y - Pv.Ay.X * Pv.Az.Y) / Det, (Pv.Ax.X * Pv.Az.Y - Pv.Az.X * Pv.Ax.Y) / Det, -1.0);
	const FVector Sun = FVector(-0.35, -0.5, 0.8).GetSafeNormal();   // from the upper left, like the players' shadows
	// the ground under its floodlights at night, the town round it in the dark; a warm low sun in the evening
	const FLinearColor StandTint = bNight ? FLinearColor(0.70f, 0.72f, 0.86f) : bSunset ? FLinearColor(1.0f, 0.86f, 0.74f) : FLinearColor::White;
	const FLinearColor TownTint = bNight ? FLinearColor(0.26f, 0.28f, 0.40f) : StandTint;
	const FLinearColor ConcourseTint = bNight ? FLinearColor(0.42f, 0.44f, 0.56f) : StandTint;
	FLinearColor Tint = StandTint;
	const FLinearColor One = FLinearColor::White;

	int32 Layer = LStands;
	enum { FitS = 1, FitT = 2, FlipS = 4, Unlit = 8, FlipT = 16 };
	auto PushPiece = [&](const FFr& F, const FVector* L, const FVector2D* Uv, int32 N, const FLinearColor& Col)
	{
		for (int32 i = 1; i + 1 < N; i += 2)
		{
			FOp Op; Op.Kind = KFace; Op.N = i + 2 < N ? 4 : 3;
			const int32 Ix[4] = { 0, i, i + 1, i + 2 < N ? i + 2 : i + 1 };
			Op.Lo = FVector2f(1e9f, 1e9f); Op.Hi = FVector2f(-1e9f, -1e9f);
			for (int32 k = 0; k < 4; ++k)
			{
				const FVector2D A = Pv.ToAtlas(F.At(L[Ix[k]]));
				Op.P[k] = FVector2f(A);
				Op.UV[k] = FVector2f(float(Uv[Ix[k]].X / SheetW), float(Uv[Ix[k]].Y / SheetH));
				Op.Lo = FVector2f(FMath::Min(Op.Lo.X, float(A.X)), FMath::Min(Op.Lo.Y, float(A.Y)));
				Op.Hi = FVector2f(FMath::Max(Op.Hi.X, float(A.X)), FMath::Max(Op.Hi.Y, float(A.Y)));
			}
			Op.Col = Col; Op.Col.A = 1.f;
			Ops[Layer].Add(Op);
		}
	};
	auto Light = [&](const FFr& F, const FVector& NL, const FLinearColor& Mul, float Shade, int32 Fl, bool& bVisible) -> FLinearColor
	{
		const FVector Nw = F.Dir(NL).GetSafeNormal();
		bVisible = FVector::DotProduct(Nw, V) < -1e-3;
		if (Fl & Unlit) return Mul;
		const float L = (0.6f + 0.4f * FMath::Max(0.f, float(FVector::DotProduct(Nw, Sun)))) * Shade;
		return Mul * Tint * L;
	};
	// a textured planar polygon: local point = Base + Sd * s + Td * t for (s, t) in Poly; the material tiles in texels
	// locked to s and t (so neighbouring faces line up), or is stretched over the polygon (FitS / FitT)
	auto TPoly = [&](const FFr& F, const FVector& Base, const FVector& Sd, const FVector& Td, const TArray<FVector2D>& Poly, const FVector& NL, int32 M, const FLinearColor& Mul, float Shade, int32 Fl)
	{
		bool bVis; const FLinearColor Col = Light(F, NL, Mul, Shade, Fl, bVis);
		if (!bVis || Poly.Num() < 3) return;
		if (M == MWhite) Fl |= FitS | FitT;   // a flat colour never needs tiling
		const FTile& T = Tiles[M];
		const double Ds = Dens(F.Dir(Sd)), Dt = Dens(F.Dir(Td));
		FVector2D Mn(1e18, 1e18), Mx(-1e18, -1e18);
		for (const FVector2D& P : Poly) { Mn = FVector2D(FMath::Min(Mn.X, P.X), FMath::Min(Mn.Y, P.Y)); Mx = FVector2D(FMath::Max(Mx.X, P.X), FMath::Max(Mx.Y, P.Y)); }
		const double Ps = T.W / Ds, Pt = T.H / Dt;
		const int32 S0 = (Fl & FitS) ? 0 : FMath::FloorToInt(Mn.X / Ps + 1e-6), S1 = (Fl & FitS) ? 0 : FMath::CeilToInt(Mx.X / Ps - 1e-6) - 1;
		const int32 T0 = (Fl & FitT) ? 0 : FMath::FloorToInt(Mn.Y / Pt + 1e-6), T1 = (Fl & FitT) ? 0 : FMath::CeilToInt(Mx.Y / Pt - 1e-6) - 1;
		for (int32 a = S0; a <= FMath::Max(S0, S1); ++a)
			for (int32 b = T0; b <= FMath::Max(T0, T1); ++b)
			{
				TArray<FVector2D> C = Poly;
				if (!(Fl & FitS)) { Clip(C, 0, a * Ps, true); Clip(C, 0, (a + 1) * Ps, false); }
				if (!(Fl & FitT)) { Clip(C, 1, b * Pt, true); Clip(C, 1, (b + 1) * Pt, false); }
				if (C.Num() < 3) continue;
				FVector L[16]; FVector2D Uv[16];
				const int32 N = FMath::Min(C.Num(), 16);
				for (int32 i = 0; i < N; ++i)
				{
					const FVector2D& P = C[i];
					L[i] = Base + Sd * P.X + Td * P.Y;
					double U = (Fl & FitS) ? (Mx.X > Mn.X ? (P.X - Mn.X) / (Mx.X - Mn.X) * T.W : 0.0) : (P.X - a * Ps) * Ds;
					double W = (Fl & FitT) ? (Mx.Y > Mn.Y ? (P.Y - Mn.Y) / (Mx.Y - Mn.Y) * T.H : 0.0) : (P.Y - b * Pt) * Dt;
					if (Fl & FlipS) U = T.W - U;
					if (Fl & FlipT) W = T.H - W;
					Uv[i] = FVector2D(T.X + FMath::Clamp(U, 0.0, double(T.W)), T.Y + FMath::Clamp(W, 0.0, double(T.H)));
				}
				PushPiece(F, L, Uv, N, Col);
			}
	};
	auto RectP = [](double s0, double s1, double t0, double t1) { return TArray<FVector2D>{ FVector2D(s0, t0), FVector2D(s1, t0), FVector2D(s1, t1), FVector2D(s0, t1) }; };
	// a vertical face across u at w (z0..z1); texture rows count down from the top (bGlobalT: from world z, so courses
	// of brick line up across faces)
	auto VT = [&](const FFr& F, float w, float u0, float u1, float z0, float z1, const FVector& N, int32 M, int32 Fl = 0, const FLinearColor& Mul = FLinearColor::White, float Shade = 1.f, bool bGlobalT = false)
	{
		if (bGlobalT) TPoly(F, FVector(0, w, 0), FVector(1, 0, 0), FVector(0, 0, -1), RectP(u0, u1, -z1, -z0), N, M, Mul, Shade, Fl);
		else TPoly(F, FVector(0, w, z1), FVector(1, 0, 0), FVector(0, 0, -1), RectP(u0, u1, 0, z1 - z0), N, M, Mul, Shade, Fl);
	};
	// a horizontal face at z (u0..u1, w0..w1); texture rows from w0 (bGlobalT: from w = 0)
	auto HT = [&](const FFr& F, float z, float u0, float u1, float w0, float w1, int32 M, int32 Fl = 0, const FLinearColor& Mul = FLinearColor::White, float Shade = 1.f, bool bGlobalT = false)
	{
		if (bGlobalT) TPoly(F, FVector(0, 0, z), FVector(1, 0, 0), FVector(0, 1, 0), RectP(u0, u1, w0, w1), NUp, M, Mul, Shade, Fl);
		else TPoly(F, FVector(0, w0, z), FVector(1, 0, 0), FVector(0, 1, 0), RectP(u0, u1, 0, w1 - w0), NUp, M, Mul, Shade, Fl);
	};
	// an end face at u (w0..w1, z0..z1), texture locked to the world
	auto ET = [&](const FFr& F, float u, float w0, float w1, float z0, float z1, const FVector& N, int32 M, int32 Fl = 0, const FLinearColor& Mul = FLinearColor::White, float Shade = 1.f)
	{ TPoly(F, FVector(u, 0, 0), FVector(0, 1, 0), FVector(0, 0, -1), RectP(w0, w1, -z1, -z0), N, M, Mul, Shade, Fl); };
	auto Box = [&](const FFr& F, float u0, float u1, float w0, float w1, float z0, float z1, int32 MFront, int32 MSide, int32 MTop, int32 FlF, int32 FlS, int32 FlT, const FLinearColor& Mul = FLinearColor::White)
	{
		// the faces that can be seen are drawn back to front: rear, far end, near end, front, top
		VT(F, w1, u0, u1, z0, z1, NOut, MSide, FlS, Mul);
		ET(F, u1, w0, w1, z0, z1, NUPos, MSide, FlS, Mul);
		ET(F, u0, w0, w1, z0, z1, NUNeg, MSide, FlS, Mul);
		VT(F, w0, u0, u1, z0, z1, NToPitch, MFront, FlF, Mul);
		HT(F, z1, u0, u1, w0, w1, MTop, FlT, Mul);
	};
	auto Solid = [&](const FFr& F, const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& N, const FLinearColor& Col, float Shade = 1.f, int32 Fl = 0)
	{
		bool bVis; const FLinearColor Cc = Light(F, N, Col, Shade, Fl, bVis);
		if (!bVis) return;
		const FTile& T = Tiles[MWhite];
		const FVector L[4] = { A, B, C, D };
		const FVector2D U(T.X + 4, T.Y + 4), Uv[4] = { U, U, U, U };
		PushPiece(F, L, Uv, 4, Cc);
	};
	auto AddSprite = [&](const FVector& World, int32 Tile, bool bFlip)
	{
		FSprite Sp; Sp.Feet = FVector2f(Pv.ToAtlas(World)); Sp.Tile = Tile; Sp.bFlip = bFlip; Sp.Hash = Rng.Next();
		FOp Op; Op.Kind = KSprite; Op.Idx = Sprites.Add(Sp);
		const float S = AtlasPerSpritePx;
		Op.Lo = FVector2f(Sp.Feet.X - 34.f * S, Sp.Feet.Y - 66.f * S); Op.Hi = FVector2f(Sp.Feet.X + 34.f * S, Sp.Feet.Y + 2.f);
		Op.Col = FLinearColor::White;   // lit by the crowd's light, like the fans
		Ops[Layer].Add(Op);
	};

	// ---- A-frame advertising boards: 6 m units with 1.6 m gaps, each a printed face leaning back toward the pitch's
	// outside, a plain back face, the two meeting at the top
	int32 BoardN = int32(Seed % uint32(NBoards));
	auto Boards = [&](const FFr& F, float U0, float U1, float W)
	{
		const FVector Right = FVector::CrossProduct(F.W, FVector(0, 0, 1));
		const bool bFlip = FVector::DotProduct(Right, F.U) < 0;   // the text reads along -u in this frame
		const float H = 0.9f, Lean = 0.32f, Unit = 6.f, Gap = 1.6f;
		const int32 N = FMath::Max(1, FMath::FloorToInt((U1 - U0 + Gap) / (Unit + Gap)));
		const float Used = N * Unit + (N - 1) * Gap, Start = (U0 + U1) * 0.5f - Used * 0.5f;
		for (int32 i = N - 1; i >= 0; --i)
		{
			const float A = Start + i * (Unit + Gap), B = A + Unit;
			const int32 M = MBoard0 + (BoardN++ % NBoards);
			// back face (leans the other way), then the ends (thin triangles), then the printed face
			const FVector Nb = FVector(0, Lean, H).GetSafeNormal() * FVector(1, 1, 1);
			TPoly(F, FVector(0, W + Lean * 2.f, 0), FVector(1, 0, 0), FVector(0, -Lean, H).GetSafeNormal(), RectP(A, B, 0, FVector(0, Lean, H).Size()), FVector(0, H, Lean).GetSafeNormal(), MBoardBack, One, 1.f, FitS | FitT);
			for (float Ue : { B, A })
			{
				const FVector Ne = Ue == A ? NUNeg : NUPos;
				bool bVis; const FLinearColor Cc = Light(F, Ne, Lin(60, 62, 66), 1.f, 0, bVis);
				if (!bVis) continue;
				const FVector L[3] = { FVector(Ue, W, 0), FVector(Ue, W + Lean * 2.f, 0), FVector(Ue, W + Lean, H) };
				const FTile& T = Tiles[MWhite]; const FVector2D U(T.X + 4, T.Y + 4), Uv[3] = { U, U, U };
				PushPiece(F, L, Uv, 3, Cc);
			}
			TPoly(F, FVector(0, W + Lean, H), FVector(1, 0, 0), FVector(0, -Lean, -H).GetSafeNormal(), RectP(A, B, 0, FVector(0, Lean, H).Size()), FVector(0, -H, Lean).GetSafeNormal(), M, One, 1.f, FitS | FitT | (bFlip ? FlipS : 0));
			(void)Nb;
		}
	};

	// ---- a tiered stand: rows of steps with seats in blocks of 14 (0.5 m each) between 1.1 m aisles
	struct FStand
	{
		const FFr* F = nullptr; float U0 = 0, U1 = 0; bool bBack = false; TArray<FRow> Rows;
		TFunction<bool(int32, float)> NoSeat, DarkAt; TFunction<void(int32)> After; TFunction<uint8(int32, float)> Pick;
		float FlagRate = 0.f;
	};
	auto BuildStand = [&](const FStand& S)
	{
		const FFr& F = *S.F;
		struct FSeg { float A, B; bool bAisle; };
		TArray<FSeg> Segs; TArray<float> Seats;
		{
			float U = S.U0 + 0.3f;
			int32 InBlock = 0; float BlockStart = S.U0;
			while (U + 0.5f <= S.U1 - 0.3f)
			{
				Seats.Add(U + 0.25f); U += 0.5f;
				if (++InBlock == 14 && U + 1.1f + 0.5f < S.U1 - 0.3f) { Segs.Add({ BlockStart, U, false }); Segs.Add({ U, U + 1.1f, true }); U += 1.1f; BlockStart = U; InBlock = 0; }
			}
			Segs.Add({ BlockStart, S.U1, false });
		}
		const int32 NR = S.Rows.Num();
		for (int32 i = 0; i < NR; ++i)
		{
			const int32 k = S.bBack ? i : NR - 1 - i;
			const FRow& R = S.Rows[k];
			const float Lit = R.Shade;
			for (int32 s = Segs.Num() - 1; s >= 0; --s)
			{
				const FSeg& G = Segs[s];
				const float Mid = (G.A + G.B) * 0.5f;
				const bool bDark = S.DarkAt && S.DarkAt(k, Mid);
				if (bDark)
				{
					// a vomitory: the passage's dark mouth in the riser, its floor going down into shadow
					VT(F, R.W, G.A, G.B, R.Z - R.Rise, R.Z, NToPitch, MTunnel, FitS | FitT, One, Lit);
					HT(F, R.Z, G.A, G.B, R.W, R.W + R.D, MWhite, 0, Lin(34, 34, 38), Lit);
					continue;
				}
				if (R.bFascia) VT(F, R.W, G.A, G.B, R.Z - R.Rise, R.Z, NToPitch, MFascia, FitT, One, Lit);
				else VT(F, R.W, G.A, G.B, R.Z - R.Rise, R.Z, NToPitch, MRiser, 0, One, Lit);
				HT(F, R.Z, G.A, G.B, R.W, R.W + R.D, G.bAisle ? MAisle : MTread, 0, One, Lit);
			}
			const float Rf = float(k) / FMath::Max(1, NR - 1);
			for (int32 j = Seats.Num() - 1; j >= 0; --j)
			{
				const float U = Seats[j];
				if ((S.NoSeat && S.NoSeat(k, U)) || (S.DarkAt && S.DarkAt(k, U))) continue;
				const float Sa = U - 0.22f, Sb = U + 0.22f;
				const float Zp = R.Z + 0.42f, Zb = R.Z + 0.86f;
				// empty seats gather at the top corners
				const float Edge = FMath::Clamp((FMath::Abs(U - (S.U0 + S.U1) * 0.5f) / ((S.U1 - S.U0) * 0.5f) - 0.55f) / 0.45f, 0.f, 1.f);
				const bool bFan = Rng.Unit() < 0.95f - 0.55f * Rf * Rf * Edge - 0.04f * Rf;
				if (!S.bBack) VT(F, R.W + 0.66f, Sa, Sb, Zp, Zb, NToPitch, MSeatBack, FitS | FitT, One, Lit);
				HT(F, Zp, Sa, Sb, R.W + 0.28f, R.W + 0.66f, MSeatPan, FitS | FitT, One, Lit);
				if (bFan)
				{
					FFan Fa;
					Fa.Hip = FVector2f(Pv.ToAtlas(F.At(FVector(U, R.W + 0.44f, R.Z + 0.45f))));
					Fa.Feet = FVector2f(Pv.ToAtlas(F.At(FVector(U, R.W + 0.30f, R.Z))));
					Fa.Fi = S.Pick ? S.Pick(k, U) : uint8(Rng.Int(20));
					Fa.Side = (Fa.Fi == 5 || Fa.Fi == 8) ? 1 : 0;   // the club sheet's away-colour fans (Tools/pipeline/fans.py)
					Fa.bBack = S.bBack ? 1 : 0;
					Fa.bFlag = (!S.bBack && Rng.Unit() < S.FlagRate) ? 1 : 0;
					Fa.Hash = Rng.Next();
					FOp Op; Op.Kind = KFan;
					Op.Idx = Fans.Add(Fa);
					const float Sp = AtlasPerSpritePx;
					Op.Lo = FVector2f(Fa.Hip.X - 34.f * Sp, FMath::Min(Fa.Hip.Y - 64.f * Sp, Fa.Feet.Y - 82.f * Sp) - (Fa.bFlag ? 40.f * Sp : 0.f));
					Op.Hi = FVector2f(Fa.Hip.X + 34.f * Sp + (Fa.bFlag ? 30.f * Sp : 0.f), FMath::Max(Fa.Hip.Y + 18.f * Sp, Fa.Feet.Y + 2.f));
					Op.Col = FLinearColor(Lit, Lit, Lit, 1.f);
					Ops[Layer].Add(Op);
				}
				if (S.bBack)
				{
					VT(F, R.W + 0.70f, Sa, Sb, Zp, Zb, NOut, MSeatBack, FitS | FitT, One * 0.72f, Lit);
					HT(F, Zb, Sa, Sb, R.W + 0.66f, R.W + 0.70f, MSeatPan, FitS | FitT, One, Lit);
				}
			}
			if (S.After) S.After(k);
		}
	};
	// the side wall at a stand's near end (u0): brick up the rake, 1.1 m above the steps (to the roof where there is
	// one), a coping along the top, an exit door at its foot. Drawn after the rows: nearer than anything in them.
	auto Gable = [&](const FStand& S, float RoofFromW, float RoofZf, float RoofZb, float RoofWb, const FVector& Nside)
	{
		const FFr& F = *S.F;
		const int32 NR = S.Rows.Num();
		const float Th = 0.3f, U = S.U0;
		auto TopAt = [&](float W) -> float
		{
			if (RoofFromW > 0.f && W >= RoofFromW) return FMath::Lerp(RoofZf, RoofZb, FMath::Clamp((W - RoofFromW) / FMath::Max(0.1f, RoofWb - RoofFromW), 0.f, 1.f)) - 0.05f;
			// the rake line: each row's height plus 1.1 m, straight from row to row
			for (int32 k = 0; k < NR; ++k)
			{
				const FRow& R = S.Rows[k];
				if (W <= R.W + R.D + 1e-3f || k == NR - 1)
				{
					const float Zn = k + 1 < NR ? S.Rows[k + 1].Z : R.Z;
					return FMath::Lerp(R.Z, Zn, FMath::Clamp((W - R.W) / R.D, 0.f, 1.f)) + 1.1f;
				}
			}
			return 2.f;
		};
		const float W0 = S.Rows[0].W - 0.15f, W1 = S.Rows.Last().W + S.Rows.Last().D;
		// sample the top line every 0.4 m (and at the roof's front edge) into a polygon strip
		TArray<float> Ws;
		for (float W = W0; W < W1; W += 0.4f) Ws.Add(W);
		if (RoofFromW > 0.f) Ws.Add(RoofFromW - 0.01f), Ws.Add(RoofFromW);
		Ws.Add(W1);
		Ws.Sort();
		// brick wall: one polygon per strip (convex: a trapezoid), texture locked to the world
		for (int32 i = 0; i + 1 < Ws.Num(); ++i)
		{
			const float Wa = Ws[i], Wb = Ws[i + 1];
			if (Wb - Wa < 1e-3f) continue;
			const float Za = TopAt(Wa), Zb = TopAt(Wb);
			TArray<FVector2D> P = { FVector2D(Wa, 0), FVector2D(Wb, 0), FVector2D(Wb, -Zb), FVector2D(Wa, -Za) };
			// in (s = w, t = -z): reorder so t grows downward
			TArray<FVector2D> Q = { FVector2D(Wa, -Za), FVector2D(Wb, -Zb), FVector2D(Wb, 0), FVector2D(Wa, 0) };
			TPoly(F, FVector(U - Th, 0, 0), FVector(0, 1, 0), FVector(0, 0, -1), Q, Nside, MBrick, One, 0.95f, 0);
			// coping: a 0.18 m band under the top line on the face, and the wall's top
			TArray<FVector2D> Cb = { FVector2D(Wa, -Za), FVector2D(Wb, -Zb), FVector2D(Wb, -Zb + 0.18f), FVector2D(Wa, -Za + 0.18f) };
			TPoly(F, FVector(U - Th - 0.01f, 0, 0), FVector(0, 1, 0), FVector(0, 0, -1), Cb, Nside, MCoping, One, 0.95f, FitT);
			Solid(F, FVector(U - Th - 0.02f, Wa, Za), FVector(U, Wa, Za), FVector(U, Wb, Zb), FVector(U - Th - 0.02f, Wb, Zb), FVector(0, -(Zb - Za), Wb - Wa).GetSafeNormal(), Lin(196, 194, 186));
		}
		// the end of the wall at the pitch
		ET(F, U - Th, W0 - 0.01f, W0, 0.f, TopAt(W0), NToPitch, MCoping, FitS | FitT);
		// an exit door near the back
		const float Dw = W1 - 3.4f;
		TPoly(F, FVector(U - Th - 0.02f, 0, 0), FVector(0, 1, 0), FVector(0, 0, -1), RectP(Dw, Dw + 1.6f, -2.3f, 0.f), Nside, MGate, One, 1.f, FitS | FitT);
	};
	auto RowsFrom = [](TArray<FRow>& Out, float W0, float Z0, int32 N, float D, float Rise, float FirstRise)
	{
		for (int32 k = 0; k < N; ++k) { FRow R; R.W = W0 + D * k; R.D = D; R.Z = Z0 + Rise * k; R.Rise = k == 0 ? FirstRise : Rise; Out.Add(R); }
	};
	// a lattice floodlight pylon (see-through bracing), its lamp head facing the pitch
	auto Pylon = [&](float X, float Y)
	{
		const float H = 30.f, B = 1.1f, Tp = 0.5f, Seg = 2.f;
		const FFr& F = FarF;
		const FTile& T = Tiles[MLattice];
		auto Wd = [&](float Z) { return FMath::Lerp(B, Tp, Z / H); };
		for (int32 Face = 0; Face < 2; ++Face)
			for (float Z = 0.f; Z < H - 1e-3f; Z += Seg)
			{
				const float Z1 = FMath::Min(H, Z + Seg), A = Wd(Z), Bb = Wd(Z1);
				FVector L[4];
				if (Face == 0) { L[0] = FVector(X - A, Y + A, Z); L[1] = FVector(X - A, Y - A, Z); L[2] = FVector(X - Bb, Y - Bb, Z1); L[3] = FVector(X - Bb, Y + Bb, Z1); }
				else { L[0] = FVector(X - A, Y - A, Z); L[1] = FVector(X + A, Y - A, Z); L[2] = FVector(X + Bb, Y - Bb, Z1); L[3] = FVector(X - Bb, Y - Bb, Z1); }
				bool bVis; const FLinearColor Cc = Light(F, Face == 0 ? NUNeg : NToPitch, One, 1.f, 0, bVis);
				if (!bVis) continue;
				const FVector2D Uv[4] = { FVector2D(T.X, T.Y + T.H), FVector2D(T.X + T.W, T.Y + T.H), FVector2D(T.X + T.W, T.Y), FVector2D(T.X, T.Y) };
				PushPiece(F, L, Uv, 4, Cc);
			}
		// the head: a frame box with 6 x 4 lamps on its face, a service platform under it
		const float Hy = Y - 0.9f;
		Box(F, X - 3.4f, X + 3.4f, Hy - 0.15f, Y + 0.3f, H - 0.6f, H - 0.4f, MSteel, MSteel, MSteel, FitT, FitT, FitS | FitT);
		Box(F, X - 3.2f, X + 3.2f, Hy, Hy + 0.5f, H - 0.4f, H + 3.6f, MSteel, MSteel, MSteel, FitS | FitT, FitS | FitT, FitS | FitT);
		VT(F, Hy - 0.01f, X - 3.0f, X + 3.0f, H - 0.2f, H + 3.4f, NToPitch, MLamp, FitS | FitT | (bNight ? Unlit : 0));
	};
	// a gantry of lamps hung under a roof's front edge (they glow at night)
	auto RoofLamps = [&](const FFr& F, float U0, float U1, float W, float Z)
	{
		for (float U = U0 + 3.f; U < U1 - 2.f; U += 6.f)
		{
			Box(F, U - 0.95f, U + 0.95f, W, W + 0.35f, Z - 0.8f, Z, MSteel, MSteel, MSteel, FitS | FitT, FitS | FitT, FitS | FitT);
			VT(F, W - 0.01f, U - 0.85f, U + 0.85f, Z - 0.72f, Z - 0.08f, NToPitch, MLamp, FitS | FitT | (bNight ? Unlit : 0));
		}
	};
	// a corrugated roof falling to the back, its white-and-club fascia, its end, the lamps
	auto Roof = [&](const FFr& F, float U0, float U1, float Wf, float Wb, float Zf, float Zb)
	{
		const FVector Td = FVector(0, Wb - Wf, Zb - Zf).GetSafeNormal();
		const float Len = FVector(0, Wb - Wf, Zb - Zf).Size();
		const FVector N = FVector::CrossProduct(FVector(1, 0, 0), Td).GetSafeNormal();
		TPoly(F, FVector(0, Wf, Zf), FVector(1, 0, 0), Td, RectP(U0, U1, 0, Len), N.Z < 0 ? -N : N, MRoof, One, 1.f, 0);
		ET(F, U0, Wf, Wb, Zb - 0.9f, Zf, NUNeg, MFascia, FitS | FitT, One, 0.85f);
		VT(F, Wf, U0, U1, Zf - 1.0f, Zf, NToPitch, MFascia, FitT);
		RoofLamps(F, U0 + 0.5f, U1 - 0.5f, Wf - 0.4f, Zf - 1.0f);
	};
	// a hanging banner on a wall face (club stripes), and photographers behind a goal
	auto Banner = [&](const FFr& F, float U, float W, float Ztop, float Wd, float Ht)
	{ VT(F, W - 0.02f, U - Wd * 0.5f, U + Wd * 0.5f, Ztop - Ht, Ztop, NToPitch, MBanner, FitS | FitT); };

	// ---- houses: a terraced row along u whose visible face is at w (faces -w), bodies to +w; slate roofs, chimneys
	auto Terrace = [&](const FFr& F, float U0, float U1, float W, bool bNearCam)
	{
		const float Dp = 7.f, Hw = 6.f, Hr = 8.6f, Each = 5.f;
		const FVector Nf = NToPitch;
		// the roof slope that faces the camera: from the eaves (w, Hw) up to the ridge (w + Dp/2, Hr)
		const FVector Td = FVector(0, -Dp * 0.5f, -(Hr - Hw)).GetSafeNormal();
		const float Len = FVector(0, Dp * 0.5f, Hr - Hw).Size();
		const FVector Nr = FVector::CrossProduct(FVector(1, 0, 0), Td).GetSafeNormal();
		(void)bNearCam;
		TPoly(F, FVector(0, W + Dp * 0.5f, Hr), FVector(1, 0, 0), Td, RectP(U0, U1, 0, Len + 0.25f), Nr.Z < 0 ? -Nr : Nr, MSlate, One, 1.f, 0);
		for (float U = U0; U + Each <= U1 + 0.01f; U += Each)
		{
			const int32 Hh = int32(PFArt::Hash(int32(U * 7), int32(W), 5) % 4);
			const FLinearColor Mul = Hh == 0 ? One : Hh == 1 ? FLinearColor(0.92f, 0.9f, 0.88f) : Hh == 2 ? FLinearColor(1.04f, 0.98f, 0.94f) : FLinearColor(0.86f, 0.86f, 0.9f);
			VT(F, W, U, U + Each, 0.f, Hw, Nf, MHouse, FitS | FitT, Mul);
			// at night some of the windows are lit (the tile's three windows: upstairs left and right, downstairs left)
			if (bNight)
			{
				static const float Wx[3] = { 8.f, 40.f, 8.f }, Wy[3] = { 8.f, 8.f, 38.f };
				for (int32 w = 0; w < 3; ++w)
					if (PFArt::Hash(int32(U * 3), int32(W) + w * 17, 7) % 5 < 2)
						VT(F, W - 0.01f, U + Wx[w] / 64.f * Each, U + (Wx[w] + 14.f) / 64.f * Each, Hw - (Wy[w] + 16.f) / 64.f * Hw, Hw - Wy[w] / 64.f * Hw, Nf, MWindow, FitS | FitT | Unlit);
			}
			// a chimney stack on every other party wall
			if (int32((U - U0) / Each) % 2 == 0)
				Box(F, U - 0.4f, U + 0.4f, W + Dp * 0.5f - 0.35f, W + Dp * 0.5f + 0.35f, Hr - 0.3f, Hr + 1.2f, MBrick, MBrick, MCoping, 0, 0, FitS | FitT);
		}
		// the gable end that faces the camera's side
		TArray<FVector2D> G = { FVector2D(W, -Hw), FVector2D(W + Dp * 0.5f, -Hr), FVector2D(W + Dp, -Hw), FVector2D(W + Dp, 0), FVector2D(W, 0) };
		TPoly(F, FVector(U0, 0, 0), FVector(0, 1, 0), FVector(0, 0, -1), G, NUNeg, MBrick, One, 0.9f, 0);
	};
	auto Tree = [&](const FFr& F, float U, float W, float H)
	{ VT(F, W, U - H * 0.42f, U + H * 0.42f, 0.f, H, NToPitch, MTree, FitS | FitT); };
	auto Car = [&](float X, float Y, const FLinearColor& Body)
	{
		const FFr& F = FarF;
		Box(F, X - 2.1f, X + 2.1f, Y - 0.9f, Y + 0.9f, 0.f, 1.45f, MCar, MCar, MWhite, FitS | FitT, FitS | FitT, FitS | FitT, Body);
		HT(F, 1.451f, X - 0.9f, X + 1.0f, Y - 0.75f, Y + 0.75f, MWhite, 0, Lin(40, 50, 62));
	};

	// ================================================================ ground (under the grass)
	Layer = LGround;
	Tint = TownTint;
	HT(FarF, 0.f, -600.f, 600.f, -500.f, 600.f, MWhite, 0, Lin(58, 62, 62), 1.f, true);   // the town beyond (flat, out of every game view)
	HT(FarF, 0.f, -170.f, 170.f, -120.f, 150.f, MTarmac, 0, One, 1.f, true);          // the streets round the ground
	Tint = ConcourseTint;
	HT(FarF, 0.f, -80.f, 80.f, -52.f, 68.f, MPaving, 0, One, 1.f, true);              // the concourse
	Tint = StandTint;
	// the run-off between the boards and the stands' walls (the grass covers the rest)
	HT(FarF, 0.f, -59.6f, 59.6f, 35.9f, 37.8f, MRunoff, 0, One, 1.f, true);
	HT(FarF, 0.f, -59.6f, 59.6f, -39.4f, -37.9f, MRunoff, 0, One, 1.f, true);
	HT(FarF, 0.f, 57.9f, 59.6f, -37.9f, 35.9f, MRunoff, 0, One, 1.f, true);
	HT(FarF, 0.f, -59.6f, -57.9f, -37.9f, 35.9f, MRunoff, 0, One, 1.f, true);
	// kerbs round the concourse, road markings, car park bays
	auto Strip = [&](float X0, float X1, float Y0, float Y1, const FLinearColor& C) { HT(FarF, 0.f, X0, X1, Y0, Y1, MWhite, 0, C); };
	const FLinearColor Kerb = Lin(170, 168, 160), Paint = Lin(220, 220, 210);
	Tint = ConcourseTint;
	Strip(-80.3f, 80.3f, 68.f, 68.3f, Kerb); Strip(-80.3f, 80.3f, -52.3f, -52.f, Kerb); Strip(-80.3f, -80.f, -52.f, 68.f, Kerb); Strip(80.f, 80.3f, -52.f, 68.f, Kerb);
	Tint = TownTint;
	for (float X = -168.f; X < 168.f; X += 6.f) { Strip(X, X + 3.f, 73.9f, 74.15f, Paint); Strip(X, X + 3.f, -79.15f, -78.9f, Paint); }
	for (float Y = -118.f; Y < 148.f; Y += 6.f) { Strip(87.9f, 88.15f, Y, Y + 3.f, Paint); Strip(-88.15f, -87.9f, Y, Y + 3.f, Paint); }
	for (float X = -60.f; X <= 60.f; X += 3.f) { Strip(X, X + 0.12f, -70.f, -64.f, Paint); }
	Strip(-60.f, 60.12f, -70.12f, -70.f, Paint);
	Tint = StandTint;

	// ================================================================ stands (after the grass, before the players)
	Layer = LStands;
	// the streets beyond the far stand and the east end are farther than anything in the ground, so they go first
	Tint = TownTint;
	for (float X = -78.f; X < 78.f; X += 26.f) Terrace(FarF, X, X + 25.f, 80.f, false);
	for (float X = -76.f; X < 78.f; X += 13.f) Tree(FarF, X, 70.5f, 6.5f + 1.5f * (int32(X) & 1));
	for (float Y = 60.f; Y > -110.f; Y -= 26.f) Terrace(EastF, Y - 25.f, Y, 92.f, false);
	for (float Y = 62.f; Y > -110.f; Y -= 12.f) Tree(FarF, 85.f, Y, 7.f);
	// far-right pylon first: the far stand is nearer the camera than it
	Tint = ConcourseTint;
	Pylon(77.f, 67.f);
	Tint = StandTint;
	// ---- far stand
	{
		FStand S; S.F = &FarF; S.U0 = -74.f; S.U1 = 74.f; S.bBack = false; S.FlagRate = 0.012f;   // corner to corner: the ends stop at its front
		RowsFrom(S.Rows, 37.6f, 1.1f, 20, 0.8f, 0.45f, 1.1f);
		const int32 Low = S.Rows.Num();
		RowsFrom(S.Rows, 37.6f + 16.f, S.Rows.Last().Z + 2.4f, 12, 0.8f, 0.5f, 2.4f);
		S.Rows[Low].bFascia = true;
		const float RoofW = S.Rows[Low + 3].W, RoofZ = S.Rows.Last().Z + 3.2f;
		for (FRow& R : S.Rows) if (R.W >= RoofW - 0.01f) R.Shade = 0.72f;
		const float BackW = S.Rows.Last().W + S.Rows.Last().D;
		// the rear wall's inside, up to the roof
		VT(FarF, BackW, S.U0, S.U1, S.Rows.Last().Z, RoofZ - 0.6f, NToPitch, MBrick, 0, One, 0.65f, true);
		S.NoSeat = [](int32 k, float U) { return k <= 2 && FMath::Abs(U) < 2.7f; };
		S.DarkAt = [](int32 k, float U) { if (k < 9 || k > 11) return false; for (float C : { -36.f, -12.f, 12.f, 36.f }) if (FMath::Abs(U - C) < 1.2f) return true; return false; };
		S.Pick = [&Rng](int32, float) { uint8 F; do F = uint8(Rng.Int(20)); while (F == 5 || F == 8); return F; };
		S.After = [&, RoofW, RoofZ, Low](int32 k)
		{
			if (k == 0)
			{
				// the players' tunnel: a canopy over the first three rows, its mouth on the pitch wall
				Box(FarF, -2.4f, 2.4f, 37.3f, 40.0f, 0.f, 2.45f, MFascia, MFascia, MRoof, FitT, FitT, 0);
				VT(FarF, 37.29f, -1.7f, 1.7f, 0.f, 2.05f, NToPitch, MTunnel, FitS | FitT);
			}
			if (k == Low + 3)
				for (float U = -72.f; U <= 72.f; U += 18.f)   // roof columns on the front of the upper tier
					Box(FarF, U - 0.18f, U + 0.18f, RoofW - 0.36f, RoofW, 0.f, RoofZ - 0.5f, MSteel, MSteel, MSteel, FitS, FitS, FitS | FitT);
		};
		BuildStand(S);
		Gable(S, RoofW - 0.5f, RoofZ, RoofZ - 1.2f, BackW + 0.6f, NUNeg);
		Roof(FarF, S.U0 - 0.5f, S.U1 + 0.5f, RoofW - 0.5f, BackW + 0.6f, RoofZ, RoofZ - 1.2f);
		// the pitch wall, its coping, banners hung over it
		VT(FarF, 37.45f, S.U0, S.U1, 0.f, 1.1f, NToPitch, MWallGreen);
		HT(FarF, 1.1f, S.U0, S.U1, 37.45f, 37.6f, MCoping, FitT);
		for (float U : { -44.f, -27.f, 19.f, 33.f, 51.f }) Banner(FarF, U, 37.45f, 1.1f, 2.6f, 0.95f);
		Boards(FarF, -57.f, 57.f, 36.05f);
		// photographers and a TV camera behind the far touchline are the match's own crew (PFMatch)
	}
	Tint = ConcourseTint;
	Pylon(-77.f, 67.f);
	Tint = StandTint;
	// ---- east end: covered, seen from the front; the away supporters in its north half, the big screen on its roof
	{
		FStand S; S.F = &EastF; S.U0 = -39.f; S.U1 = 37.4f; S.bBack = false; S.FlagRate = 0.015f;
		RowsFrom(S.Rows, 59.4f, 1.1f, 18, 0.8f, 0.45f, 1.1f);
		const float RoofW = S.Rows[10].W, RoofZ = S.Rows.Last().Z + 2.9f;
		for (FRow& R : S.Rows) if (R.W >= RoofW - 0.01f) R.Shade = 0.74f;
		const float BackW = S.Rows.Last().W + S.Rows.Last().D;
		VT(EastF, BackW, S.U0, S.U1, S.Rows.Last().Z, RoofZ - 0.6f, NToPitch, MBrick, 0, One, 0.65f, true);
		S.DarkAt = [](int32 k, float U) { return k >= 6 && k <= 7 && (FMath::Abs(U + 18.f) < 1.0f || FMath::Abs(U - 4.f) < 1.0f); };
		S.Pick = [&Rng](int32, float U) -> uint8
		{
			if (U > 8.f) { static const uint8 Away[] = { 5, 8, 5, 8, 5, 8, 11, 14 }; return Away[Rng.Int(8)]; }
			uint8 F; do F = uint8(Rng.Int(20)); while (F == 5 || F == 8); return F;
		};
		S.After = [&, RoofW, RoofZ](int32 k)
		{
			if (k == 10) for (float U = -33.f; U <= 33.f; U += 16.5f) Box(EastF, U - 0.18f, U + 0.18f, RoofW - 0.36f, RoofW, 0.f, RoofZ - 0.5f, MSteel, MSteel, MSteel, FitS, FitS, FitS | FitT);
		};
		BuildStand(S);
		Gable(S, RoofW - 0.5f, RoofZ, RoofZ - 1.0f, BackW + 0.6f, NUNeg);
		Roof(EastF, S.U0 - 0.5f, S.U1 + 0.5f, RoofW - 0.5f, BackW + 0.6f, RoofZ, RoofZ - 1.0f);
		// the big screen: on legs on the roof, facing the pitch; its face is drawn live (score, clock)
		{
			const float Sw = 0.f, Hw = 4.2f, Zb = RoofZ + 0.6f, Zt = Zb + 4.6f, Wd = RoofW + 1.2f;
			Box(EastF, Sw - 0.5f, Sw - 0.2f, Wd + 0.1f, Wd + 0.4f, RoofZ - 0.4f, Zb, MSteel, MSteel, MSteel, FitS, FitS, FitS | FitT);
			Box(EastF, Sw + 0.2f, Sw + 0.5f, Wd + 0.1f, Wd + 0.4f, RoofZ - 0.4f, Zb, MSteel, MSteel, MSteel, FitS, FitS, FitS | FitT);
			Box(EastF, Sw - Hw - 0.3f, Sw + Hw + 0.3f, Wd, Wd + 0.6f, Zb - 0.3f, Zt + 0.3f, MSteel, MSteel, MCoping, FitS | FitT, FitS | FitT, FitS | FitT);
			VT(EastF, Wd - 0.01f, Sw - Hw, Sw + Hw, Zb, Zt, NToPitch, MScreen, FitS | FitT | Unlit);
			// the live face: columns run along -u (the screen reads left to right for someone on the pitch)
			FScreen Sc;
			const FVector2D O = Pv.ToAtlas(EastF.At(FVector(Sw + Hw - 0.25f, Wd - 0.02f, Zt - 0.3f)));
			Sc.O = FVector2f(O);
			Sc.U = FVector2f(Pv.ToAtlas(EastF.At(FVector(Sw - Hw + 0.25f, Wd - 0.02f, Zt - 0.3f))) - O);
			Sc.V = FVector2f(Pv.ToAtlas(EastF.At(FVector(Sw + Hw - 0.25f, Wd - 0.02f, Zb + 0.3f))) - O);
			FOp Op; Op.Kind = KScreen; Op.Idx = Screens.Add(Sc);
			Op.Lo = FVector2f(1e9f, 1e9f); Op.Hi = FVector2f(-1e9f, -1e9f);
			for (const FVector2f& Pc : { Sc.O, Sc.O + Sc.U, Sc.O + Sc.V, Sc.O + Sc.U + Sc.V }) { Op.Lo = FVector2f(FMath::Min(Op.Lo.X, Pc.X), FMath::Min(Op.Lo.Y, Pc.Y)); Op.Hi = FVector2f(FMath::Max(Op.Hi.X, Pc.X), FMath::Max(Op.Hi.Y, Pc.Y)); }
			Op.Col = FLinearColor(1.f, 0.82f, 0.25f, 1.f);
			Ops[Layer].Add(Op);
		}
		VT(EastF, 59.25f, S.U0, S.U1, 0.f, 1.1f, NToPitch, MWallGreen);
		HT(EastF, 1.1f, S.U0, S.U1, 59.25f, 59.4f, MCoping, FitT);
		for (float U : { -30.f, -10.f, 16.f, 28.f }) Banner(EastF, U, 59.25f, 1.1f, 2.6f, 0.95f);
		// the boards, then the photographers behind the east goal (on the pitch side of the boards: nearer)
		Boards(EastF, -37.f, 35.f, 58.05f);
		for (int32 i = 6; i >= 0; --i)
		{
			const float Y = (i < 3 ? -11.f : 5.f) + (i % 4) * 2.1f + Rng.Range(-0.3f, 0.3f);
			if (FMath::Abs(Y) < 4.5f) continue;
			AddSprite(FVector(57.0f + Rng.Range(-0.2f, 0.3f), Y, 0.f), MPhoto0 + Rng.Int(NPhoto), true);
		}
	}
	// ---- west end: an open kop, seen from behind (backs of the boards, then the rows from the front, its side wall,
	// then its facade); photographers behind the west goal on the pitch side of the boards
	{
		for (int32 i = 0; i < 7; ++i)
		{
			const float Y = (i < 3 ? 11.f : -5.f) - (i % 4) * 2.1f + Rng.Range(-0.3f, 0.3f);
			if (FMath::Abs(Y) < 4.5f) continue;
			AddSprite(FVector(-57.0f - Rng.Range(-0.2f, 0.3f), Y, 0.f), MPhoto0 + Rng.Int(NPhoto), false);
		}
		Boards(WestF, -37.f, 35.f, 58.05f);
		FStand S; S.F = &WestF; S.U0 = -39.f; S.U1 = 37.4f; S.bBack = true;
		RowsFrom(S.Rows, 59.4f, 1.1f, 18, 0.8f, 0.45f, 1.1f);
		S.DarkAt = [](int32 k, float U) { return k >= 6 && k <= 8 && (FMath::Abs(U + 18.f) < 1.2f || FMath::Abs(U - 4.f) < 1.2f); };
		S.Pick = [&Rng](int32, float) { uint8 F; do F = uint8(Rng.Int(20)); while (F == 5 || F == 8); return F; };
		BuildStand(S);
		Gable(S, -1.f, 0.f, 0.f, 0.f, NUNeg);
		const float BackW = S.Rows.Last().W + S.Rows.Last().D, TopZ = S.Rows.Last().Z + 1.2f;
		HT(WestF, TopZ, S.U0 - 0.3f, S.U1, BackW - 0.3f, BackW, MCoping, FitT);
		VT(WestF, BackW, S.U0 - 0.3f, S.U1, 0.f, TopZ, NOut, MBrick, 0, One, 1.f, true);
		for (float U = S.U0 + 6.f; U < S.U1 - 4.f; U += 14.f) VT(WestF, BackW + 0.02f, U, U + 1.8f, 0.f, 2.6f, NOut, MGate, FitS | FitT);
		for (float U = S.U0 + 12.f; U < S.U1 - 4.f; U += 14.f) VT(WestF, BackW + 0.02f, U, U + 1.2f, 4.2f, 5.2f, NOut, MWindow, FitS | FitT);
		VT(WestF, BackW + 0.01f, S.U0 - 0.3f, S.U1, TopZ - 0.6f, TopZ - 0.1f, NOut, MFascia, FitT);
		for (float U = S.U0 + 3.f; U < S.U1 - 2.f; U += 10.f) VT(WestF, BackW + 0.03f, U, U + 0.12f, 0.f, TopZ - 0.65f, NOut, MSteel, FitS);
	}
	// the streets beyond the west end: nearer the camera than the stand, so after it
	Tint = TownTint;
	for (float Y = 60.f; Y > -110.f; Y -= 26.f) Terrace(EastF, Y - 25.f, Y, -106.f, false);   // faces -x: the frame is the east one
	for (float Y = 62.f; Y > -110.f; Y -= 12.f) Tree(FarF, -85.f, Y, 7.f);
	Tint = StandTint;

	// ================================================================ near side (after the players)
	Layer = LNear;
	{
		Boards(NearF, -57.f, 57.f, 38.05f);
		FStand S; S.F = &NearF; S.U0 = -74.f; S.U1 = 74.f; S.bBack = true;
		RowsFrom(S.Rows, 39.2f, 1.0f, 8, 0.85f, 0.28f, 1.0f);
		auto InDugout = [](float U) { const float A = FMath::Abs(U); return A > 4.2f && A < 13.8f; };
		S.NoSeat = [InDugout](int32 k, float U) { return k <= 1 && InDugout(U); };
		S.Pick = [&Rng](int32, float) { uint8 F; do F = uint8(Rng.Int(20)); while (F == 5 || F == 8); return F; };
		S.After = [&](int32 k)
		{
			if (k != 1) return;
			// the dugouts: sunk into the front of the paddock, perspex roofs
			for (float Sg : { -1.f, 1.f })
			{
				const float A = Sg < 0 ? -13.8f : 4.2f, B = Sg < 0 ? -4.2f : 13.8f;
				Box(NearF, A, B, 38.6f, 40.95f, 0.f, 1.75f, MWallGreen, MWallGreen, MPerspex, FitT, FitT, FitS | FitT);
			}
		};
		BuildStand(S);
		Gable(S, -1.f, 0.f, 0.f, 0.f, NUNeg);
		const float BackW = S.Rows.Last().W + S.Rows.Last().D, TopZ = S.Rows.Last().Z + 1.1f;
		HT(NearF, TopZ, S.U0 - 0.3f, S.U1, BackW - 0.3f, BackW, MCoping, FitT);
		VT(NearF, BackW, S.U0 - 0.3f, S.U1, 0.f, TopZ, NOut, MBrick, 0, One, 1.f, true);
		// the paddock's back: a club-colour band under the coping, turnstile gates, windows and downpipes
		VT(NearF, BackW + 0.01f, S.U0 - 0.3f, S.U1, TopZ - 0.55f, TopZ - 0.1f, NOut, MFascia, FitT);
		for (float U = S.U0 + 8.f; U < S.U1 - 4.f; U += 16.f) VT(NearF, BackW + 0.02f, U, U + 1.8f, 0.f, 2.4f, NOut, MGate, FitS | FitT);
		for (float U = S.U0 + 13.f; U < S.U1 - 4.f; U += 16.f) VT(NearF, BackW + 0.02f, U, U + 1.2f, 1.2f, 2.2f, NOut, MWindow, FitS | FitT);
		for (float U = S.U0 + 4.f; U < S.U1 - 2.f; U += 12.f) VT(NearF, BackW + 0.03f, U, U + 0.12f, 0.f, TopZ - 0.6f, NOut, MSteel, FitS);
	}
	// the car park and the street on the near side (nearer than the stand)
	{
		Tint = TownTint;
		static const FLinearColor Cars[] = { Lin(180, 40, 40), Lin(40, 70, 150), Lin(220, 220, 210), Lin(60, 60, 66), Lin(40, 120, 70), Lin(200, 170, 60), Lin(120, 120, 128), Lin(140, 30, 50) };
		for (float X = 58.5f; X > -60.f; X -= 3.f)
			if (Rng.Unit() < 0.7f) Car(X, -67.f, Cars[Rng.Int(8)]);
		for (float X = 70.f; X > -80.f; X -= 11.f) Tree(FarF, X, -74.5f, 6.f + Rng.Range(0.f, 2.f));
		for (float X = 78.f; X > -104.f; X -= 26.f) Terrace(FarF, X - 25.f, X, -86.f, true);
	}
	int32 NQ = 0; for (const TArray<FOp>& L : Ops) NQ += L.Num();
	UE_LOG(LogPF, Log, TEXT("PF stadium3d: %d ops (%d / %d / %d), %d fans, %d sprites"), NQ, Ops[LGround].Num(), Ops[LStands].Num(), Ops[LNear].Num(), Fans.Num(), Sprites.Num());
}

// ------------------------------------------------------------------------------------------------ test tour
namespace
{
	struct FTour
	{
		bool bRead = false; float S = 0.f; double T0 = 0.0; TArray<float> Zooms;
		void Read()
		{
			if (bRead) return;
			bRead = true;
			FParse::Value(FCommandLine::Get(), TEXT("pfstadtour="), S);
			FString Zs;
			if (FParse::Value(FCommandLine::Get(), TEXT("pfstadzooms="), Zs, false)) { TArray<FString> Parts; Zs.ParseIntoArray(Parts, TEXT(",")); for (const FString& P : Parts) Zooms.Add(FCString::Atof(*P)); }
			float Z = 0.f;
			if (Zooms.Num() == 0 && FParse::Value(FCommandLine::Get(), TEXT("pfstadzoom="), Z) && Z > 0.f) Zooms.Add(Z);
			T0 = FPlatformTime::Seconds();
		}
		int32 Step() const { return S > 0.f ? int32((FPlatformTime::Seconds() - T0) / S) : 0; }
	};
	FTour GTour;
	const TCHAR* GTourNames[] = { TEXT("kickoff"), TEXT("far_tunnel"), TEXT("far_left"), TEXT("far_right"), TEXT("far_left_corner"), TEXT("far_right_corner"),
		TEXT("west_end_north"), TEXT("west_end_south"), TEXT("east_end_north"), TEXT("east_end_south"), TEXT("near_left_corner"), TEXT("near_right_corner"),
		TEXT("near_left"), TEXT("near_dugouts"), TEXT("west_goal"), TEXT("east_goal") };
	// kick-off, the far side (centre, tunnel, both corners), both goals and the four corners of each end, the near side
	const FVector2D GTourPts[] = { { 0, 0 }, { 0, 33 }, { -30, 33 }, { 30, 33 }, { -54, 33 }, { 54, 33 }, { -56, 12 }, { -56, -12 }, { 56, 12 }, { 56, -12 },
		{ -54, -34 }, { 54, -34 }, { -20, -34 }, { 10, -34 }, { -52, 0 }, { 52, 0 } };
}

float FPFStadium3D::TourZoom(float GameZoom, float ScreenH)
{
	GTour.Read();
	if (!GTour.Zooms.Num()) return GameZoom;
	const int32 NP = UE_ARRAY_COUNT(GTourPts);
	const float Z = GTour.Zooms[(GTour.S > 0.f ? GTour.Step() / NP : 0) % GTour.Zooms.Num()];
	return FMath::Max(0.25f, ScreenH / 1080.f * Z);
}

bool FPFStadium3D::TourCam(FVector2D& Cam, const FPFProjection& Proj)
{
	GTour.Read();
	if (GTour.S <= 0.f) return false;
	// at the whole-ground zoom the camera holds the centre of the stadium (beyond the play area's limits)
	const int32 NP = UE_ARRAY_COUNT(GTourPts);
	// -pfstadshots: one named screenshot per stop (stad_<stage>_z<zoom>_<where>.png), then quit after the last zoom
	{
		static const bool bShots = FParse::Param(FCommandLine::Get(), TEXT("pfstadshots"));
		static int32 LastShot = -1;
		const int32 St = GTour.Step();
		const double InStep = (FPlatformTime::Seconds() - GTour.T0) / GTour.S - St;
		const int32 NZ = FMath::Max(1, GTour.Zooms.Num());
		if (bShots && St >= NP * NZ) FPlatformMisc::RequestExit(false);
		else if (bShots && St != LastShot && InStep > 0.6)
		{
			LastShot = St;
			const float Z = GTour.Zooms.Num() ? GTour.Zooms[(St / NP) % NZ] : 0.f;
			FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("stad_%s_z%.2f_%s.png"), *GTourStage, Z, GTourNames[St % NP]), false, false);
		}
	}
	const bool bOverview = GTour.Zooms.Num() && GTour.Zooms[(GTour.Step() / NP) % GTour.Zooms.Num()] < 1.f;
	Cam = bOverview ? Proj.ToAtlas(FVector(0.f, 10.f, 0.f)) + FVector2D((GTour.Step() % NP) % 2 ? 0.f : 0.f, 0.f) : Proj.GroundToAtlas(GTourPts[GTour.Step() % NP]);
	return bOverview;
}

// ------------------------------------------------------------------------------------------------ drawing
void FPFStadium3D::DrawLayer(UCanvas* C, int32 Layer, const FVector2D& TL, float Zoom, float FanScale, const FPFCrowdView& Crowd) const
{
	if (!C || !Sheet || !Sheet->GetResource()) return;
	const double Tm0 = FPlatformTime::Seconds();
	const float SW = C->ClipX, SH = C->ClipY;
	const FVector2f Lo(TL.X, TL.Y), Hi(TL.X + SW / Zoom, TL.Y + SH / Zoom);
	const FVector2f T0(TL.X, TL.Y);
	TArray<FCanvasUVTri> Tris;
	Tris.Reserve(8192);
	const FTile& Wt = Tiles.IsValidIndex(MWhite) ? Tiles[MWhite] : FTile();
	const FVector2D WUV((Wt.X + 4.f) / SheetW, (Wt.Y + 4.f) / SheetH);
	const float Bob = FMath::Sin(Crowd.Clock * 90.f);
	const int32 FanRows = FrontRows;
	auto Rect = [&](const FVector2D& P0, const FVector2D& P1, const FVector2D& U0, const FVector2D& U1, const FLinearColor& Col)
	{
		FCanvasUVTri A, B;
		A.V0_Pos = P0; A.V1_Pos = FVector2D(P1.X, P0.Y); A.V2_Pos = P1;
		A.V0_UV = U0; A.V1_UV = FVector2D(U1.X, U0.Y); A.V2_UV = U1;
		B.V0_Pos = P0; B.V1_Pos = P1; B.V2_Pos = FVector2D(P0.X, P1.Y);
		B.V0_UV = U0; B.V1_UV = U1; B.V2_UV = FVector2D(U0.X, U1.Y);
		A.V0_Color = A.V1_Color = A.V2_Color = B.V0_Color = B.V1_Color = B.V2_Color = Col;
		Tris.Add(A); Tris.Add(B);
	};
	auto TileUV = [&](int32 M, float Fx0, float Fy0, float Fx1, float Fy1, FVector2D& U0, FVector2D& U1)
	{
		const FTile& T = Tiles[M];
		U0 = FVector2D((T.X + Fx0 * T.W) / SheetW, (T.Y + Fy0 * T.H) / SheetH);
		U1 = FVector2D((T.X + Fx1 * T.W) / SheetW, (T.Y + Fy1 * T.H) / SheetH);
	};
	for (const FOp& Op : Ops[Layer])
	{
		if (Op.Hi.X < Lo.X || Op.Lo.X > Hi.X || Op.Hi.Y < Lo.Y || Op.Lo.Y > Hi.Y) continue;
		if (Op.Kind == KFace)
		{
			FVector2D S[4];
			for (int32 i = 0; i < Op.N; ++i) S[i] = FVector2D((Op.P[i] - T0) * Zoom);
			FCanvasUVTri A; A.V0_Pos = S[0]; A.V1_Pos = S[1]; A.V2_Pos = S[2];
			A.V0_UV = FVector2D(Op.UV[0]); A.V1_UV = FVector2D(Op.UV[1]); A.V2_UV = FVector2D(Op.UV[2]);
			A.V0_Color = A.V1_Color = A.V2_Color = Op.Col;
			Tris.Add(A);
			if (Op.N == 4)
			{
				FCanvasUVTri B = A; B.V1_Pos = S[2]; B.V2_Pos = S[3]; B.V1_UV = FVector2D(Op.UV[2]); B.V2_UV = FVector2D(Op.UV[3]);
				Tris.Add(B);
			}
			continue;
		}
		if (Op.Kind == KSprite)
		{
			const FSprite& Sp = Sprites[Op.Idx];
			const FVector2D F = FVector2D((Sp.Feet - T0) * Zoom);
			const FVector2D P0(FMath::RoundToFloat(F.X - 32.f * FanScale), FMath::RoundToFloat(F.Y - 63.f * FanScale));
			const FVector2D P1(P0.X + FMath::RoundToFloat(64.f * FanScale), P0.Y + FMath::RoundToFloat(64.f * FanScale));
			FVector2D U0, U1; TileUV(Sp.Tile, Sp.bFlip ? 1.f : 0.f, 0.f, Sp.bFlip ? 0.f : 1.f, 1.f, U0, U1);
			Rect(P0, P1, U0, U1, Crowd.Light * Op.Col);
			// a photographer's flash when a goal goes in
			if (Crowd.bGoal && ((int32(Crowd.T * 9.f) + int32(Sp.Hash & 7)) % 7) == 0)
			{
				const FVector2D Fc(F.X + (Sp.bFlip ? -6.f : 6.f) * FanScale, F.Y - 40.f * FanScale);
				const float R = 5.f * FanScale;
				Rect(Fc - FVector2D(R, R), Fc + FVector2D(R, R), WUV, WUV, FLinearColor(1.f, 1.f, 0.95f, 1.f));
			}
			continue;
		}
		if (Op.Kind == KScreen)
		{
			// the big screen: two lines of amber dot-matrix text, centred, each lit dot a parallelogram on the face
			const FScreen& Sc = Screens[Op.Idx];
			const int32 Cols = 70, Rows = 19;
			auto Cell = [&](int32 Cx, int32 Cy, const FLinearColor& Col)
			{
				const FVector2f A = Sc.O + Sc.U * (float(Cx) / Cols) + Sc.V * (float(Cy) / Rows);
				const FVector2f Du = Sc.U * (0.9f / Cols), Dv = Sc.V * (0.9f / Rows);
				FCanvasUVTri T1, T2;
				const FVector2D P0((A - T0) * Zoom), P1((A + Du - T0) * Zoom), P2((A + Du + Dv - T0) * Zoom), P3((A + Dv - T0) * Zoom);
				T1.V0_Pos = P0; T1.V1_Pos = P1; T1.V2_Pos = P2; T2.V0_Pos = P0; T2.V1_Pos = P2; T2.V2_Pos = P3;
				T1.V0_UV = T1.V1_UV = T1.V2_UV = T2.V0_UV = T2.V1_UV = T2.V2_UV = WUV;
				T1.V0_Color = T1.V1_Color = T1.V2_Color = T2.V0_Color = T2.V1_Color = T2.V2_Color = Col;
				Tris.Add(T1); Tris.Add(T2);
			};
			auto Line = [&](const FString& S, int32 Row0, const FLinearColor& Col)
			{
				const int32 W = S.Len() * 6 - 1, X0 = FMath::Max(0, (Cols - W) / 2);
				for (int32 c = 0; c < S.Len(); ++c)
				{
					const TCHAR* G = PFArt::Glyph57(S[c]);
					if (!G) continue;
					for (int32 r = 0; r < 7; ++r) for (int32 k = 0; k < 5; ++k) if (G[r * 5 + k] == '1' && X0 + c * 6 + k < Cols) Cell(X0 + c * 6 + k, Row0 + r, Col);
				}
			};
			const bool bFlash = Crowd.bGoal && (int32(Crowd.T * 4.f) & 1);
			Line(bFlash ? FString(TEXT("GOAL")) : Crowd.Screen1, 2, bFlash ? FLinearColor(1.f, 1.f, 0.6f) : FLinearColor(1.f, 0.72f, 0.16f));
			Line(Crowd.Screen2, 11, FLinearColor(0.6f, 1.f, 0.5f));
			continue;
		}
		// a supporter: seated, or on his feet (a goal, a big moment) - the same rules as the v0.9 crowd
		const FFan& F = Fans[Op.Idx];
		const uint32 Hh = F.Hash;
		int32 React = 0;
		if (Crowd.Mood != 0 && FanRows >= 8 && (Hh >> 28) < 13)
			React = Crowd.Mood > 0 ? (F.Side == Crowd.Team ? 1 : -1) : (F.Side == Crowd.Team ? -1 : 0);
		const bool bStand = React != 0 || (Crowd.bGoal && (Hh >> 28) < 11);
		int32 Row = bStand && FanRows >= 2 ? 1 : 0;
		if (React > 0) { static const int32 Seq[] = { 2, 3, 4, 3 }; Row = Seq[(int32((Crowd.T + (Hh >> 20 & 7) * 0.05f) * 7.f)) % 4]; }
		if (React < 0) { const float Tn = (Crowd.Mood > 0 ? 5.f : 2.f) - Crowd.T; Row = Tn < 0.25f ? 5 : Tn < 1.2f + ((Hh >> 24) & 3) * 0.2f ? 6 : 7; }
		Row = FMath::Min(Row, FanRows - 1);
		if (F.bBack) Row += FanRows;
		const float Hop = (bStand && React >= 0) ? FMath::RoundToFloat(FMath::Max(0.f, Bob * ((Hh & 3) == 0 ? 1.f : -1.f)) * 2.f) * FanScale : 0.f;
		FVector2D Anchor = FVector2D(((bStand ? F.Feet : F.Hip) - T0) * Zoom);
		Anchor = FVector2D(FMath::RoundToFloat(Anchor.X), FMath::RoundToFloat(Anchor.Y));
		const float Ay = bStand ? 79.f : 63.f;
		const FVector2D P0(FMath::RoundToFloat(Anchor.X - 32.f * FanScale), FMath::RoundToFloat(Anchor.Y - Ay * FanScale - Hop));
		const FVector2D P1(P0.X + FMath::RoundToFloat(64.f * FanScale), P0.Y + FMath::RoundToFloat(CellH * FanScale));
		const FLinearColor Col = Crowd.Light * Op.Col;
		Rect(P0, P1, FVector2D(F.Fi * 64.f / SheetW, float(Row * CellH) / SheetH), FVector2D((F.Fi + 1) * 64.f / SheetW, float((Row + 1) * CellH) / SheetH), Col);
		if (F.bFlag && Tiles.IsValidIndex(MFlag))
		{
			// a flag on a pole from his right hand, waving (4 frames), held higher when he is up
			const int32 Fr = (int32(Crowd.Clock * 40.f + (Hh & 15)) & 3);
			const FVector2D Hand(Anchor.X + 9.f * FanScale, Anchor.Y - (bStand ? 44.f : 28.f) * FanScale - Hop);
			const FVector2D Q0(FMath::RoundToFloat(Hand.X - 1.f * FanScale), FMath::RoundToFloat(Hand.Y - 40.f * FanScale));
			const FVector2D Q1(Q0.X + FMath::RoundToFloat(24.f * FanScale), Q0.Y + FMath::RoundToFloat(16.f * FanScale));
			FVector2D U0, U1; TileUV(MFlag, Fr / 4.f, 0.f, (Fr + 1) / 4.f, 1.f, U0, U1);
			Rect(Q0, Q1, U0, U1, Col);
			// the rest of the pole down to his hand
			FVector2D V0, V1; TileUV(MFlag, Fr / 4.f, 0.f, Fr / 4.f + 2.f / 96.f, 1.f, V0, V1);
			Rect(FVector2D(Q0.X, Q1.Y), FVector2D(Q0.X + FMath::Max(1.f, FMath::RoundToFloat(2.f * FanScale)), FMath::RoundToFloat(Hand.Y)), V0, V1, Col);
		}
	}
	if (Tris.Num())
	{
		FCanvasTriangleItem TI(Tris, Sheet->GetResource());
		TI.BlendMode = SE_BLEND_Masked;
		C->DrawItem(TI);
	}
	// cost on the game thread (culling + building the triangle list), logged now and then
	static double Acc[LNum] = {}; static int32 Frames[LNum] = {}; static int32 MaxTris[LNum] = {};
	Acc[Layer] += FPlatformTime::Seconds() - Tm0; MaxTris[Layer] = FMath::Max(MaxTris[Layer], Tris.Num());
	if (++Frames[Layer] == 600)
	{
		UE_LOG(LogPF, Log, TEXT("PF stadium3d layer %d: %.3f ms a frame, up to %d triangles"), Layer, Acc[Layer] * 1000.0 / Frames[Layer], MaxTris[Layer]);
		Acc[Layer] = 0; Frames[Layer] = 0; MaxTris[Layer] = 0;
	}
}

void FPFStadium3D::DrawGround(UCanvas* C, const FVector2D& TL, float Zoom) const { DrawLayer(C, LGround, TL, Zoom, 1.f, FPFCrowdView()); }
void FPFStadium3D::DrawStands(UCanvas* C, const FVector2D& TL, float Zoom, float FanScale, const FPFCrowdView& Crowd) const { DrawLayer(C, LStands, TL, Zoom, FanScale, Crowd); }
void FPFStadium3D::DrawNear(UCanvas* C, const FVector2D& TL, float Zoom, float FanScale, const FPFCrowdView& Crowd) const { DrawLayer(C, LNear, TL, Zoom, FanScale, Crowd); }
