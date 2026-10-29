

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MWVehicleDriveInput.h"
#include "MWVehicleMovementComponent.generated.h"

/**
 * 
 */
UCLASS()
class MW_API UMWVehicleMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()
	
public:
	UMWVehicleMovementComponent();

	void SetDriveInput(const FMWVehicleDriveInput& NewInput);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void PhysicsRotation(float DeltaTime) override;
	
	UFUNCTION(BlueprintPure, Category="MW|Drive")
	bool IsDriftControlActive() const;
	
	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
	
protected:
	// 관성 감속
	UPROPERTY(EditDefaultsOnly, Category="MW|Drive")
	float CoastDeceleration = 120.f;

	// 최대 브레이크 감속
	UPROPERTY(EditDefaultsOnly, Category="MW|Drive")
	float FullBrakeDeceleration = 2600.f;

	// 회전 속도
	UPROPERTY(EditDefaultsOnly, Category="MW|Drive")
	float TurnRateDegPerSecond = 100.f;

	// 최대 조향 속도
	UPROPERTY(EditDefaultsOnly, Category="MW|Drive")
	float FullSteeringSpeed = 500.f;

	// 최소 조향 속도
	UPROPERTY(EditDefaultsOnly, Category="MW|Drive")
	float MinSteeringSpeed = 20.f;
	
	UPROPERTY(EditDefaultsOnly, Category="MW|Drive", meta=(ClampMin=0.f, Units="cm/s"))
	float MinDriftSpeed = 500.f;
	
	UPROPERTY(EditDefaultsOnly, Category="MW|Drive", meta=(ClampMin=0.f, ClampMax=1.0f))
	float MinDriftSteering = 0.1f;
	
	UPROPERTY(EditDefaultsOnly, Category="MW|Drive", meta=(ClampMin=0.f, ClampMax=1.0f))
	float DriftFrictionMultiplier = 0.2f;
	
	UPROPERTY(EditDefaultsOnly, Category="MW|Drive", meta=(ClampMin=0.f))
	float DriftTurnRateMultiplier = 1.25f;
	
private:
	FMWVehicleDriveInput DriveInput;
};
