

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "Gameplay/Construction/MWTrackSelectionTypes.h"
#include "MWGameplayPlayerController.generated.h"

class AMWVehiclePawn;
class UEnhancedInputComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UInputMappingContext;
class UMWInputData;
class UUserWidget;
class AMWTrackSelectionBoard;

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
	
	UFUNCTION(BlueprintCallable, Category = "MW|Construction")
	void RequestConfirmTrack(int32 SessionId, int32 Revision, int32 OfferId);
	void SetTrackSelectionBoard(AMWTrackSelectionBoard* Board);
	void RequestTrackHover(int32 SessionId, int32 Revision, int32 OfferId);
	void RequestTrackCursor(int32 SessionId, int32 Revision, bool bVisible, const FVector& Position);
	
	UPROPERTY(EditDefaultsOnly, Category = "MW|Construction")
	TSubclassOf<UUserWidget> TrackSelectionWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> TrackSelectionWidget;

protected:
	virtual void SetupInputComponent() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	UFUNCTION(BlueprintImplementableEvent, Category = "MW|Construction")
	void OnTrackSelectionReply(const FMWTrackSelectionReply& Reply);

private:
	void ApplyGameplayInputMode();
	void RefreshDrivingInput();
	void ClearDrivingInput();
	AMWVehiclePawn* GetInputVehicle() const;

	// Construction
	UFUNCTION(Server, Reliable)
	void ServerConfirmTrack(int32 SessionId, int32 Revision, int32 OfferId, int32 RequestId);

	UFUNCTION(Client, Reliable)
	void ClientTrackSelectionReply(const FMWTrackSelectionReply& Reply);
	UFUNCTION(Server, Unreliable)
	void ServerUpdateTrackHover(int32 SessionId, int32 Revision, int32 OfferId);
	UFUNCTION(Server, Unreliable)
	void ServerUpdateTrackCursor(int32 SessionId, int32 Revision, int32 Sequence, bool bVisible, FVector_NetQuantize Position);
	void HandleTrackSelectPressed();
	TWeakObjectPtr<AMWTrackSelectionBoard> SelectionBoard;
	TWeakObjectPtr<UEnhancedInputComponent> SelectionInputComponent;
	UPROPERTY(Transient) TObjectPtr<UMWInputData> SelectionInputData;
	uint32 SelectionBindingHandle = 0;
	int32 NextCursorSequence = 0;

	int32 LastTrackSelectionRequestId = 0;
	
	// Driving
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
