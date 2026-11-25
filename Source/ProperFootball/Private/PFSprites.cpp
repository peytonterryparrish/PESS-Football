#include "PFSprites.h"
#include "PFCore.h"
#include "Engine/Texture2D.h"

const TCHAR* FPFSpriteSet::DirName(EPFDir D)
{
	static const TCHAR* N[] = { TEXT("south"), TEXT("south-east"), TEXT("east"), TEXT("north-east"), TEXT("north"), TEXT("north-west"), TEXT("west"), TEXT("south-west") };
	return N[FMath::Clamp(int32(D), 0, 7)];
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
	UE_LOG(LogPF, Log, TEXT("PF sprite set %s: %d frames, %d poses"), *Id, Frames.Num(), Poses.Num());
	return true;
}

const FPFFrame& FPFSpriteSet::Get(EPFDir Dir, FName Pose) const
{
	static FPFFrame None;
	const int32 D = int32(Dir);
	// same pose, then neighbouring directions, then idle
	static const int32 Order[] = { 0, 1, -1, 2, -2, 3, -3, 4 };
	for (FName P : { Pose, FName(TEXT("idle")) })
		for (int32 O : Order)
		{
			const EPFDir DD = EPFDir((D + O + 8) % 8);
			if (const FPFFrame* F = Frames.Find(FName(*FString::Printf(TEXT("%s/%s"), DirName(DD), *P.ToString())))) return *F;
		}
	return None;
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
		Add(TEXT("shot"),     { TEXT("shot_plant"), TEXT("shot_back"), TEXT("shot_strike"), TEXT("shot_follow"), TEXT("idle") }, { 0.05f, 0.06f, 0.07f, 0.14f, 0.05f }, 2);
		Add(TEXT("pass"),     { TEXT("pass_back"), TEXT("pass_strike"), TEXT("idle") }, { 0.05f, 0.09f, 0.05f }, 1);
		Add(TEXT("lob"),      { TEXT("shot_back"), TEXT("shot_strike"), TEXT("shot_follow") }, { 0.06f, 0.08f, 0.12f }, 1);
		Add(TEXT("volley"),   { TEXT("volley_strike"), TEXT("shot_follow") }, { 0.10f, 0.14f }, 0);
		Add(TEXT("header"),   { TEXT("header_stand"), TEXT("header"), TEXT("header_stand") }, { 0.10f, 0.18f, 0.10f }, 1);
		Add(TEXT("slide"),    { TEXT("tackle_stand"), TEXT("slide"), TEXT("slide"), TEXT("getup_kneel"), TEXT("idle") }, { 0.06f, 0.30f, 0.25f, 0.22f, 0.05f }, 1);
		Add(TEXT("tackle"),   { TEXT("tackle_stand"), TEXT("block"), TEXT("idle") }, { 0.08f, 0.14f, 0.05f }, 0);
		Add(TEXT("fall"),     { TEXT("stumble"), TEXT("fall_ground"), TEXT("fall_ground"), TEXT("getup_kneel"), TEXT("idle") }, { 0.12f, 0.35f, 0.35f, 0.25f, 0.05f });
		Add(TEXT("celebrate"),{ TEXT("celebrate_arms_up"), TEXT("celebrate_fist"), TEXT("celebrate_arms_up"), TEXT("celebrate_airplane") }, { 0.4f, 0.4f, 0.4f, 0.8f });
		Add(TEXT("knee_slide"),{ TEXT("celebrate_knee_slide"), TEXT("celebrate_arms_up") }, { 1.0f, 1.0f });
		Add(TEXT("chest"),    { TEXT("chest_control"), TEXT("idle") }, { 0.20f, 0.05f });
		Add(TEXT("throwin"),  { TEXT("throwin_back"), TEXT("throwin_release"), TEXT("idle") }, { 0.25f, 0.15f, 0.08f }, 1);
		Add(TEXT("dejected"), { TEXT("hands_on_head") }, { 2.0f });
		Add(TEXT("feint"),    { TEXT("turn_plant"), TEXT("dribble_touchR") }, { 0.12f, 0.12f });   // right-stick skill move
		// goalkeeper
		Add(TEXT("gk_dive_low_R"), { TEXT("gk_ready"), TEXT("gk_dive_low_R1"), TEXT("gk_stretch_R"), TEXT("getup_kneel") }, { 0.04f, 0.08f, 0.55f, 0.42f }, 2);
		Add(TEXT("gk_dive_high_R"), { TEXT("gk_ready"), TEXT("gk_dive_high_R1"), TEXT("gk_stretch_R"), TEXT("getup_kneel") }, { 0.04f, 0.08f, 0.55f, 0.42f }, 2);
		Add(TEXT("gk_dive_low_L"), { TEXT("gk_ready"), TEXT("gk_dive_low_L1"), TEXT("gk_stretch_L"), TEXT("getup_kneel") }, { 0.04f, 0.08f, 0.55f, 0.42f }, 2);
		Add(TEXT("gk_dive_high_L"), { TEXT("gk_ready"), TEXT("gk_dive_high_L1"), TEXT("gk_stretch_L"), TEXT("getup_kneel") }, { 0.04f, 0.08f, 0.55f, 0.42f }, 2);
		Add(TEXT("gk_catch"),  { TEXT("gk_catch_chest"), TEXT("gk_hold_ball") }, { 0.18f, 0.2f });
		Add(TEXT("gk_catch_high"), { TEXT("gk_catch_high"), TEXT("gk_hold_ball") }, { 0.22f, 0.2f });
		Add(TEXT("gk_pickup"), { TEXT("gk_scoop"), TEXT("gk_catch_chest"), TEXT("gk_hold_ball") }, { 0.16f, 0.10f, 0.15f });   // gathers a ground ball into his arms
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
