#include "PFSprites.h"
#include "PFCore.h"
#include "Engine/Texture2D.h"

const TCHAR* FPFSpriteSet::DirName(EPFDir D)
{
	// slot k is k*15 degrees clockwise from south; the 45-degree slots keep PixelLab's 8 names, the 30-degree ones are aNNN
	static const TCHAR* N[24] = { TEXT("south"), TEXT("a015"), TEXT("a030"), TEXT("south-east"), TEXT("a060"), TEXT("a075"), TEXT("east"), TEXT("a105"),
		TEXT("a120"), TEXT("north-east"), TEXT("a150"), TEXT("a165"), TEXT("north"), TEXT("a195"), TEXT("a210"), TEXT("north-west"), TEXT("a240"),
		TEXT("a255"), TEXT("west"), TEXT("a285"), TEXT("a300"), TEXT("south-west"), TEXT("a330"), TEXT("a345") };
	return N[((int32(D) % 24) + 24) % 24];
}

int32 FPFSpriteSet::SlotOf(const FString& Name)
{
	for (int32 k = 0; k < 24; ++k) if (Name == DirName(EPFDir(k))) return k;
	return -1;
}

bool FPFSpriteSet::Load(const FString& InId)
{
	Id = InId;
	TSharedPtr<FJsonObject> J = PF::LoadJson(FString::Printf(TEXT("GameData/Sprites/%s.json"), *Id));
	if (!J.IsValid()) return false;
	Kind = J->GetStringField(TEXT("kind"));
	Cell = J->GetIntegerField(TEXT("cell"));
	bKitKeyed = J->GetBoolField(TEXT("kit_keyed"));
	const TArray<TSharedPtr<FJsonValue>>& Pv = J->GetArrayField(TEXT("pivot"));
	Pivot = FVector2D(Pv[0]->AsNumber(), Pv[1]->AsNumber());
	const TArray<TSharedPtr<FJsonValue>>& Sz = J->GetArrayField(TEXT("size"));
	AtlasPx = FVector2D(Sz[0]->AsNumber(), Sz[1]->AsNumber());
	// kit-keyed atlases must keep raw 8-bit codes, so they load as linear textures; the material decodes sRGB itself
	Atlas = PF::LoadTexture(J->GetStringField(TEXT("atlas")), !bKitKeyed);
	if (!Atlas) return false;
	const TSharedPtr<FJsonObject>& F = J->GetObjectField(TEXT("frames"));
	for (const auto& KV : F->Values)
	{
		const FString Key = *KV.Key;
		const TArray<TSharedPtr<FJsonValue>>& A = KV.Value->AsArray();
		FPFFrame Fr;
		const double X = A[0]->AsNumber() * Cell, Y = A[1]->AsNumber() * Cell;
		Fr.UV0 = FVector2D(X / AtlasPx.X, Y / AtlasPx.Y);
		Fr.UV1 = FVector2D((X + Cell) / AtlasPx.X, (Y + Cell) / AtlasPx.Y);
		Fr.bFlip = A.Num() > 2 && A[2]->AsNumber() > 0.5;
		Fr.bValid = true;
		Frames.Add(FName(*Key), Fr);
		FString Dir, Pose;
		if (Key.Split(TEXT("/"), &Dir, &Pose)) Poses.Add(FName(*Pose));
	}
	// v0.8: hand anchors (Tools/pipeline/hand_anchors.py), so a held ball is drawn in the gloves
	// v0.9.4: officials and managers never hold the ball and have no anchors: only look when the file is there (no log warning)
	const FString HandsRel = FString::Printf(TEXT("GameData/Sprites/%s_hands.json"), *Id);
	if (TSharedPtr<FJsonObject> HJ = FPaths::FileExists(PF::DataPath(HandsRel)) ? PF::LoadJson(HandsRel) : nullptr)
		for (const auto& KV : HJ->Values)
		{
			const TArray<TSharedPtr<FJsonValue>>& A = KV.Value->AsArray();
			if (A.Num() >= 2) Hands.Add(FName(*KV.Key), FVector2D(A[0]->AsNumber(), A[1]->AsNumber()));
		}
	// v0.9.8: resolve every pose's 24 slots to the nearest direction drawn (8- or 12-direction art, or a mix)
	TMap<FName, TArray<int32>> Have;
	TSet<int32> AllSlots;
	for (const auto& KV : Frames)
	{
		FString Dir, Pose;
		if (!KV.Key.ToString().Split(TEXT("/"), &Dir, &Pose)) continue;
		const int32 Sl = SlotOf(Dir);
		if (Sl >= 0) { Have.FindOrAdd(FName(*Pose)).AddUnique(Sl); AllSlots.Add(Sl); }
	}
	DirStep = (AllSlots.Contains(2) || AllSlots.Contains(4) || AllSlots.Contains(8) || AllSlots.Contains(10)) ? 30.f : 45.f;
	for (const auto& KV : Have)
	{
		TArray<FName>& R = Resolved.Add(KV.Key); TArray<uint8>& RS = ResolvedSlot.Add(KV.Key);
		R.SetNum(24); RS.SetNum(24);
		for (int32 k = 0; k < 24; ++k)
		{
			int32 Best = KV.Value[0], BestD = 99;
			for (int32 Sl : KV.Value) { const int32 Dd = FMath::Min((Sl - k + 24) % 24, (k - Sl + 24) % 24); if (Dd < BestD || (Dd == BestD && Sl > Best)) { BestD = Dd; Best = Sl; } }
			R[k] = FName(*FString::Printf(TEXT("%s/%s"), DirName(EPFDir(Best)), *KV.Key.ToString())); RS[k] = uint8(Best);
		}
	}
	UE_LOG(LogPF, Log, TEXT("PF sprite set %s: %d frames, %d poses, %d hand anchors, %d-direction art"), *Id, Frames.Num(), Poses.Num(), Hands.Num(), DirStep < 40.f ? 12 : 8);
	return true;
}

const FPFFrame& FPFSpriteSet::Get(EPFDir Dir, FName Pose, uint8* OutSlot) const
{
	static FPFFrame None;
	const int32 D = ((int32(Dir) % 24) + 24) % 24;
	for (FName P : { Pose, FName(TEXT("idle")) })
		if (const TArray<FName>* R = Resolved.Find(P))
			if (const FPFFrame* F = Frames.Find((*R)[D])) { if (OutSlot) *OutSlot = ResolvedSlot[P][D]; return *F; }
	return None;
}

const FVector2D* FPFSpriteSet::HandAt(EPFDir Dir, FName Pose) const
{
	const int32 D = ((int32(Dir) % 24) + 24) % 24;
	if (const TArray<FName>* R = Resolved.Find(Pose)) return Hands.Find((*R)[D]);
	return nullptr;
}

FName FPFClip::At(float T, int32* OutIndex) const
{
	float Acc = 0;
	for (int32 i = 0; i < Poses.Num(); ++i)
	{
		Acc += Times[i];
		if (T < Acc || i == Poses.Num() - 1) { if (OutIndex) *OutIndex = i; return Poses[i]; }
	}
	if (OutIndex) *OutIndex = 0;
	return Poses.Num() ? Poses[0] : NAME_None;
}

namespace PFClips
{
	static TMap<FName, FPFClip> Make()
	{
		TMap<FName, FPFClip> M;
		auto Add = [&M](const TCHAR* Name, std::initializer_list<const TCHAR*> P, std::initializer_list<float> T, int32 Ev = -1)
		{
			FPFClip C;
			for (const TCHAR* S : P) C.Poses.Add(FName(S));
			for (float F : T) C.Times.Add(F);
			C.EventIndex = Ev;
			M.Add(FName(Name), C);
		};
		// timings in seconds at animation rate 1.0; the ball leaves on EventIndex
		// v0.9 arcade poses (Tools/pixellab/poses_base.py ARC_*): bodies without them draw the v0.8 pose instead
		// (FPFMatch::Draw, ArcFallback). Big back-swing, strike, a long-held high follow-through.
		Add(TEXT("shot"),     { TEXT("shot_a_plant"), TEXT("shot_a_back"), TEXT("shot_a_strike"), TEXT("shot_a_follow"), TEXT("idle") }, { 0.05f, 0.07f, 0.06f, 0.20f, 0.04f }, 2);
		// v0.9.8 goal kick after the run-up (ISS study: plant 0.08, strike 0.06, a long follow-through 0.25)
		Add(TEXT("goal_kick"), { TEXT("shot_a_plant"), TEXT("shot_a_back"), TEXT("shot_a_strike"), TEXT("shot_a_follow"), TEXT("idle") }, { 0.08f, 0.06f, 0.06f, 0.25f, 0.04f }, 2);
		Add(TEXT("pass"),     { TEXT("pass_back"), TEXT("pass_strike"), TEXT("idle") }, { 0.05f, 0.09f, 0.05f }, 1);
		Add(TEXT("lob"),      { TEXT("shot_a_back"), TEXT("shot_a_strike"), TEXT("shot_a_follow") }, { 0.06f, 0.08f, 0.14f }, 1);
		Add(TEXT("volley"),   { TEXT("volley_strike"), TEXT("shot_follow") }, { 0.10f, 0.14f }, 0);
		Add(TEXT("header"),   { TEXT("header_stand"), TEXT("header"), TEXT("header_stand") }, { 0.10f, 0.18f, 0.10f }, 1);
		// ISS slide (docs/ISS_STUDY.md 5.3): drop 0.05 s, one held flat slide 0.33 s, hand down, crouch up: ~0.73 s
		Add(TEXT("slide"),    { TEXT("slide_go"), TEXT("slide_flat"), TEXT("slide_hand"), TEXT("slide_up"), TEXT("idle") }, { 0.06f, 0.38f, 0.14f, 0.18f, 0.03f }, 1);   // v0.9: the flat slide held a little longer than ISS so it reads
		Add(TEXT("tackle"),   { TEXT("tackle_lunge"), TEXT("tackle_poke"), TEXT("idle") }, { 0.07f, 0.15f, 0.05f }, 0);
		Add(TEXT("fall"),     { TEXT("stumble"), TEXT("fall_ground"), TEXT("fall_ground"), TEXT("getup_kneel"), TEXT("idle") }, { 0.12f, 0.35f, 0.35f, 0.25f, 0.05f });
		Add(TEXT("celebrate"),{ TEXT("celebrate_arms_up"), TEXT("celebrate_fist"), TEXT("celebrate_arms_up"), TEXT("celebrate_point") }, { 0.4f, 0.4f, 0.4f, 0.8f });   // v0.8: the airplane frames failed the sprite audit (66 broken cells)
		Add(TEXT("knee_slide"),{ TEXT("celebrate_knee_slide"), TEXT("celebrate_arms_up") }, { 1.0f, 1.0f });
		Add(TEXT("chest"),    { TEXT("chest_control"), TEXT("idle") }, { 0.20f, 0.05f });
		Add(TEXT("throwin"),  { TEXT("throwin_back"), TEXT("throwin_release"), TEXT("idle") }, { 0.25f, 0.15f, 0.08f }, 1);
		Add(TEXT("dejected"), { TEXT("hands_on_head") }, { 2.0f });
		Add(TEXT("feint"),    { TEXT("turn_plant"), TEXT("dribble_touchR") }, { 0.12f, 0.12f });   // right-stick skill move
		// goalkeeper
		// v0.9.5 (ISS frame-by-frame): spring, one long-held stretch through the flight (low 0.29 s, high 0.45 s), down
		// flat about 0.4 s, then a kneel and up (ISS: back up 21-29 frames after landing)
		Add(TEXT("gk_dive_low_R"), { TEXT("gk_ready"), TEXT("gk_push_R"), TEXT("gk_stretch_R"), TEXT("getup_kneel"), TEXT("gk_ready") }, { 0.03f, 0.06f, 0.62f, 0.18f, 0.05f }, 2);
		Add(TEXT("gk_dive_high_R"), { TEXT("gk_ready"), TEXT("gk_push_R"), TEXT("gk_stretch_R"), TEXT("getup_kneel"), TEXT("gk_ready") }, { 0.03f, 0.06f, 0.74f, 0.18f, 0.05f }, 2);
		Add(TEXT("gk_dive_low_L"), { TEXT("gk_ready"), TEXT("gk_push_L"), TEXT("gk_stretch_L"), TEXT("getup_kneel"), TEXT("gk_ready") }, { 0.03f, 0.06f, 0.62f, 0.18f, 0.05f }, 2);
		Add(TEXT("gk_dive_high_L"), { TEXT("gk_ready"), TEXT("gk_push_L"), TEXT("gk_stretch_L"), TEXT("getup_kneel"), TEXT("gk_ready") }, { 0.03f, 0.06f, 0.74f, 0.18f, 0.05f }, 2);
		// v0.9.5 (ISS: after a ground or chest catch he always goes down on the ball, 0.7-1.8 s): catch, kneel on it, up
		Add(TEXT("gk_catch"),  { TEXT("gk_catch_chest"), TEXT("getup_kneel"), TEXT("gk_hold_ball") }, { 0.18f, 0.38f, 0.2f });
		Add(TEXT("gk_catch_high"), { TEXT("gk_catch_high"), TEXT("gk_hold_ball") }, { 0.22f, 0.2f });
		Add(TEXT("gk_pickup"), { TEXT("gk_scoop"), TEXT("gk_catch_chest"), TEXT("getup_kneel"), TEXT("gk_hold_ball") }, { 0.16f, 0.10f, 0.30f, 0.15f });   // gathers a ground ball into his arms
		Add(TEXT("gk_punt"),   { TEXT("gk_hold_ball"), TEXT("gk_punt_drop"), TEXT("gk_punt_strike"), TEXT("idle") }, { 0.15f, 0.12f, 0.16f, 0.1f }, 2);
		Add(TEXT("gk_throw"),  { TEXT("gk_throw_back"), TEXT("gk_throw_release"), TEXT("idle") }, { 0.2f, 0.14f, 0.08f }, 1);
		// officials
		Add(TEXT("ref_whistle"), { TEXT("ref_whistle") }, { 0.8f });
		Add(TEXT("ref_card"),    { TEXT("ref_point"), TEXT("ref_card"), TEXT("ref_card") }, { 0.3f, 0.8f, 0.8f });
		Add(TEXT("lino_flag"),   { TEXT("lino_flag_up") }, { 1.2f });
		return M;
	}

	const FPFClip& Get(FName Name)
	{
		static const TMap<FName, FPFClip> M = Make();
		static FPFClip Empty;
		const FPFClip* C = M.Find(Name);
		return C ? *C : Empty;
	}
}
