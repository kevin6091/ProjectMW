#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Types/SlateEnums.h"
#include "Layout/Margin.h"
#include "UI/MWVehicleUITypes.h"
#include "MWVehicleSelectionWidget.generated.h"

class AMWLobbyGameState;
class AMWPlayerState;
class UButton;
class UCanvasPanel;
class UGameViewportClient;
class SWidget;
class SWindow;
class UHorizontalBox;
class UTextBlock;
class UTexture2D;
class UUniformGridPanel;
class UWrapBox;
class UMWVehicleCardWidget;
class UMWVehiclePlayerEntryWidget;
class UMWVehiclePlayerMarkerWidget;

// Art-only development samples. No VehicleId, Pawn or gameplay registration.
USTRUCT(BlueprintType)
struct FMWVehiclePreviewCard
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UTexture2D> Thumbnail;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bUnavailable = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText UnavailableReason;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMWVehicleSelectionRequested, FGameplayTag, VehicleAssetID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMWVehicleSelectionBackRequested);

UCLASS(Abstract)
class MW_API UMWVehicleSelectionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "MW|Vehicle UI") void RefreshVehicles();
	// Releases a failed/local request; this does not establish server confirmation.
	UFUNCTION(BlueprintCallable, Category = "MW|Vehicle UI") void FinishSelectionRequest(const FText& Message);
	UFUNCTION(BlueprintPure, Category = "MW|Vehicle UI") int32 GetVisibleVehicleCount() const { return Cards.Num(); }
	UFUNCTION(BlueprintPure, Category = "MW|Vehicle UI") FGameplayTag GetSelectedVehicleAssetId() const;
	UFUNCTION(BlueprintPure, Category = "MW|Vehicle UI") FText GetSelectedDisplayName() const;
	UFUNCTION(BlueprintPure, Category = "MW|Vehicle UI") bool IsPreviewLocked() const;
	UFUNCTION(BlueprintCallable, Category = "MW|Vehicle UI|Preview") void SelectPreviewScenario(int32 Index);
	UFUNCTION(BlueprintCallable, Category = "MW|Vehicle UI|Preview") void SelectPreviewLocalSlot(int32 SlotIndex);
	UFUNCTION(BlueprintCallable, Category = "MW|Vehicle UI|Preview") void SetPreviewControlsVisible(bool bVisible);
	void SetWorldPreviewMarkerScales(float PinnedScale, float CardScale);
	UFUNCTION(BlueprintPure, Category = "MW|Vehicle UI|Preview") int32 GetActivePreviewScenarioIndex() const { return ActivePreviewScenarioIndex; }
	UFUNCTION(BlueprintPure, Category = "MW|Vehicle UI") int32 GetDisplayedSlotCount() const { return ParticipantViews.Num(); }
	UFUNCTION(BlueprintPure, Category = "MW|Vehicle UI") TArray<FMWVehiclePlayerPresentation> GetParticipantPresentation() const { return ParticipantViews; }
	UFUNCTION(BlueprintCallable, Category = "MW|Vehicle UI") void RefreshPlayers();

	UPROPERTY(BlueprintAssignable, Category = "MW|Vehicle UI") FMWVehicleSelectionRequested OnSelectionRequested;
	UPROPERTY(BlueprintAssignable, Category = "MW|Vehicle UI") FMWVehicleSelectionBackRequested OnBackRequested;

	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Data") FGameplayTag VehicleAssetRoot;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Data")
	TMap<FGameplayTag, TSoftObjectPtr<UTexture2D>> VehicleThumbnails;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Designer") int32 ColumnCount = 0;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Designer") TSubclassOf<UMWVehicleCardWidget> CardWidgetClass;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Designer") TSubclassOf<UMWVehiclePlayerEntryWidget> PlayerEntryWidgetClass;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Designer") TEnumAsByte<EHorizontalAlignment> CardHorizontalAlignment = HAlign_Fill;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Designer") TEnumAsByte<EVerticalAlignment> CardVerticalAlignment = VAlign_Fill;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Designer") FMargin PlayerEntryPadding;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Designer") TArray<FLinearColor> ParticipantTints;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Designer") FLinearColor FallbackParticipantTint;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Designer") FLinearColor WaitingSlotTint;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Preview") bool bPreviewOnly = false;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Preview") bool bShowPreviewControls = true;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Preview") TArray<FMWVehiclePreviewCard> PreviewCards;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Preview") TArray<FMWVehiclePreviewScenario> PreviewScenarios;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Preview") int32 DefaultPreviewScenarioIndex = 0;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Preview") FText PreviewPlayerSelectingText;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Preview") FText PreviewPlayerLockedText;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Text") FText EmptyCatalogText;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Text") FText PreviewText;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Text") FText PreviewLockedText;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Text") FText SelectionCompleteText;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Text") FText WaitingForPlayersText;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Text") FText ConfirmVehicleText;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Text") FText MissingServerContractText;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Text") FText ChooseVehicleText;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Text") FText RequestPendingText;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Text") FText InvalidVehicleText;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Text") FText RegisteredCountFormat;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Text") FText WaitingPlayerText;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Text") FText UnselectedVehicleText;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Text") FText MissingServerSelectionText;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Text") FText ParticipantMarkerFormat;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Text") FText PreviewLocalSlotFormat;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Text") FText PreviewCapacityFormat;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCanvasPanel> CursorLayer;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UMWVehiclePlayerMarkerWidget> LocalPlayerCursor;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UUniformGridPanel> Grid_Vehicles;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWrapBox> Wrap_Vehicles;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UHorizontalBox> Box_Players;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_Confirm;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_Reset;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_Back;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_Refresh;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_Status;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_SelectionDetail;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> Legend_Cancel;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_Error;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_RegisteredCount;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> PreviewNotice;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_NextPreviewScenario;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_NextPreviewLocalPlayer;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_PreviewCapacity;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_PreviewScenario;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_PreviewLocalPlayer;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> PreviewControls;

private:
	UPROPERTY(Transient) TArray<TObjectPtr<UMWVehicleCardWidget>> Cards;
	UPROPERTY(Transient) TArray<TObjectPtr<UMWVehiclePlayerEntryWidget>> PlayerEntries;
	UPROPERTY(Transient) TArray<FMWVehiclePlayerPresentation> ParticipantViews;
	UPROPERTY(Transient) FMWVehiclePreviewScenario ActivePreview;
	TWeakObjectPtr<UMWVehicleCardWidget> SelectedCard;
	TWeakObjectPtr<AMWLobbyGameState> BoundGameState;
	// UI layout only: retain client-side seats while the actual PlayerArray changes.
	TMap<TWeakObjectPtr<AMWPlayerState>, int32> PlayerSlots;
	int32 ActivePreviewScenarioIndex = INDEX_NONE;
	bool bRequestPending = false;
	void ClearCards();
	void AddCard(UMWVehicleCardWidget* Card);
	void ApplyCardMarkerScale(UMWVehicleCardWidget* Card) const;
	void RefreshControls();
	bool IsSelectionLocked() const;
	bool AreAllPreviewParticipantsConfirmed() const;
	void RefreshPreviewControls();
	void RefreshPreviewSelection();
	void BuildPreviewParticipants();
	void BuildLobbyParticipants(AMWLobbyGameState* State);
	void RenderParticipants();
	void RefreshCardPointers();
	void RefreshLocalCursor();
	void UpdateLocalCursor(const FPointerEvent& MouseEvent);
	void HideLocalCursor();
	void BindCursorWindow();
	void RegisterSoftwareCursor();
	void ReleaseSoftwareCursor();
	FVector2D GetSoftwareCursorSize() const;
	UPROPERTY(Transient) TObjectPtr<UMWVehiclePlayerMarkerWidget> SoftwareCursor;
	TWeakObjectPtr<UGameViewportClient> CursorViewport;
	TSharedPtr<SWidget> SoftwareCursorSlate;
	TSharedPtr<SWidget> PreviousSoftwareCursor;
	bool bPreviouslyUsedSoftwareCursors = false;
	bool CancelLocalSelection();
	TWeakPtr<SWindow> CursorWindow;
	FDelegateHandle WindowDeactivatedHandle;
	bool bCursorInUI = false;
	// Local presentation only; the existing SelectedCard/Preview data owns the choice.
	bool bLocalMarkerPinned = false;
	float CardMarkerScale = 1.f;
	bool bHasPointerPosition = false;
	FVector2D LastPointerPosition = FVector2D::ZeroVector;
	FMWVehiclePreviewParticipant* FindLocalPreviewParticipant();
	const FMWVehiclePreviewParticipant* FindLocalPreviewParticipant() const;
	FMWVehiclePlayerPresentation MakePreviewPresentation(const FMWVehiclePreviewParticipant& Participant) const;
	FMWVehiclePlayerPresentation MakeWaitingPresentation() const;
	FLinearColor GetParticipantTint(int32 SlotIndex) const;
	UFUNCTION() void HandleNextPreviewScenario();
	UFUNCTION() void HandleNextPreviewLocalPlayer();
	UFUNCTION() void HandleCardChosen(UMWVehicleCardWidget* Card);
	UFUNCTION() void HandleCardHover(UMWVehicleCardWidget* Card, bool bHovered);
	UFUNCTION() void HandleConfirm();
	UFUNCTION() void HandleReset();
	UFUNCTION() void HandleBack();
};
