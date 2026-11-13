#pragma once

#include "CoreMinimal.h"
#include "UI/Gameplay/MWGameplaySign.h"
#include "Widgets/SCompoundWidget.h"

class APlayerController;
class AMWVehiclePawn;
class STextBlock;

// 소유 차량의 주행 정보만 표시한다. 이동 값이나 서버 경기 판정을 변경하지 않는다.
class SMWRaceHUD : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SMWRaceHUD)
    {
    }

        SLATE_ARGUMENT(APlayerController*, Controller)
        SLATE_ARGUMENT(bool, bItemPickupTest)

    SLATE_END_ARGS()

    void Construct(const FArguments& Args);

    virtual void Tick(const FGeometry& Geometry, double CurrentTime, float DeltaTime) override;

private:
    // 위치 차분 대신 기존 이동 속도를 읽어 텔레포트·서버 위치 보정을 속도로 오인하지 않는다.
    void UpdateSpeed(const AMWVehiclePawn* Vehicle, double CurrentTime);

    TWeakObjectPtr<APlayerController> Controller;

    TWeakObjectPtr<const AMWVehiclePawn> DisplayedVehicle;

    TSharedPtr<SWidget> SpeedPanel;

    TSharedPtr<STextBlock> SpeedText;

    FMWGameplayUIArt Art;

    int32 DisplayedSpeedKmh = INDEX_NONE;

    double NextSpeedUpdateTime = 0.0;

    bool bItemPickupTest = false;
};
