#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Gameplay/Construction/MWTrackSelectionTypes.h"
#include "MWTrackSelectionWidget.generated.h"

class UMWRemoteCursorOverlay;
class UMWVehiclePlayerMarkerWidget;
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
	void ShowPlacementStatus(const FText& Status);
	void ShowPlacementEndpoint(const FVector* WorldPosition);
	void PulsePlacementEndpoint(const FVector& WorldPosition);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullRect,
		FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_SessionStatus;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCanvasPanel> Canvas_Cursors;

private:
	static constexpr float SnapContactPulseSeconds = 0.20f;

	// Actor마다 복제 도착 순서가 달라 초기 준비 중에만 상태를 재조회한다.
	void TryBindState();
	void Refresh();
	UFUNCTION() void HandleStateChanged();

	TWeakObjectPtr<UMWTrackConstructionComponent> BoundConstruction;
	TArray<TWeakObjectPtr<UMWPlayerConstructionComponent>> BoundPlayers;
	FTimerHandle BindRetryTimer;
	FMWTrackSelectionSession DisplayedSession;
	FText RequestError;
	bool bRequestPending = false;
	bool bShowingPlacement = false;
	TOptional<FVector> PlacementEndpoint;
	TOptional<FVector> ContactPulse;
	float ContactPulseAge = 0.0f;
	int32 AcknowledgedOffer = INDEX_NONE;
	UPROPERTY(Transient) TObjectPtr<UMWRemoteCursorOverlay> RemoteCursors;
	UPROPERTY(Transient) TSubclassOf<UMWVehiclePlayerMarkerWidget> PointerClass;
};
