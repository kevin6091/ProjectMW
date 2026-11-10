#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/VehicleSelection/MWVehicleUITypes.h"
#include "MWVehiclePlayerEntryWidget.generated.h"

class UImage;
class UTextBlock;
class UTexture2D;

UCLASS(Abstract)
class MW_API UMWVehiclePlayerEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "MW|Vehicle UI")
	void SetPresentation(const FMWVehiclePlayerPresentation& Player);
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Display") FLinearColor NeutralDisplayTint;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Display") FLinearColor ConfirmedDisplayTint;

protected:
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_PlayerName;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_VehicleName;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_SelectionState;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_PlayerMarker;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> Image_Vehicle;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> Image_DisplayBackground;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> Image_IdentityBadge;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> LocalIndicator;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> LocalFrame;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> SelectionDetails;
};
