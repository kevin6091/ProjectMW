// 기존 화살표 끝을 클릭 좌표에 맞추는 Slate 커서 보정이다. UI 확대 배율과 구분한다.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"

inline constexpr float MWPointerDesignSize = 176.f;

inline FVector2D MWGetPointerHotspot(UUserWidget* Marker)
{
    FVector2D Hotspot(MWPointerDesignSize * .5f);
    if (UWidget* Arrow = Marker->GetWidgetFromName(TEXT("Image_PointerArrow")))
        if (auto* Slot = Cast<UCanvasPanelSlot>(Arrow->Slot))
        {
            const FVector2D Size = Slot->GetSize();

            const FVector2D Pivot = Arrow->GetRenderTransformPivot() * Size;
            // 원본 512픽셀 화살표에서 실제로 보이는 첫 끝점은 (256,14)다.
            FVector2D Tip = FVector2D(.5f, 14.f / 512.f) * Size - Pivot;

            const FWidgetTransform Transform = Arrow->GetRenderTransform();
            Tip *= Transform.Scale;
            const float Angle = FMath::DegreesToRadians(Transform.Angle);
            const FVector2D Rotated(Tip.X * FMath::Cos(Angle) - Tip.Y * FMath::Sin(Angle),
                                    Tip.X * FMath::Sin(Angle) + Tip.Y * FMath::Cos(Angle));
            Hotspot = Slot->GetPosition() + Pivot + Rotated + Transform.Translation;
        }
    return Hotspot / MWPointerDesignSize;
}

// Slate의 소프트웨어 커서는 포인터 중앙을 기준으로 그리므로 그림을 옮겨
// 화살표 끝이 실제 클릭 좌표에 오도록 맞춘다.
inline TSharedRef<SWidget> MWMakePointerCursor(UUserWidget* Marker, TAttribute<FVector2D> PixelSize)
{
    const FVector2D Hotspot = MWGetPointerHotspot(Marker);
    return SNew(SBox)
        .WidthOverride_Lambda(
            [PixelSize]()
            {
                return FOptionalSize(PixelSize.Get().X);
            })
        .HeightOverride_Lambda(
            [PixelSize]()
            {
                return FOptionalSize(PixelSize.Get().Y);
            })
            // Slate는 소프트웨어 커서 루트를 직접 그리므로 부모의 자식 배치에
            // 보정 변환을 적용한다. 루트 자체의 변환은 커서 그리기에서 반영되지 않는다.
            [SNew(SScaleBox)
                 .Stretch(EStretch::ScaleToFit)
                 .RenderTransform_Lambda(
                     [PixelSize, Hotspot]() -> TOptional<FSlateRenderTransform>
                     {
                         const FVector2D Size = PixelSize.Get();

                         return FSlateRenderTransform(Size * .5f - Hotspot * FMath::Min(Size.X, Size.Y));
                     })[SNew(SBox)
                            .WidthOverride(MWPointerDesignSize)
                            .HeightOverride(MWPointerDesignSize)[Marker->TakeWidget()]]];
}
