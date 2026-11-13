#include "UI/Gameplay/MWRaceSpeedometer.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"

namespace
{
    const FVector2f DesignSize(360.f, 340.f);

    const FVector2f DialCenter(180.f, 166.f);

    constexpr float StartAngleDegrees = 135.f;

    constexpr float SweepAngleDegrees = 270.f;

    constexpr int32 MajorIntervals = 6;

    constexpr int32 MinorIntervals = MajorIntervals * 5;

    // 프로젝트의 게시판 파랑·노랑과 공통 글꼴을 사용하되 계기 면은 어둡게 구분한다.
    const FLinearColor FaceColor = FLinearColor::FromSRGBColor(FColor(13, 25, 38, 235));

    const FLinearColor RimColor = FLinearColor::FromSRGBColor(FColor(126, 174, 227));

    const FLinearColor AccentColor = FLinearColor::FromSRGBColor(FColor(255, 187, 49));

    FVector2f PointOnDial(float Ratio, float Radius)
    {
        const float Radians = FMath::DegreesToRadians(StartAngleDegrees + SweepAngleDegrees * Ratio);
        return DialCenter + FVector2f(FMath::Cos(Radians), FMath::Sin(Radians)) * Radius;
    }
}

void SMWRaceSpeedometer::Construct(const FArguments& Args)
{
    // 눈금 상한은 HUD 설정에서 받는다. 실제 속도 값이나 차량 최고 속도를 제한하지 않는다.
    MaxSpeedKmh = FMath::IsFinite(Args._MaxSpeedKmh) ? FMath::Clamp(Args._MaxSpeedKmh, 30.f, 600.f) : 120.f;
    Dial = FSlateRoundedBoxBrush(FaceColor, 156.f, RimColor.CopyWithNewOpacity(.65f), 2.f);

    for (int32 Index = 0; Index <= MajorIntervals; ++Index)
    {
        TickLabels.Add(FText::AsNumber(FMath::RoundToInt(MaxSpeedKmh * Index / MajorIntervals)));
    }

    SetSpeed(0.f, true);
}

void SMWRaceSpeedometer::SetSpeed(float SpeedKmh, bool bResetNeedle)
{
    TargetSpeedKmh = FMath::IsFinite(SpeedKmh) ? FMath::Max(0.f, SpeedKmh) : 0.f;

    if (bResetNeedle)
    {
        NeedleSpeedKmh = TargetSpeedKmh;
    }

    const int32 RoundedSpeed = FMath::RoundToInt(TargetSpeedKmh);
    if (DisplayedSpeedKmh != RoundedSpeed)
    {
        DisplayedSpeedKmh = RoundedSpeed;
        SpeedLabel = FText::AsNumber(DisplayedSpeedKmh);
    }

    Invalidate(EInvalidateWidgetReason::Paint);
}

void SMWRaceSpeedometer::Tick(const FGeometry& Geometry, double CurrentTime, float DeltaTime)
{
    SLeafWidget::Tick(Geometry, CurrentTime, DeltaTime);

    // 시각 바늘만 부드럽게 따라간다. 프레임별 속도·이동 값을 차량에 돌려쓰지 않는다.
    const float PreviousSpeed = NeedleSpeedKmh;
    NeedleSpeedKmh = FMath::FInterpTo(NeedleSpeedKmh, TargetSpeedKmh, DeltaTime, 12.f);

    if (PreviousSpeed != NeedleSpeedKmh)
    {
        Invalidate(EInvalidateWidgetReason::Paint);
    }
}

FVector2D SMWRaceSpeedometer::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
    return FVector2D(DesignSize);
}

int32 SMWRaceSpeedometer::OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
                                 FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style,
                                 bool bParentEnabled) const
{
    const FLinearColor Tint = Style.GetColorAndOpacityTint();

    const auto Box = [&](FVector2f Position, FVector2f Size, const FSlateBrush& Brush, FLinearColor Color)
    {
        FSlateDrawElement::MakeBox(Elements, Layer,
            Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Position)), &Brush,
            ESlateDrawEffect::NoPixelSnapping, Color * Tint);
    };

    const auto Line = [&](const TArray<FVector2f>& Points, FLinearColor Color, float Width)
    {
        FSlateDrawElement::MakeLines(Elements, Layer + 1, Geometry.ToPaintGeometry(), Points,
            ESlateDrawEffect::None, Color * Tint, true, Width);
    };

    const auto Arc = [&](float Begin, float End, float Radius, FLinearColor Color, float Width)
    {
        // 화면 배율과 무관한 벡터 선으로 그려 작은 텍스처를 확대하지 않는다.
        TArray<FVector2f> Points;
        constexpr int32 Segments = 96;
        Points.Reserve(Segments + 1);
        for (int32 Index = 0; Index <= Segments; ++Index)
        {
            Points.Add(PointOnDial(FMath::Lerp(Begin, End, static_cast<float>(Index) / Segments), Radius));
        }

        FSlateDrawElement::MakeLines(Elements, Layer + 1, Geometry.ToPaintGeometry(), MoveTemp(Points),
            ESlateDrawEffect::None, Color * Tint, true, Width);
    };

    const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
    const auto Text = [&](const FText& Label, FVector2f Center, float Size, FLinearColor Color)
    {
        const FSlateFontInfo Font = Art.Font(Size);
        const FVector2f Extent(FontMeasure->Measure(Label, Font));
        FSlateDrawElement::MakeText(Elements, Layer + 2,
            Geometry.ToPaintGeometry(Extent, FSlateLayoutTransform(Center - Extent * .5f)),
            Label, Font, ESlateDrawEffect::None, Color * Tint);
    };

    Box(FVector2f(18.f, 4.f), FVector2f(324.f, 324.f), Dial, FaceColor);
    Arc(0.f, 1.f, 146.f, RimColor.CopyWithNewOpacity(.3f), 2.f);
    Arc(0.f, 1.f, 137.f, FLinearColor(1.f, 1.f, 1.f, .12f), 5.f);

    const float SpeedRatio = FMath::Clamp(NeedleSpeedKmh / MaxSpeedKmh, 0.f, 1.f);
    if (SpeedRatio > 0.f)
    {
        Arc(0.f, SpeedRatio, 137.f, AccentColor, 5.f);
    }

    for (int32 Index = 0; Index <= MinorIntervals; ++Index)
    {
        const float Ratio = static_cast<float>(Index) / MinorIntervals;
        const bool bMajor = Index % 5 == 0;
        Line({PointOnDial(Ratio, bMajor ? 120.f : 127.f), PointOnDial(Ratio, 132.f)},
            bMajor ? FLinearColor::White : RimColor.CopyWithNewOpacity(.65f), bMajor ? 2.f : 1.f);
    }

    for (int32 Index = 0; Index < TickLabels.Num(); ++Index)
    {
        Text(TickLabels[Index], PointOnDial(static_cast<float>(Index) / MajorIntervals, 104.f),
             16.f, FLinearColor::White);
    }

    // 바늘 끝과 허브를 먼저 그린 뒤 속도 숫자를 아래에 따로 배치한다.
    Line({DialCenter, PointOnDial(SpeedRatio, 118.f)}, AccentColor.CopyWithNewOpacity(.15f), 8.f);
    Line({DialCenter, PointOnDial(SpeedRatio, 118.f)}, AccentColor, 2.5f);
    Box(DialCenter - FVector2f(6.f, 6.f), FVector2f(12.f, 12.f), Hub, RimColor);

    Text(SpeedLabel, FVector2f(180.f, 237.f), 54.f, FLinearColor::White);
    Text(NSLOCTEXT("MWRaceHUD", "SpeedUnit", "km/h"), FVector2f(180.f, 285.f), 16.f, RimColor);

    return Layer + 2;
}
