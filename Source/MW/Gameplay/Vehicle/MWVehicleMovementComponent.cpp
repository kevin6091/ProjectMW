

#include "Gameplay/Vehicle/MWVehicleMovementComponent.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Components/PrimitiveComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "CollisionShape.h"

UMWVehicleMovementComponent::UMWVehicleMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UMWVehicleMovementComponent::SetDriveInput(const FMWVehicleDriveInput& NewInput)
{
	// 각 입력을 허용 범위로 제한
	DriveInput.Throttle = FMath::IsFinite(NewInput.Throttle) ? FMath::Clamp(NewInput.Throttle, 0.f, 1.f) : 0.f;
	DriveInput.Reverse = FMath::IsFinite(NewInput.Reverse) ? FMath::Clamp(NewInput.Reverse, 0.f, 1.f) : 0.f;
	DriveInput.Steering = FMath::IsFinite(NewInput.Steering) ? FMath::Clamp(NewInput.Steering, -1.f, 1.f) : 0.f;
	DriveInput.bDriftRequested = NewInput.bDriftRequested;

}

void UMWVehicleMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!FMath::IsFinite(DeltaTime) || ShouldSkipUpdate(DeltaTime) || !bDriveConfigReady || !PawnOwner || !UpdatedPrimitive ||
		UpdatedPrimitive->IsSimulatingPhysics())
	{
		return;
	}

	if (GetNetMode() != NM_Standalone)
	{
		if (AMWVehiclePawn* Vehicle = Cast<AMWVehiclePawn>(PawnOwner))
		{
			Vehicle->TickNetworkDrive(DeltaTime);
			return;
		}
		if (!PawnOwner->IsLocallyControlled())
		{
			return;
		}
	}

	ConsumeInputVector();
	// 프레임이 느려져도 조향/접지 계산을 작은 시간 간격으로 수행한다.
	constexpr float MaxSimulationStep = 1.f / 120.f;
	constexpr float MaxSimulationTime = 0.25f;
	float RemainingTime = FMath::Min(DeltaTime, MaxSimulationTime);
	while (RemainingTime > SMALL_NUMBER && IsValid(PawnOwner) && IsValid(UpdatedComponent))
	{
		const float Step = FMath::Min(RemainingTime, MaxSimulationStep);
		SimulateDriveStep(Step);
		RemainingTime -= Step;
	}
}

// 독립 실행, 서버 재현, 소유자의 예측과 재실행이 같은 충돌·접지 계산을 사용한다.
void UMWVehicleMovementComponent::SimulateDriveStep(float DeltaTime)
{
	UpdateGroundState();
	UpdateSteering(DeltaTime);
	if (bGrounded)
	{
		UpdateDriveVelocity(DeltaTime);
	}
	else
	{
		UpdateAirVelocity(DeltaTime);
	}
	MoveVehicle(DeltaTime);
	UpdateGroundState();
	AdjustGroundHeight();
	UpdateComponentVelocity();
}

void UMWVehicleMovementComponent::CaptureDriveSnapshot(FMWVehicleDriveSnapshot& OutSnapshot) const
{
	const USceneComponent* SnapshotRoot = UpdatedComponent ? UpdatedComponent.Get() : GetOwner()->GetRootComponent();
	if (SnapshotRoot)
	{
		OutSnapshot.Location = SnapshotRoot->GetComponentLocation();
		OutSnapshot.Rotation = SnapshotRoot->GetComponentRotation();
	}
	OutSnapshot.Velocity = Velocity;
	OutSnapshot.bGrounded = bGrounded;
}

void UMWVehicleMovementComponent::ApplyDriveSnapshot(const FMWVehicleDriveSnapshot& Snapshot)
{
	// 서버 위치를 복원할 때 Sweep하거나 Pawn의 텔레포트 콜백으로 입력을 초기화하지 않는다.
	UpdatedComponent->SetWorldLocationAndRotation(Snapshot.Location, Snapshot.Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	Velocity = Snapshot.Velocity;
	bGrounded = Snapshot.bGrounded;
	TargetRotation = UpdatedComponent->GetComponentQuat();
	UpdateComponentVelocity();
}

void UMWVehicleMovementComponent::ResetDriveInput()
{
	DriveInput = FMWVehicleDriveInput{};
	ConsumeInputVector();
}

void UMWVehicleMovementComponent::OnTeleported()
{
	Super::OnTeleported();
	ResetDriveInput();
	bGrounded = false;
	GroundNormal = FVector::UpVector;
	GroundDistance = 0.f;
	TargetRotation = UpdatedComponent ? UpdatedComponent->GetComponentQuat() : FQuat::Identity;
}

bool UMWVehicleMovementComponent::IsDriftControlActive() const
{
	if (!bDriveConfigReady || !PawnOwner || !UpdatedComponent)
	{
		return false;
	}

	// 공중에서는 드리프트 제어를 적용하지 않는다.
	if (!bGrounded)
	{
		return false;
	}

	const AController* Controller = PawnOwner->GetController();

	if (!Controller || Controller->IsMoveInputIgnored())
	{
		return false;
	}

	if (!DriveInput.bDriftRequested)
	{
		return false;
	}

	if (DriveInput.Reverse > KINDA_SMALL_NUMBER)
	{
		return false;
	}

	if (FMath::Abs(DriveInput.Steering) <= DriveConfig.MinDriftSteering)
	{
		return false;
	}

	const FVector Forward = UpdatedComponent->GetForwardVector();
	const float ForwardSpeed = FVector::DotProduct(Velocity, Forward);

	return ForwardSpeed > DriveConfig.MinDriftSpeed;
}

float UMWVehicleMovementComponent::GetMaxSpeed() const
{
	if (!bDriveConfigReady || !PawnOwner || !UpdatedComponent)
	{
		return 0.f;
	}

	const float ForwardSpeed = FVector::DotProduct(Velocity, UpdatedComponent->GetForwardVector());
	const float ForwardSpeedLimit = FMath::Max(DriveConfig.MaxForwardSpeed, 0.f);
	const float Threshold = FMath::Max(DriveConfig.DirectionChangeSpeedThreshold, 0.f);
	const float ReverseSpeedLimit = FMath::Min(ForwardSpeedLimit, FMath::Max(DriveConfig.MaxReverseSpeed, 0.f));

	if (ForwardSpeed < -Threshold)
	{
		return ReverseSpeedLimit;
	}

	if (ForwardSpeed > Threshold)
	{
		return ForwardSpeedLimit;
	}

	float EffectiveThrottle = 0.f;
	float EffectiveBrake = 0.f;

	ResolveDriveControl(EffectiveThrottle, EffectiveBrake);

	if (EffectiveThrottle < -KINDA_SMALL_NUMBER)
	{
		return ReverseSpeedLimit;
	}

	if (EffectiveThrottle <= 0.f && ForwardSpeed < -KINDA_SMALL_NUMBER)
	{
		return ReverseSpeedLimit;
	}

	return ForwardSpeedLimit;
}

void UMWVehicleMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	bDriveConfigReady = false;
	if (!UpdatedPrimitive || UpdatedPrimitive != GetOwner()->GetRootComponent() ||
		!UpdatedPrimitive->IsQueryCollisionEnabled() || UpdatedPrimitive->IsSimulatingPhysics())
	{
		UE_LOG(LogTemp, Error, TEXT("%s: Vehicle movement requires a non-simulating collision root with query collision enabled."), *GetNameSafe(GetOwner()));
		StopMovementImmediately();
		SetComponentTickEnabled(false);
		return;
	}

	if (!DriveData)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s: DriveData is not assigned."),
			*GetNameSafe(GetOwner()));

		StopMovementImmediately();
		SetComponentTickEnabled(false);
		return;
	}

	TArray<FText> Errors;

	if (!DriveData->ValidateConfig(Errors))
	{
		for (const FText& Error : Errors)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("%s: %s"),
				*GetNameSafe(GetOwner()),
				*Error.ToString());
		}

		StopMovementImmediately();
		SetComponentTickEnabled(false);
		return;
	}

	DriveConfig = DriveData->GetConfig();
	TargetRotation = UpdatedComponent->GetComponentQuat();
	bDriveConfigReady = true;
}

void UMWVehicleMovementComponent::ResolveDriveControl(float& OutThrottle, float& OutBrake) const
{
	// 기본 상태: 추진·강한 제동 없음
	OutThrottle = 0.f;
	OutBrake = 0.f;

	if (!bDriveConfigReady || !PawnOwner || !UpdatedComponent)
	{
		return;
	}

	const AController* Controller = PawnOwner->GetController();

	if (!Controller || Controller->IsMoveInputIgnored())
	{
		return;
	}

	const float ForwardRequest = DriveInput.Throttle;
	const float ReverseRequest = DriveInput.Reverse;

	const bool bForwardRequested = ForwardRequest > KINDA_SMALL_NUMBER;
	const bool bReverseRequested = ReverseRequest > KINDA_SMALL_NUMBER;

	if (bForwardRequested && bReverseRequested)
	{
		// 전진과 후진이 동시에 요청되면 제동으로 처리
		OutBrake = 1.f;
		return;
	}

	if (!bForwardRequested && !bReverseRequested)
	{
		// 아무 입력이 없으면 제동 없음
		return;
	}

	const float RequestedThrottle = ForwardRequest - ReverseRequest;
	const FVector Forward = UpdatedComponent->GetForwardVector();

	const float ForwardSpeed = FVector::DotProduct(Velocity, Forward);
	const float Threshold = FMath::Max(DriveConfig.DirectionChangeSpeedThreshold, 0.f);

	const bool bOpposingMovement =
		(ForwardSpeed > Threshold && RequestedThrottle < 0.f) ||
		(ForwardSpeed < -Threshold && RequestedThrottle > 0.f);

	if (bOpposingMovement)
	{
		OutBrake = FMath::Abs(RequestedThrottle);
		return;
	}

	const bool bNearZeroForwardSpeed = FMath::Abs(ForwardSpeed) <= Threshold;
	const bool bStillSliding = Velocity.SizeSquared2D() > FMath::Square(Threshold);

	if (bNearZeroForwardSpeed && bStillSliding)
	{
		// 차량이 거의 정지 상태이지만 관성으로 미끄러지는 경우, 제동을 적용
		OutBrake = FMath::Abs(RequestedThrottle);
		return;
	}

	OutThrottle = RequestedThrottle;
}

void UMWVehicleMovementComponent::UpdateDriveVelocity(float DeltaTime)
{
	if (!bDriveConfigReady ||
		!PawnOwner ||
		!UpdatedComponent ||
		DeltaTime <= 0.f)
	{
		return;
	}

	if (!bGrounded)
	{
		return;
	}

	// 이번 조향 결과를 기준으로 바닥 위 전방 방향을 구한다.
	const FVector Forward = FVector::VectorPlaneProject(TargetRotation.GetAxisX(), GroundNormal).GetSafeNormal();

	if (Forward.IsNearlyZero())
	{
		return;
	}

	const FVector Right = FVector::CrossProduct(GroundNormal, Forward).GetSafeNormal();

	// 접지면과 평행한 속도로 계산한다.
	FVector GroundVelocity = FVector::VectorPlaneProject(Velocity, GroundNormal);

	float EffectiveThrottle = 0.f;
	float EffectiveBrake = 0.f;

	ResolveDriveControl(EffectiveThrottle, EffectiveBrake);

	const bool bBraking = EffectiveBrake > KINDA_SMALL_NUMBER;
	const bool bAccelerating = !FMath::IsNearlyZero(EffectiveThrottle);

	if (bBraking || !bAccelerating)
	{
		// 추진을 놓으면 자연 감속, 브레이크를 누르면 강한 감속
		const float Deceleration = FMath::Lerp(
			DriveConfig.CoastDeceleration,
			DriveConfig.FullBrakeDeceleration,
			EffectiveBrake);

		const float CurrentSpeed = GroundVelocity.Size();

		if (CurrentSpeed > KINDA_SMALL_NUMBER)
		{
			const float NewSpeed = FMath::Max(CurrentSpeed - Deceleration * DeltaTime, 0.f);

			GroundVelocity *= NewSpeed / CurrentSpeed;
		}
	}
	else
	{
		const float TravelSign = EffectiveThrottle > 0.f ? 1.f : -1.f;
		const float ForwardSpeed = FVector::DotProduct(GroundVelocity, Forward);
		// 요청한 추진 방향으로 얼마나 빨리 움직이고 있는지
		const float SpeedInRequestedDirection = ForwardSpeed * TravelSign;

		const float SpeedLimit = GetMaxSpeed();
		const float RemainingSpeed = FMath::Max(SpeedLimit - SpeedInRequestedDirection, 0.f);
		const float RequestedSpeedIncrease =
			DriveConfig.DriveAcceleration *
			FMath::Abs(EffectiveThrottle) *
			DeltaTime;
		const float AppliedSpeedIncrease = FMath::Min(RequestedSpeedIncrease, RemainingSpeed);

		GroundVelocity += Forward * TravelSign * AppliedSpeedIncrease;
	}

	// 드리프트 중에는 횡방향 그립을 낮춘다.
	const float GripMultiplier = IsDriftControlActive()
		? FMath::Clamp(DriveConfig.DriftFrictionMultiplier, 0.f, 1.f)
		: 1.f;

	const float Grip = FMath::Max(DriveConfig.LateralGrip * GripMultiplier, 0.f);
	const float LateralSpeed = FVector::DotProduct(GroundVelocity, Right);
	const float GripAlpha = 1.f - FMath::Exp(-Grip * DeltaTime);

	GroundVelocity -= Right * LateralSpeed * GripAlpha;

	Velocity = GroundVelocity;
}

void UMWVehicleMovementComponent::UpdateSteering(float DeltaTime)
{
	if (!UpdatedComponent)
	{
		return;
	}

	// 조향하지 않는 경우에는 현재 회전을 유지한다.
	TargetRotation = UpdatedComponent->GetComponentQuat();

	if (!bDriveConfigReady || !PawnOwner || DeltaTime <= 0.f)
	{
		return;
	}

	if (!bGrounded)
	{
		return;
	}

	const AController* Controller = PawnOwner->GetController();

	if (!Controller || Controller->IsMoveInputIgnored())
	{
		return;
	}

	const float Steering = DriveInput.Steering;

	if (FMath::IsNearlyZero(Steering))
	{
		return;
	}

	const float ForwardSpeed = FVector::DotProduct(Velocity, UpdatedComponent->GetForwardVector());

	const float SpeedMagnitude = FMath::Abs(ForwardSpeed);

	// 정지 또는 아주 느린 상태에서는 차체를 돌리지 않는다.
	if (SpeedMagnitude <= DriveConfig.MinSteeringSpeed)
	{
		return;
	}

	const float SpeedFactor = FMath::Clamp(
		SpeedMagnitude / FMath::Max(DriveConfig.FullSteeringSpeed, 1.f),
		0.f,
		1.f);

	// 후진 중에는 같은 조향 입력에 대한 차체 회전 방향이 반대다.
	const float TravelSign = ForwardSpeed >= 0.f ? 1.f : -1.f;

	const float TurnMultiplier = IsDriftControlActive()
		? FMath::Max(DriveConfig.DriftTurnRateMultiplier, 0.f)
		: 1.f;

	const float DeltaYaw =
		Steering *
		DriveConfig.TurnRateDegPerSecond *
		TurnMultiplier *
		SpeedFactor *
		TravelSign *
		DeltaTime;

	FRotator NewRotation = UpdatedComponent->GetComponentRotation();

	NewRotation.Yaw += DeltaYaw;

	const FQuat CandidateRotation = NewRotation.Quaternion();
	if (UpdatedPrimitive && GetWorld())
	{
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(PawnOwner);
		const FCollisionResponseParams Responses(UpdatedPrimitive->GetCollisionResponseToChannels());
		const FCollisionShape Shape = UpdatedPrimitive->GetCollisionShape();
		const FVector Location = UpdatedComponent->GetComponentLocation();
		// 이동 Sweep은 회전 궤적을 보장하지 않으므로 중간/최종 자세도 검사한다.
		const FQuat MidRotation = FQuat::Slerp(TargetRotation, CandidateRotation, 0.5f);
		if (GetWorld()->OverlapBlockingTestByChannel(Location, MidRotation,
			UpdatedPrimitive->GetCollisionObjectType(), Shape, QueryParams, Responses) ||
			GetWorld()->OverlapBlockingTestByChannel(Location, CandidateRotation,
			UpdatedPrimitive->GetCollisionObjectType(), Shape, QueryParams, Responses))
		{
			return;
		}
	}
	TargetRotation = CandidateRotation;
}

void UMWVehicleMovementComponent::UpdateGroundState()
{
	// 이번 검사에서 바닥을 찾지 못하면 비접지 상태로 남긴다.
	bGrounded = false;
	GroundNormal = FVector::UpVector;
	GroundDistance = 0.f;

	if (!bDriveConfigReady || !PawnOwner || !UpdatedComponent)
	{
		return;
	}

	UWorld* World = GetWorld();

	const UPrimitiveComponent* Collision = Cast<UPrimitiveComponent>(UpdatedComponent);

	if (!World || !Collision)
	{
		return;
	}

	const FVector Start = Collision->GetComponentLocation();
	const FVector End = Start - FVector::UpVector * DriveConfig.GroundProbeDistance;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(PawnOwner);
	const FCollisionResponseParams ResponseParams(Collision->GetCollisionResponseToChannels());

	FHitResult Hit;

	const bool bHit = World->SweepSingleByChannel(Hit, Start, End,
		Collision->GetComponentQuat(), Collision->GetCollisionObjectType(),
		Collision->GetCollisionShape(), QueryParams, ResponseParams);

	if (!bHit || !Hit.IsValidBlockingHit())
	{
		return;
	}

	const float MinGroundDot = FMath::Cos(FMath::DegreesToRadians(DriveConfig.MaxGroundAngle));
	const float GroundDot = FVector::DotProduct(Hit.ImpactNormal, FVector::UpVector);

	// 너무 가파른 면은 주행 가능한 바닥으로 취급하지 않는다.
	if (GroundDot < MinGroundDot)
	{
		return;
	}

	// 위로 향하는 외력이나 경사 끝에서 이탈하는 속도를 접지 처리로 지우지 않는다.
	if (FVector::DotProduct(Velocity, Hit.ImpactNormal) > DriveConfig.GroundDetachSpeed)
	{
		return;
	}

	bGrounded = true;
	GroundNormal = Hit.ImpactNormal;
	GroundDistance = Hit.Distance;
}

void UMWVehicleMovementComponent::UpdateAirVelocity(float DeltaTime)
{
	if (!bDriveConfigReady || !PawnOwner || !UpdatedComponent || DeltaTime <= 0.f)
	{
		return;
	}

	if (bGrounded)
	{
		return;
	}

	const float GravityZ = GetGravityZ() * DriveConfig.GravityScale;

	Velocity.Z += GravityZ * DeltaTime;
	Velocity.Z = FMath::Max(Velocity.Z, -DriveConfig.MaxFallSpeed);
}

void UMWVehicleMovementComponent::MoveVehicle(float DeltaTime)
{
	if (!bDriveConfigReady || !PawnOwner || !UpdatedComponent || DeltaTime <= 0.f)
	{
		return;
	}

	constexpr int32 MaxMoveAttempts = 3;
	TArray<FVector, TInlineAllocator<MaxMoveAttempts>> BlockingNormals;
	float RemainingTime = DeltaTime;
	FQuat MoveRotation = TargetRotation;
	for (int32 Attempt = 0; Attempt < MaxMoveAttempts; ++Attempt)
	{
		const FVector MoveDelta = Velocity * RemainingTime;
		FHitResult Hit(1.f);
		SafeMoveUpdatedComponent(MoveDelta, MoveRotation, true, Hit);
		if (!IsValid(PawnOwner) || !IsValid(UpdatedComponent))
		{
			return;
		}

		// SafeMove의 겹침 해소가 실패하면 막힌 이동 속도를 다음 스텝에 남기지 않는다.
		if (Hit.bStartPenetrating)
		{
			Velocity = FVector::ZeroVector;
			break;
		}
		if (!Hit.IsValidBlockingHit())
		{
			break;
		}

		HandleImpact(Hit, RemainingTime, MoveDelta);
		if (!IsValid(PawnOwner) || !IsValid(UpdatedComponent))
		{
			return;
		}

		const double IntoSurfaceSpeed = FVector::DotProduct(Velocity, Hit.Normal);
		Velocity -= Hit.Normal * FMath::Min(IntoSurfaceSpeed, 0.0);

		// 다음 벽에 투영한 속도가 앞선 벽 안쪽을 향하면 두 면의 교선으로 제한한다.
		for (const FVector& PreviousNormal : BlockingNormals)
		{
			if (FVector::DotProduct(Velocity, PreviousNormal) < -KINDA_SMALL_NUMBER)
			{
				const FVector Edge = FVector::CrossProduct(PreviousNormal, Hit.Normal).GetSafeNormal();
				Velocity = Edge * FVector::DotProduct(Velocity, Edge);
				break;
			}
		}
		BlockingNormals.Add(Hit.Normal);

		if (BlockingNormals.ContainsByPredicate([this](const FVector& Normal)
		{
			return FVector::DotProduct(Velocity, Normal) < -KINDA_SMALL_NUMBER;
		}))
		{
			Velocity = FVector::ZeroVector;
			break;
		}

		RemainingTime *= 1.f - Hit.Time;
		if (RemainingTime <= SMALL_NUMBER || Velocity.IsNearlyZero())
		{
			break;
		}
		MoveRotation = UpdatedComponent->GetComponentQuat();
	}
}

void UMWVehicleMovementComponent::AdjustGroundHeight()
{
	if (!bDriveConfigReady || !PawnOwner || !UpdatedComponent || !bGrounded)
	{
		return;
	}
	// 착지로 간격이 작아졌을 때도 위로 보정해 목표 간격을 회복한다.
	const float CorrectionDistance = GroundDistance - DriveConfig.GroundClearance;
	if (FMath::Abs(CorrectionDistance) <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	FHitResult Hit(1.f);
	SafeMoveUpdatedComponent(-FVector::UpVector * CorrectionDistance,
		UpdatedComponent->GetComponentQuat(), true, Hit);
	// 간격 보정은 주행 속도에 합산하지 않는다.
	UpdateGroundState();
}
