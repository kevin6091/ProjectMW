#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Fonts/SlateFontInfo.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class UFont;
class UTexture2D;

namespace MWGameplayUI
{
	inline constexpr float DesignWidth = 1920.f;
	inline constexpr float DesignHeight = 1080.f;

	// The source frames were captured at 1920 x 1080. Keep that composition
	// within the safe 16:9 area even when the PIE viewport has another aspect.
	MW_API TSharedRef<SWidget> FitDesignCanvas(TSharedRef<SWidget> Content,
		EHorizontalAlignment Horizontal = HAlign_Center, EVerticalAlignment Vertical = VAlign_Center);
}

struct MW_API FMWGameplayUIArt
{
	FMWGameplayUIArt();

	FSlateFontInfo Font(float Size) const;
	const FSlateBrush* LogoBrush() const;
	const FSlateBrush* BoltBrush() const;

private:
	TStrongObjectPtr<UObject> DisplayFont;
	TStrongObjectPtr<UObject> Logo;
	TStrongObjectPtr<UObject> Bolt;
	FSlateBrush LogoImage;
	FSlateBrush BoltImage;
	FSlateRoundedBoxBrush FallbackBolt{FLinearColor(.34f, .39f, .44f), 12.f};
};

class MW_API SMWGameplayPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMWGameplayPanel) {}
		SLATE_ARGUMENT(FVector2D, Size)
		SLATE_ARGUMENT(FLinearColor, Fill)
		SLATE_ARGUMENT(FLinearColor, Border)
		SLATE_ARGUMENT(float, BorderWidth)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);

private:
	FMWGameplayUIArt Art;
	FSlateRoundedBoxBrush Plate{FLinearColor::Transparent};
};

class MW_API SMWGameplaySign : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMWGameplaySign) {}
		SLATE_ARGUMENT(FText, Label)
		SLATE_ARGUMENT(bool, AnimateLoading)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
		FSlateWindowElementList& DrawElements, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;

private:
	EActiveTimerReturnType AdvanceLoading(double CurrentTime, float DeltaTime);

	FMWGameplayUIArt Art;
	FText Label;
	FSlateFontInfo SignFont;
	TOptional<double> LoadingStartedAt;
	float LoadingAngle = 0.f;
	float LoadingScale = 1.f;
	bool bAnimateLoading = false;
};
