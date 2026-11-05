

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "MWGameplayPlayerController.generated.h"

class AMWVehiclePawn;
class UEnhancedInputComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UInputMappingContext;
class UMWInputData;

// 로컬 입력 바인딩과 현재 조종 중인 차량으로의 입력 전달을 담당한다.
UCLASS()
class MW_API AMWGameplayPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMWGameplayPlayerController();

	virtual void BeginPlay() override;
	virtual void ReceivedPlayer() override;
	virtual void SetPawn(APawn* InPawn) override;
	virtual void OnRep_PlayerState() override;
	virtual void PostSeamlessTravel() override;

protected:
	virtual void SetupInputComponent() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void ApplyGameplayInputMode();
	void RefreshDrivingInput();
	void ClearDrivingInput();
	AMWVehiclePawn* GetInputVehicle() const;

	TWeakObjectPtr<AMWVehiclePawn> InputVehicle;
	TWeakObjectPtr<UEnhancedInputComponent> DrivingInputComponent;
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> DrivingInputSubsystem;
	TWeakObjectPtr<const UInputMappingContext> DrivingMappingContext;
	TArray<uint32> DrivingBindingHandles;

	UPROPERTY(Transient)
	TObjectPtr<UMWInputData> BoundDrivingInputData;

	void HandleAccel(const FInputActionValue& Value);
	void HandleReverse(const FInputActionValue& Value);
	void HandleSteer(const FInputActionValue& Value);
	void StopAccel();
	void StopReverse();
	void StopSteer();
	void StartDrift();
	void StopDrift();
};
