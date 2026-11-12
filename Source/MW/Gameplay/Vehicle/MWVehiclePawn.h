

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "MWVehicleNetworkTypes.h"
#include "Engine/EngineTypes.h"
#include "MWVehiclePawn.generated.h"

class UMWInputData;
class UMWItemComponent;
class USpringArmComponent;
class UCameraComponent;
class UBoxComponent;
class UMWVehicleMovementComponent;
class UMWVehicleSkidMarkComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** Per-wheel authoring data; axes are in the mesh parent's coordinates. */
USTRUCT(BlueprintType)
struct FMWVehicleWheelVisual
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wheel", meta=(UseComponentPicker, AllowedClasses="/Script/Engine.StaticMeshComponent"))
    FComponentReference Mesh;

    // Radius in cm at Pawn scale 1, including the authored body/wheel scaling.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wheel", meta=(ClampMin="0.01", Units="cm"))
    float RadiusCm = 25.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wheel")
    FVector RollAxis = FVector(-1, 0, 0);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wheel")
    bool bSteers = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wheel")
    FVector SteeringAxis = FVector::UpVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wheel", meta=(ClampMin="0", ClampMax="90", Units="deg"))
    float MaxSteeringAngleDegrees = 35.f;
};

UCLASS()
class MW_API AMWVehiclePawn : public APawn
{
	GENERATED_BODY()

public:
	AMWVehiclePawn();

	UFUNCTION(BlueprintCallable, Category="MW|Drive")
	void SubmitDriveInput(const FMWVehicleDriveInput& NewInput);
	const FMWVehicleDriveInput& GetDriveInput() const { return DriveInput; }
	UMWInputData* GetDrivingInputData() const { return DrivingInputData; }
	virtual UPawnMovementComponent* GetMovementComponent() const override;
	UMWItemComponent* GetItemComponent() const { return ItemComponent; }
	virtual void Tick(float DeltaSeconds) override;
	virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void PostInitializeComponents() override;
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TeleportSucceeded(bool bIsATest) override;

    // BP selects its own mesh components and values; no vehicle-name branches.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MW|Visual|Wheels")
    TArray<FMWVehicleWheelVisual> WheelVisuals;

    // Visual response in 1/s; 12 reaches 95% in about 0.25 s. Zero disables blending.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MW|Visual|Wheels", meta=(ClampMin="0", UIMax="30"))
    float WheelSteeringInterpSpeed = 12.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MW|Collision")
	TObjectPtr<UBoxComponent> CollisionRoot;

	// 기존 볼록 충돌 에셋 설정은 보존한다. 이번 통합에서는 박스 루트만 사용한다.
	UPROPERTY(EditDefaultsOnly, Category="MW|Collision")
	TObjectPtr<UStaticMesh> DrivingCollisionMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MW|Drive")
	TObjectPtr<UMWVehicleMovementComponent> VehicleMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MW|Items")
	TObjectPtr<UMWItemComponent> ItemComponent;

	UPROPERTY(EditDefaultsOnly, Category="MW|Input")
	TObjectPtr<UMWInputData> DrivingInputData;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MW|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MW|Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MW|Drive")
	TObjectPtr<USceneComponent> DrivePresentation;

private:
	friend class UMWVehicleMovementComponent;

	UPROPERTY(Transient)
	float BoosterCameraBlend = 0.f;


    struct FCachedVisualWheel
    {
        TWeakObjectPtr<UStaticMeshComponent> Mesh;
        FQuat RestRotation = FQuat::Identity;
        FVector RollAxis;
        FVector SteeringAxis;
        double RadiusCm = 0;
        double RollRadians = 0;
        double MaxSteeringRadians = 0;
        bool bSteers = false;
    };

    TArray<FCachedVisualWheel> CachedVisualWheels;
    FVector PreviousVisualLocation = FVector::ZeroVector;
    bool bVisualLocationValid = false;
    double VisualSteering = 0;
    uint32 VisualStreamId = 0;
    FDelegateHandle VisualRootTransformHandle;
    void InitializeWheelVisuals();
    void UpdateWheelVisuals(float DeltaSeconds);

    UPROPERTY(Transient)
    TObjectPtr<UMWVehicleSkidMarkComponent> SkidMarks;
    bool bHasRearSkidWheels = false;
    void UpdateSkidMarks(bool bResetTrail, double ActorScale);



	struct FPredictedMove
	{
		FMWVehicleMoveCommand Command;
		FMWVehicleDriveSnapshot Result;
		TArray<FMWVehicleContactSample, TInlineAllocator<3>> Contacts;
	};

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
	};

	FMWVehicleDriveInput DriveInput;

	UPROPERTY(Replicated)
	FMWVehicleDriveSnapshot ServerDriveSnapshot;

	TMap<uint32, FMWVehicleDriveInput> QueuedServerMoves;
	TArray<FPredictedMove> PredictedMoves;
	TArray<FPredictedContactMotion, TInlineAllocator<3>> PredictedContactMotions;
	FMWVehicleDriveSnapshot LastAuthoritativeSnapshot;
	TArray<FMWVehicleDriveSnapshot, TInlineAllocator<12>> ProxySnapshots;
	uint32 LastProcessedMoveId = 0;
	uint32 NextMoveId = 1;
	uint32 AppliedStreamId = 0;
	uint32 AppliedRevision = 0;
	float DriveStepTime = 0.f;
	float SnapshotTime = 0.f;
	float SendTime = 0.f;
	double ProxyPlaybackTime = 0.0;
	double ProxyInterpolationDelay = 0.0;
	float RemoteInputSilence = 0.f;
	float RemoteInputGapTime = 0.f;
	bool bHadRemoteInput = false;
	bool bWasPredicting = false;
	bool bPredictionBufferFull = false;
	bool bContactSnapshotPending = false;

	UFUNCTION(Server, Unreliable)
	void ServerSubmitDriveMoves(uint32 StreamId, const TArray<FMWVehicleMoveCommand>& Moves);

	void TickNetworkDrive(float DeltaTime);
	void TickAuthorityDrive(float DeltaTime);
	void TickPredictedDrive(float DeltaTime);
	void TickProxyDrive(float DeltaTime);
	void ResetDriveStream();
	void PublishDriveSnapshot(bool bNewStream = false);
	void ReconcilePredictedDrive();
	void SimulatePredictedMove(FPredictedMove& Move);
	void SmoothDrivePresentation(float DeltaTime);
	double GetDriveServerTime() const;

};
