#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Gameplay/Construction/MWTrackSelectionTypes.h"
#include "MWTrackSelectionWidget.generated.h"

class UTextBlock;
class UCanvasPanel;
class UMWTrackConstructionComponent;
class UMWPlayerConstructionComponent;

// 월드의 트랙은 Board가 표시하고, WBP는 현황 안내와 원격 커서를 표시한다.
UCLASS(Abstract)
class MW_API UMWTrackSelectionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void HandleSelectionReply(const FMWTrackSelectionReply& Reply);
	void ConfirmOffer(int32 OfferId);
	void ShowPlacementStatus(const FText& Players, const FText& Status);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_Players;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_SessionStatus;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCanvasPanel> Canvas_Cursors;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Title;

private:
	// Actor마다 복제 도착 순서가 달라 초기 준비 중에만 상태를 재조회한다.
	void TryBindState();
	void Refresh();
	FText GetOfferName(int32 OfferId) const;
	UFUNCTION() void HandleStateChanged();

	TWeakObjectPtr<UMWTrackConstructionComponent> BoundConstruction;
	TArray<TWeakObjectPtr<UMWPlayerConstructionComponent>> BoundPlayers;
	FTimerHandle BindRetryTimer;
	FMWTrackSelectionSession DisplayedSession;
	FText RequestError;
	bool bRequestPending = false;
	bool bShowingPlacement = false;
	int32 AcknowledgedOffer = INDEX_NONE;
	UPROPERTY(Transient) TMap<int32, TObjectPtr<UTextBlock>> RemoteCursorLabels;
	TMap<int32, FVector> SmoothedCursorPositions;
};
