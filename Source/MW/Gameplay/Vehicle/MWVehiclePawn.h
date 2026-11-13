

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "MWVehicleNetworkTypes.h"
#include "MWVehiclePawn.generated.h"

class UMWInputData;
class USpringArmComponent;
class UCameraComponent;
class UBoxComponent;
class UMWVehicleMovementComponent;
class UMWVehiclePresentationComponent;

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

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MW|Drive")
	TObjectPtr<USceneComponent> DrivePresentation;

	UPROPERTY(VisibleAnywhere, Category="MW|Drive")
	TObjectPtr<UMWVehiclePresentationComponent> PresentationTick;

private:
	friend class UMWVehicleMovementComponent;
	friend class UMWVehiclePresentationComponent;

	// 같은 상대에게 닿아 있는 동안, 이전 스냅샷의 속도로 연속 충격을 만들지 않도록 하는 로컬 추정치.
	struct FPredictedContactMotion
	{
		FObjectKey Vehicle;
		uint32 StreamId = 0;
		uint32 Revision = 0;
		float RemainingTime = 0.f;
		FVector Location = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		FVector BeforeImpactVelocity = FVector::ZeroVector;
		FQuat Rotation = FQuat::Identity;
		float ImpactYawRate = 0.f;
	};

	struct FPredictedMove
	{
		FMWVehicleMoveCommand Command;
		FMWVehicleDriveSnapshot Result;
		TArray<FMWVehicleContactSample, TInlineAllocator<3>> Contacts;
		TArray<FPredictedContactMotion, TInlineAllocator<3>> ContactMotions;
		// 이 입력을 처음 예측한 서버 추정 시각. 재실행해도 패킷 도착 시각으로 바꾸지 않는다.
		double ResultTime = 0.0;
	};

	FMWVehicleDriveInput DriveInput;

	UPROPERTY(Replicated)
	FMWVehicleDriveSnapshot ServerDriveSnapshot;

	TMap<uint32, FMWVehicleDriveInput> QueuedServerMoves;
	TArray<FPredictedMove> PredictedMoves;
	TArray<FPredictedContactMotion, TInlineAllocator<3>> PredictedContactMotions;
	TArray<FPredictedContactMotion, TInlineAllocator<3>> LastAckedContactMotions;
	FMWVehicleDriveSnapshot LastAuthoritativeSnapshot;
	TArray<FMWVehicleDriveSnapshot, TInlineAllocator<12>> ProxySnapshots;
	// 충돌체는 고정 스텝 결과에 둔다. 직전 회전은 남은 짧은 표시 시간의 Yaw 추정에만 쓴다.
	FTransform PreviousDriveTransform = FTransform::Identity;
	FVector PresentationCorrectionOffset = FVector::ZeroVector;
	FQuat PresentationCorrectionRotation = FQuat::Identity;
	bool bHasPreviousDriveTransform = false;
	uint32 LastProcessedMoveId = 0;
	uint32 NextMoveId = 1;
	uint32 AppliedStreamId = 0;
	uint32 AppliedRevision = 0;
	float DriveStepTime = 0.f;
	float SnapshotTime = 0.f;
	float SendTime = 0.f;
	double ProxyPlaybackTime = 0.0;
	double ProxyInterpolationDelay = 0.0;
	// 상대 외형의 실제 표시 결과. 목표 위치가 바뀌어도 물리 속도와 보정 속도를 분리한다.
	FMWVehicleDriveSnapshot LastProxyPresentation;
	bool bHasProxyPresentation = false;
	float RemoteInputSilence = 0.f;
	float RemoteInputGapTime = 0.f;
	bool bHadRemoteInput = false;
	bool bWasPredicting = false;
	bool bPredictionBufferFull = false;
	bool bContactSnapshotPending = false;
	float ContactPresentationWeight = 0.f;
	float ContactPresentationTime = 0.f;
	FVector ContactPresentationNormal = FVector::ZeroVector;
	TWeakObjectPtr<AMWVehiclePawn> ContactPresentationOther;
	bool bContactPresentationPending = false;

	UFUNCTION(Server, Unreliable)
	void ServerSubmitDriveMoves(uint32 StreamId, const TArray<FMWVehicleMoveCommand>& Moves);

	void TickNetworkDrive(float DeltaTime);
	void TickAuthorityDrive(float DeltaTime);
	void TickPredictedDrive(float DeltaTime);
	void TickProxyDrive(float DeltaTime);
	FMWVehicleDriveSnapshot GetProxyDrivePresentation(float DeltaTime);
	bool GetContactPrediction(const AMWVehiclePawn* Other, double ServerTime, FMWVehicleContactSample& OutContact,
		bool bUseContactMotion = false) const;
	void NotifyDriveContact(AMWVehiclePawn* Other, const FVector& Normal);
	void UpdateDrivePresentation(float DeltaTime);
	void UpdateNearbyPresentation(FMWVehicleDriveSnapshot& Presentation, float DeltaTime);
	void SmoothProxyPresentation(FMWVehicleDriveSnapshot& Presentation, float DeltaTime);
	void ResetDriveStream();
	void PublishDriveSnapshot(bool bNewStream = false);
	void ReconcilePredictedDrive();
	void SimulatePredictedMove(FPredictedMove& Move, bool bReplaying = false);
	void SmoothDrivePresentation(float DeltaTime);
	FTransform GetLocalDrivePresentation() const;
	void ResetDrivePresentation();
	double GetDriveServerTime() const;
	double GetDrivePredictionTime() const;

};
