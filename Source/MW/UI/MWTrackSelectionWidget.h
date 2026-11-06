#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Gameplay/Construction/MWTrackSelectionTypes.h"
#include "MWTrackSelectionWidget.generated.h"

class UButton;
class UTextBlock;
class UMWTrackConstructionComponent;
class UMWPlayerConstructionComponent;

// WBP_TrackSelectionTest의 화면을 서버가 복제한 선택 상태와 연결한다.
UCLASS(Abstract)
class MW_API UMWTrackSelectionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void HandleSelectionReply(const FMWTrackSelectionReply& Reply);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_Offer_0;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_Offer_1;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_Offer_2;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_OfferName_0;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_OfferName_1;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_OfferName_2;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_OfferState_0;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_OfferState_1;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_OfferState_2;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_SessionStatus;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_PlayerName_0;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_PlayerName_1;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_PlayerSelection_0;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_PlayerSelection_1;

private:
	// Actor마다 복제 도착 순서가 달라 초기 준비 중에만 상태를 재조회한다.
	void TryBindState();
	void Refresh();
	void ConfirmSlot(int32 SlotIndex);
	FText GetOfferName(int32 OfferId) const;
	UFUNCTION() void HandleStateChanged();
	UFUNCTION() void HandleOffer0();
	UFUNCTION() void HandleOffer1();
	UFUNCTION() void HandleOffer2();

	TWeakObjectPtr<UMWTrackConstructionComponent> BoundConstruction;
	TArray<TWeakObjectPtr<UMWPlayerConstructionComponent>> BoundPlayers;
	FTimerHandle BindRetryTimer;
	FMWTrackSelectionSession DisplayedSession;
	FText RequestError;
	bool bRequestPending = false;
};
