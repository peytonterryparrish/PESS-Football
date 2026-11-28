// Proper Football - the game flow: every screen (title, menus, match, career) is drawn on the HUD canvas with
// pixel fonts and the owner's UI sprites, and driven by the pad. Controller-first: every screen works with A/B + stick.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PFMatch.h"
#include "PFFlow.generated.h"

class UCanvas;
class UTexture2D;
class UMaterialInterface;
class APFMatchActor;
struct FPFPadState;

// bitmap font built from the UI pack's glyph sprites (Tools/pipeline/build_data.py)
struct FPFFont
{
	TObjectPtr<UTexture2D> Tex = nullptr;
	int32 Line = 30;
	int32 Space = 8;
	FVector2D Sheet = FVector2D(1, 1);
	struct FGlyph { int32 X, Y, W, H, InkX, Adv; };
	TMap<int32, FGlyph> Glyphs;
	bool Load(const FString& Name);
	float Measure(const FString& S, float Scale) const;
};

enum class EPFAlign : uint8 { Left, Center, Right };

struct FPFMenuItem
{
	FString Label;
	TFunction<FString()> Value;              // optional right-hand value (cycled with left/right)
	TFunction<void(int32)> OnChange;         // -1 / +1
	TFunction<void()> OnSelect;
	bool bEnabled = true;
};

UENUM()
enum class EPFScreen : uint8 { Title, Main, Friendly, Match, Result, Career, Options, PreMatch };

UCLASS()
class PROPERFOOTBALL_API UPFFlow : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	static UPFFlow* Get(const UObject* Ctx);
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	void Begin(UWorld* World);
	void Tick(float Dt, const FPFPadState& Pad);
	void Draw(UCanvas* C, UMaterialInterface* SpriteMat);

	// ---- drawing helpers (also used by the career screens)
	float U = 1;   // UI scale: 1 at 1080p
	void Text(UCanvas* C, const FString& S, float X, float Y, float Scale = 1, FLinearColor Col = FLinearColor::White, EPFAlign Align = EPFAlign::Left, bool bSquad = false);
	void Tex(UCanvas* C, const FString& Path, float X, float Y, float W, float H, FLinearColor Tint = FLinearColor::White);
	void Box(UCanvas* C, float X, float Y, float W, float H, FLinearColor Col);
	UTexture2D* GetTex(const FString& Path);
	void DrawMenu(UCanvas* C, const FString& Title, TArray<FPFMenuItem>& Items, int32 Sel, float X, float Y, float W);
	bool MenuInput(TArray<FPFMenuItem>& Items, int32& Sel, const FPFPadState& Pad);

	void Go(EPFScreen S);
	void PlaySfx(FName Name, float Volume = 1.f);
	void SetMusic(FName Name);   // NAME_None stops it
	UPROPERTY() TMap<FName, TObjectPtr<class USoundBase>> Sounds;
	UPROPERTY() TObjectPtr<class UAudioComponent> Music;
	FName MusicName;
	void StartFriendly();
	void RunCareerTest(int32 Seasons);
	void RunMatchTest(int32 N);
	void RunFeelTest();
	void StartMatch(const FPFMatchSetup& S, TFunction<void(const FPFMatch&)> OnDone);

	EPFScreen Screen = EPFScreen::Title;
	UPROPERTY() TObjectPtr<APFMatchActor> MatchActor;
	UPROPERTY() TMap<FString, TObjectPtr<UTexture2D>> TexCache;
	UPROPERTY() TArray<TObjectPtr<UTexture2D>> Keep;
	FPFFont FontScore, FontSquad;
	FString BgPath;
	float Time = 0;
	int32 Sel = 0;
	bool bPause = false;
	int32 PauseSel = 0;
	int32 FriendlyHome = 0, FriendlyAway = 1, FriendlySide = 0, FriendlyStage = 0, FriendlyLen = 1, CameraOpt = 3;
	int32 SpeedOpt = 1;   // v0.10 GAME SPEED: 0 slow, 1 normal (FPFMatch::NormalSpeed), 2 fast (the v0.9.9 pace)   // v0.9.2: WIDE (owner's pick)
	int32 SizeOpt = 0;   // v0.9.5 PLAYER SIZE; v0.9.6 removed the option (always the v0.9.1 size), kept only so old option files still load
	TFunction<void(const FPFMatch&)> MatchDone;
	FPFMatch* LastMatch = nullptr;
	TWeakObjectPtr<UWorld> WorldRef;
	bool bManageView = false;      // career "manage it": fast dots, real sprites for highlights
	float PreMatchT = 0;
	float ScreenT = 0;          // v0.9.4: seconds on the current screen (the result screen ignores presses for a moment)
	bool bDotsNow = false;      // v0.9.4: the manage view drew dots this frame (no sprite positions for the ratings)
	int32 MusicVol = 7, SfxVol = 8;     // 0..10, saved in Saved/pf_options.txt
	void LoadOptions();
	void SaveOptions();
	void ApplyCamera();
	int32 ShotsLeft = 0, ShotIndex = 0;
	float ShotGap = 3.f, NextShot = 4.f, QuitAt = -1.f;

private:
	void DrawTitle(UCanvas* C);
	void DrawMatchHUD(UCanvas* C, const FPFMatch& M);
	void DrawDots(UCanvas* C, const FPFMatch& M);
	void DrawBackground(UCanvas* C, float Dim = 0.f);
	void PickBackground(const FString& Collection = TEXT(""));
};
