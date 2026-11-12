

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

// 보이는 바퀴가 없는 차량의 자국용 접지 정보다. 위치는 Pawn 로컬 좌표(cm)다.
USTRUCT(BlueprintType)
struct FMWVehicleVirtualSkidWheel
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skid Mark", meta = (Units = "cm"))
    FVector LocalCenter = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skid Mark", meta = (ClampMin = "0.01", Units = "cm"))
    float RadiusCm = 25.f;
};

/** 바퀴별 편집 데이터다. 회전축과 조향축은 메시 부모의 좌표계를 기준으로 한다. */
USTRUCT(BlueprintType)
struct FMWVehicleWheelVisual
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wheel",
              meta = (UseComponentPicker, AllowedClasses = "/Script/Engine.StaticMeshComponent"))
    FComponentReference Mesh;

    // Pawn 스케일 1 기준 cm 반지름이며 작성한 차체·바퀴 크기를 포함한다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wheel", meta = (ClampMin = "0.01", Units = "cm"))
    float RadiusCm = 25.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wheel")
    FVector RollAxis = FVector(-1, 0, 0);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wheel")
    bool bSteers = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wheel")
    FVector SteeringAxis = FVector::UpVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wheel",
              meta = (ClampMin = "0", ClampMax = "90", Units = "deg"))
    float MaxSteeringAngleDegrees = 35.f;
};

UCLASS()
class MW_API AMWVehiclePawn : public APawn
{
	GENERATED_BODY()

public:
	AMWVehiclePawn();

    UFUNCTION(BlueprintCallable, Category = "MW|Drive")
	void SubmitDriveInput(const FMWVehicleDriveInput& NewInput);
	const FMWVehicleDriveInput& GetDriveInput() const { return DriveInput; }
	UMWInputData* GetDrivingInputData() const { return DrivingInputData; }
	virtual UPawnMovementComponent* GetMovementComponent() const override;
    UMWItemComponent* GetItemComponent() const
    {
        return ItemComponent;
    }

    // 기존 Pawn 틱 뒤에 캐시한 바퀴·자국 표시만 갱신한다.
    virtual void Tick(float DeltaSeconds) override;

    // 기존 차량 시점에 부스터 표시용 카메라 변화를 적용하며 이동·서버 보정 값은 바꾸지 않는다.
    virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    // 현재 통합의 박스 이동·충돌 루트를 유지하고 기존 아이템 중첩 이벤트를 허용한다.
    virtual void PostInitializeComponents() override;
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TeleportSucceeded(bool bIsATest) override;

    // BP가 바퀴 메시와 수치를 지정한다. 차종 이름을 검사하는 분기를 만들지 않는다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MW|Visual|Wheels")
    TArray<FMWVehicleWheelVisual> WheelVisuals;

    // 비어 있으면 기존 바퀴 메시를 사용한다. 지정하면 이 접지점만 자국을 남긴다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MW|Visual|Skid Marks")
    TArray<FMWVehicleVirtualSkidWheel> VirtualSkidWheels;

    // 시각 조향 응답의 단위는 1/초다. 12는 약 0.25초에 95% 도달하며 0이면 보간하지 않는다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MW|Visual|Wheels", meta = (ClampMin = "0", UIMax = "30"))
    float WheelSteeringInterpSpeed = 12.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MW|Collision")
	TObjectPtr<UBoxComponent> CollisionRoot;

    // 기존 볼록 충돌 에셋 설정은 보존한다. 이번 통합에서는 박스 루트만 사용한다.
    UPROPERTY(EditDefaultsOnly, Category = "MW|Collision")
    TObjectPtr<UStaticMesh> DrivingCollisionMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MW|Drive")
	TObjectPtr<UMWVehicleMovementComponent> VehicleMovement;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MW|Items")
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
    TArray<FMWVehicleVirtualSkidWheel> CachedVirtualSkidWheels;
    FVector PreviousVisualLocation = FVector::ZeroVector;
    bool bVisualLocationValid = false;
    double VisualSteering = 0;
    uint32 VisualStreamId = 0;
    FDelegateHandle VisualRootTransformHandle;

    // BP의 바퀴 참조·축·반지름을 한 번 검증해 캐시한다. 매 프레임 컴포넌트를 다시 찾지 않는다.
    void InitializeWheelVisuals();

    // 캐시한 시각 바퀴의 회전·조향만 갱신한다. 실제 이동 입력과 서버 스냅샷을 덮어쓰지 않는다.
    void UpdateWheelVisuals(float DeltaSeconds);

    UPROPERTY(Transient)
    TObjectPtr<UMWVehicleSkidMarkComponent> SkidMarks;
    bool bHasRearSkidWheels = false;

    // 로컬 드리프트에 실제 바퀴 또는 BP 가상 접지점을 적용해 자국을 표시한다.
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
	// 충돌체는 고정 스텝 결과에 두고, 차체와 카메라는 이전·현재 결과 사이를 표시한다.
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
	FMWVehicleDriveSnapshot GetProxyDrivePresentation(float DeltaTime);
	void ResetDriveStream();
	void PublishDriveSnapshot(bool bNewStream = false);
	void ReconcilePredictedDrive();
	void SimulatePredictedMove(FPredictedMove& Move);
	void SmoothDrivePresentation(float DeltaTime);
	FTransform GetInterpolatedDriveTransform() const;
	void ResetDrivePresentation();
	double GetDriveServerTime() const;

};
