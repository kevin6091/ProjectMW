#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/VehicleSelection/MWVehicleUITypes.h"
#include "MWVehiclePlayerEntryWidget.generated.h"

class UImage;
class UTextBlock;
class UTexture2D;

// 하단 참가자 칸의 선택 완료와 플레이어 색 테두리를 표시한다.
UCLASS(Abstract)
class MW_API UMWVehiclePlayerEntryWidget : public UUserWidget
{
    GENERATED_BODY()

public:

    // 서버 상태로 만든 참가자 정보를 표시하고 확정 슬롯 테두리에 플레이어 색을 반영한다.
    UFUNCTION(BlueprintCallable, Category = "MW|Vehicle UI")
    void SetPresentation(const FMWVehiclePlayerPresentation& Player);

    UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Display")
    FLinearColor NeutralDisplayTint;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Display")
    FLinearColor ConfirmedDisplayTint;

protected:

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Text_PlayerName;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Text_VehicleName;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Text_SelectionState;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Text_PlayerMarker;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> Image_Vehicle;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UImage> Image_DisplayBackground;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UImage> Image_IdentityBadge;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> LocalIndicator;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UImage> LocalFrame;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> SelectionDetails;
};
