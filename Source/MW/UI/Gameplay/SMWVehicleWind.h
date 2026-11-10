#pragma once

#include "Rendering/DrawElements.h"
#include "Widgets/SLeafWidget.h"

class AMWVehiclePawn;

class SMWVehicleWind : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SMWVehicleWind) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&);
	void Update(const AMWVehiclePawn* Vehicle, float DeltaTime);

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
		FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;

private:
	enum class EMode : uint8 { Off, Cruise, Booster };

	struct FRay
	{
		FVector2f Direction;
		float Seed;
	};

	TArray<FRay> Rays;
	TArray<SlateIndex> Indices;
	mutable TArray<FSlateVertex> Vertices;
	EMode Mode = EMode::Off;
	float Age = 0.f;
	float Strength = 0.f;
	float Burst = 0.f;
};
