#include "PFCareer.h"
#include "PFFlow.h"
#include "PFGame.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"

UPFCareer* UPFCareer::Get(const UObject* Ctx)
{
	const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
	UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UPFCareer>() : nullptr;
}

void UPFCareer::Enter() {}

void UPFCareer::Tick(float Dt, const FPFPadState& Pad)
{
	if (Pad.pB || Pad.pA) if (UPFFlow* F = UPFFlow::Get(this)) F->Go(EPFScreen::Main);
}

void UPFCareer::Draw(UCanvas* C, UMaterialInterface* SpriteMat)
{
	UPFFlow* F = UPFFlow::Get(this);
	if (!F) return;
	F->Box(C, 0, 0, C->ClipX, C->ClipY, FLinearColor(0.01f, 0.02f, 0.06f, 1));
	F->Text(C, TEXT("CAREER - COMING IN v0.5"), C->ClipX * 0.5f, C->ClipY * 0.45f, 1.4f, FLinearColor::White, EPFAlign::Center);
}
