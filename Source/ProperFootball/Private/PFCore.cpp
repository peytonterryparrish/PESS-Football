#include "PFCore.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/Texture2D.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogPF);

namespace PF
{
	FString DataPath(const FString& Rel) { return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / Rel); }

	bool LoadText(const FString& Rel, FString& Out)
	{
		if (FFileHelper::LoadFileToString(Out, *DataPath(Rel))) return true;
		UE_LOG(LogPF, Warning, TEXT("PF missing text %s"), *Rel);
		return false;
	}

	TSharedPtr<FJsonObject> LoadJson(const FString& Rel)
	{
		FString S;
		if (!LoadText(Rel, S)) return nullptr;
		TSharedPtr<FJsonObject> Obj;
		TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(S);
		if (!FJsonSerializer::Deserialize(R, Obj) || !Obj.IsValid())
		{
			// top-level arrays are wrapped so callers always get an object
			TArray<TSharedPtr<FJsonValue>> Arr;
			TSharedRef<TJsonReader<>> R2 = TJsonReaderFactory<>::Create(S);
			if (FJsonSerializer::Deserialize(R2, Arr))
			{
				Obj = MakeShared<FJsonObject>();
				Obj->SetArrayField(TEXT("items"), Arr);
				return Obj;
			}
			UE_LOG(LogPF, Warning, TEXT("PF bad json %s"), *Rel);
			return nullptr;
		}
		return Obj;
	}

	UTexture2D* LoadTexture(const FString& Rel, bool bSRGB, bool bSmooth)
	{
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *DataPath(Rel)))
		{
			UE_LOG(LogPF, Warning, TEXT("PF missing png %s"), *Rel);
			return nullptr;
		}
		IImageWrapperModule& IWM = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		TSharedPtr<IImageWrapper> W = IWM.CreateImageWrapper(EImageFormat::PNG);
		TArray<uint8> Raw;
		if (!W.IsValid() || !W->SetCompressed(Bytes.GetData(), Bytes.Num()) || !W->GetRaw(ERGBFormat::BGRA, 8, Raw)) return nullptr;
		const int32 Wd = W->GetWidth(), Ht = W->GetHeight();
		UTexture2D* T = UTexture2D::CreateTransient(Wd, Ht, PF_B8G8R8A8);
		if (!T) return nullptr;
		T->Filter = bSmooth ? TF_Bilinear : TF_Nearest;
		T->SRGB = bSRGB;
		T->NeverStream = true;
		T->LODGroup = bSmooth ? TEXTUREGROUP_UI : TEXTUREGROUP_Pixels2D;
		T->AddressX = TA_Clamp; T->AddressY = TA_Clamp;
		void* Dst = T->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(Dst, Raw.GetData(), Raw.Num());
		T->GetPlatformData()->Mips[0].BulkData.Unlock();
		T->UpdateResource();
		return T;
	}
}

static FVector2D JsonVec2(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, FVector2D Def)
{
	const TArray<TSharedPtr<FJsonValue>>* A;
	if (O.IsValid() && O->TryGetArrayField(Key, A) && A->Num() >= 2) return FVector2D((*A)[0]->AsNumber(), (*A)[1]->AsNumber());
	return Def;
}

bool FPFProjection::Load()
{
	TSharedPtr<FJsonObject> P = PF::LoadJson(TEXT("ArtSource/Packs/R08_Final_93_Club_Stadiums_4K/Projection.json"));
	if (!P.IsValid()) return false;
	Origin = JsonVec2(P, TEXT("origin_4k_px"), Origin);
	AtlasSize = JsonVec2(P, TEXT("atlas_4k_size_px"), AtlasSize);
	const TArray<TSharedPtr<FJsonValue>>* M;
	if (P->TryGetArrayField(TEXT("world_xyz_to_4k"), M) && M->Num() == 2)
	{
		const TArray<TSharedPtr<FJsonValue>>& R0 = (*M)[0]->AsArray();
		const TArray<TSharedPtr<FJsonValue>>& R1 = (*M)[1]->AsArray();
		Ax = FVector2D(R0[0]->AsNumber(), R1[0]->AsNumber());
		Ay = FVector2D(R0[1]->AsNumber(), R1[1]->AsNumber());
		Az = FVector2D(R0[2]->AsNumber(), R1[2]->AsNumber());
	}
	const TArray<TSharedPtr<FJsonValue>>* K;
	if (P->TryGetArrayField(TEXT("kickoff_reference_16x9_crop_xywh_4k_px"), K) && K->Num() == 4)
		KickoffView = FVector2D((*K)[2]->AsNumber(), (*K)[3]->AsNumber());
	UE_LOG(LogPF, Log, TEXT("PF projection Ax=%s Ay=%s Az=%s"), *Ax.ToString(), *Ay.ToString(), *Az.ToString());
	return true;
}

FVector2D FPFProjection::AtlasToGround(const FVector2D& A) const
{
	const FVector2D D = A - Origin;
	const double Det = Ax.X * Ay.Y - Ay.X * Ax.Y;
	return FVector2D((D.X * Ay.Y - Ay.X * D.Y) / Det, (Ax.X * D.Y - D.X * Ax.Y) / Det);
}

FVector2D FPFProjection::ScreenDirToWorld(const FVector2D& S) const
{
	const double Det = Ax.X * Ay.Y - Ay.X * Ax.Y;
	return FVector2D((S.X * Ay.Y - Ay.X * S.Y) / Det, (Ax.X * S.Y - S.X * Ax.Y) / Det);
}
