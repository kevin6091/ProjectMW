#pragma once

#include "Components/InstancedStaticMeshComponent.h"
#include "MWVehicleSkidMarkComponent.generated.h"

UCLASS()
class UMWVehicleSkidMarkComponent : public UInstancedStaticMeshComponent
{
	GENERATED_BODY()

public:
	UMWVehicleSkidMarkComponent();

	void SampleWheel(int32 WheelIndex, const FVector& WheelCenter, double RadiusCm, const UPrimitiveComponent& DriveCollision);
	void ResetTrail() { PreviousContacts.Reset(); }

private:
	struct FWheelContact
	{
		FVector Position;
		FVector Normal;
	};

	void AddMark(const FTransform& Transform);

	TMap<int32, FWheelContact> PreviousContacts;
	int32 NextMarkToReplace = 0;
};
