#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/VehicleSelection/MWVehicleUITypes.h"
#include "MWVehiclePlayerMarkerWidget.generated.h"

class UImage;
class UTextBlock;

// 기존 원형·화살표 외형에 현재 참가자 데이터만 반영한다.
UCLASS(Abstract)
class MW_API UMWVehiclePlayerMarkerWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // 참가자별 색·닉네임·로컬 여부로 같은 커서 외형을 갱신한다.
    void SetPresentation(const FMWVehiclePlayerPresentation& Player, bool bShowCursorName = false);

    UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Marker")
    FText SelectingText;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Marker")
    FText ConfirmedText;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Marker")
    FText HoverText;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Marker")
    float SelectingOpacity;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Marker")
    float HoverOpacity;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Marker")
    float ConfirmedOpacity;

protected:

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> Image_Badge;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UImage> Image_PointerArrow;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Text_Player;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Text_State;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> LocalIndicator;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> CursorPlayerName;
};
