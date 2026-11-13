#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MWVehicleSelectionPreviewActor.generated.h"

class APlayerController;
class UWidgetComponent;
class UMWVehicleSelectionWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMWVehiclePreviewClosed);

// PIE 개발용 표시 호스트다. 경기 진행 판정이나 Pawn 생성에는 관여하지 않는다.
UCLASS()
class MW_API AMWVehicleSelectionPreviewActor : public AActor
{
    GENERATED_BODY()

public:
    AMWVehicleSelectionPreviewActor();

    UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Preview")
    TSubclassOf<UMWVehicleSelectionWidget> PreviewWidgetClass;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MW|Vehicle UI|Preview")
    TObjectPtr<UWidgetComponent> PreviewSurface;

    UPROPERTY(EditAnywhere, Category = "MW|Vehicle UI|Preview")
    bool bOpenOnBeginPlay = false;

    UPROPERTY(EditInstanceOnly, Category = "MW|Vehicle UI|Preview")
    TObjectPtr<AActor> PreviewViewTarget;

    UPROPERTY(BlueprintAssignable, Category = "MW|Vehicle UI|Preview")
    FMWVehiclePreviewClosed OnPreviewClosed;

    // 기존 개발 표시 위젯을 준비해 월드 표면에 연결한다. Pawn이나 서버 경기를 만들지 않는다.
    UFUNCTION(BlueprintCallable, Category = "MW|Vehicle UI|Preview")
    bool OpenPreview();

    // 위젯·입력 표시를 정리해 개발 화면을 닫은 뒤 참조가 남지 않게 한다.
    UFUNCTION(BlueprintCallable, Category = "MW|Vehicle UI|Preview")
    void ClosePreview();

protected:
    // 설정된 개발 표시 시작 정책을 따르고 기존 차량 위젯의 월드 표면을 준비한다.
    virtual void BeginPlay() override;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

    UPROPERTY(Transient)
    TObjectPtr<UMWVehicleSelectionWidget> PreviewWidget;

    TWeakObjectPtr<APlayerController> PreviewController;

    TWeakObjectPtr<AActor> PreviousViewTarget;

    bool bPreviousCursorVisible = false;
};
