

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "MWVehicleNetworkTypes.h"
#include "MWVehiclePawn.generated.h"

class UMWInputData;
class USpringArmComponent;
class UCameraComponent;
class UBoxComponent;
class USceneComponent;
class UMWVehicleMovementComponent;

UCLASS()
class MW_API AMWVehiclePawn : public APawn
{
	GENERATED_BODY()

public:
	AMWVehiclePawn();

	void SubmitDriveInput(const FMWVehicleDriveInput& NewInput);
	const FMWVehicleDriveInput& GetDriveInput() const { return DriveInput; }
	UMWInputData* GetDrivingInputData() const { return DrivingInputData; }
	virtual UPawnMovementComponent* GetMovementComponent() const override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaTime) override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TeleportSucceeded(bool bIsATest) override;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MW|Collision")
	TObjectPtr<UBoxComponent> CollisionRoot;
	
	// 차량 메시와 카메라에 표시용 보정을 적용할 부모.
	// 실제 이동과 충돌은 CollisionRoot가 담당한다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MW|Visual")
	TObjectPtr<USceneComponent> VisualRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MW|Drive")
	TObjectPtr<UMWVehicleMovementComponent> VehicleMovement;

	UPROPERTY(EditDefaultsOnly, Category="MW|Input")
	TObjectPtr<UMWInputData> DrivingInputData;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MW|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MW|Camera")
	TObjectPtr<UCameraComponent> FollowCamera;
	
	// 높이 오차가 사라지는 응답 속도.
	// 값이 클수록 실제 차체를 빠르게 따라간다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MW|Visual", meta = (ClampMin = "0.1"))
	float VisualHeightResponse = 18.f;
	// 기울기 변화에 대한 응답 속도.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MW|Visual", meta = (ClampMin = "0.1"))
	float VisualTiltResponse = 18.f;
	// 실제 충돌체와 표시 차체 사이에 허용할 최대 높이 차이.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MW|Visual", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxVisualHeightOffset = 1.f;
	// 실제 차체와 표시 차체 사이에 허용할 최대 기울기 차이.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MW|Visual", meta = (ClampMin = "0.0", ClampMax = "45.0"))
	float MaxVisualTiltLagAngle = 3.f;
	// 한 프레임에 이 거리보다 크게 이동하면 표시 상태를 초기화한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MW|Visual", meta = (ClampMin = "1.0", Units = "cm"))
	float VisualResetDistance = 300.f;
	// 큰 회전 변화도 표시 상태 초기화 대상으로 처리한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MW|Visual", meta = (ClampMin = "1.0", ClampMax = "180.0"))
	float VisualResetAngle = 45.f;

private:
	friend class UMWVehicleMovementComponent;

	struct FPredictedMove
	{
		FMWVehicleMoveCommand Command;
		FMWVehicleDriveSnapshot Result;
	};

	FMWVehicleDriveInput DriveInput;

	UPROPERTY(Replicated)
	FMWVehicleDriveSnapshot ServerDriveSnapshot;

	TMap<uint32, FMWVehicleDriveInput> QueuedServerMoves;
	TArray<FPredictedMove> PredictedMoves;
	FMWVehicleDriveSnapshot ProxyStart;
	uint32 LastProcessedMoveId = 0;
	uint32 NextMoveId = 1;
	uint32 AppliedStreamId = 0;
	uint32 AppliedRevision = 0;
	float DriveStepTime = 0.f;
	float SnapshotTime = 0.f;
	float SendTime = 0.f;
	float ProxyBlendTime = 0.f;
	float RemoteInputSilence = 0.f;
	bool bHadRemoteInput = false;
	bool bWasPredicting = false;

	UFUNCTION(Server, Unreliable)
	void ServerSubmitDriveMoves(uint32 StreamId, const TArray<FMWVehicleMoveCommand>& Moves);

	void TickNetworkDrive(float DeltaTime);
	void TickAuthorityDrive(float DeltaTime);
	void TickPredictedDrive(float DeltaTime);
	void TickProxyDrive(float DeltaTime);
	void ResetDriveStream();
	void PublishDriveSnapshot(bool bNewStream = false);
	void ReconcilePredictedDrive();

	void ResetGroundVisual();
	void UpdateGroundVisual(float DeltaTime);

	// 표시 상태가 초기화됐는지.
	bool bGroundVisualInitialized = false;
	// 이전 표시 프레임의 월드 위치.
	// 부모 이동에 의해 바뀌지 않는 별도 계산 상태다.
	FVector PreviousVisualWorldLocation = FVector::ZeroVector;
	// 보간 중인 표시 차체의 위쪽 방향.
	FVector SmoothedVisualUp = FVector::UpVector;
	// 현재 표시용 높이 오프셋. 단위: cm
	double VisualHeightOffset = 0.0;
	// 큰 위치·회전 변화를 감지하기 위한 실제 충돌체의 이전 변환.
	FTransform PreviousCollisionTransform = FTransform::Identity;
};
