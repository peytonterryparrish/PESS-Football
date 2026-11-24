// Proper Football - career mode (endless English pyramid). Filled in from v0.5.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PFCareer.generated.h"

class UCanvas;
class UMaterialInterface;
struct FPFPadState;

UCLASS()
class PROPERFOOTBALL_API UPFCareer : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	static UPFCareer* Get(const UObject* Ctx);
	void Enter();
	void Tick(float Dt, const FPFPadState& Pad);
	void Draw(UCanvas* C, UMaterialInterface* SpriteMat);
};
