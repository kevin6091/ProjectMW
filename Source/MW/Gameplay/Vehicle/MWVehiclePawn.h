

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "MWVehicleNetworkTypes.h"
#include "Engine/EngineTypes.h"
#include "MWVehiclePawn.generated.h"

class UMWInputData;
class USpringArmComponent;
class UCameraComponent;
class UBoxComponent;
class UMWVehicleMovementComponent;
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

	void SubmitDriveInput(const FMWVehicleDriveInput& NewInput);
	const FMWVehicleDriveInput& GetDriveInput() const { return DriveInput; }
	UMWInputData* GetDrivingInputData() const { return DrivingInputData; }
	virtual UPawnMovementComponent* GetMovementComponent() const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TeleportSucceeded(bool bIsATest) override;

    // BP selects its own mesh components and values; no vehicle-name branches.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MW|Visual|Wheels")
    TArray<FMWVehicleWheelVisual> WheelVisuals;

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
	friend class UMWVehicleMovementComponent;

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
    uint32 VisualStreamId = 0;
    FDelegateHandle VisualRootTransformHandle;
    void InitializeWheelVisuals();
    void UpdateWheelVisuals(float DeltaSeconds);



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

};
