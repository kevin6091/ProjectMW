#include "Gameplay/Vehicle/Visual/MWVehicleSkidMarkComponent.h"

#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr int32 MaxMarksPerVehicle = 384;
	constexpr double PlaneSizeCm = 100.0;
	constexpr double SurfaceLiftCm = 0.8;
	constexpr double MaxGapCm = 160.0;
}

UMWVehicleSkidMarkComponent::UMWVehicleSkidMarkComponent()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Rubber(
		TEXT("/Game/Gameplay/Vehicles/Art/Materials/M_MW_TireSkid.M_MW_TireSkid"));
	if (Plane.Succeeded() && Rubber.Succeeded())
	{
		SetStaticMesh(Plane.Object);
		SetMaterial(0, Rubber.Object);
	}

	SetMobility(EComponentMobility::Movable);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	SetCastShadow(false);
	SetReceivesDecals(false);
}

void UMWVehicleSkidMarkComponent::SampleWheel(
	int32 WheelIndex, const FVector& WheelCenter, double RadiusCm, const UPrimitiveComponent& DriveCollision)
{
	UWorld* World = GetWorld();
	if (!World || !GetStaticMesh())
	{
		return;
	}

	// A wheel can hang past the edge even while the movement hull is still grounded.
	const FVector Probe = FVector::UpVector * RadiusCm;
	FHitResult Hit;
	const FCollisionQueryParams Query(SCENE_QUERY_STAT(MWTireSkid), true, GetOwner());
	const FCollisionResponseParams Responses(DriveCollision.GetCollisionResponseToChannels());
	if (!World->LineTraceSingleByChannel(Hit, WheelCenter + Probe * 0.25, WheelCenter - Probe * 1.6,
			DriveCollision.GetCollisionObjectType(), Query, Responses)
		|| !Hit.IsValidBlockingHit() || Hit.ImpactNormal.Z < 0.5 || Cast<APawn>(Hit.GetActor()))
	{
		PreviousContacts.Remove(WheelIndex);
		return;
	}

	const FWheelContact Current{Hit.ImpactPoint, Hit.ImpactNormal};
	if (const FWheelContact* Previous = PreviousContacts.Find(WheelIndex))
	{
		const FVector Delta = Current.Position - Previous->Position;
		const double Distance = Delta.Length();
		if (Distance < RadiusCm * 0.5)
		{
			return;
		}

		if (Distance <= MaxGapCm && FVector::DotProduct(Previous->Normal, Current.Normal) >= 0.9)
		{
			const FVector Normal = (Previous->Normal + Current.Normal).GetSafeNormal();
			const FVector Center = (Previous->Position + Current.Position) * 0.5 + Normal * SurfaceLiftCm;
			const double WidthCm = RadiusCm * 0.65;
			const FQuat Rotation = FRotationMatrix::MakeFromXZ(Delta, Normal).ToQuat();
			const FVector Scale((Distance + WidthCm * 0.2) / PlaneSizeCm, WidthCm / PlaneSizeCm, 1.0);
			AddMark(FTransform(Rotation, Center, Scale));
		}
	}

	PreviousContacts.Add(WheelIndex, Current);
}

void UMWVehicleSkidMarkComponent::AddMark(const FTransform& Transform)
{
	// Reuse the oldest world-space mark so long races have a bounded rendering cost.
	if (GetInstanceCount() < MaxMarksPerVehicle)
	{
		AddInstance(Transform, /*bWorldSpace=*/true);
		return;
	}
	UpdateInstanceTransform(NextMarkToReplace, Transform, /*bWorldSpace=*/true, /*bMarkRenderStateDirty=*/true);
	NextMarkToReplace = (NextMarkToReplace + 1) % MaxMarksPerVehicle;
}
