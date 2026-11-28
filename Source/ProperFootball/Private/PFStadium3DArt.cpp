// Proper Football - v0.10.0 stadium pixel art: every surface of the 3D ground (PFStadium3D.cpp) is textured from small
// hand-tuned procedural pixel-art tiles painted here into the same sheet as the supporters, so the whole ground still
// draws as one nearest-sampled triangle list. Each tile uses a short palette with ordered (Bayer) dithering between its
// steps, like 16-bit art: speckled concrete steps with a lighter nosing, stained risers, plastic seats in the club colour,
// English-bond brick, corrugated roof sheeting with lap lines and rust, steel lattice with see-through gaps, floodlight
// heads, A-frame advertising boards with a 5x7 pixel font, slate roofs and terraced house fronts for the streets round
// the ground. Tiles carry a 2-texel wrapped border so a nearest sample on a tile's edge never picks up its neighbour.
#include "PFStadium3D.h"
#include "Misc/FileHelper.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"

namespace PFArt
{
	uint32 Hash(int32 X, int32 Y, int32 S)
	{
		uint32 H = uint32(X) * 374761393u + uint32(Y) * 668265263u + uint32(S) * 2246822519u + 0x9E3779B9u;
		H = (H ^ (H >> 13)) * 1274126177u;
		return H ^ (H >> 16);
	}
	float N(int32 X, int32 Y, int32 S) { return (Hash(X, Y, S) & 0xFFFF) / 65535.f; }
	float Bayer(int32 X, int32 Y) { static const int32 B[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 }; return (B[(Y & 3) * 4 + (X & 3)] + 0.5f) / 16.f; }
	// smooth value noise with a period (so tiles wrap)
	float Value(float X, float Y, int32 Cell, int32 PX, int32 PY, int32 S)
	{
		const int32 Cx = FMath::Max(1, PX / Cell), Cy = FMath::Max(1, PY / Cell);
		const float Fx = X / Cell, Fy = Y / Cell;
		const int32 X0 = FMath::FloorToInt(Fx), Y0 = FMath::FloorToInt(Fy);
		const float Tx = Fx - X0, Ty = Fy - Y0;
		auto At = [&](int32 I, int32 J) { return N(((I % Cx) + Cx) % Cx, ((J % Cy) + Cy) % Cy, S); };
		const float Sx = Tx * Tx * (3 - 2 * Tx), Sy = Ty * Ty * (3 - 2 * Ty);
		return FMath::Lerp(FMath::Lerp(At(X0, Y0), At(X0 + 1, Y0), Sx), FMath::Lerp(At(X0, Y0 + 1), At(X0 + 1, Y0 + 1), Sx), Sy);
	}
	FColor Ramp(const TArray<FColor>& Pal, float V, int32 X, int32 Y)
	{
		const float F = FMath::Clamp(V, 0.f, 1.f) * (Pal.Num() - 1);
		int32 I = FMath::FloorToInt(F);
		if (F - I > Bayer(X, Y)) ++I;
		return Pal[FMath::Clamp(I, 0, Pal.Num() - 1)];
	}
	FColor Scale(const FColor& C, float K) { return FColor(uint8(FMath::Clamp(C.R * K, 0.f, 255.f)), uint8(FMath::Clamp(C.G * K, 0.f, 255.f)), uint8(FMath::Clamp(C.B * K, 0.f, 255.f)), C.A); }
	FColor Mix(const FColor& A, const FColor& B, float T) { return FColor(uint8(FMath::Lerp(float(A.R), float(B.R), T)), uint8(FMath::Lerp(float(A.G), float(B.G), T)), uint8(FMath::Lerp(float(A.B), float(B.B), T)), uint8(FMath::Lerp(float(A.A), float(B.A), T))); }
	// a 5-step ramp from one colour: dark .. light
	TArray<FColor> RampOf(const FColor& C, float Lo = 0.55f, float Hi = 1.25f, int32 Steps = 5)
	{
		TArray<FColor> R;
		for (int32 i = 0; i < Steps; ++i) R.Add(Scale(C, FMath::Lerp(Lo, Hi, float(i) / (Steps - 1))));
		return R;
	}

	// 5x7 font: rows top to bottom, 5 bits each
	const TCHAR* Glyph57(TCHAR Ch)
	{
		switch (FChar::ToUpper(Ch))
		{
		case 'A': return TEXT("01110100011000111111100011000110001"); case 'B': return TEXT("11110100011000111110100011000111110");
		case 'C': return TEXT("01110100011000010000100001000101110"); case 'D': return TEXT("11110100011000110001100011000111110");
		case 'E': return TEXT("11111100001000011110100001000011111"); case 'F': return TEXT("11111100001000011110100001000010000");
		case 'G': return TEXT("01110100011000010111100011000101111"); case 'H': return TEXT("10001100011000111111100011000110001");
		case 'I': return TEXT("01110001000010000100001000010001110"); case 'J': return TEXT("00111000100001000010000101001001100");
		case 'K': return TEXT("10001100101010011000101001001010001"); case 'L': return TEXT("10000100001000010000100001000011111");
		case 'M': return TEXT("10001110111010110101100011000110001"); case 'N': return TEXT("10001110011010110011100011000110001");
		case 'O': return TEXT("01110100011000110001100011000101110"); case 'P': return TEXT("11110100011000111110100001000010000");
		case 'Q': return TEXT("01110100011000110001101011001001101"); case 'R': return TEXT("11110100011000111110101001001010001");
		case 'S': return TEXT("01111100001000001110000010000111110"); case 'T': return TEXT("11111001000010000100001000010000100");
		case 'U': return TEXT("10001100011000110001100011000101110"); case 'V': return TEXT("10001100011000110001100010101000100");
		case 'W': return TEXT("10001100011000110101101011010101010"); case 'X': return TEXT("10001100010101000100010101000110001");
		case 'Y': return TEXT("10001100010101000100001000010000100"); case 'Z': return TEXT("11111000010001000100010001000011111");
		case '0': return TEXT("01110100011001110101110011000101110"); case '1': return TEXT("00100011000010000100001000010001110");
		case '2': return TEXT("01110100010000100010001000100011111"); case '3': return TEXT("11110000010000101110000010000111110");
		case '4': return TEXT("00010001100101010010111110001000010"); case '5': return TEXT("11111100001111000001000011000101110");
		case '6': return TEXT("00110010001000011110100011000101110"); case '7': return TEXT("11111000010001000100010000100001000");
		case '8': return TEXT("01110100011000101110100011000101110"); case '9': return TEXT("01110100011000101111000010001001100");
		case ':': return TEXT("00000001000010000000001000010000000"); case '-': return TEXT("00000000000000011111000000000000000");
		case '\'': return TEXT("00100001000100000000000000000000000");
		default: return nullptr;
		}
	}
}

using namespace PFArt;

namespace
{
	struct FPaint
	{
		TArray<FColor>& Px; int32 SW; int32 X, Y, W, H;
		void Set(int32 x, int32 y, const FColor& C) { if (x < 0 || y < 0 || x >= W || y >= H) return; Px[(Y + y) * SW + X + x] = C; }
		FColor Get(int32 x, int32 y) const { x = ((x % W) + W) % W; y = ((y % H) + H) % H; return Px[(Y + y) * SW + X + x]; }
		void Fill(const FColor& C) { for (int32 y = 0; y < H; ++y) for (int32 x = 0; x < W; ++x) Set(x, y, C); }
		void Rect(int32 x0, int32 y0, int32 x1, int32 y1, const FColor& C) { for (int32 y = y0; y < y1; ++y) for (int32 x = x0; x < x1; ++x) Set(x, y, C); }
		// the 2-texel border: the tile wrapped round, so nearest samples at an edge stay inside the material
		void Pad()
		{
			for (int32 y = -2; y < H + 2; ++y)
				for (int32 x = -2; x < W + 2; ++x)
					if (x < 0 || y < 0 || x >= W || y >= H) Px[(Y + y) * SW + X + x] = Get(x, y);
		}
		// text in the 5x7 font, each font pixel Sx x Sy texels, with a drop shadow
		int32 TextWidth(const FString& S, int32 Sx) const { return S.Len() * 6 * Sx - Sx; }
		void Text(const FString& S, int32 x0, int32 y0, int32 Sx, int32 Sy, const FColor& Fg, const FColor& Sh, bool bShadow)
		{
			for (int32 Pass = bShadow ? 0 : 1; Pass < 2; ++Pass)
				for (int32 c = 0; c < S.Len(); ++c)
				{
					const TCHAR* G = Glyph57(S[c]);
					if (!G) continue;
					for (int32 r = 0; r < 7; ++r)
						for (int32 k = 0; k < 5; ++k)
							if (G[r * 5 + k] == '1')
								Rect(x0 + (c * 6 + k) * Sx + (Pass == 0 ? 1 : 0), y0 + r * Sy + (Pass == 0 ? 1 : 0), x0 + (c * 6 + k + 1) * Sx + (Pass == 0 ? 1 : 0), y0 + (r + 1) * Sy + (Pass == 0 ? 1 : 0), Pass == 0 ? Sh : Fg);
				}
		}
	};
}

const TCHAR* FPFStadium3D::BoardWord(int32 I)
{
	static const TCHAR* Words[] = { TEXT("PIX COLA"), TEXT("PROPER"), TEXT("GOAL TV"), TEXT("ROVERS ALE"), TEXT("KICK OFF"), TEXT("TURBO"),
		TEXT("MATCHDAY"), TEXT("SUPER BOOTS"), TEXT("FOOTBALL"), TEXT("NORTHGATE 96"), TEXT("PF RADIO"), TEXT("RED LION") };
	return Words[((I % 12) + 12) % 12];
}

void FPFStadium3D::PaintMaterials(TArray<FColor>& Px, const FColor& Seat, const FColor& SeatTrim, bool bNight)
{
	Tiles.SetNum(MNum);
	// shelf packing into the free part of the sheet: right of the supporters, then below them
	struct FArea { int32 X0, X1, Y0, Y1, Cx, Cy, ShH; };
	FArea Areas[2] = { { 64 * 20 + 4, SheetW, 0, CellH * FrontRows * 2 + 4, 0, 0, 0 }, { 0, SheetW, CellH * FrontRows * 2 + 8, SheetH, 0, 0, 0 } };
	for (FArea& A : Areas) { A.Cx = A.X0; A.Cy = A.Y0; }
	auto Alloc = [&](int32 M, int32 W, int32 H) -> FPaint
	{
		const int32 Pw = W + 4, Ph = H + 4;
		for (FArea& A : Areas)
		{
			if (A.Cx + Pw > A.X1) { A.Cx = A.X0; A.Cy += A.ShH; A.ShH = 0; }
			if (A.Cy + Ph > A.Y1 || Pw > A.X1 - A.X0) continue;
			FTile T; T.X = A.Cx + 2; T.Y = A.Cy + 2; T.W = W; T.H = H;
			A.Cx += Pw; A.ShH = FMath::Max(A.ShH, Ph);
			Tiles[M] = T;
			return FPaint{ Px, SheetW, T.X, T.Y, W, H };
		}
		UE_LOG(LogPF, Warning, TEXT("PF stadium3d: sheet full at material %d"), M);
		Tiles[M] = FTile{ 0, SheetH - 8, 4, 4 };
		static TArray<FColor> Dummy; Dummy.SetNumZeroed(68 * 68);
		return FPaint{ Dummy, 68, 2, 2, 1, 1 };
	};

	const TArray<FColor> Conc = { FColor(92, 90, 86), FColor(118, 116, 110), FColor(140, 138, 131), FColor(160, 158, 150), FColor(184, 182, 173), FColor(204, 202, 192) };
	const FColor Yellow(226, 190, 58), YellowD(180, 148, 42);

	{ FPaint P = Alloc(MWhite, 8, 8); P.Fill(FColor::White); P.Pad(); }
	// ---- tread: speckled concrete, a bright nosing at the front edge, shadow toward the riser behind, joints
	{
		FPaint P = Alloc(MTread, 256, 40);
		for (int32 y = 0; y < P.H; ++y)
			for (int32 x = 0; x < P.W; ++x)
			{
				float V = 0.56f + 0.16f * (Value(x, y, 8, P.W, P.H, 11) - 0.5f) + 0.14f * (N(x, y, 3) - 0.5f);
				if (y == 0) V = 0.95f; else if (y == 1) V = 0.8f; else if (y == 2) V = 0.38f;
				if (y >= 26) V -= (y - 26) * 0.022f;
				if (x % 64 == 0 && y > 2) V = 0.3f;
				if (N(x / 2, y / 2, 7) > 0.985f && y > 3) V -= 0.25f;   // gum and puddle spots
				P.Set(x, y, Ramp(Conc, V, x, y));
			}
		P.Pad();
	}
	// ---- riser: darker concrete, a lip under the step above, drip stains, a dark foot
	{
		FPaint P = Alloc(MRiser, 256, 16);
		for (int32 x = 0; x < P.W; ++x)
		{
			const bool bStain = N(x, 0, 21) < 0.12f; const int32 Len = 3 + int32(N(x, 1, 21) * 9);
			for (int32 y = 0; y < P.H; ++y)
			{
				float V = 0.36f + 0.12f * (N(x, y, 5) - 0.5f) + 0.1f * (Value(x, y, 16, P.W, P.H, 13) - 0.5f);
				if (y == 0) V = 0.62f; else if (y == 1) V = 0.5f;
				if (y >= P.H - 2) V -= 0.14f;
				if (bStain && y < Len) V -= 0.1f;
				if (x % 64 == 0) V = 0.2f;
				P.Set(x, y, Ramp(Conc, V, x, y));
			}
		}
		P.Pad();
	}
	// ---- aisle step: lighter concrete, yellow painted nosings at the front and the half step
	{
		FPaint P = Alloc(MAisle, 64, 40);
		for (int32 y = 0; y < P.H; ++y)
			for (int32 x = 0; x < P.W; ++x)
			{
				const float V = 0.68f + 0.12f * (N(x, y, 9) - 0.5f) - (y >= 13 && y < 20 ? 0.05f : 0.f);
				FColor C = Ramp(Conc, V, x, y);
				if (y <= 1 || (y >= 20 && y <= 21)) C = (y == 1 || y == 21) ? YellowD : Yellow;
				if (y == 19 || y == 2) C = Conc[1];
				P.Set(x, y, C);
			}
		P.Pad();
	}
	// ---- seats in the club colour: a moulded back with rounded top corners, and the pan
	const TArray<FColor> SeatR = RampOf(Seat, 0.5f, 1.3f, 5);
	{
		FPaint P = Alloc(MSeatBack, 16, 14);
		for (int32 y = 0; y < P.H; ++y)
			for (int32 x = 0; x < P.W; ++x)
			{
				float V = 0.55f - y * 0.012f;
				if (y <= 1) V = 0.85f;
				if (x <= 1) V += 0.15f; if (x >= 13) V -= 0.18f;
				if (y >= 11) V = 0.18f;
				FColor C = Ramp(SeatR, V, x, y);
				if (x == 0 || x == 15) C = SeatR[0];
				if ((y == 0 && (x <= 1 || x >= 14)) || (y == 1 && (x == 0 || x == 15))) C = FColor(0, 0, 0, 0);
				P.Set(x, y, C);
			}
		P.Pad();
	}
	{
		FPaint P = Alloc(MSeatPan, 16, 12);
		for (int32 y = 0; y < P.H; ++y)
			for (int32 x = 0; x < P.W; ++x)
			{
				float V = 0.6f - y * 0.025f;
				if (y <= 1) V = 0.95f;
				if (x == 0 || x == 15) V = 0.15f;
				P.Set(x, y, Ramp(SeatR, V, x, y));
			}
		P.Pad();
	}
	// ---- brick: English bond, four brick tones, soot toward the foot handled by the caller's shade
	{
		FPaint P = Alloc(MBrick, 96, 48);
		const FColor Bricks[] = { FColor(132, 72, 56), FColor(146, 82, 62), FColor(118, 64, 50), FColor(156, 94, 70), FColor(108, 60, 48) };
		const FColor Mortar(168, 158, 144), MortarD(132, 124, 112);
		for (int32 y = 0; y < P.H; ++y)
			for (int32 x = 0; x < P.W; ++x)
			{
				const int32 Row = y / 4, Off = (Row & 1) ? 6 : 0;
				const int32 Bx = (x + Off) / 12;
				const bool bM = (y % 4) == 3 || ((x + Off) % 12) == 11;
				FColor C;
				if (bM) C = ((y % 4) == 3 && ((x + Off) % 12) != 11) ? Mortar : MortarD;
				else
				{
					const FColor B = Bricks[Hash(Bx, Row, 31) % 5];
					const float K = 0.92f + 0.16f * N(x, y, 33) + ((y % 4) == 0 ? 0.08f : 0.f);
					C = Scale(B, K);
				}
				P.Set(x, y, C);
			}
		P.Pad();
	}
	// ---- the painted pitch wall: club-green panels, seams, grime toward the ground
	{
		FPaint P = Alloc(MWallGreen, 96, 36);
		const TArray<FColor> G = { FColor(30, 60, 42), FColor(38, 76, 54), FColor(46, 90, 64), FColor(58, 108, 76), FColor(76, 128, 92) };
		for (int32 y = 0; y < P.H; ++y)
			for (int32 x = 0; x < P.W; ++x)
			{
				float V = 0.55f + 0.1f * (N(x, y, 41) - 0.5f) - (y > 24 ? (y - 24) * 0.03f : 0.f) + (y <= 1 ? 0.3f : 0.f);
				if (x % 48 == 0) V = 0.15f; else if (x % 48 == 1) V = 0.8f;
				P.Set(x, y, Ramp(G, V, x, y));
			}
		P.Pad();
	}
	// ---- roof sheeting: corrugations along the slope, lap lines, rivets, rust patches
	{
		FPaint P = Alloc(MRoof, 64, 96);
		const TArray<FColor> R = { FColor(78, 86, 94), FColor(100, 108, 116), FColor(122, 130, 138), FColor(144, 152, 158), FColor(170, 176, 180) };
		const float Prof[8] = { 0.95f, 0.78f, 0.62f, 0.52f, 0.4f, 0.28f, 0.42f, 0.66f };
		for (int32 y = 0; y < P.H; ++y)
			for (int32 x = 0; x < P.W; ++x)
			{
				float V = Prof[x % 8] + 0.06f * (N(x, y, 51) - 0.5f);
				if (y % 48 == 0) V = 0.1f; else if (y % 48 == 1) V = 0.9f;
				FColor C = Ramp(R, V, x, y);
				if (x % 8 == 3 && y % 16 == 8) C = R[4];
				const float Rust = Value(x, y, 16, P.W, P.H, 53);
				if (Rust > 0.72f && N(x, y, 55) < (Rust - 0.72f) * 3.f) C = Mix(C, FColor(132, 82, 52), 0.55f);
				P.Set(x, y, C);
			}
		P.Pad();
	}
	// ---- fascia: the club colour between white trims, panel seams
	{
		FPaint P = Alloc(MFascia, 128, 40);
		const TArray<FColor> C = RampOf(Seat, 0.6f, 1.25f, 5);
		for (int32 y = 0; y < P.H; ++y)
			for (int32 x = 0; x < P.W; ++x)
			{
				FColor Cc;
				if (y <= 1) Cc = FColor(232, 232, 226); else if (y == 2) Cc = FColor(196, 196, 190); else if (y == 3) Cc = Scale(Seat, 0.45f);
				else if (y >= 36 && y <= 37) Cc = FColor(226, 226, 220); else if (y >= 38) Cc = FColor(60, 60, 64);
				else { float V = 0.6f - (y - 4) * 0.008f + 0.08f * (N(x, y, 61) - 0.5f); if (x % 64 == 0) V = 0.1f; Cc = Ramp(C, V, x, y); }
				P.Set(x, y, Cc);
			}
		P.Pad();
	}
	// ---- steel (columns, frames): a lit edge, rivets
	{
		FPaint P = Alloc(MSteel, 16, 32);
		const TArray<FColor> S = { FColor(44, 48, 54), FColor(62, 68, 76), FColor(82, 90, 98), FColor(108, 116, 124), FColor(140, 148, 154) };
		for (int32 y = 0; y < P.H; ++y)
			for (int32 x = 0; x < P.W; ++x)
			{
				float V = x <= 1 ? 0.9f : x >= 13 ? 0.15f : 0.5f + 0.1f * (N(x, y, 71) - 0.5f);
				if ((x == 4 || x == 11) && y % 8 == 4) V = 1.f;
				P.Set(x, y, Ramp(S, V, x, y));
			}
		P.Pad();
	}
	// ---- lattice (pylon faces): X bracing between legs, the gaps transparent
	{
		FPaint P = Alloc(MLattice, 32, 32);
		const FColor L(150, 156, 162), M(96, 104, 112), D(58, 64, 70);
		for (int32 y = 0; y < P.H; ++y)
			for (int32 x = 0; x < P.W; ++x)
			{
				FColor C(0, 0, 0, 0);
				const bool bLeg = x <= 2 || x >= 29, bRung = y <= 1, bDiag = FMath::Abs(x - y) <= 1 || FMath::Abs(x - (31 - y)) <= 1;
				if (bLeg) C = x == 0 || x == 29 ? L : x == 2 || x == 31 ? D : M;
				else if (bRung) C = y == 0 ? L : D;
				else if (bDiag) C = (FMath::Abs(x - y) == 1 || FMath::Abs(x - (31 - y)) == 1) ? D : M;
				P.Set(x, y, C);
			}
		P.Pad();
	}
	// ---- floodlight head: 6 x 4 lamps in a dark frame; lit at night
	{
		FPaint P = Alloc(MLamp, 96, 64);
		P.Fill(FColor(34, 36, 40));
		for (int32 r = 0; r < 4; ++r)
			for (int32 c = 0; c < 6; ++c)
			{
				const int32 Cx = c * 16 + 8, Cy = r * 16 + 8;
				for (int32 y = -7; y <= 7; ++y)
					for (int32 x = -7; x <= 7; ++x)
					{
						const float D = FMath::Sqrt(float(x * x + y * y));
						FColor C;
						if (D > 7.2f) continue;
						if (D > 6.f) C = FColor(110, 116, 122);
						else if (bNight) C = D < 2.5f ? FColor(255, 255, 250) : D < 4.5f ? FColor(255, 246, 210) : FColor(236, 214, 150);
						else C = (x < -1 && y < -1 && D < 4.f) ? FColor(226, 232, 236) : D < 3.f ? FColor(176, 184, 190) : FColor(132, 140, 148);
						P.Set(Cx + x, Cy + y, C);
					}
			}
		P.Pad();
	}
	// ---- tunnel mouth: a white frame round a dark passage that fades into black
	{
		FPaint P = Alloc(MTunnel, 64, 48);
		for (int32 y = 0; y < P.H; ++y)
			for (int32 x = 0; x < P.W; ++x)
			{
				const float Dx = FMath::Min(x, P.W - 1 - x) / 12.f, Dy = y / 14.f;
				const float V = FMath::Clamp(0.35f - FMath::Min(Dx, Dy) * 0.3f, 0.f, 1.f);
				FColor C = Ramp({ FColor(8, 8, 10), FColor(22, 22, 26), FColor(40, 40, 46), FColor(64, 64, 70) }, V, x, y);
				if (y >= P.H - 4) C = Ramp(Conc, 0.35f, x, y);
				P.Set(x, y, C);
			}
		P.Pad();
	}
	// ---- ground: tarmac (streets, car parks), paving slabs (the concourse), the pale run-off concrete
	{
		FPaint P = Alloc(MTarmac, 256, 256);
		const TArray<FColor> T = { FColor(46, 50, 50), FColor(56, 60, 60), FColor(66, 70, 69), FColor(78, 82, 80) };
		for (int32 y = 0; y < P.H; ++y)
			for (int32 x = 0; x < P.W; ++x)
				P.Set(x, y, Ramp(T, 0.45f + 0.25f * (Value(x, y, 32, P.W, P.H, 81) - 0.5f) + 0.3f * (N(x, y, 83) - 0.5f), x, y));
		// cracks: short random walks
		for (int32 k = 0; k < 6; ++k)
		{
			int32 x = int32(N(k, 0, 85) * P.W), y = int32(N(k, 1, 85) * P.H);
			for (int32 s = 0; s < 40; ++s) { P.Set(((x % P.W) + P.W) % P.W, ((y % P.H) + P.H) % P.H, T[0]); x += (Hash(k, s, 87) & 1) ? 1 : 0; y += (Hash(k, s, 89) % 3) - 1; }
		}
		P.Pad();
	}
	{
		FPaint P = Alloc(MPaving, 256, 256);
		for (int32 y = 0; y < P.H; ++y)
			for (int32 x = 0; x < P.W; ++x)
			{
				const int32 Sx = x / 16, Sy = y / 16;
				const float Tone = 0.45f + 0.25f * (N(Sx, Sy, 91) - 0.5f);
				float V = Tone + 0.1f * (N(x, y, 93) - 0.5f);
				if (x % 16 == 0 || y % 16 == 0) V = 0.12f;
				else if (x % 16 == 1 || y % 16 == 1) V += 0.12f;
				P.Set(x, y, Ramp(Conc, V, x, y));
			}
		P.Pad();
	}
	{
		FPaint P = Alloc(MRunoff, 64, 64);
		for (int32 y = 0; y < P.H; ++y)
			for (int32 x = 0; x < P.W; ++x)
				P.Set(x, y, Ramp(Conc, 0.7f + 0.1f * (N(x, y, 95) - 0.5f) + 0.06f * (Value(x, y, 16, P.W, P.H, 97) - 0.5f), x, y));
		P.Pad();
	}
	// ---- dugout roof: perspex with diagonal glints
	{
		FPaint P = Alloc(MPerspex, 48, 32);
		for (int32 y = 0; y < P.H; ++y)
			for (int32 x = 0; x < P.W; ++x)
			{
				FColor C = Ramp({ FColor(120, 140, 154), FColor(146, 166, 180), FColor(170, 190, 202) }, 0.5f + 0.15f * (N(x, y, 101) - 0.5f), x, y);
				if (((x + y) % 14) < 2) C = FColor(214, 226, 232);
				if (x <= 1 || y <= 1 || x >= P.W - 2 || y >= P.H - 2) C = FColor(78, 84, 90);
				P.Set(x, y, C);
			}
		P.Pad();
	}
	// ---- turnstile gate: green roller shutter in a frame, a yellow sign
	{
		FPaint P = Alloc(MGate, 32, 48);
		for (int32 y = 0; y < P.H; ++y)
			for (int32 x = 0; x < P.W; ++x)
			{
				FColor C = (y % 3 == 0) ? FColor(40, 70, 52) : (y % 3 == 1 ? FColor(70, 104, 80) : FColor(56, 88, 66));
				if (y < 8) C = Yellow;
				if (x <= 1 || x >= P.W - 2 || y <= 1) C = FColor(48, 44, 42);
				P.Set(x, y, C);
			}
		P.Text(TEXT("IN"), 10, 1, 1, 1, FColor(30, 30, 30), FColor(0, 0, 0), false);
		P.Pad();
	}
	// ---- window: frame, sash, dark glass with a reflection
	{
		FPaint P = Alloc(MWindow, 24, 20);
		for (int32 y = 0; y < P.H; ++y)
			for (int32 x = 0; x < P.W; ++x)
			{
				FColor C = ((x - y) % 9 == 0 || (x - y) % 9 == 1) ? FColor(96, 112, 128) : FColor(40, 48, 60);
				if (bNight) C = ((x - y) % 9 == 0) ? FColor(255, 236, 170) : (y > 12 ? FColor(214, 160, 70) : FColor(240, 196, 100));   // lit from inside
				if (x <= 1 || x >= P.W - 2 || y <= 1 || y >= P.H - 2 || x == 11 || x == 12 || y == 9) C = FColor(214, 210, 200);
				if (y >= P.H - 2) C = FColor(150, 146, 138);
				P.Set(x, y, C);
			}
		P.Pad();
	}
	// ---- the back of an A-frame board: grey sheet with two struts
	{
		FPaint P = Alloc(MBoardBack, 64, 30);
		for (int32 y = 0; y < P.H; ++y)
			for (int32 x = 0; x < P.W; ++x)
			{
				FColor C = Ramp({ FColor(52, 54, 58), FColor(64, 66, 70), FColor(76, 78, 82) }, 0.5f + 0.2f * (N(x, y, 111) - 0.5f), x, y);
				if (FMath::Abs(x - 16) <= 1 || FMath::Abs(x - 47) <= 1) C = FColor(100, 104, 110);
				if (y <= 1 || y >= P.H - 2) C = FColor(40, 42, 46);
				P.Set(x, y, C);
			}
		P.Pad();
	}
	{ FPaint P = Alloc(MCoping, 16, 8); for (int32 y = 0; y < 8; ++y) for (int32 x = 0; x < 16; ++x) P.Set(x, y, Ramp(Conc, y == 0 ? 0.98f : y >= 6 ? 0.4f : 0.82f + 0.08f * (N(x, y, 121) - 0.5f), x, y)); P.Pad(); }
	// ---- slate roof (the terraced streets): staggered slates in courses
	{
		FPaint P = Alloc(MSlate, 64, 48);
		const TArray<FColor> S = { FColor(44, 48, 60), FColor(58, 62, 76), FColor(72, 76, 90), FColor(88, 92, 104) };
		for (int32 y = 0; y < P.H; ++y)
			for (int32 x = 0; x < P.W; ++x)
			{
				const int32 Row = y / 4, Off = (Row & 1) ? 4 : 0;
				float V = 0.45f + 0.35f * (N((x + Off) / 8, Row, 131) - 0.5f) + 0.1f * (N(x, y, 133) - 0.5f);
				if (y % 4 == 3) V = 0.05f; else if (y % 4 == 0) V += 0.15f;
				if ((x + Off) % 8 == 0) V -= 0.2f;
				P.Set(x, y, Ramp(S, V, x, y));
			}
		P.Pad();
	}
	// ---- a terraced house front: brick, two sash windows up, a window and a door down, a sill band
	{
		FPaint P = Alloc(MHouse, 64, 64);
		const FTile& B = Tiles[MBrick];
		for (int32 y = 0; y < P.H; ++y) for (int32 x = 0; x < P.W; ++x) P.Set(x, y, Px[(B.Y + (y % B.H)) * SheetW + B.X + (x % B.W)]);
		const FTile& Wn = Tiles[MWindow];
		auto Win = [&](int32 x0, int32 y0) { for (int32 y = 0; y < 16; ++y) for (int32 x = 0; x < 14; ++x) P.Set(x0 + x, y0 + y, Px[(Wn.Y + y * Wn.H / 16) * SheetW + Wn.X + x * Wn.W / 14]); P.Rect(x0 - 1, y0 + 16, x0 + 15, y0 + 18, FColor(200, 196, 186)); };
		Win(8, 8); Win(40, 8); Win(8, 38);
		P.Rect(40, 36, 54, 64, FColor(60, 30, 26)); P.Rect(42, 38, 52, 62, FColor(88, 40, 34)); P.Rect(39, 34, 55, 36, FColor(200, 196, 186)); P.Set(50, 50, Yellow);
		P.Rect(0, 30, 64, 32, FColor(186, 180, 168));
		P.Pad();
	}
	// ---- street tree (a billboard): dithered canopy, trunk
	{
		FPaint P = Alloc(MTree, 40, 48);
		const TArray<FColor> L = { FColor(30, 62, 34), FColor(44, 84, 42), FColor(62, 106, 50), FColor(88, 130, 62) };
		for (int32 y = 0; y < P.H; ++y)
			for (int32 x = 0; x < P.W; ++x)
			{
				const float Dx = (x - 19.5f) / 18.f, Dy = (y - 16.f) / 16.f;
				const float R = Dx * Dx + Dy * Dy, Edge = 1.f - 0.25f * N(x / 3, y / 3, 141);
				FColor C(0, 0, 0, 0);
				if (R < Edge) C = Ramp(L, 0.75f - 0.45f * (Dx + Dy * 0.8f + 1.f) * 0.5f + 0.2f * (N(x / 2, y / 2, 143) - 0.5f), x, y);
				else if (y >= 28 && FMath::Abs(x - 19.5f) < 2.f) C = x < 20 ? FColor(92, 66, 44) : FColor(64, 46, 32);
				P.Set(x, y, C);
			}
		P.Pad();
	}
	// ---- a parked car's side (grey body: tinted by the caller), windows, wheels
	{
		FPaint P = Alloc(MCar, 32, 14);
		for (int32 y = 0; y < P.H; ++y)
			for (int32 x = 0; x < P.W; ++x)
			{
				FColor C(0, 0, 0, 0);
				if (y >= 5 && y < 12) C = y == 5 ? FColor(250, 250, 250) : y >= 10 ? FColor(150, 150, 150) : FColor(210, 210, 210);
				if (y >= 1 && y < 5 && x >= 7 && x < 24) C = (x == 7 || x == 23 || y == 1) ? FColor(200, 200, 200) : FColor(40, 50, 62);
				if (y >= 10 && ((x >= 4 && x < 10) || (x >= 22 && x < 28))) C = FColor(24, 24, 26);
				P.Set(x, y, C);
			}
		P.Pad();
	}
	// ---- the big screen's dark dot-matrix face (the score and clock are drawn on it live)
	{
		FPaint P = Alloc(MScreen, 96, 54);
		for (int32 y = 0; y < P.H; ++y)
			for (int32 x = 0; x < P.W; ++x)
			{
				FColor C = (x % 2 == 0 && y % 2 == 0) ? FColor(30, 32, 30) : FColor(14, 15, 16);
				if (x <= 2 || x >= P.W - 3 || y <= 2 || y >= P.H - 3) C = (x <= 0 || y <= 0) ? FColor(120, 126, 132) : FColor(60, 64, 70);
				P.Set(x, y, C);
			}
		P.Pad();
	}
	// ---- a supporter's flag on a pole: 4 frames of a wave, club colour and white halves
	{
		FPaint P = Alloc(MFlag, 96, 16);
		for (int32 f = 0; f < 4; ++f)
			for (int32 x = 0; x < 24; ++x)
			{
				const float Ph = f * 1.5708f;
				const int32 Dy = FMath::RoundToInt(2.f * FMath::Sin(x * 0.4f - Ph) * (x / 23.f));
				for (int32 y = 0; y < 16; ++y)
				{
					FColor C(0, 0, 0, 0);
					if (x <= 1) C = y < 16 ? FColor(70, 60, 50) : C;   // the pole
					else if (y - Dy >= 1 && y - Dy < 11)
					{
						const bool bTop = (y - Dy) < 6;
						const FColor Base = bTop ? Seat : SeatTrim;
						const float Shade = 0.85f + 0.25f * FMath::Cos(x * 0.4f - Ph);
						C = Scale(Base, Shade);
					}
					P.Set(f * 24 + x, y, C);
				}
			}
		P.Pad();
	}
	// ---- a draped banner over a wall: club-colour stripes, a white border
	{
		FPaint P = Alloc(MBanner, 32, 48);
		for (int32 y = 0; y < P.H; ++y)
			for (int32 x = 0; x < P.W; ++x)
			{
				FColor C = ((x / 6) & 1) ? SeatTrim : Seat;
				C = Scale(C, 0.9f + 0.15f * FMath::Sin(x * 0.8f));
				if (x <= 1 || x >= P.W - 2 || y <= 1) C = FColor(230, 230, 224);
				if (y >= P.H - 6 && ((x + 2) % 6) < 3 && y >= P.H - 6 + ((x % 3) * 2)) C = FColor(0, 0, 0, 0);   // a ragged hem
				P.Set(x, y, C);
			}
		P.Pad();
	}
	// ---- A-frame advertising boards: twelve sponsors, crisp 5x7 lettering with a drop shadow, trim stripes
	{
		struct FB { FColor Bg, Fg, Ac; };
		const FB Pal[] = { { FColor(30, 44, 100), FColor(240, 240, 232), FColor(220, 40, 40) }, { FColor(176, 32, 36), FColor(250, 212, 72), FColor(250, 212, 72) },
			{ FColor(236, 236, 228), FColor(184, 30, 34), FColor(30, 44, 100) }, { FColor(28, 100, 62), FColor(240, 240, 232), FColor(250, 212, 72) },
			{ FColor(238, 192, 48), FColor(24, 24, 30), FColor(24, 24, 30) }, { FColor(24, 24, 30), FColor(244, 146, 46), FColor(244, 146, 46) },
			{ FColor(62, 132, 196), FColor(250, 250, 246), FColor(30, 44, 100) }, { FColor(120, 30, 60), FColor(246, 234, 210), FColor(246, 234, 210) },
			{ FColor(240, 240, 234), FColor(24, 90, 160), FColor(24, 90, 160) }, { FColor(40, 40, 46), FColor(110, 220, 120), FColor(110, 220, 120) },
			{ FColor(204, 70, 30), FColor(250, 250, 240), FColor(40, 40, 46) }, { FColor(250, 250, 244), FColor(30, 30, 34), FColor(200, 30, 36) } };
		for (int32 b = 0; b < NBoards; ++b)
		{
			FPaint P = Alloc(MBoard0 + b, 256, 32);
			const FB& C = Pal[b % 12];
			for (int32 y = 0; y < P.H; ++y)
				for (int32 x = 0; x < P.W; ++x)
				{
					FColor Cc = Scale(C.Bg, 0.96f + 0.06f * (N(x, y, 151 + b) - 0.5f) + (y < 6 ? 0.04f : 0.f));
					if (y <= 1 || y >= P.H - 3) Cc = C.Ac;
					if (y == 0) Cc = Scale(C.Ac, 1.2f);
					if (x <= 1 || x >= P.W - 2) Cc = Scale(C.Bg, 0.6f);
					P.Set(x, y, Cc);
				}
			const FString Word = BoardWord(b);
			const int32 Sx = Word.Len() > 10 ? 3 : 3, Sy = 3;
			const int32 Tw = P.TextWidth(Word, Sx);
			const int32 Logo = 18;
			const int32 X0 = (P.W - Tw - Logo) / 2 + Logo, Y0 = (P.H - 21) / 2;
			// a small mark before the word: a disc in the accent colour
			for (int32 y = -6; y <= 6; ++y) for (int32 x = -6; x <= 6; ++x) if (x * x + y * y <= 36) P.Set(X0 - 12 + x, P.H / 2 + y, (x * x + y * y <= 9) ? C.Fg : C.Ac);
			P.Text(Word, X0, Y0, Sx, Sy, C.Fg, Scale(C.Bg, 0.45f), true);
			P.Pad();
		}
	}
	// ---- the pitch-side photographers (the owner's sideline camera-operator sprites)
	for (int32 i = 0; i < NPhoto; ++i)
	{
		FPaint P = Alloc(MPhoto0 + i, 64, 64);
		TArray<uint8> Bytes; TArray<uint8> Raw;
		if (!FFileHelper::LoadFileToArray(Bytes, *PF::DataPath(FString::Printf(TEXT("GameData/UI/sid_camera_operator_%03d.png"), i + 1)))) { P.Pad(); continue; }
		IImageWrapperModule& IWM = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		TSharedPtr<IImageWrapper> Wr = IWM.CreateImageWrapper(EImageFormat::PNG);
		if (Wr.IsValid() && Wr->SetCompressed(Bytes.GetData(), Bytes.Num()) && Wr->GetRaw(ERGBFormat::BGRA, 8, Raw) && Wr->GetWidth() == 64 && Wr->GetHeight() == 64)
		{
			const FColor* S = reinterpret_cast<const FColor*>(Raw.GetData());
			for (int32 y = 0; y < 64; ++y) for (int32 x = 0; x < 64; ++x) P.Set(x, y, S[y * 64 + x]);
		}
		// no wrapped border for a sprite: keep it transparent
		for (int32 y = -2; y < 66; ++y) for (int32 x = -2; x < 66; ++x) if (x < 0 || y < 0 || x >= 64 || y >= 64) Px[(P.Y + y) * SheetW + P.X + x] = FColor(0, 0, 0, 0);
	}
}
