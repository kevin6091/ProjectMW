#include "UI/Gameplay/MWGameplaySign.h"

#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Styling/CoreStyle.h"
#include "UI/Gameplay/MWGameplayHUDSubsystem.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"

namespace
{
	constexpr float DesignWidth = 1920.f;
	constexpr float DesignHeight = 1080.f;
}

TSharedRef<SWidget> MWGameplayUI::FitDesignCanvas(TSharedRef<SWidget> Content,
	EHorizontalAlignment Horizontal, EVerticalAlignment Vertical)
{
	return SNew(SScaleBox).Stretch(EStretch::ScaleToFit).HAlign(Horizontal).VAlign(Vertical)
	[
		SNew(SBox).WidthOverride(DesignWidth).HeightOverride(DesignHeight)[Content]
	];
}

FMWGameplayUIArt::FMWGameplayUIArt()
{
	const auto* Settings = GetDefault<UMWGameplayHUDSubsystem>();
	DisplayFont.Reset(Settings->DisplayFont.LoadSynchronous());
	Logo.Reset(Settings->BoardLogo.LoadSynchronous());
	Bolt.Reset(Settings->BoardBolt.LoadSynchronous());
	LogoImage.SetResourceObject(Logo.Get());
	BoltImage.SetResourceObject(Bolt.Get());
	LogoImage.DrawAs = ESlateBrushDrawType::Image;
	BoltImage.DrawAs = ESlateBrushDrawType::Image;
}

FSlateFontInfo FMWGameplayUIArt::Font(float Size) const
{
	return DisplayFont ? FSlateFontInfo(DisplayFont.Get(), Size)
		: FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), Size);
}

const FSlateBrush* FMWGameplayUIArt::LogoBrush() const
{
	return Logo ? &LogoImage : FCoreStyle::Get().GetBrush(TEXT("NoBrush"));
}

const FSlateBrush* FMWGameplayUIArt::BoltBrush() const
{
	return Bolt ? &BoltImage : &FallbackBolt;
}

void SMWGameplayPanel::Construct(const FArguments& Args)
{
	Plate = FSlateRoundedBoxBrush(Args._Fill, 24.f, Args._Border, Args._BorderWidth);
	ChildSlot
	[
		SNew(SBox).WidthOverride(Args._Size.X).HeightOverride(Args._Size.Y)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SBorder).BorderImage(&Plate).Padding(0.f)
				[
					Args._Content.Widget
				]
			]
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(18.f)
			[SNew(SBox).WidthOverride(22.f).HeightOverride(22.f)[SNew(SImage).Image(Art.BoltBrush())]]
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(18.f)
			[SNew(SBox).WidthOverride(22.f).HeightOverride(22.f)[SNew(SImage).Image(Art.BoltBrush())]]
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(18.f)
			[SNew(SBox).WidthOverride(22.f).HeightOverride(22.f)[SNew(SImage).Image(Art.BoltBrush())]]
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(18.f)
			[SNew(SBox).WidthOverride(22.f).HeightOverride(22.f)[SNew(SImage).Image(Art.BoltBrush())]]
		]
	];
}

void SMWGameplaySign::Construct(const FArguments& Args)
{
	Label = Args._Label;
	bAnimateLoading = Args._AnimateLoading;
	SignFont = Art.Font(bAnimateLoading ? 74.f : 47.f);
	const float Size = bAnimateLoading ? 568.f : 385.f;
	ChildSlot[SNew(SBox).WidthOverride(Size).HeightOverride(Size)];

	if (bAnimateLoading)
		RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SMWGameplaySign::AdvanceLoading));
}

EActiveTimerReturnType SMWGameplaySign::AdvanceLoading(double CurrentTime, float)
{
	if (!GetVisibility().IsVisible()) return EActiveTimerReturnType::Continue;
	if (!LoadingStartedAt.IsSet()) LoadingStartedAt = CurrentTime;

	// The recorded sign holds, turns edge-on to its blank back, then settles.
	const float Cycle = FMath::Fmod(static_cast<float>(CurrentTime - LoadingStartedAt.GetValue()), 1.85f);
	const float Turn = FMath::Clamp((Cycle - .16f) / .56f, 0.f, 1.f);
	LoadingAngle = FMath::SmoothStep(0.f, 1.f, Turn) * 2.f * PI;
	LoadingScale = .88f + .18f * FMath::Cos(Cycle * 2.f * PI / 1.85f);
	Invalidate(EInvalidateWidgetReason::Paint);
	return EActiveTimerReturnType::Continue;
}

int32 SMWGameplaySign::OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
	FSlateWindowElementList& DrawElements, int32 Layer, const FWidgetStyle&, bool) const
{
	const FVector2f Center(Geometry.GetLocalSize() * .5);
	const float Radius = FMath::Min(Center.X, Center.Y) * LoadingScale;
	const float Face = FMath::Cos(LoadingAngle);
	const float Depth = FMath::Sin(LoadingAngle);
	const auto Project = [Center, Radius, Face, Depth](FVector2f Point, float Thickness)
	{
		const float Perspective = 1.f / (1.f + .20f * (Point.X * Depth + Thickness * Face));
		return Center + FVector2f(Point.X * Face - Thickness * Depth, Point.Y) * (Radius * Perspective);
	};

	TArray<FSlateVertex> Vertices;
	TArray<SlateIndex> Indices;
	const auto Polygon = [&](TArrayView<const FVector2f> Points, FColor Color, float Scale = 1.f, float Z = 0.f)
	{
		const SlateIndex First = static_cast<SlateIndex>(Vertices.Num());
		for (const FVector2f Point : Points)
		{
			Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Geometry.GetAccumulatedRenderTransform(),
				Project(Point * Scale, Z), FVector2f(.5f, .5f), Color));
		}
		for (int32 Index = 1; Index + 1 < Points.Num(); ++Index)
			Indices.Append({ First, static_cast<SlateIndex>(First + Index), static_cast<SlateIndex>(First + Index + 1) });
	};

	// Short bevels at the tips and a separately projected rear plate keep an
	// edge visible through the flip. The facets are vector artwork.
	const FVector2f Outline[] = {
		{-.027f, -.986f}, {.027f, -.986f}, {.986f, -.027f}, {.986f, .027f},
		{.027f, .986f}, {-.027f, .986f}, {-.986f, .027f}, {-.986f, -.027f}
	};
	Polygon(Outline, FColor(127, 105, 68), 1.f, .034f);
	Polygon(Outline, FColor(255, 187, 49));
	Polygon(Outline, FColor(50, 43, 31), .914f);
	Polygon(Outline, FColor(255, 162, 22), .872f);

	if (bAnimateLoading)
	{
		const FVector2f Bright[] = {{-.86f, 0.f}, {-.33f, -.52f}, {.08f, .14f}};
		const FVector2f Shade[] = {{-.04f, -.85f}, {.37f, -.50f}, {.03f, -.07f}};
		const FVector2f Lower[] = {{.10f, .72f}, {.43f, -.04f}, {.79f, .08f}};
		Polygon(Bright, FColor(255, 188, 39));
		Polygon(Shade, FColor(229, 136, 14));
		Polygon(Lower, FColor(255, 180, 29));
	}

	if (Face > 0.f)
	{
		for (const FVector2f Axis : { FVector2f(0.f, -1.f), FVector2f(1.f, 0.f), FVector2f(0.f, 1.f), FVector2f(-1.f, 0.f) })
		{
			FVector2f Disc[12];
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(Disc); ++Index)
			{
				const float Angle = Index * (2.f * PI / UE_ARRAY_COUNT(Disc));
				Disc[Index] = Axis * .79f + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * .025f;
			}
			Polygon(Disc, FColor(80, 79, 72));
		}
	}

	const auto Renderer = FSlateApplication::Get().GetRenderer();
	const auto Handle = Renderer->GetResourceHandle(*FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")));
	FSlateDrawElement::MakeCustomVerts(DrawElements, Layer, Handle, Vertices, Indices, nullptr, 0, 0);

	if (Face > .12f)
	{
		const FVector2D TextSize = Renderer->GetFontMeasureService()->Measure(Label, SignFont);
		const FVector2D TextScale(Face * LoadingScale, LoadingScale);
		const FVector2D TextOrigin = FVector2D(Center) - TextSize * TextScale * .5;
		const auto TextGeometry = Geometry.MakeChild(TextSize, FSlateLayoutTransform(TextOrigin),
			FSlateRenderTransform(FScale2D(TextScale)), FVector2D::ZeroVector);
		FSlateDrawElement::MakeText(DrawElements, Layer + 1, TextGeometry.ToPaintGeometry(), Label, SignFont,
			ESlateDrawEffect::None, bAnimateLoading ? FLinearColor(.065f, .045f, .027f) : FLinearColor::White);
	}
	return Layer + 1;
}
