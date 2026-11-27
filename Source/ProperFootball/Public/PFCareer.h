// Proper Football - career mode screens (endless English pyramid). The model and simulation live in PFCareerData.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PFCareerData.h"
#include "PFCareer.generated.h"

class UCanvas;
class UMaterialInterface;
class UPFFlow;
struct FPFPadState;

UENUM()
enum class EPFCScreen : uint8 { Entry, Appointed, Hub, MatchChoice, Squad, PlayerCard, Tactics, Table, Fixtures, Transfers, TransferTarget, Academy, Scouting, Prospects, Club, Offers, Message, History };

UCLASS()
class PROPERFOOTBALL_API UPFCareer : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	static UPFCareer* Get(const UObject* Ctx);
	void Enter();
	void Tick(float Dt, const FPFPadState& Pad);
	void Draw(UCanvas* C, UMaterialInterface* SpriteMat);
	// called by the flow when a career match ends
	void OnMatchDone(const FPFMatch& M, bool bYouth);

	FPFCareerSim Sim;
	bool bLoaded = false;
	EPFCScreen Screen = EPFCScreen::Entry;
	TArray<EPFCScreen> Back;
	int32 Sel = 0, Scroll = 0;
	int32 TableDiv = -1;
	int32 SelPerson = -1;
	int32 TransferDiv = 2, TransferRole = 0, TransferSort = 0;
	int32 BidFeeStep = 0, BidWageStep = 0;
	int32 ScoutSel = 0, RegionSel = 0;
	FString Message;
	EPFCScreen AfterMessage = EPFCScreen::Hub;
	int32 PendingFixture = -1;
	bool bManage = false;
	bool bYouthMatch = false;
	int32 HalfLenSetting = 1;
	int32 ListCount = 0;   // rows on the current list screen (input clamps to it)
	bool bConfirmNew = false;   // v0.9.4: NEW CAREER pressed once over an existing save

private:
	UPFFlow* F() const;
	void Go(EPFCScreen S, bool bPush = true);
	void Pop();
	void Msg(const FString& Text, EPFCScreen Then = EPFCScreen::Hub);
	void PlayUserMatch(int32 Mode);   // 0 play, 1 manage (watch), 2 instant
	void AdvanceDay();
	void DrawHeader(UCanvas* C, const FString& Title);
	void DrawHub(UCanvas* C);
	void DrawList(UCanvas* C, const FString& Title, const TArray<FString>& Head, const TArray<float>& Cols, const TArray<TArray<FString>>& Rows, const TArray<FLinearColor>& RowCol, float Top = 230.f);
	TArray<int32> TransferList() const;
	TArray<int32> SquadList(bool bAcademy) const;
	FString Money(int64 V) const;
	FString Stars(int32 Ovr) const;
	void DrawPersonCard(UCanvas* C, const FPFPerson& P, bool bHidden, float X, float Y);
	TArray<FString> HubItems() const;
};
