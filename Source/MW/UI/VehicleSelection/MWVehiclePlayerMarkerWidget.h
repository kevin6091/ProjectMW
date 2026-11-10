#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/VehicleSelection/MWVehicleUITypes.h"
#include "MWVehiclePlayerMarkerWidget.generated.h"

class UImage;
class UTextBlock;

UCLASS(Abstract)
class MW_API UMWVehiclePlayerMarkerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetPresentation(const FMWVehiclePlayerPresentation& Player);
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Marker") FText SelectingText;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Marker") FText ConfirmedText;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Marker") FText HoverText;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Marker") float SelectingOpacity;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Marker") float HoverOpacity;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Marker") float ConfirmedOpacity;

protected:
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> Image_Badge;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> Image_PointerArrow;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_Player;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_State;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> LocalIndicator;
};
