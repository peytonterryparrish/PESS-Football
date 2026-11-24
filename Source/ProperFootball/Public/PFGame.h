// Proper Football - framework: game instance, mode, controller (Enhanced Input built in code), HUD and the match actor.
#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "InputActionValue.h"
#include "PFMatch.h"
#include "PFGame.generated.h"

class UInputAction;
class UInputMappingContext;
class UMaterialInterface;
class UFont;

// Pad state the controller gathers each frame (gameplay and menus read it).
struct FPFPadState
{
	FVector2D Move = FVector2D::ZeroVector;
	bool bSprint = false;
	bool bA = false, bB = false, bX = false, bY = false, bLB = false, bRB = false, bStart = false, bBack = false;     // held
	bool pA = false, pB = false, pX = false, pY = false, pLB = false, pRB = false, pStart = false, pBack = false;     // pressed this frame
	bool rX = false;                                                                                                  // X released this frame
	bool pUp = false, pDown = false, pLeft = false, pRight = false;                                                   // menu navigation (stick or d-pad, repeating)
};

UCLASS()
class PROPERFOOTBALL_API UPFGameInstance : public UGameInstance
{
	GENERATED_BODY()
public:
	virtual void Init() override;
};

UCLASS()
class PROPERFOOTBALL_API APFMatchActor : public AActor
{
	GENERATED_BODY()
public:
	APFMatchActor();
	virtual void Tick(float Dt) override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	void StartMatch(const FPFMatchSetup& S);
	FPFMatch Match;
	bool bRunning = false;
	bool bPaused = false;
};

UCLASS()
class PROPERFOOTBALL_API APFPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	APFPlayerController();
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float Dt) override;
	FPFPadState Pad;

private:
	UPROPERTY() TObjectPtr<UInputMappingContext> Ctx;
	UPROPERTY() TObjectPtr<UInputAction> IA_Move;
	UPROPERTY() TArray<TObjectPtr<UInputAction>> Buttons;   // A B X Y LB RB Start Back Sprint
	FPFPadState Prev;
	float NavRepeat = 0;
	FVector2D PrevNav = FVector2D::ZeroVector;
	void OnMove(const FInputActionValue& V);
	void OnMoveDone(const FInputActionValue& V);
	FVector2D MoveRaw = FVector2D::ZeroVector;
	TArray<TArray<FKey>> ButtonKeys;   // same order as Buttons; read directly so input never depends on the input-class config
};

UCLASS()
class PROPERFOOTBALL_API APFHUD : public AHUD
{
	GENERATED_BODY()
public:
	APFHUD();
	virtual void DrawHUD() override;
	UPROPERTY() TObjectPtr<UMaterialInterface> SpriteMat;
};

UCLASS()
class PROPERFOOTBALL_API APFGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	APFGameMode();
	virtual void BeginPlay() override;
};
