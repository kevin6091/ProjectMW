#pragma once

#include "CoreMinimal.h"
#include "MWVehicleDriveInput.generated.h"

USTRUCT(BlueprintType)
struct MW_API FMWVehicleDriveInput
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	float Throttle = 0.f;

	UPROPERTY(BlueprintReadWrite)
	float Reverse = 0.f;

	UPROPERTY(BlueprintReadWrite)
	float Steering = 0.f;

	UPROPERTY(BlueprintReadWrite)
	bool bDriftRequested = false;
};