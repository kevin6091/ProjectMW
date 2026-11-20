#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "UI/VehicleSelection/MWVehicleUITypes.h"
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

// 카드 표시와 선택 의도 알림을 담당한다. 실제 차량 확정은 서버 상태를 따른다.
UCLASS(Abstract)
class MW_API UMWVehicleCardWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // 실제 차량 정의와 카탈로그 ID를 카드 표시 모델에 연결한다.
    void SetVehicle(FGameplayTag AssetId, UMWVehicleDefinitionData* Definition, TSoftObjectPtr<UTexture2D> Thumbnail,
                    bool bAvailable, const FText& UnavailableReason);

    // 외형 검토용 이름·이미지를 사용하며 실제 게임 차량으로 등록하지 않는다.
    void SetPreview(const FText& Name, TSoftObjectPtr<UTexture2D> Thumbnail, bool bAvailable,
                    const FText& UnavailableReason);

    // 화면에서 고른 카드 표시만 바꾸고 실제 서버 확정 값은 건드리지 않는다.
    void SetSelected(bool bSelected);

    // 대기·완료 상태에서 입력만 잠그고 카드 내용은 읽을 수 있게 둔다.
    void SetInteractionLocked(bool bLocked);

    // 기존 카드의 참가자 표시 데이터를 갱신한다. 차량 ID나 서버 승인은 변경하지 않는다.
    void SetParticipants(const TArray<FMWVehiclePlayerPresentation>& Participants);

    FGameplayTag GetVehicleAssetId() const
    {
        return VehicleAssetID;
    }

    const FText& GetDisplayName() const
    {
        return DisplayName;
    }

    TSoftObjectPtr<UTexture2D> GetThumbnail() const
    {
        return Thumbnail;
    }

    bool IsAvailable() const
    {
        return bAvailable;
    }

    bool IsSelected() const
    {
        return bSelected;
    }

    UPROPERTY(BlueprintAssignable, Category = "MW|Vehicle UI")
    FMWVehicleCardChosen OnChosen;

    UPROPERTY(BlueprintAssignable, Category = "MW|Vehicle UI")
    FMWVehicleCardHoverChanged OnHoverChanged;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Style")
    FLinearColor NormalTint;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Style")
    FLinearColor HoverTint;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Style")
    FLinearColor SelectedTint;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Style")
    FLinearColor UnavailableTint;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Style")
    FLinearColor FrameTint;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Style")
    FLinearColor PressedFrameTint;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Style", meta = (ClampMin = "0", ClampMax = "1"))
    float UnavailableImageOpacity = 0;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Markers")
    TSubclassOf<UMWVehiclePlayerMarkerWidget> MarkerWidgetClass;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Markers")
    bool bShowStandaloneSelectionMarker = true;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Markers")
    int32 MarkerColumnCount = 0;

protected:
    // 기존 카드 버튼의 클릭·호버·누름 이벤트를 한 번 연결하고 현재 외형을 반영한다.
    virtual void NativeConstruct() override;

    // 이 카드가 연결한 버튼 이벤트와 표시 참조를 해제한다.
    virtual void NativeDestruct() override;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> Button_Vehicle;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> Image_Frame;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> Image_Outline;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> Image_Vehicle;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Text_Name;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Text_Unavailable;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UWidget> SelectionMarker;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UWidget> MissingImageNotice;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UUniformGridPanel> Grid_PlayerMarkers;

private:

    UPROPERTY(Transient)
    FGameplayTag VehicleAssetID;

    UPROPERTY(Transient)
    FText DisplayName;

    UPROPERTY(Transient)
    FText UnavailableReason;

    UPROPERTY(Transient)
    TSoftObjectPtr<UTexture2D> Thumbnail;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMWVehiclePlayerMarkerWidget>> PlayerMarkers;

    FLinearColor LocalParticipantTint;

    bool bHasLocalParticipant = false;

    bool bAvailable = false;

    bool bSelected = false;

    bool bHovered = false;

    bool bPressed = false;

    bool bInteractionLocked = false;

    // 실제 선택·잠금·사용 가능 상태에 맞춰 카드 외형과 입력을 함께 갱신한다.
    void Refresh();

    // 사용 가능한 카드의 선택 의도를 위젯 소유자에게 알린다.
    UFUNCTION()
    void HandleChosen();

    // 호버 상태를 갱신하고 기존 선택 화면에 해당 카드의 포커스 의도를 전달한다.
    UFUNCTION()
    void HandleHovered();

    // 호버가 끝난 카드 표시를 되돌려 이전 강조가 남지 않게 한다.
    UFUNCTION()
    void HandleUnhovered();

    // 누르는 동안의 표시 상태를 갱신하고 실제 실행은 클릭 처리에 맡긴다.
    UFUNCTION()
    void HandlePressed();

    // 누름 표시를 해제하고 현재 선택·호버 기준으로 외형을 갱신한다.
    UFUNCTION()
    void HandleReleased();
};
