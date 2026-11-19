#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Gameplay/Construction/MWTrackSelectionTypes.h"
#include "MWTrackSelectionWidget.generated.h"

class UMWRemoteCursorOverlay;
class UMWVehiclePlayerMarkerWidget;
class UBorder;
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
	void PositionOfferLabel(int32 OfferId, const FVector& WorldPosition, bool bHovered);
	void ShowPlacementStatus(const FText& Players, const FText& Status);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_Players;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_SessionStatus;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCanvasPanel> Canvas_Cursors;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Title;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> PlacementStatusSpace;

	// 차량 선택 이름표와 같은 판면을 사용한다. 브러시 색은 기본색, 호버 색은 별도로 편집한다.
	UPROPERTY(EditDefaultsOnly, Category = "Selection|Appearance")
	FSlateBrush OfferPlateBrush;

	UPROPERTY(EditDefaultsOnly, Category = "Selection|Appearance")
	FMargin OfferPlatePadding = FMargin(12.f, 3.f);

	UPROPERTY(EditDefaultsOnly, Category = "Selection|Appearance")
	FLinearColor OfferHoverColor = FLinearColor::FromSRGBColor(FColor(77, 204, 255));

private:
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UBorder>> OfferPlates;

	// Actor마다 복제 도착 순서가 달라 초기 준비 중에만 상태를 재조회한다.
	UBorder* CreateOfferPlate();
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
	UPROPERTY(Transient) TObjectPtr<UMWRemoteCursorOverlay> RemoteCursors;
	UPROPERTY(Transient) TSubclassOf<UMWVehiclePlayerMarkerWidget> PointerClass;
};
