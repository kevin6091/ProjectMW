

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "MWVehicleDriveInput.h"
#include "MWVehiclePawn.generated.h"

class UMWInputData;
class USpringArmComponent;
class UCameraComponent;
class UBoxComponent;
class UMWVehicleMovementComponent;

UCLASS()
class MW_API AMWVehiclePawn : public APawn
{
	GENERATED_BODY()

public:
	AMWVehiclePawn();

	void SubmitDriveInput(const FMWVehicleDriveInput& NewInput);
	virtual UPawnMovementComponent* GetMovementComponent() const override;
	virtual void UnPossessed() override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TeleportSucceeded(bool bIsATest) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MW|Collision")
	TObjectPtr<UBoxComponent> CollisionRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MW|Drive")
	TObjectPtr<UMWVehicleMovementComponent> VehicleMovement;

	UPROPERTY(EditDefaultsOnly, Category="MW|Input")
	TObjectPtr<UMWInputData> DrivingInputData;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MW|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MW|Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

private:
	FMWVehicleDriveInput DriveInput;

	void HandleAccel(const FInputActionValue& Value);
	void HandleReverse(const FInputActionValue& Value);
	void HandleSteer(const FInputActionValue& Value);

	void StopAccel();
	void StopReverse();
	void StopSteer();

	void StartDrift();
	void StopDrift();
	void RemoveDrivingMappingContext();
};
