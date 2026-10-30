

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "MWVehicleDriveInput.h"
#include "MWVehicleCharacter.generated.h"

class UMWInputData;
class USpringArmComponent;
class UCameraComponent;

UCLASS()
class MW_API AMWVehicleCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AMWVehicleCharacter(const FObjectInitializer& ObjectInitializer);
	
	void SubmitDriveInput(const FMWVehicleDriveInput& NewInput);
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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
};
