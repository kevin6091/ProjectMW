#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "UI/MWVehicleUITypes.h"
#include "MWVehicleCardWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UTexture2D;
class UMWVehicleDefinitionData;
class UMWVehicleCardWidget;
class UMWVehiclePlayerMarkerWidget;
class UUniformGridPanel;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMWVehicleCardChosen, UMWVehicleCardWidget*, Card);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMWVehicleCardHoverChanged, UMWVehicleCardWidget*, Card, bool, bHovered);

UCLASS(Abstract)
class MW_API UMWVehicleCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetVehicle(FGameplayTag AssetId, UMWVehicleDefinitionData* Definition,
		TSoftObjectPtr<UTexture2D> Thumbnail, bool bAvailable, const FText& UnavailableReason);
	void SetPreview(const FText& Name, TSoftObjectPtr<UTexture2D> Thumbnail,
		bool bAvailable, const FText& UnavailableReason);
	void SetSelected(bool bSelected);
	void SetInteractionLocked(bool bLocked);
	void SetParticipants(const TArray<FMWVehiclePlayerPresentation>& Participants);
	FGameplayTag GetVehicleAssetId() const { return VehicleAssetID; }
	const FText& GetDisplayName() const { return DisplayName; }
	TSoftObjectPtr<UTexture2D> GetThumbnail() const { return Thumbnail; }
	bool IsAvailable() const { return bAvailable; }
	bool IsSelected() const { return bSelected; }

	UPROPERTY(BlueprintAssignable, Category = "MW|Vehicle UI") FMWVehicleCardChosen OnChosen;
	UPROPERTY(BlueprintAssignable, Category = "MW|Vehicle UI") FMWVehicleCardHoverChanged OnHoverChanged;

	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Style") FLinearColor NormalTint;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Style") FLinearColor HoverTint;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Style") FLinearColor SelectedTint;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Style") FLinearColor UnavailableTint;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Style") FLinearColor FrameTint;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Style") FLinearColor PressedFrameTint;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Style", meta = (ClampMin = "0", ClampMax = "1"))
	float UnavailableImageOpacity = 0;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Markers") TSubclassOf<UMWVehiclePlayerMarkerWidget> MarkerWidgetClass;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Markers") bool bShowStandaloneSelectionMarker = true;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Markers") int32 MarkerColumnCount = 0;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_Vehicle;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> Image_Frame;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> Image_Outline;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> Image_Vehicle;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_Name;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_Unavailable;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> SelectionMarker;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> MissingImageNotice;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UUniformGridPanel> Grid_PlayerMarkers;

private:
	UPROPERTY(Transient) FGameplayTag VehicleAssetID;
	UPROPERTY(Transient) FText DisplayName;
	UPROPERTY(Transient) FText UnavailableReason;
	UPROPERTY(Transient) TSoftObjectPtr<UTexture2D> Thumbnail;
	UPROPERTY(Transient) TArray<TObjectPtr<UMWVehiclePlayerMarkerWidget>> PlayerMarkers;
	FLinearColor LocalParticipantTint;
	bool bHasLocalParticipant = false;
	bool bAvailable = false;
	bool bSelected = false;
	bool bHovered = false;
	bool bPressed = false;
	bool bInteractionLocked = false;
	void Refresh();
	UFUNCTION() void HandleChosen();
	UFUNCTION() void HandleHovered();
	UFUNCTION() void HandleUnhovered();
	UFUNCTION() void HandlePressed();
	UFUNCTION() void HandleReleased();
};
