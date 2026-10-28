

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

private:
	FMWVehicleDriveInput DriveInput;
};
