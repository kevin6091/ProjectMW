

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MWVehicleDriveData.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType)
struct MW_API FMWVehicleDriveConfig
{
	GENERATED_BODY()
	
	// 관성 감속
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Braking", meta = (AllowPrivateAccess = "true"))
	float CoastDeceleration = 120.f;
	
	// 최대 브레이크 감속
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Braking", meta = (AllowPrivateAccess = "true"))
	float FullBrakeDeceleration = 2600.f;
	
	// 방향 전환 감속 임계값
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reverse", meta = (AllowPrivateAccess = "true"))
	float DirectionChangeSpeedThreshold = 30.f;

	// 최대 후진 속도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reverse", meta = (AllowPrivateAccess = "true"))
	float MaxReverseSpeed = 600.f;
	
	// 회전 속도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steering", meta = (AllowPrivateAccess = "true"))
	float TurnRateDegPerSecond = 100.f;
	
	// 최대 조향 속도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steering", meta = (AllowPrivateAccess = "true"))
	float FullSteeringSpeed = 500.f;
	
	// 최소 조향 속도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steering", meta = (AllowPrivateAccess = "true"))
	float MinSteeringSpeed = 20.f;
	
	// 최소 드리프트 속도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drift", meta = (AllowPrivateAccess = "true"))
	float MinDriftSpeed = 200.f;	
	
	// 최소 드리프트 조향
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drift", meta = (AllowPrivateAccess = "true"))
	float MinDriftSteering = 0.5f;
	
	// 드리프트 마찰 계수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drift", meta = (AllowPrivateAccess = "true"))
	float DriftFrictionMultiplier = 0.2f;
	
	// 드리프트 회전 속도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drift", meta = (AllowPrivateAccess = "true"))
	float DriftTurnRateMultiplier = 1.25f;
};

UCLASS()
class MW_API UMWVehicleDriveData : public UDataAsset
{
	GENERATED_BODY()
	
public:
	const FMWVehicleDriveConfig& GetConfig() const
	{
		return Config;
	}

	bool ValidateConfig(TArray<FText>& OutErrors) const;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drive", meta = (AllowPrivateAccess = "true"))
	FMWVehicleDriveConfig Config;
};
