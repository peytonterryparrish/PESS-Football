// Proper Football - v0.9.8 3D stadium (see PFStadium3D.h).
//
// One ground for every match: the grass and its markings, the goals and everyone on the pitch stay 2D; everything round
// them is a model in world metres (x along the pitch, y across, z up) drawn through the same parallel projection as the
// pitch (FPFProjection with heights scaled by HeightScale, exactly as the players and goals are drawn). The camera sits
// south and a little west of the pitch, looking down: the far (north) stand and the east end face it, the near (south)
// paddock and the west end are seen from behind.
//
//   far stand   two tiers (20 + 12 rows, rake 0.45-0.5 m per 0.8 m row), fascia, roof over the upper tier on columns,
//               players' tunnel, vomitories, aisles every 14 seats
//   east end    18 rows under a roof (the away supporters in its north half)
//   west end    18 rows, open (a kop), seen from behind
//   near side   ad boards (their backs), dugouts and a shallow 8-row paddock seen from behind, kept low so it never
//               hides play
//   far corners floodlight pylons
//
// Every face is culled against the view direction and lit flat from one sun; faces and fans are emitted in painter's
// order by construction (rows back to front for the stands facing the camera, front to back for those seen from
// behind; along a row from the far end), so one triangle list per layer draws the whole ground. The projected
// geometry is cached in atlas pixels; a frame only offsets, scales and culls it.
#include "PFStadium3D.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Misc/FileHelper.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "UObject/GCObject.h"

namespace
{
	FLinearColor Srgb(int32 R, int32 G, int32 B) { return FLinearColor::FromSRGBColor(FColor(uint8(R), uint8(G), uint8(B))); }

	// 3x5 pixel font for the advertising boards (only the letters the boards use)
	const TCHAR* Glyph(TCHAR Ch)
	{
		switch (Ch)
		{
		case 'A': return TEXT("010101111101101"); case 'B': return TEXT("110101110101110"); case 'C': return TEXT("011100100100011");
		case 'D': return TEXT("110101101101110"); case 'E': return TEXT("111100110100111"); case 'F': return TEXT("111100110100100");
		case 'G': return TEXT("011100101101011"); case 'H': return TEXT("101101111101101"); case 'I': return TEXT("111010010010111");
		case 'K': return TEXT("101101110101101"); case 'L': return TEXT("100100100100111"); case 'M': return TEXT("101111111101101");
		case 'N': return TEXT("110101101101101"); case 'O': return TEXT("010101101101010"); case 'P': return TEXT("110101110100100");
		case 'R': return TEXT("110101110101101"); case 'S': return TEXT("011100010001110"); case 'T': return TEXT("111010010010010");
		case 'U': return TEXT("101101101101111"); case 'V': return TEXT("101101101101010"); case 'W': return TEXT("101101111111101");
		case 'X': return TEXT("101101010101101"); case 'Y': return TEXT("101101010010010"); case '9': return TEXT("111101111001111");
		case '6': return TEXT("111100111101111"); default: return nullptr;
		}
	}

	// a stand's local frame: world = U * u + W * w + Z * z (w grows away from the pitch)
	struct FFr { FVector U, W; FVector At(float u, float w, float z) const { return U * u + W * w + FVector(0, 0, z); } FVector Dir(const FVector& L) const { return U * L.X + W * L.Y + FVector(0, 0, L.Z); } };
	const FFr FarF{ FVector(1, 0, 0), FVector(0, 1, 0) };
	const FFr NearF{ FVector(1, 0, 0), FVector(0, -1, 0) };
	const FFr EastF{ FVector(0, 1, 0), FVector(1, 0, 0) };
	const FFr WestF{ FVector(0, 1, 0), FVector(-1, 0, 0) };
	const FVector NToPitch(0, -1, 0), NOut(0, 1, 0), NUp(0, 0, 1), NUNeg(-1, 0, 0), NUPos(1, 0, 0);

	struct FRow { float W = 0, D = 0.8f, Z = 0, Rise = 0.45f, Shade = 1.f; bool bFascia = false; };
}

// ------------------------------------------------------------------------------------------------ fan sheet
// The home club's supporter sheet (GameData/Fans/<slug>.png: 20 fans x 8 rows of 64 x 80 cells - seated, standing, three
// cheer and three "no!" frames) plus the same 8 rows seen from behind, derived here: the head becomes the back of the
// head (hair, hat or bald crown), and a seated fan loses what is below the hips (his legs go away from the camera, under
// the seat). A white block at the bottom lets the stand's flat faces share the texture and the draw call.
bool FPFStadium3D::BuildSheet(const FString& Rel)
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
	SheetW = 64 * 20;
	SheetH = CellH * FrontRows * 2 + 8;
	TArray<FColor> Px; Px.SetNumZeroed(SheetW * SheetH);
	const FColor* Src = reinterpret_cast<const FColor*>(Raw.GetData());
	// front rows, bottom-aligned in 80 px cells
	for (int32 r = 0; r < FrontRows; ++r)
		for (int32 y = 0; y < SrcCH; ++y)
			for (int32 x = 0; x < FMath::Min(SW, SheetW); ++x)
				Px[(r * CellH + (CellH - SrcCH) + y) * SheetW + x] = Src[(r * SrcCH + y) * SW + x];
	// back views
	for (int32 r = 0; r < FrontRows; ++r)
		for (int32 f = 0; f < 20; ++f)
		{
			const int32 X0 = f * 64, Y0 = (FrontRows + r) * CellH, S0 = r * CellH;
			auto At = [&](int32 x, int32 y) -> FColor& { return Px[(Y0 + y) * SheetW + X0 + x]; };
			for (int32 y = 0; y < CellH; ++y) for (int32 x = 0; x < 64; ++x) At(x, y) = Px[(S0 + y) * SheetW + X0 + x];
			if (r == 0) for (int32 y = 66; y < CellH; ++y) for (int32 x = 0; x < 64; ++x) At(x, y) = FColor(0, 0, 0, 0);   // seated: no legs
			// the head: from the top of the figure (centre columns) down to the shoulders (14+ px wide there)
			int32 Top = -1, Sh = -1;
			for (int32 y = 0; y < CellH && Sh < 0; ++y)
			{
				int32 N = 0; for (int32 x = 24; x < 40; ++x) N += At(x, y).A > 128;
				if (N > 0 && Top < 0) Top = y;
				if (Top >= 0 && N >= 14) Sh = y;
			}
			if (Top < 0 || Sh < 0 || Sh - Top > 18) continue;
			// hair: the darkest colour in the top three rows of the head
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
	// white block for the flat faces
	for (int32 y = SheetH - 8; y < SheetH; ++y) for (int32 x = 0; x < 8; ++x) Px[y * SheetW + x] = FColor::White;
	WhiteUV = FVector2f(4.f / SheetW, (SheetH - 4.f) / SheetH);

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
		const FColor Gc(60, 160, 60, 255);   // B G R A order in memory doesn't matter for a grey-free green test (g > r, g > b)
		FMemory::Memcpy(G->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE), &Gc, 4);
		G->GetPlatformData()->Mips[0].BulkData.Unlock();
		G->Filter = TF_Nearest; G->NeverStream = true; G->UpdateResource();
	}
	Green = G;
	UE_LOG(LogPF, Log, TEXT("PF stadium3d fan sheet %s: %d front rows + back views"), *Use, FrontRows);
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
	if (!Sheet && !BuildSheet(FanSheetRel)) { UE_LOG(LogPF, Warning, TEXT("PF stadium3d: no fan sheet")); }
	for (TArray<FOp>& L : Ops) L.Reset();
	Fans.Reset();
	BuiltAz = Pv.Az;
	AtlasPerSpritePx = Pv.Az.Size() * 1.82f / 64.f;
	PF::FRng Rng(Seed ^ 0x5EA75u);

	// the view direction (into the scene): Ax*vx + Ay*vy - Az = 0 with vz = -1
	const double Det = Pv.Ax.X * Pv.Ay.Y - Pv.Ay.X * Pv.Ax.Y;
	const FVector V((Pv.Az.X * Pv.Ay.Y - Pv.Ay.X * Pv.Az.Y) / Det, (Pv.Ax.X * Pv.Az.Y - Pv.Az.X * Pv.Ax.Y) / Det, -1.0);
	const FVector Sun = FVector(-0.35, -0.5, 0.8).GetSafeNormal();   // from the upper left, like the players' shadows
	const bool bNight = Stage.StartsWith(TEXT("Floodlit")), bSunset = Stage == TEXT("Sunset");
	const FLinearColor Tint = bNight ? FLinearColor(0.50f, 0.52f, 0.68f) : bSunset ? FLinearColor(1.0f, 0.84f, 0.72f) : FLinearColor::White;

	// palette: restrained, a little below the pitch's saturation so the grass stays the brightest thing on screen
	auto Mute = [](FLinearColor C, float Sat, float Val) { const float L = C.GetLuminance(); return FLinearColor(FMath::Lerp(L, C.R, Sat) * Val, FMath::Lerp(L, C.G, Sat) * Val, FMath::Lerp(L, C.B, Sat) * Val, 1.f); };
	auto FromCode = [](const FLinearColor& C) { return Srgb(FMath::RoundToInt(C.R * 255.f), FMath::RoundToInt(C.G * 255.f), FMath::RoundToInt(C.B * 255.f)); };
	FLinearColor SeatC = FromCode(HomeShirt);
	if (SeatC.GetLuminance() > 0.45f) { const FLinearColor Tc = FromCode(HomeTrim); SeatC = Tc.GetLuminance() < 0.3f && Tc.GetLuminance() > 0.01f ? Tc : Srgb(64, 74, 96); }
	if (SeatC.GetLuminance() < 0.006f) SeatC = Srgb(52, 54, 62);
	SeatC = Mute(SeatC, 0.8f, 0.9f);
	const FLinearColor Club = SeatC;
	const FLinearColor Concrete = Srgb(150, 148, 140), ConcreteRise = Srgb(118, 116, 110), Nosing = Srgb(176, 174, 166), AisleC = Srgb(172, 168, 156);
	const FLinearColor Dark = Srgb(26, 26, 30), Hole = Srgb(62, 60, 60), Brick = Srgb(122, 74, 58), BrickDark = Srgb(96, 58, 46), Steel = Srgb(70, 76, 82), RoofA = Srgb(128, 138, 146), RoofB = Srgb(112, 122, 130);
	const FLinearColor White = Srgb(226, 226, 220), BoardBack = Srgb(58, 60, 64), BoardTop = Srgb(88, 90, 94);

	int32 Layer = LStands;
	auto Push = [&](const FVector (&Wp)[4], const FLinearColor& Col)
	{
		FOp Op;
		Op.Lo = FVector2f(1e9f, 1e9f); Op.Hi = FVector2f(-1e9f, -1e9f);
		for (int32 i = 0; i < 4; ++i)
		{
			const FVector2D A = Pv.ToAtlas(Wp[i]);
			Op.P[i] = FVector2f(A);
			Op.Lo = FVector2f(FMath::Min(Op.Lo.X, float(A.X)), FMath::Min(Op.Lo.Y, float(A.Y)));
			Op.Hi = FVector2f(FMath::Max(Op.Hi.X, float(A.X)), FMath::Max(Op.Hi.Y, float(A.Y)));
		}
		Op.Col = Col; Op.Col.A = 1.f;
		Ops[Layer].Add(Op);
	};
	// a flat face in a stand's frame; culled when it faces away, lit by its normal
	auto Quad = [&](const FFr& F, FVector a, FVector b, FVector c, FVector d, const FVector& NLocal, const FLinearColor& Col, float Shade = 1.f, bool bLit = true)
	{
		const FVector N = F.Dir(NLocal);
		if (FVector::DotProduct(N, V) >= -1e-3) return;
		const float L = bLit ? (0.58f + 0.42f * FMath::Max(0.f, float(FVector::DotProduct(N, Sun)))) * Shade : 1.f;
		const FVector Wp[4] = { F.At(a.X, a.Y, a.Z), F.At(b.X, b.Y, b.Z), F.At(c.X, c.Y, c.Z), F.At(d.X, d.Y, d.Z) };
		Push(Wp, bLit ? Col * Tint * L : Col);
	};
	// axis-aligned pieces in a frame: a vertical face at w (u0..u1, z0..z1), a horizontal one at z (u0..u1, w0..w1),
	// a vertical end face at u (w0..w1, z0..z1)
	auto VFace = [&](const FFr& F, float w, float u0, float u1, float z0, float z1, const FVector& N, const FLinearColor& Col, float Shade = 1.f, bool bLit = true)
	{ Quad(F, FVector(u0, w, z0), FVector(u1, w, z0), FVector(u1, w, z1), FVector(u0, w, z1), N, Col, Shade, bLit); };
	auto HFace = [&](const FFr& F, float z, float u0, float u1, float w0, float w1, const FLinearColor& Col, float Shade = 1.f)
	{ Quad(F, FVector(u0, w0, z), FVector(u1, w0, z), FVector(u1, w1, z), FVector(u0, w1, z), NUp, Col, Shade); };
	auto EFace = [&](const FFr& F, float u, float w0, float w1, float z0, float z1, const FVector& N, const FLinearColor& Col, float Shade = 1.f)
	{ Quad(F, FVector(u, w0, z0), FVector(u, w1, z0), FVector(u, w1, z1), FVector(u, w0, z1), N, Col, Shade); };
	auto Box = [&](const FFr& F, float u0, float u1, float w0, float w1, float z0, float z1, const FLinearColor& Front, const FLinearColor& Side, const FLinearColor& Top)
	{
		// the faces that can be seen are drawn back to front: rear, far end, near end, front, top
		VFace(F, w1, u0, u1, z0, z1, NOut, Side);
		EFace(F, u1, w0, w1, z0, z1, NUPos, Side);
		EFace(F, u0, w0, w1, z0, z1, NUNeg, Side);
		VFace(F, w0, u0, u1, z0, z1, NToPitch, Front);
		HFace(F, z1, u0, u1, w0, w1, Top);
	};

	// ---- advertising boards (0.9 m, 6 m panels, the panel's word in a 3x5 pixel font)
	static const TCHAR* Words[] = { TEXT("PIX COLA"), TEXT("PROPER"), TEXT("GOAL TV"), TEXT("ROVERS ALE"), TEXT("KICK OFF"), TEXT("TURBO"), TEXT("MATCHDAY"), TEXT("SUPER BOOTS"), TEXT("FOOTBALL"), TEXT("NORTHGATE 96") };
	struct FBoardPal { FLinearColor Bg, Fg; };
	const FBoardPal BPal[] = { { Srgb(30, 44, 96), Srgb(236, 236, 228) }, { Srgb(170, 34, 38), Srgb(246, 206, 70) }, { Srgb(228, 228, 220), Srgb(176, 32, 34) },
		{ Srgb(32, 96, 62), Srgb(236, 236, 228) }, { Srgb(232, 188, 48), Srgb(24, 24, 28) }, { Srgb(26, 26, 30), Srgb(238, 142, 44) }, { Srgb(64, 130, 190), Srgb(240, 240, 236) } };
	int32 BoardN = int32(Seed % 7u);
	auto Boards = [&](const FFr& F, float U0, float U1, float W)
	{
		const FVector Right = FVector::CrossProduct(F.W, FVector(0, 0, 1));
		const float S = FVector::DotProduct(Right, F.U) >= 0 ? 1.f : -1.f;   // which way the text reads along u
		const float H = 0.9f, T = 0.12f, Px = 0.1f;
		const int32 N = FMath::Max(1, FMath::RoundToInt((U1 - U0) / 6.f));
		const float Pw = (U1 - U0) / N;
		for (int32 i = N - 1; i >= 0; --i)
		{
			const float A = U0 + i * Pw, B = A + Pw;
			const FBoardPal& P = BPal[(BoardN++) % 7];
			VFace(F, W + T, A, B, 0.f, H, NOut, BoardBack);
			HFace(F, H, A, B, W, W + T, BoardTop);
			VFace(F, W, A, B, 0.f, H, NToPitch, P.Bg);
			VFace(F, W - 0.005f, A, B, 0.04f, 0.1f, NToPitch, P.Fg, 0.8f);   // a trim line along the foot
			const FString Wd = Words[(BoardN * 3) % UE_ARRAY_COUNT(Words)];
			const float Len = (Wd.Len() * 4 - 1) * Px;
			const float Start = (A + B) * 0.5f - S * Len * 0.5f;
			for (int32 c = 0; c < Wd.Len(); ++c)
			{
				const TCHAR* G = Glyph(Wd[c]);
				if (!G) continue;
				for (int32 r = 0; r < 5; ++r)
					for (int32 k = 0; k < 3; ++k)
					{
						if (G[r * 3 + k] != '1') continue;
						const float Ua = Start + S * (c * 4 + k) * Px, Ub = Ua + S * Px;
						const float Z1 = 0.72f - r * Px;
						VFace(F, W - 0.01f, FMath::Min(Ua, Ub), FMath::Max(Ua, Ub), Z1 - Px, Z1, NToPitch, P.Fg);
					}
			}
		}
	};

	// ---- a tiered stand: rows of steps with seats in blocks of 14 (0.5 m each) between 1.1 m aisles
	struct FStand
	{
		const FFr* F = nullptr; float U0 = 0, U1 = 0; bool bBack = false; TArray<FRow> Rows;
		TFunction<bool(int32, float)> NoSeat, DarkAt; TFunction<void(int32)> After; TFunction<uint8(int32, float)> Pick;
	};
	auto BuildStand = [&](const FStand& S)
	{
		const FFr& F = *S.F;
		// seat and aisle layout along u
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
			// riser (the face toward the pitch under this row) and tread, segment by segment, far end first
			for (int32 s = Segs.Num() - 1; s >= 0; --s)
			{
				const FSeg& G = Segs[s];
				const float Mid = (G.A + G.B) * 0.5f;
				const bool bDark = S.DarkAt && S.DarkAt(k, Mid);
				const FLinearColor RiseC = bDark ? Hole : R.bFascia ? Club : ConcreteRise;
				VFace(F, R.W, G.A, G.B, R.Z - R.Rise, R.Z, NToPitch, RiseC, Lit);
				if (R.bFascia)
				{
					VFace(F, R.W - 0.01f, G.A, G.B, R.Z - 0.25f, R.Z - 0.12f, NToPitch, White, Lit);
					VFace(F, R.W - 0.01f, G.A, G.B, R.Z - R.Rise + 0.1f, R.Z - R.Rise + 0.2f, NToPitch, White, Lit * 0.85f);
				}
				if (bDark) { HFace(F, R.Z, G.A, G.B, R.W, R.W + R.D, Hole * 0.8f, Lit); continue; }
				HFace(F, R.Z, G.A, G.B, R.W + 0.1f, R.W + R.D, G.bAisle ? AisleC : Concrete, Lit);
				HFace(F, R.Z, G.A, G.B, R.W, R.W + 0.1f, Nosing, Lit);
				if (G.bAisle) HFace(F, R.Z + 0.001f, G.A, G.B, R.W + R.D * 0.5f - 0.04f, R.W + R.D * 0.5f + 0.04f, ConcreteRise, Lit);   // the half step
			}
			// the seats, far end first: nearly all taken, the empty ones gathered at the top corners
			{
				const float Rf = float(k) / FMath::Max(1, NR - 1);
				for (int32 j = Seats.Num() - 1; j >= 0; --j)
				{
					const float U = Seats[j];
					if ((S.NoSeat && S.NoSeat(k, U)) || (S.DarkAt && S.DarkAt(k, U))) continue;
					const float Sa = U - 0.21f, Sb = U + 0.21f;
					const float Zp = R.Z + 0.42f, Zb = R.Z + 0.84f;
					// empty seats gather at the top corners
					const float Edge = FMath::Clamp((FMath::Abs(U - (S.U0 + S.U1) * 0.5f) / ((S.U1 - S.U0) * 0.5f) - 0.55f) / 0.45f, 0.f, 1.f);
					const bool bFan = Rng.Unit() < 0.94f - 0.55f * Rf * Rf * Edge - 0.04f * Rf;
					if (!S.bBack)
					{
						VFace(F, R.W + 0.66f, Sa, Sb, Zp, Zb, NToPitch, SeatC * 0.82f, Lit);
						HFace(F, Zp, Sa, Sb, R.W + 0.30f, R.W + 0.66f, SeatC, Lit);
					}
					else HFace(F, Zp, Sa, Sb, R.W + 0.30f, R.W + 0.66f, SeatC, Lit);
					if (bFan)
					{
						FFan Fa;
						Fa.Hip = FVector2f(Pv.ToAtlas(F.At(U, R.W + 0.44f, R.Z + 0.45f)));
						Fa.Feet = FVector2f(Pv.ToAtlas(F.At(U, R.W + 0.30f, R.Z)));
						Fa.Fi = S.Pick ? S.Pick(k, U) : uint8(Rng.Int(20));
						Fa.Side = (Fa.Fi == 5 || Fa.Fi == 8) ? 1 : 0;   // the club sheet's away-colour fans (Tools/pipeline/fans.py)
						Fa.bBack = S.bBack ? 1 : 0;
						Fa.Hash = Rng.Next();
						FOp Op;
						Op.Fan = Fans.Add(Fa);
						const float Sp = AtlasPerSpritePx;
						Op.Lo = FVector2f(Fa.Hip.X - 34.f * Sp, FMath::Min(Fa.Hip.Y - 64.f * Sp, Fa.Feet.Y - 82.f * Sp));
						Op.Hi = FVector2f(Fa.Hip.X + 34.f * Sp, FMath::Max(Fa.Hip.Y + 18.f * Sp, Fa.Feet.Y + 2.f));
						Op.Col = FLinearColor(Lit, Lit, Lit, 1.f);
						Ops[Layer].Add(Op);
					}
					if (S.bBack)
					{
						VFace(F, R.W + 0.70f, Sa, Sb, Zp, Zb, NOut, SeatC * 0.7f, Lit);
						HFace(F, Zb, Sa, Sb, R.W + 0.66f, R.W + 0.70f, SeatC * 1.1f, Lit);
					}
				}
			}
			if (S.After) S.After(k);
		}
		// the stand's near end (u0): one face over every row's step profile, nearer than anything in the rows
		for (int32 k = 0; k < NR; ++k)
		{
			const FRow& R = S.Rows[k];
			EFace(F, S.U0, R.W, R.W + R.D, 0.f, R.Z, NUNeg, ConcreteRise, 0.95f);
			EFace(F, S.U0 - 0.01f, R.W, R.W + R.D, R.Z - 0.12f, R.Z, NUNeg, Nosing, 0.95f);
		}
	};
	auto RowsFrom = [](TArray<FRow>& Out, float W0, float Z0, int32 N, float D, float Rise, float FirstRise)
	{
		for (int32 k = 0; k < N; ++k) { FRow R; R.W = W0 + D * k; R.D = D; R.Z = Z0 + Rise * k; R.Rise = k == 0 ? FirstRise : Rise; Out.Add(R); }
	};
	// a lattice floodlight pylon with a lamp head facing the pitch
	auto Pylon = [&](float X, float Y)
	{
		const float H = 27.f, B = 1.0f, Tp = 0.45f;
		const FFr& F = FarF;
		Quad(F, FVector(X - B, Y + B, 0), FVector(X - B, Y - B, 0), FVector(X - Tp, Y - Tp, H), FVector(X - Tp, Y + Tp, H), NUNeg, Steel);
		Quad(F, FVector(X - B, Y - B, 0), FVector(X + B, Y - B, 0), FVector(X + Tp, Y - Tp, H), FVector(X - Tp, Y - Tp, H), NToPitch, Steel * 1.05f);
		// bracing on the front face
		for (float Z = 0.f; Z < H - 0.5f; Z += 3.f)
		{
			auto Wd = [&](float Zz) { return FMath::Lerp(B, Tp, Zz / H); };
			const float Z1 = Z + 3.f;
			const float y = Y - Wd(Z) - 0.02f;
			const FVector L0(X - Wd(Z), y, Z), R0(X + Wd(Z), y, Z), L1(X - Wd(Z1), y, Z1), R1(X + Wd(Z1), y, Z1);
			const float T = 0.07f;
			Quad(F, L0 + FVector(-T, 0, 0), L0 + FVector(T, 0, 0), R1 + FVector(T, 0, 0), R1 + FVector(-T, 0, 0), NToPitch, Dark * 2.f);
			Quad(F, R0 + FVector(-T, 0, 0), R0 + FVector(T, 0, 0), L1 + FVector(T, 0, 0), L1 + FVector(-T, 0, 0), NToPitch, Dark * 2.f);
			Quad(F, L1 + FVector(0, 0, -T), R1 + FVector(0, 0, -T), R1 + FVector(0, 0, T), L1 + FVector(0, 0, T), NToPitch, Dark * 2.f);
		}
		// the head: a dark frame, 6 x 4 lamps
		const float Hy = Y - 0.7f;
		Quad(F, FVector(X - 3.2f, Hy + 0.4f, H - 0.4f), FVector(X - 3.2f, Hy, H - 0.4f), FVector(X - 3.2f, Hy, H + 3.4f), FVector(X - 3.2f, Hy + 0.4f, H + 3.4f), NUNeg, Steel);
		VFace(F, Hy, X - 3.2f, X + 3.2f, H - 0.4f, H + 3.4f, NToPitch, Srgb(40, 42, 46));
		HFace(F, H + 3.4f, X - 3.2f, X + 3.2f, Hy, Hy + 0.4f, Steel);
		const FLinearColor Lamp = bNight ? Srgb(255, 250, 226) : Srgb(206, 210, 204);
		for (int32 r = 0; r < 4; ++r)
			for (int32 c = 0; c < 6; ++c)
			{
				const float U0 = X - 3.0f + c * 1.0f + 0.12f, Z0 = H - 0.2f + r * 0.9f + 0.1f;
				VFace(F, Hy - 0.01f, U0, U0 + 0.76f, Z0, Z0 + 0.7f, NToPitch, Lamp, 1.f, !bNight);
			}
	};

	// a gantry of lamps hung under a roof's front edge (they glow at night)
	auto RoofLamps = [&](const FFr& F, float U0, float U1, float W, float Z)
	{
		const FLinearColor Lamp = bNight ? Srgb(255, 248, 220) : Srgb(196, 200, 196);
		for (float U = U0 + 3.f; U < U1 - 2.f; U += 6.f)
		{
			VFace(F, W, U - 0.9f, U + 0.9f, Z - 0.75f, Z, NToPitch, Srgb(44, 46, 50));
			VFace(F, W - 0.01f, U - 0.8f, U - 0.05f, Z - 0.65f, Z - 0.1f, NToPitch, Lamp, 1.f, !bNight);
			VFace(F, W - 0.01f, U + 0.05f, U + 0.8f, Z - 0.65f, Z - 0.1f, NToPitch, Lamp, 1.f, !bNight);
		}
	};

	// ================================================================ ground (under the grass)
	Layer = LGround;
	HFace(FarF, 0.f, -260.f, 260.f, -140.f, 260.f, Srgb(62, 70, 60));          // the streets and car parks round the ground
	HFace(FarF, 0.f, -78.f, 78.f, -49.f, 66.f, Srgb(104, 102, 96));            // the ground's own concrete
	HFace(FarF, 0.f, -59.6f, 59.6f, -39.4f, 37.8f, Srgb(150, 146, 134));       // the run-off strip between the boards and the walls

	// ================================================================ stands (after the grass, before the players)
	Layer = LStands;
	// far-right pylon first: the far stand is nearer the camera than it
	Pylon(82.f, 47.f);
	// ---- far stand
	{
		FStand S; S.F = &FarF; S.U0 = -74.f; S.U1 = 74.f; S.bBack = false;   // corner to corner: the ends stop at its front
		RowsFrom(S.Rows, 37.6f, 1.1f, 20, 0.8f, 0.45f, 1.1f);
		const int32 Low = S.Rows.Num();
		RowsFrom(S.Rows, 37.6f + 16.f, S.Rows.Last().Z + 2.4f, 12, 0.8f, 0.5f, 2.4f);
		S.Rows[Low].bFascia = true;
		const float RoofW = S.Rows[Low + 3].W, RoofZ = S.Rows.Last().Z + 3.2f;
		for (FRow& R : S.Rows) if (R.W >= RoofW - 0.01f) R.Shade = 0.72f;
		const float BackW = S.Rows.Last().W + S.Rows.Last().D;
		// the rear wall's inside, up to the roof
		VFace(FarF, BackW, S.U0, S.U1, S.Rows.Last().Z, RoofZ - 0.6f, NToPitch, BrickDark, 0.72f);
		S.NoSeat = [](int32 k, float U) { return k <= 2 && FMath::Abs(U) < 2.7f; };
		S.DarkAt = [](int32 k, float U) { if (k < 9 || k > 11) return false; for (float C : { -36.f, -12.f, 12.f, 36.f }) if (FMath::Abs(U - C) < 1.2f) return true; return false; };
		S.Pick = [&Rng](int32, float) { uint8 F; do F = uint8(Rng.Int(20)); while (F == 5 || F == 8); return F; };
		S.After = [&, RoofW, RoofZ, Low](int32 k)
		{
			if (k == 0)
			{
				// the players' tunnel: a canopy over the first four rows, its mouth on the pitch wall
				// (it sits in the first rows, no higher than the fourth, so it never hides the crowd behind it)
				Box(FarF, -2.4f, 2.4f, 37.3f, 40.0f, 0.f, 2.45f, Club, Club * 0.8f, Srgb(176, 186, 194));
				HFace(FarF, 2.451f, -2.2f, 2.2f, 37.5f, 39.8f, Srgb(150, 162, 172));
				VFace(FarF, 37.29f, -1.7f, 1.7f, 0.f, 1.95f, NToPitch, Dark);
				VFace(FarF, 37.28f, -2.4f, 2.4f, 2.2f, 2.35f, NToPitch, White);
			}
			if (k == Low + 3)
				for (float U = -72.f; U <= 72.f; U += 18.f)   // roof columns on the front of the upper tier
					Box(FarF, U - 0.18f, U + 0.18f, RoofW - 0.36f, RoofW, 0.f, RoofZ - 0.5f, Steel * 1.2f, Steel, Steel);
		};
		BuildStand(S);
		// the roof: corrugated sheets falling to the back, a white fascia with a band of the club colour
		for (float U = S.U0 - 0.5f; U < S.U1 + 0.5f; U += 1.2f)
		{
			const float Ub = FMath::Min(U + 1.2f, S.U1 + 0.5f);
			Quad(FarF, FVector(U, RoofW - 0.5f, RoofZ), FVector(Ub, RoofW - 0.5f, RoofZ), FVector(Ub, BackW + 0.6f, RoofZ - 1.2f), FVector(U, BackW + 0.6f, RoofZ - 1.2f), FVector(0, 0.15f, 1).GetSafeNormal(),
				(FMath::RoundToInt((U + 60.f) / 1.2f) & 1) ? RoofA : RoofB);
		}
		VFace(FarF, RoofW - 0.5f, S.U0 - 0.5f, S.U1 + 0.5f, RoofZ - 1.0f, RoofZ, NToPitch, White);
		VFace(FarF, RoofW - 0.51f, S.U0 - 0.5f, S.U1 + 0.5f, RoofZ - 0.75f, RoofZ - 0.35f, NToPitch, Club);
		RoofLamps(FarF, S.U0, S.U1, RoofW - 0.55f, RoofZ - 1.0f);
		EFace(FarF, S.U0 - 0.5f, RoofW - 0.5f, BackW + 0.6f, RoofZ - 1.2f, RoofZ, NUNeg, White * 0.8f);
		// the pitch wall's coping and the boards in front of it
		HFace(FarF, 1.1f, S.U0, S.U1, 37.45f, 37.6f, White, 0.95f);
		VFace(FarF, 37.45f, S.U0, S.U1, 0.f, 1.1f, NToPitch, Srgb(44, 84, 60));
		Boards(FarF, -58.f, 58.f, 36.05f);
	}
	Pylon(-82.f, 47.f);
	// ---- east end: covered, seen from the front; the away supporters in its north half
	{
		FStand S; S.F = &EastF; S.U0 = -39.f; S.U1 = 37.4f; S.bBack = false;
		RowsFrom(S.Rows, 59.4f, 1.1f, 18, 0.8f, 0.45f, 1.1f);
		const float RoofW = S.Rows[10].W, RoofZ = S.Rows.Last().Z + 2.9f;
		for (FRow& R : S.Rows) if (R.W >= RoofW - 0.01f) R.Shade = 0.74f;
		const float BackW = S.Rows.Last().W + S.Rows.Last().D;
		VFace(EastF, BackW, S.U0, S.U1, S.Rows.Last().Z, RoofZ - 0.6f, NToPitch, BrickDark, 0.74f);
		S.DarkAt = [](int32 k, float U) { return k >= 6 && k <= 7 && (FMath::Abs(U + 18.f) < 1.0f || FMath::Abs(U - 4.f) < 1.0f); };
		S.Pick = [&Rng](int32, float U) -> uint8
		{
			if (U > 8.f) { static const uint8 Away[] = { 5, 8, 5, 8, 5, 8, 11, 14 }; return Away[Rng.Int(8)]; }
			uint8 F; do F = uint8(Rng.Int(20)); while (F == 5 || F == 8); return F;
		};
		S.After = [&, RoofW, RoofZ](int32 k)
		{
			if (k == 10) for (float U = -33.f; U <= 33.f; U += 16.5f) Box(EastF, U - 0.18f, U + 0.18f, RoofW - 0.36f, RoofW, 0.f, RoofZ - 0.5f, Steel * 1.2f, Steel, Steel);
		};
		BuildStand(S);
		for (float U = S.U1 + 0.5f - 1.2f; U > S.U0 - 1.7f; U -= 1.2f)
		{
			const float Ua = FMath::Max(U, S.U0 - 0.5f), Ub = U + 1.2f;
			Quad(EastF, FVector(Ua, RoofW - 0.5f, RoofZ), FVector(Ub, RoofW - 0.5f, RoofZ), FVector(Ub, BackW + 0.6f, RoofZ - 1.0f), FVector(Ua, BackW + 0.6f, RoofZ - 1.0f), FVector(0, 0.15f, 1).GetSafeNormal(),
				(FMath::RoundToInt((U + 60.f) / 1.2f) & 1) ? RoofA : RoofB);
		}
		VFace(EastF, RoofW - 0.5f, S.U0 - 0.5f, S.U1 + 0.5f, RoofZ - 0.9f, RoofZ, NToPitch, White);
		VFace(EastF, RoofW - 0.51f, S.U0 - 0.5f, S.U1 + 0.5f, RoofZ - 0.7f, RoofZ - 0.3f, NToPitch, Club);
		RoofLamps(EastF, S.U0, S.U1, RoofW - 0.55f, RoofZ - 0.9f);
		EFace(EastF, S.U0 - 0.5f, RoofW - 0.5f, BackW + 0.6f, RoofZ - 1.0f, RoofZ, NUNeg, White * 0.8f);
		HFace(EastF, 1.1f, S.U0, S.U1, 59.25f, 59.4f, White, 0.95f);
		VFace(EastF, 59.25f, S.U0, S.U1, 0.f, 1.1f, NToPitch, Srgb(44, 84, 60));
		Boards(EastF, -38.f, 36.f, 58.05f);
	}
	// ---- west end: an open kop, seen from behind (backs of the boards, then the rows from the front, then its facade)
	{
		Boards(WestF, -38.f, 36.f, 58.05f);
		FStand S; S.F = &WestF; S.U0 = -39.f; S.U1 = 37.4f; S.bBack = true;
		RowsFrom(S.Rows, 59.4f, 1.1f, 18, 0.8f, 0.45f, 1.1f);
		S.DarkAt = [](int32 k, float U) { return k >= 6 && k <= 8 && (FMath::Abs(U + 18.f) < 1.2f || FMath::Abs(U - 4.f) < 1.2f); };
		S.Pick = [&Rng](int32, float) { uint8 F; do F = uint8(Rng.Int(20)); while (F == 5 || F == 8); return F; };
		BuildStand(S);
		const float BackW = S.Rows.Last().W + S.Rows.Last().D, TopZ = S.Rows.Last().Z + 1.2f;
		HFace(WestF, TopZ, S.U0, S.U1, BackW - 0.3f, BackW, White, 0.9f);
		VFace(WestF, BackW, S.U0, S.U1, 0.f, TopZ, NOut, Brick);
		for (float Z = 1.2f; Z < TopZ - 0.5f; Z += 1.6f) VFace(WestF, BackW + 0.01f, S.U0, S.U1, Z, Z + 0.12f, NOut, BrickDark);
		for (float U = S.U0 + 6.f; U < S.U1 - 4.f; U += 14.f) VFace(WestF, BackW + 0.02f, U, U + 2.4f, 0.f, 2.6f, NOut, Srgb(70, 60, 56));   // turnstile gates
	}

	// ================================================================ near side (after the players)
	Layer = LNear;
	{
		Boards(NearF, -58.f, 58.f, 38.05f);
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
				Box(NearF, A, B, 38.6f, 40.95f, 0.f, 1.75f, Srgb(60, 64, 70), Srgb(70, 76, 84), Srgb(150, 170, 184));
				HFace(NearF, 1.751f, A + 0.2f, B - 0.2f, 38.8f, 40.75f, Srgb(170, 192, 204));
			}
		};
		BuildStand(S);
		const float BackW = S.Rows.Last().W + S.Rows.Last().D, TopZ = S.Rows.Last().Z + 1.1f;
		HFace(NearF, TopZ, S.U0, S.U1, BackW - 0.3f, BackW, White, 0.9f);
		VFace(NearF, BackW, S.U0, S.U1, 0.f, TopZ, NOut, Brick);
		for (float Z = 0.9f; Z < TopZ - 0.4f; Z += 1.3f) VFace(NearF, BackW + 0.01f, S.U0, S.U1, Z, Z + 0.1f, NOut, BrickDark);
		for (float U = S.U0 + 8.f; U < S.U1 - 4.f; U += 16.f) VFace(NearF, BackW + 0.02f, U, U + 2.4f, 0.f, 2.4f, NOut, Srgb(70, 60, 56));
	}
	int32 NQ = 0; for (const TArray<FOp>& L : Ops) NQ += L.Num();
	UE_LOG(LogPF, Log, TEXT("PF stadium3d: %d ops (%d / %d / %d), %d fans"), NQ, Ops[LGround].Num(), Ops[LStands].Num(), Ops[LNear].Num(), Fans.Num());
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
	Tris.Reserve(4096);
	const FVector2D WUV(WhiteUV.X, WhiteUV.Y);
	const float Bob = FMath::Sin(Crowd.Clock * 90.f);
	const int32 FanRows = FrontRows;
	for (const FOp& Op : Ops[Layer])
	{
		if (Op.Hi.X < Lo.X || Op.Lo.X > Hi.X || Op.Hi.Y < Lo.Y || Op.Lo.Y > Hi.Y) continue;
		if (Op.Fan < 0)
		{
			FVector2D S[4];
			for (int32 i = 0; i < 4; ++i) S[i] = FVector2D((Op.P[i] - T0) * Zoom);
			FCanvasUVTri A; A.V0_Pos = S[0]; A.V1_Pos = S[1]; A.V2_Pos = S[2];
			A.V0_UV = A.V1_UV = A.V2_UV = WUV; A.V0_Color = A.V1_Color = A.V2_Color = Op.Col;
			FCanvasUVTri B = A; B.V1_Pos = S[2]; B.V2_Pos = S[3];
			Tris.Add(A); Tris.Add(B);
			continue;
		}
		// a supporter: seated, or on his feet (a goal, a big moment) - the same rules as the v0.9 crowd
		const FFan& F = Fans[Op.Fan];
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
		const float U0 = F.Fi * 64.f / SheetW, U1 = (F.Fi + 1) * 64.f / SheetW;
		const float V0 = float(Row * CellH) / SheetH, V1 = float((Row + 1) * CellH) / SheetH;
		const FLinearColor Col = Crowd.Light * Op.Col;
		FCanvasUVTri A, B;
		A.V0_Pos = P0; A.V1_Pos = FVector2D(P1.X, P0.Y); A.V2_Pos = P1;
		A.V0_UV = FVector2D(U0, V0); A.V1_UV = FVector2D(U1, V0); A.V2_UV = FVector2D(U1, V1);
		B.V0_Pos = P0; B.V1_Pos = P1; B.V2_Pos = FVector2D(P0.X, P1.Y);
		B.V0_UV = FVector2D(U0, V0); B.V1_UV = FVector2D(U1, V1); B.V2_UV = FVector2D(U0, V1);
		A.V0_Color = A.V1_Color = A.V2_Color = B.V0_Color = B.V1_Color = B.V2_Color = Col;
		Tris.Add(A); Tris.Add(B);
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
