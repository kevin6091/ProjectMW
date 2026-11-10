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
            // First visible tip of the unchanged 512px arrow texture is (256,14).
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

// Slate centers a software cursor on the pointer. Move the drawing so the
// arrow tip is the click point.
inline TSharedRef<SWidget> MWMakePointerCursor(UUserWidget* Marker, TAttribute<FVector2D> PixelSize)
{
    const FVector2D Hotspot = MWGetPointerHotspot(Marker);
    return SNew(SBox)
        .WidthOverride_Lambda([PixelSize]() { return FOptionalSize(PixelSize.Get().X); })
        .HeightOverride_Lambda([PixelSize]() { return FOptionalSize(PixelSize.Get().Y); })
        // Slate paints the software cursor root directly. Its parent's arrangement
        // must apply this transform to a child; a transform on the root is ignored.
        [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
        .RenderTransform_Lambda([PixelSize, Hotspot]() -> TOptional<FSlateRenderTransform>
        {
            const FVector2D Size = PixelSize.Get();
            return FSlateRenderTransform(Size * .5f - Hotspot * FMath::Min(Size.X, Size.Y));
        })
            [SNew(SBox).WidthOverride(MWPointerDesignSize).HeightOverride(MWPointerDesignSize)[Marker->TakeWidget()]]];
}
