

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MWVehicleDriveInput.h"
#include "Data/MWVehicleDriveData.h"
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
	virtual float GetMaxSpeed() const override;
	
protected:
	virtual void BeginPlay() override;
	
	UPROPERTY(EditDefaultsOnly, Category="MW|Drive")
	TObjectPtr<UMWVehicleDriveData> DriveData;
	
private:
	UPROPERTY(Transient)
	FMWVehicleDriveConfig DriveConfig;
	
	FMWVehicleDriveInput DriveInput;
	
	void ResolveDriveControl(float& OutThrottle, float& OutBrake) const;
};
