#pragma once

#include "CoreMinimal.h"
#include "UI/Gameplay/MWGameplaySign.h"
#include "Widgets/SLeafWidget.h"

// 속도 표시 전용 계기판. 차량의 이동·제한 속도·서버 판정에는 관여하지 않는다.
class SMWRaceSpeedometer : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SMWRaceSpeedometer)
        : _MaxSpeedKmh(120.f)
    {
    }

        SLATE_ARGUMENT(float, MaxSpeedKmh)

    SLATE_END_ARGS()

    void Construct(const FArguments& Args);

    // 숫자는 실제 속도, 바늘은 표시용 보간 값을 사용한다.
    void SetSpeed(float SpeedKmh, bool bResetNeedle);

    virtual void Tick(const FGeometry& Geometry, double CurrentTime, float DeltaTime) override;

    virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
                          FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style,
                          bool bParentEnabled) const override;

private:
    FMWGameplayUIArt Art;

    FSlateRoundedBoxBrush Dial{FLinearColor::Transparent};

    FSlateRoundedBoxBrush Hub{FLinearColor::White};

    TArray<FText> TickLabels;

    FText SpeedLabel;

    float MaxSpeedKmh = 120.f;

    float TargetSpeedKmh = 0.f;

    float NeedleSpeedKmh = 0.f;

    int32 DisplayedSpeedKmh = INDEX_NONE;
};
