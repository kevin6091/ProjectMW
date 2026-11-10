#include "UI/Gameplay/SMWVehicleWind.h"

#include "Framework/Application/SlateApplication.h"
#include "Gameplay/Items/MWItemComponent.h"
#include "Gameplay/Vehicle/MWVehicleMovementComponent.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

DEFINE_LOG_CATEGORY_STATIC(LogMWVehicleWind, Log, All);

namespace
{
	constexpr int32 RayCount = 26;
	constexpr int32 RibbonColumns = 5;
	constexpr int32 RibbonRows = 3;
	constexpr float CruiseStart = 0.62f;
	constexpr float CruiseFull = 0.98f;
	constexpr float CruiseStrength = 0.44f;
	constexpr float InvisibleStrength = 0.012f;
}

void SMWVehicleWind::Construct(const FArguments&)
{
	SetVisibility(EVisibility::Hidden);
	SetClipping(EWidgetClipping::ClipToBounds);
	ForceVolatile(true);
	Rays.Reserve(RayCount);
	Vertices.Reserve(RayCount * RibbonRows * RibbonColumns);
	Indices.Reserve(RayCount * (RibbonRows - 1) * (RibbonColumns - 1) * 6);
	for (int32 Index = 0; Index < RayCount; ++Index)
	{
		const float Seed = FMath::Frac((Index + 1) * 0.618034f);
		const float Angle = UE_TWO_PI * (Index + 0.65f * Seed) / RayCount;
		float Sin = 0.f;
		float Cos = 0.f;
		FMath::SinCos(&Sin, &Cos, Angle);
		const float EdgeScale = 0.5f / FMath::Max(FMath::Abs(Cos), FMath::Abs(Sin));
		Rays.Add({FVector2f(Cos, Sin) * EdgeScale, Seed});

		for (int32 Row = 0; Row < RibbonRows - 1; ++Row)
		{
			for (int32 Column = 0; Column < RibbonColumns - 1; ++Column)
			{
				const SlateIndex Corner = (Index * RibbonRows + Row) * RibbonColumns + Column;
				const SlateIndex NextRow = Corner + RibbonColumns;
				Indices.Append({Corner, SlateIndex(Corner + 1), NextRow,
					SlateIndex(Corner + 1), SlateIndex(NextRow + 1), NextRow});
			}
		}
	}
}

void SMWVehicleWind::Update(const AMWVehiclePawn* Vehicle, float DeltaTime)
{
	float SpeedRatio = 0.f;
	float BoosterTime = 0.f;
	if (Vehicle)
	{
		const auto* Movement = Cast<UMWVehicleMovementComponent>(Vehicle->GetMovementComponent());
		const float ForwardLimit = Movement ? Movement->GetUnboostedForwardSpeed() : 0.f;
		const float ForwardSpeed = FVector::DotProduct(Vehicle->GetVelocity(), Vehicle->GetActorForwardVector());
		SpeedRatio = ForwardLimit > 0.f ? FMath::Max(0.f, ForwardSpeed) / ForwardLimit : 0.f;
		BoosterTime = Vehicle->GetItemComponent()->GetBoosterTimeRemaining();
	}

	const float Cruise = FMath::SmoothStep(CruiseStart, CruiseFull, SpeedRatio) * CruiseStrength;
	const float TargetBurst = FMath::SmoothStep(0.f, 0.30f, BoosterTime);
	Burst = Vehicle ? FMath::FInterpTo(Burst, TargetBurst, DeltaTime, 20.f) : 0.f;
	const float TargetStrength = FMath::Lerp(Cruise, 1.f, Burst);
	Strength = Vehicle ? FMath::FInterpTo(Strength, TargetStrength, DeltaTime,
		TargetStrength > Strength ? 17.f : 8.f) : 0.f;

	const bool bVisible = Strength > InvisibleStrength;
	if (bVisible != (GetVisibility() == EVisibility::HitTestInvisible))
	{
		SetVisibility(bVisible ? EVisibility::HitTestInvisible : EVisibility::Hidden);
	}
	Age = bVisible ? Age + DeltaTime * FMath::Lerp(1.f, 1.9f, Burst) : 0.f;

	const EMode NewMode = BoosterTime > 0.f ? EMode::Booster : bVisible ? EMode::Cruise : EMode::Off;
	if (Mode != NewMode)
	{
		Mode = NewMode;
		const TCHAR* Name = Mode == EMode::Booster ? TEXT("Booster") : Mode == EMode::Cruise ? TEXT("Cruise") : TEXT("Off");
		UE_LOG(LogMWVehicleWind, Verbose, TEXT("Wind mode: %s, forward speed ratio %.2f"), Name, SpeedRatio);
	}
}

int32 SMWVehicleWind::OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
	FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle&, bool) const
{
	const FVector2f Size(Geometry.GetLocalSize());
	if (Strength <= InvisibleStrength || Size.X < 64.f || Size.Y < 64.f || !FSlateApplication::IsInitialized())
	{
		return Layer;
	}

	const FVector2f Center = Size * FVector2f(0.5f, 0.46f);
	const float PixelScale = FMath::Clamp(Size.Y / 900.f, 0.65f, 1.8f);
	const auto Transform = Geometry.GetAccumulatedRenderTransform();
	Vertices.Reset();
	for (int32 Index = 0; Index < Rays.Num(); ++Index)
	{
		const FRay& Ray = Rays[Index];
		const FVector2f Direction = Ray.Direction * Size;
		const FVector2f Normal = FVector2f(-Direction.Y, Direction.X).GetSafeNormal();
		const float Travel = FMath::Frac(Ray.Seed + Age * FMath::Lerp(0.82f, 1.48f, Ray.Seed));
		const float Tail = FMath::Lerp(FMath::Lerp(0.67f, 0.44f, Burst), 0.97f, Travel);
		const float Length = FMath::Lerp(0.13f, 0.31f, Burst) * FMath::Lerp(0.72f, 1.25f, Ray.Seed);
		const float Head = Tail + Length;
		const float Alpha = Strength * FMath::Sin(UE_PI * Travel) * FMath::Lerp(0.48f, 0.94f, Ray.Seed)
			* (Index % 2 ? FMath::Lerp(0.28f, 1.f, Burst) : 1.f);
		const FLinearColor Tint = FMath::Lerp(FLinearColor(0.38f, 0.77f, 1.f),
			FLinearColor(1.f, 0.96f, 0.87f), Ray.Seed);
		const float Width = PixelScale * FMath::Lerp(2.4f, 6.7f, Burst) * FMath::Lerp(0.65f, 1.18f, Ray.Seed);
		const float Radius[] = {Tail, Tail + Length * 0.72f, Head};
		const float RowAlpha[] = {0.f, Alpha, 0.f};
		const float RowWidth[] = {0.16f, 1.f, 0.22f};
		constexpr float Offsets[] = {-1.f, -0.48f, 0.f, 0.48f, 1.f};
		const float EdgeAlpha = FMath::Lerp(0.38f, 0.52f, Burst);
		const float Opacities[] = {0.f, EdgeAlpha, 1.f, EdgeAlpha, 0.f};
		const FLinearColor EdgeTint(0.12f, 0.34f, 0.55f);
		for (int32 Row = 0; Row < RibbonRows; ++Row)
		{
			for (int32 Column = 0; Column < RibbonColumns; ++Column)
			{
				const FVector2f Position = Center + Direction * Radius[Row]
					+ Normal * (Width * RowWidth[Row] * Offsets[Column]);
				const FLinearColor& ColumnTint = Column == RibbonColumns / 2 ? Tint : EdgeTint;
				const FColor Color = ColumnTint.CopyWithNewOpacity(RowAlpha[Row] * Opacities[Column]).ToFColorSRGB();
				Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
					Transform, Position, FVector2f(0.5f, 0.5f), Color));
			}
		}
	}
	const FSlateBrush* Brush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	const FSlateResourceHandle Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*Brush);
	FSlateDrawElement::MakeCustomVerts(Elements, Layer, Handle, Vertices, Indices, nullptr, 0, 0);
	return Layer;
}
