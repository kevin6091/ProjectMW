#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "System/MWRoomTypes.h"
#include "MWRaceHUDWidget.generated.h"

class AMWVehiclePawn;
class UImage;
class UMaterialInstanceDynamic;
class UPanelWidget;
class UTextBlock;

// 디자인은 WBP_RaceHUD에 두고 실제 속도와 기존 경기 단계만 전달한다.
UCLASS(Abstract)
class MW_API UMWRaceHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // 개발 맵에서는 경기 단계 없이 계기판을 표시한다.
    void SetSpeedometerOnly(bool bEnabled);

protected:
    virtual void NativePreConstruct() override;

    virtual void NativeConstruct() override;

    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;

    virtual void NativeDestruct() override;

    // 아래 값은 WBP의 Class Defaults에서 편집한다. 차량 성능에는 영향을 주지 않는다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "계기판", meta = (ClampMin = "30", ClampMax = "600"))
    float MaxSpeedKmh = 120.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "계기판")
    float NeedleStartAngle = -135.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "계기판")
    float NeedleEndAngle = 135.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "계기판", meta = (ClampMin = "1"))
    float NeedleResponse = 12.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "계기판")
    FName SpeedRatioParameter = TEXT("SpeedRatio");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "경기 안내")
    TMap<EMWRoomPhase, FText> PhaseLabels;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UWidget> Panel_Speed;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> Image_Dial;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> Image_Needle;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Text_Speed;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UWidget> Panel_Phase;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Text_Phase;

    // Designer에서 눈금 숫자 위젯을 추가·제거해도 자식 개수에 맞춰 상한을 분배한다.
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UPanelWidget> Panel_Ticks;

private:
    void UpdatePhase(TOptional<EMWRoomPhase> Phase);

    void SampleSpeed(const AMWVehiclePawn* Vehicle, double CurrentTime);

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> DialMaterial;

    TWeakObjectPtr<const AMWVehiclePawn> ObservedVehicle;

    TOptional<EMWRoomPhase> DisplayedPhase;

    float TargetSpeedKmh = 0.f;

    float NeedleSpeedKmh = 0.f;

    float DisplayedRatio = -1.f;

    int32 DisplayedSpeedKmh = INDEX_NONE;

    double NextSpeedSampleTime = 0.0;

    bool bSpeedometerOnly = false;
};
