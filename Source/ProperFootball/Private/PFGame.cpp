#include "PFGame.h"
#include "PFFlow.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

void UPFGameInstance::Init()
{
	Super::Init();
	UE_LOG(LogPF, Log, TEXT("PF Proper Football starting"));
}

// ------------------------------------------------------------------------------------------------ match actor
APFMatchActor::APFMatchActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = false;
}

void APFMatchActor::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(InThis, Collector);
	CastChecked<APFMatchActor>(InThis)->Match.AddReferencedObjects(Collector);
}

void APFMatchActor::StartMatch(const FPFMatchSetup& S)
{
	Match = FPFMatch();
	Match.Init(S);
	bRunning = true;
	bPaused = false;
}

void APFMatchActor::Tick(float Dt)
{
	Super::Tick(Dt);
}

// ------------------------------------------------------------------------------------------------ controller
APFPlayerController::APFPlayerController()
{
	bShowMouseCursor = false;
}

static UInputAction* MakeAction(UObject* Outer, const TCHAR* Name, EInputActionValueType Type)
{
	UInputAction* A = NewObject<UInputAction>(Outer, Name);
	A->ValueType = Type;
	return A;
}

void APFPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	Ctx = NewObject<UInputMappingContext>(this, TEXT("PF_Context"));
	IA_Move = MakeAction(this, TEXT("IA_Move"), EInputActionValueType::Axis2D);
	{
		FEnhancedActionKeyMapping& M = Ctx->MapKey(IA_Move, EKeys::Gamepad_Left2D);
		UInputModifierDeadZone* DZ = NewObject<UInputModifierDeadZone>(this);
		DZ->LowerThreshold = 0.18f; DZ->Type = EDeadZoneType::Radial;
		M.Modifiers.Add(DZ);
		// keyboard fallback: WASD / arrows. Y is screen-down positive in game, so W/Up = -Y.
		auto Key = [&](FKey K, bool bSwizzle, bool bNegate)
		{
			FEnhancedActionKeyMapping& KM = Ctx->MapKey(IA_Move, K);
			if (bSwizzle) { UInputModifierSwizzleAxis* S = NewObject<UInputModifierSwizzleAxis>(this); S->Order = EInputAxisSwizzle::YXZ; KM.Modifiers.Add(S); }
			if (bNegate) KM.Modifiers.Add(NewObject<UInputModifierNegate>(this));
		};
		Key(EKeys::D, false, false); Key(EKeys::A, false, true); Key(EKeys::S, true, false); Key(EKeys::W, true, true);
		Key(EKeys::Right, false, false); Key(EKeys::Left, false, true); Key(EKeys::Down, true, false); Key(EKeys::Up, true, true);
		// d-pad as movement too (menus)
		Key(EKeys::Gamepad_DPad_Right, false, false); Key(EKeys::Gamepad_DPad_Left, false, true); Key(EKeys::Gamepad_DPad_Down, true, false); Key(EKeys::Gamepad_DPad_Up, true, true);
	}
	// buttons: A B X Y LB RB Start Back Sprint (gamepad first, keyboard fallback)
	const TArray<TArray<FKey>> Keys = {
		{ EKeys::Gamepad_FaceButton_Bottom, EKeys::J, EKeys::Enter, EKeys::SpaceBar },  // A: pass / select
		{ EKeys::Gamepad_FaceButton_Right, EKeys::L, EKeys::BackSpace },               // B: lob / back
		{ EKeys::Gamepad_FaceButton_Left, EKeys::K },                                  // X: shoot / slide
		{ EKeys::Gamepad_FaceButton_Top, EKeys::I },                                   // Y: through ball
		{ EKeys::Gamepad_LeftShoulder, EKeys::Q },                                     // LB: switch player
		{ EKeys::Gamepad_RightShoulder, EKeys::E },                                    // RB
		{ EKeys::Gamepad_Special_Right, EKeys::Escape, EKeys::P },                     // Start: pause
		{ EKeys::Gamepad_Special_Left, EKeys::Tab },                                   // Back
		{ EKeys::Gamepad_RightTrigger, EKeys::LeftShift, EKeys::RightShift },          // Sprint
	};
	Buttons.Reset();
	for (int32 i = 0; i < Keys.Num(); ++i)
	{
		UInputAction* A = MakeAction(this, *FString::Printf(TEXT("IA_Btn%d"), i), EInputActionValueType::Boolean);
		for (const FKey& K : Keys[i]) Ctx->MapKey(A, K);
		Buttons.Add(A);
	}
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &APFPlayerController::OnMove);
		EIC->BindAction(IA_Move, ETriggerEvent::Completed, this, &APFPlayerController::OnMoveDone);
	}
	if (ULocalPlayer* LP = GetLocalPlayer())
		if (UEnhancedInputLocalPlayerSubsystem* Sub = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			Sub->AddMappingContext(Ctx, 0);
}

void APFPlayerController::OnMove(const FInputActionValue& V) { MoveRaw = V.Get<FVector2D>(); }
void APFPlayerController::OnMoveDone(const FInputActionValue& V) { MoveRaw = FVector2D::ZeroVector; }

void APFPlayerController::PlayerTick(float Dt)
{
	Super::PlayerTick(Dt);
	UEnhancedInputLocalPlayerSubsystem* Sub = GetLocalPlayer() ? GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	auto Held = [&](int32 i) -> bool
	{
		if (!Sub || !Buttons.IsValidIndex(i)) return false;
		const FInputActionValue V = Sub->GetPlayerInput() ? Sub->GetPlayerInput()->GetActionValue(Buttons[i]) : FInputActionValue();
		return V.Get<bool>();
	};
	Prev = Pad;
	FPFPadState N;
	// Gamepad_Left2D reports up as +Y; the game uses screen space (y down). Keyboard mappings were built y-down already,
	// so flip only when the stick is the source: sticks never produce exact axis-aligned unit values like keys do.
	FVector2D M = MoveRaw;
	const float StickY = GetInputAnalogKeyState(EKeys::Gamepad_LeftY);
	const float StickX = GetInputAnalogKeyState(EKeys::Gamepad_LeftX);
	if (FVector2D(StickX, StickY).SizeSquared() > 0.03f) M = FVector2D(StickX, -StickY);
	if (M.SizeSquared() > 1.f) M.Normalize();
	N.Move = M;
	N.bA = Held(0); N.bB = Held(1); N.bX = Held(2); N.bY = Held(3); N.bLB = Held(4); N.bRB = Held(5); N.bStart = Held(6); N.bBack = Held(7); N.bSprint = Held(8);
	N.pA = N.bA && !Prev.bA; N.pB = N.bB && !Prev.bB; N.pX = N.bX && !Prev.bX; N.pY = N.bY && !Prev.bY;
	N.pLB = N.bLB && !Prev.bLB; N.pRB = N.bRB && !Prev.bRB; N.pStart = N.bStart && !Prev.bStart; N.pBack = N.bBack && !Prev.bBack;
	N.rX = !N.bX && Prev.bX;
	// menu navigation with repeat
	FVector2D Nav(FMath::Abs(M.X) > 0.5f ? FMath::Sign(M.X) : 0.f, FMath::Abs(M.Y) > 0.5f ? FMath::Sign(M.Y) : 0.f);
	if (Nav != PrevNav) { NavRepeat = 0.38f; if (!Nav.IsZero()) { N.pLeft = Nav.X < 0; N.pRight = Nav.X > 0; N.pUp = Nav.Y < 0; N.pDown = Nav.Y > 0; } }
	else if (!Nav.IsZero()) { NavRepeat -= Dt; if (NavRepeat <= 0) { NavRepeat = 0.11f; N.pLeft = Nav.X < 0; N.pRight = Nav.X > 0; N.pUp = Nav.Y < 0; N.pDown = Nav.Y > 0; } }
	PrevNav = Nav;
	Pad = N;
	if (UPFFlow* Flow = UPFFlow::Get(this)) Flow->Tick(Dt, Pad);
}

// ------------------------------------------------------------------------------------------------ HUD: draws the whole game (pitch, sprites, HUD, menus)
APFHUD::APFHUD()
{
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> M(TEXT("/Game/PF/M_PFSprite.M_PFSprite"));
	if (M.Succeeded()) SpriteMat = M.Object;
}

void APFHUD::DrawHUD()
{
	Super::DrawHUD();
	if (UPFFlow* Flow = UPFFlow::Get(this)) Flow->Draw(Canvas, SpriteMat);
}

// ------------------------------------------------------------------------------------------------ game mode
APFGameMode::APFGameMode()
{
	PlayerControllerClass = APFPlayerController::StaticClass();
	HUDClass = APFHUD::StaticClass();
	DefaultPawnClass = nullptr;
}

void APFGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (UPFFlow* Flow = UPFFlow::Get(this)) Flow->Begin(GetWorld());
}
