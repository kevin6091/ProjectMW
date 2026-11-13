

#include "Gameplay/Vehicle/MWVehicleMovementComponent.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Components/PrimitiveComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "Engine/ScopedMovementUpdate.h"
#include "CollisionShape.h"
#include "Math/RotationMatrix.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogMWVehicleSupport, Log, All);

namespace
{
	constexpr ECollisionChannel VehicleGroundChannel = ECC_GameTraceChannel1;
	// 접지 검사마다 기록하므로 평소에는 끄고 재현 테스트에서만 활성화한다.
	static TAutoConsoleVariable<int32> CVarVehicleSupportLog(
		TEXT("mw.Vehicle.SupportLog"), 0,
		TEXT("Vehicle ground support diagnostics: 0=off, 1=log every ground query."));
}

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
	UpdateGroundRotation(DeltaTime);
	// 자세와 중심 위치가 바뀌었으므로 다시 검사한다.
	UpdateGroundState();
	
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
	OutSnapshot.GroundNormal = GroundNormal.GetSafeNormal();
}

void UMWVehicleMovementComponent::ApplyDriveSnapshot(const FMWVehicleDriveSnapshot& Snapshot)
{
	// 서버 위치를 복원할 때 Sweep하거나 Pawn의 텔레포트 콜백으로 입력을 초기화하지 않는다.
	UpdatedComponent->SetWorldLocationAndRotation(Snapshot.Location, Snapshot.Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	Velocity = Snapshot.Velocity;
	bGrounded = Snapshot.bGrounded;
	// 압축 전송된 법선은 정규화해서 계산에 사용한다.
	GroundNormal = FVector(Snapshot.GroundNormal).GetSafeNormal();
	
	if (GroundNormal.IsNearlyZero())
	{
		GroundNormal = GetGroundProbeUp();
		bGrounded = false;
	}
	
	GroundDistance = 0.f;
	GroundNormalLoadAcceleration = 0.0;
	
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
	GroundNormalLoadAcceleration = 0.0;
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

FQuat UMWVehicleMovementComponent::CalculateSteeringRotation(const FQuat& BaseRotation, float DeltaTime) const
{
	if (!bDriveConfigReady || !PawnOwner || !bGrounded || DeltaTime <= 0.f)
	{
		return BaseRotation;
	}
	
	const AController* Controller = PawnOwner->GetController();

	if (!Controller || Controller->IsMoveInputIgnored())
	{
		return BaseRotation;
	}
	
	const float Steering = DriveInput.Steering;

	if (FMath::IsNearlyZero(Steering))
	{
		// 조향이 없어도 앞에서 계산한 노면 정렬은 유지한다.
		return BaseRotation;
	}
	
	const FVector Up = GroundNormal.GetSafeNormal();

	const FVector Forward = FVector::VectorPlaneProject(BaseRotation.GetAxisX(), Up).GetSafeNormal();
	
	if (Up.IsNearlyZero() || Forward.IsNearlyZero())
	{
		return BaseRotation;
	}
	
	// 노면을 따라가는 전방 속도로 조향 강도를 계산한다.
	const float ForwardSpeed = FVector::DotProduct(Velocity, Forward);
	
	const float SpeedMagnitude = FMath::Abs(ForwardSpeed);
	
	if (SpeedMagnitude <= DriveConfig.MinSteeringSpeed)
	{
		return BaseRotation;
	}

	const float SpeedFactor = FMath::Clamp(
		SpeedMagnitude / FMath::Max(DriveConfig.FullSteeringSpeed, 1.f),
		0.f,
		1.f);

	// 후진할 때는 조향 방향을 반대로 적용한다.
	const float TravelSign = ForwardSpeed >= 0.f ? 1.f : -1.f;

	const float TurnMultiplier = IsDriftControlActive()
		? FMath::Max(DriveConfig.DriftTurnRateMultiplier, 0.f)
		: 1.f;

	const float DeltaAngleDegrees =
		Steering
		* DriveConfig.TurnRateDegPerSecond
		* TurnMultiplier
		* SpeedFactor
		* TravelSign
		* DeltaTime;

	// 월드 Z 대신 현재 노면 법선을 회전축으로 사용한다.
	const FQuat SteeringRotation(Up, FMath::DegreesToRadians(DeltaAngleDegrees));

	// 월드 공간의 노면 축 회전을 기준 자세에 적용한다.
	return (SteeringRotation * BaseRotation).GetNormalized();
}

FVector UMWVehicleMovementComponent::CalculateRotationGroundOffset(const FQuat& FromRotation,
	const FQuat& ToRotation) const
{
	if (!bGrounded || !UpdatedPrimitive)
	{
		return FVector::ZeroVector;
	}
	
	const FVector Normal = GroundNormal.GetSafeNormal();

	if (Normal.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}
	
	const FCollisionShape Shape = UpdatedPrimitive->GetCollisionShape();

	if (!Shape.IsBox())
	{
		return FVector::ZeroVector;
	}
	
	// 실제 충돌체 크기를 사용하므로 BP에서 조정한 크기도 반영된다.
	const FVector Extent = Shape.GetBox();

	const auto GetSupportDistance = [&](const FQuat& Rotation) -> double
	{
		// 각 Box 축이 노면 법선 방향으로 차지하는 길이를 합한다.
		return Extent.X * FMath::Abs(FVector::DotProduct(Rotation.GetAxisX(), Normal))
			+ Extent.Y * FMath::Abs(FVector::DotProduct(Rotation.GetAxisY(), Normal))
			+ Extent.Z * FMath::Abs(FVector::DotProduct(Rotation.GetAxisZ(), Normal));
	};	

	const double Before = GetSupportDistance(FromRotation);
	const double After = GetSupportDistance(ToRotation);

	return Normal * (After - Before);
}

bool UMWVehicleMovementComponent::IsVehiclePoseBlocked(const FVector& Location, const FQuat& Rotation, float ShapeInflation) const
{
	const UWorld* World = GetWorld();

	if (!World || !PawnOwner || !UpdatedPrimitive)
	{
		// 검사가 불가능한 상태에서는 자세 변경을 허용하지 않는다.
		return true;
	}
	
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(PawnOwner);
	
	const FCollisionResponseParams Responses(UpdatedPrimitive->GetCollisionResponseToChannels());
	
	return World->OverlapBlockingTestByChannel(
		Location,
		Rotation,
		UpdatedPrimitive->GetCollisionObjectType(),
		UpdatedPrimitive->GetCollisionShape(FMath::Max(ShapeInflation, 0.f)),
		QueryParams,
		Responses);
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
	
	const FVector Gravity(0.0, 0.0, GetGravityZ() * DriveConfig.GravityScale);
	// 중력 중 노면과 평행한 성분만 지상 이동에 반영한다.
	const FVector TangentialGravity = FVector::VectorPlaneProject(Gravity, GroundNormal);
	
	GroundVelocity += TangentialGravity * DeltaTime;

	// 이후 엔진·브레이크·횡방향 그립이 요청하는 속도 변화를
	// 분리해서 계산하기 위한 기준값이다.
	const FVector VelocityBeforeTireForces = GroundVelocity;

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

	const FVector RequestedTireDeltaVelocity = GroundVelocity - VelocityBeforeTireForces;

	const double MaxTireAcceleration = DriveConfig.TireFrictionCoefficient * GroundNormalLoadAcceleration;
	const double MaxTireDeltaSpeed = MaxTireAcceleration * DeltaTime;

	// 종방향과 횡방향 힘이 하나의 접지 한계를 공유하도록 한다.
	const FVector AppliedTireDeltaVelocity = RequestedTireDeltaVelocity.GetClampedToMaxSize(MaxTireDeltaSpeed);

	Velocity = VelocityBeforeTireForces + AppliedTireDeltaVelocity;
}

void UMWVehicleMovementComponent::UpdateGroundState()
{
	const bool bWasGrounded = bGrounded;
	const FVector PreviousNormal = GroundNormal.GetSafeNormal();
	FGroundSupportDiagnostics Diagnostics;
	
	// 이번 검사에서 바닥을 찾지 못하면 비접지 상태로 남긴다.
	bGrounded = false;
	GroundNormal = GetGroundProbeUp();
	GroundDistance = 0.f;
	GroundNormalLoadAcceleration = 0.0;
	
	FHitResult Candidate;
	
	if (!FindGroundCandidate(Candidate))
	{
		LogGroundSupport(TEXT("NoCandidate"), bWasGrounded, nullptr, Diagnostics);
		return;
	}
	
	const FVector CandidateNormal = FVector(Candidate.ImpactNormal).GetSafeNormal();
	
	if (CandidateNormal.IsNearlyZero())
	{
		LogGroundSupport(TEXT("InvalidNormal"), bWasGrounded, &Candidate, Diagnostics);
		return;
	}
	
	const double OutwardSpeed = FVector::DotProduct(Velocity, Candidate.ImpactNormal);
	
	if (!bWasGrounded && OutwardSpeed > DriveConfig.GroundDetachSpeed)
	{
		LogGroundSupport(TEXT("OutwardVelocity"), bWasGrounded, &Candidate, Diagnostics);
		return;
	}
	
	FVector AcceptedVelocity = Velocity;
	
	if (bWasGrounded)
	{
		// 기존 접지에서 연속된 면으로 이어지는 경우.
		if (PreviousNormal.IsNearlyZero())
		{
			LogGroundSupport(TEXT("InvalidPreviousNormal"), bWasGrounded, &Candidate, Diagnostics);
			return;
		}

		const float MinContinuityDot = FMath::Cos(FMath::DegreesToRadians(DriveConfig.MaxGroundAngle));

		if (FVector::DotProduct(PreviousNormal, CandidateNormal) < MinContinuityDot)
		{
			LogGroundSupport(TEXT("DiscontinuousNormal"), bWasGrounded, &Candidate, Diagnostics);
			return;
		}

		const FVector PreviousTangentVelocity = FVector::VectorPlaneProject(Velocity, PreviousNormal);

		if (!TryTransportGroundTangentVelocity(
			PreviousTangentVelocity,
			PreviousNormal,
			CandidateNormal,
			AcceptedVelocity))
		{
			LogGroundSupport(TEXT("InvalidTransport"), bWasGrounded, &Candidate, Diagnostics);
			return;
		}
	}
	else
	{
		// 착지는 현재 공중 속도를 새 노면에 투영한다.
		AcceptedVelocity = FVector::VectorPlaneProject(Velocity, CandidateNormal);
	}
	
	const double NormalLoad = CalculateGroundNormalLoadAcceleration(Candidate, AcceptedVelocity, Diagnostics);

	if (!FMath::IsFinite(NormalLoad))
	{
		LogGroundSupport(TEXT("NonFiniteLoad"), bWasGrounded, &Candidate, Diagnostics);
		return;
	}
	
	// 유지 중인 접지에만 작은 수치 오차를 허용한다.
	// 새로 착지할 때는 음수 하중을 허용하지 않는다.
	const double MinimumAllowedLoad = bWasGrounded
		? -static_cast<double>(DriveConfig.GroundSupportTolerance)
		: 0.0;

	if (NormalLoad < MinimumAllowedLoad)
	{
		LogGroundSupport(TEXT("InsufficientLoad"), bWasGrounded, &Candidate, Diagnostics);
		// 아직 Velocity를 투영한 값으로 덮어쓰지 않았으므로,
		// 기존 속도를 유지한 채 공중 이동으로 넘어간다.
		return;
	}

	// 모든 검사가 끝난 뒤 상태와 속도를 함께 확정한다.
	Velocity = AcceptedVelocity;
	bGrounded = true;
	GroundNormal = Candidate.ImpactNormal;
	GroundDistance = Candidate.Distance;
	
	GroundNormalLoadAcceleration = FMath::Max(NormalLoad, 0.0);
	LogGroundSupport(TEXT("Accepted"), bWasGrounded, &Candidate, Diagnostics);
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

void UMWVehicleMovementComponent::ApplyExternalVelocityChange(const FVector& DeltaVelocity)
{
	if (!bDriveConfigReady || DeltaVelocity.ContainsNaN())
	{
		return;
	}
	
	const bool bHadGroundContact = bGrounded;
	const FVector PreviousVelocity = Velocity;

	const FVector ContactNormal = GroundNormal.GetSafeNormal();
	
	Velocity += DeltaVelocity;

	if (bHadGroundContact && !ContactNormal.IsNearlyZero())
	{
		const double AddedOutwardSpeed = FVector::DotProduct(DeltaVelocity, ContactNormal);

		const double TotalOutwardSpeed = FVector::DotProduct(Velocity, ContactNormal);

		// 바깥쪽 충격이 실제 분리 속도를 만들었을 때 해제한다.
		if (AddedOutwardSpeed > 0.f && TotalOutwardSpeed > DriveConfig.GroundDetachSpeed)
		{
			bGrounded = false;
			GroundDistance = 0.f;
			GroundNormalLoadAcceleration = 0.0;
		}
	}

	if (CVarVehicleSupportLog.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogMWVehicleSupport, Log,
			TEXT("VehicleExternalVelocity Time=%.6f Frame=%llu Actor=%s WasGrounded=%d Grounded=%d ")
			TEXT("BeforeVX=%.6f BeforeVY=%.6f BeforeVZ=%.6f DX=%.6f DY=%.6f DZ=%.6f ")
			TEXT("VX=%.6f VY=%.6f VZ=%.6f"),
			GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0, GFrameCounter, *GetPathNameSafe(PawnOwner),
			bHadGroundContact, bGrounded, PreviousVelocity.X, PreviousVelocity.Y, PreviousVelocity.Z,
			DeltaVelocity.X, DeltaVelocity.Y, DeltaVelocity.Z, Velocity.X, Velocity.Y, Velocity.Z);
	}
	UpdateComponentVelocity();
}

void UMWVehicleMovementComponent::ResolveContactVelocity(const FHitResult& Hit)
{
	if (!bDriveConfigReady || !Hit.IsValidBlockingHit())
	{
		return;
	}
	
	const FVector CollisionNormal = FVector(Hit.Normal).GetSafeNormal();

	if (CollisionNormal.IsNearlyZero())
	{
		return;
	}
	
	const double IntoSurfaceSpeed = FVector::DotProduct(Velocity, CollisionNormal);

	if (IntoSurfaceSpeed >= 0.f)
	{
		return;
	}
	
	const FVector DeltaVelocity = -CollisionNormal * IntoSurfaceSpeed;
	const UPrimitiveComponent* HitComponent = Hit.GetComponent();
	const FVector SurfaceNormal = FVector(Hit.ImpactNormal).GetSafeNormal();
	const FVector ReferenceUp = bGrounded ? GroundNormal.GetSafeNormal() : GetGroundProbeUp();

	const float MinGroundDot = FMath::Cos(FMath::DegreesToRadians(DriveConfig.MaxGroundAngle));

	const bool bSupportContact =
		HitComponent
		&& HitComponent->GetCollisionResponseToChannel(VehicleGroundChannel) == ECR_Block
		&& !SurfaceNormal.IsNearlyZero()
		&& !ReferenceUp.IsNearlyZero()
		&& FVector::DotProduct(SurfaceNormal, ReferenceUp) >= MinGroundDot;

	// 접지 검사 사이에서 충돌 보정이 접지를 해제하는 경우도 추적한다.
	// Hit.Normal과 ImpactNormal이 다를 수 있으므로 둘을 구분해 기록한다.
	if (CVarVehicleSupportLog.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogMWVehicleSupport, Log,
			TEXT("VehicleContact Time=%.6f Frame=%llu Actor=%s HitActor=%s Component=%s Support=%d ")
			TEXT("Into=%.6f NX=%.6f NY=%.6f NZ=%.6f ImpactNX=%.6f ImpactNY=%.6f ImpactNZ=%.6f ")
			TEXT("BeforeVX=%.6f BeforeVY=%.6f BeforeVZ=%.6f DX=%.6f DY=%.6f DZ=%.6f"),
			GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0, GFrameCounter, *GetPathNameSafe(PawnOwner),
			*GetPathNameSafe(Hit.GetActor()), *GetPathNameSafe(HitComponent), bSupportContact,
			IntoSurfaceSpeed, CollisionNormal.X, CollisionNormal.Y, CollisionNormal.Z,
			SurfaceNormal.X, SurfaceNormal.Y, SurfaceNormal.Z,
			Velocity.X, Velocity.Y, Velocity.Z, DeltaVelocity.X, DeltaVelocity.Y, DeltaVelocity.Z);
	}
	
	if (bSupportContact)
	{
		// 도로면을 따라가도록 접촉 속도를 보정한다.
		Velocity += DeltaVelocity;
	}
	else
	{
		// 장애물 접촉으로 분리 속도가 생겼다면 접지를 해제한다.
		ApplyExternalVelocityChange(DeltaVelocity);
	}
}

bool UMWVehicleMovementComponent::FindGroundCandidate(FHitResult& OutHit) const
{
	OutHit.Reset();

	if (!bDriveConfigReady || !PawnOwner || !UpdatedComponent)
	{
		return false;
	}
	
	const UWorld* World = GetWorld();

	const UPrimitiveComponent* Collision = Cast<UPrimitiveComponent>(UpdatedComponent);

	if (!World || !Collision)
	{
		return false;
	}
	
	const FVector ProbeUp = GetGroundProbeUp();
	const FVector Start = Collision->GetComponentLocation();

	const FVector End = Start - ProbeUp * DriveConfig.GroundProbeDistance;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(PawnOwner);

	const float MinGroundDot = FMath::Cos(FMath::DegreesToRadians(DriveConfig.MaxGroundAngle));

	const FCollisionShape Shape = Collision->GetCollisionShape();
	
	const auto SweepGround = [&](const FCollisionShape& ProbeShape) -> bool
	{
		return World->SweepSingleByChannel(
			OutHit,
			Start,
			End,
			Collision->GetComponentQuat(),
			VehicleGroundChannel,
			ProbeShape,
			QueryParams);
	};

	if (!SweepGround(Shape))
	{
		return false;
	}

	if (OutHit.bStartPenetrating && Shape.IsBox() && FVector::DotProduct(OutHit.ImpactNormal, ProbeUp) < MinGroundDot)
	{
		constexpr double SideInset = 0.5;

		FVector ProbeExtent = Shape.GetBox();

		ProbeExtent.X -= FMath::Min(SideInset, ProbeExtent.X * 0.5);
		ProbeExtent.Y -= FMath::Min(SideInset, ProbeExtent.Y * 0.5);

		OutHit.Reset();

		if (!SweepGround(FCollisionShape::MakeBox(ProbeExtent)))
		{
			return false;
		}
	}

	if (!OutHit.IsValidBlockingHit())
	{
		return false;
	}

	// 차량의 검사 방향에서 도로 후보로 인정할 수 있는 면인지 확인한다.
	return FVector::DotProduct(OutHit.ImpactNormal, ProbeUp) >= MinGroundDot;
}

void UMWVehicleMovementComponent::UpdateGroundRotation(float DeltaTime)
{
	if (!UpdatedComponent)
	{
		return;
	}
	
	// 공중이거나 자세 변경이 실패해도 실제 회전과 일치시킨다.
	TargetRotation = UpdatedComponent->GetComponentQuat();
	
	if (!bDriveConfigReady || !PawnOwner || !UpdatedPrimitive || !bGrounded || DeltaTime <= 0.f)
	{
		return;
	}

	const FCollisionShape Shape = UpdatedPrimitive->GetCollisionShape();

	if (!Shape.IsBox())
	{
		return;
	}
	
	const FVector Normal = GroundNormal.GetSafeNormal();

	if (Normal.IsNearlyZero())
	{
		return;
	}
	
	const FQuat StartRotation = TargetRotation;

	const FQuat AlignedRotation = CalculateGroundAlignRotation(StartRotation, DeltaTime);

	const FQuat DesiredRotation = CalculateSteeringRotation(AlignedRotation, DeltaTime);

	const double AngleDegrees = FMath::RadiansToDegrees(StartRotation.AngularDistance(DesiredRotation));
	
	if (AngleDegrees <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	
	// 일반적인 회전은 약 1도 단위로 나눈다.
	const int32 Steps = FMath::Clamp(FMath::CeilToInt(AngleDegrees / 1.0), 1, 16);
	
	const double Radius = Shape.GetBox().Size();

	FScopedMovementUpdate ScopedMove(UpdatedComponent, EScopedUpdate::DeferredUpdates);
	
	const auto MoveTo = [&](const FVector& Location, const FQuat& Rotation) -> bool
	{
		FHitResult Hit;

		SafeMoveUpdatedComponent(
			Location - UpdatedComponent->GetComponentLocation(),
			Rotation,
			true,
			Hit);

		return IsValid(UpdatedComponent)
			&& !Hit.bStartPenetrating
			&& !Hit.IsValidBlockingHit()
			&& UpdatedComponent->GetComponentLocation().Equals(
				Location, 0.01)
			&& UpdatedComponent->GetComponentQuat().Equals(
				Rotation, 0.0001);
	};
	
	for (int32 Step = 1; Step <= Steps; ++Step)
    {
        const FVector FromLocation = UpdatedComponent->GetComponentLocation();

        const FQuat FromRotation = UpdatedComponent->GetComponentQuat();

        const FQuat ToRotation = FQuat::Slerp(
            StartRotation,
            DesiredRotation,
            static_cast<double>(Step) / Steps).GetNormalized();

        const FQuat MidRotation = FQuat::Slerp(
            FromRotation,
            ToRotation,
            0.5).GetNormalized();

        const FVector Offset = CalculateRotationGroundOffset(FromRotation, ToRotation);

        const FVector MidOffset = CalculateRotationGroundOffset(FromRotation, MidRotation);

        // 중간 자세에서 이 구간의 회전 궤적을 덮도록
        // 검사 Box의 각 반크기에 여유를 추가한다.
        const double StepAngle = FromRotation.AngularDistance(ToRotation);

        const double Padding = 2.0 * Radius * FMath::Sin(StepAngle * 0.25) + 0.01;

        const double NormalProjection =
            FMath::Abs(FVector::DotProduct(
                MidRotation.GetAxisX(), Normal))
            + FMath::Abs(FVector::DotProduct(
                MidRotation.GetAxisY(), Normal))
            + FMath::Abs(FVector::DotProduct(
                MidRotation.GetAxisZ(), Normal));

        // 중간 회전과 검사 여유까지 수용할 간격을 확보한다.
        const double Lift = FMath::Max(
            0.0,
            FMath::Max(
                FVector::DotProduct(Offset, Normal),
                FVector::DotProduct(MidOffset, Normal) + Padding * NormalProjection));

        const FVector RaisedLocation = FromLocation + Normal * Lift;

        const FVector ToLocation = FromLocation + Offset;

        const bool bRotationBlocked = IsVehiclePoseBlocked(
            RaisedLocation,
            MidRotation,
            static_cast<float>(Padding));

        if (bRotationBlocked
            || !MoveTo(RaisedLocation, FromRotation)
            || !MoveTo(RaisedLocation, ToRotation)
            || !MoveTo(ToLocation, ToRotation))
        {
            // 간격 확보와 앞 구간의 회전까지 모두 되돌린다.
            ScopedMove.RevertMove();

            TargetRotation = UpdatedComponent->GetComponentQuat();
        	
            return;
        }
    }

    TargetRotation = UpdatedComponent->GetComponentQuat();
}

bool UMWVehicleMovementComponent::TryTransportGroundTangentVelocity(
	const FVector& InTangentVelocity,
	const FVector& FromNormal, 
	const FVector& ToNormal, 
	FVector& OutTangentVelocity)
{
	// 실패할 경우 입력값을 유지한다.
	OutTangentVelocity = InTangentVelocity;

	if (InTangentVelocity.ContainsNaN() || FromNormal.ContainsNaN() || ToNormal.ContainsNaN())
	{
		return false;
	}
	
	const FVector OldNormal = FromNormal.GetSafeNormal();
	const FVector NewNormal = ToNormal.GetSafeNormal();

	if (OldNormal.IsNearlyZero() || NewNormal.IsNearlyZero())
	{
		return false;
	}
	
	// 급격하게 다른 방향의 면을 연속된 노면으로 연결하지 않는다.
	// 최종 허용 각도·거리 조건은 접지 유지 판정에서 추가한다.
	if (FVector::DotProduct(OldNormal, NewNormal) <= 0.f)
	{
		return false;
	}
	
	// 이전 노면에 평행한 성분만 운반한다.
	const FVector OldTangentVelocity = FVector::VectorPlaneProject(InTangentVelocity, OldNormal);
	
	// 이전 노면 법선을 새 노면 법선으로 보내는 최소 회전.
	const FQuat SurfaceRotation = FQuat::FindBetweenNormals(OldNormal, NewNormal);
	
	OutTangentVelocity = SurfaceRotation.RotateVector(OldTangentVelocity);

	return !OutTangentVelocity.ContainsNaN();
}

double UMWVehicleMovementComponent::CalculateDownforceAcceleration(double TangentSpeed) const
{
	if (!bDriveConfigReady || !FMath::IsFinite(TangentSpeed))
	{
		return 0.0;
	}

	const double ReferenceSpeed = DriveConfig.DownforceReferenceSpeed;

	if (ReferenceSpeed <= 0.0)
	{
		return 0.0;
	}
	
	const double SpeedRatio = FMath::Max(TangentSpeed, 0.0) / ReferenceSpeed;

	const double Acceleration = DriveConfig.DownforceAtReferenceSpeed * FMath::Square(SpeedRatio);

	return FMath::Clamp(Acceleration, 0.0, static_cast<double>(DriveConfig.MaxDownforceAcceleration));
}

double UMWVehicleMovementComponent::EstimateRequiredNormalAcceleration(const FHitResult& Contact,
	const FVector& TangentVelocity, FGroundSupportDiagnostics& Diagnostics) const
{
	UWorld* World = GetWorld();

	if (!World || !PawnOwner)
	{
		Diagnostics.CurvatureResult = TEXT("NoWorld");
		return 0.0;
	}

	const double SpeedSquared = TangentVelocity.SizeSquared();

	// 정지 상태에서는 경로를 따라 휘기 위한 가속도가 필요 없다.
	if (SpeedSquared <= KINDA_SMALL_NUMBER)
	{
		Diagnostics.CurvatureResult = TEXT("Stationary");
		return 0.0;
	}

	const FVector Normal = FVector(Contact.ImpactNormal).GetSafeNormal();
	const FVector TravelDirection = TangentVelocity.GetSafeNormal();
	
	if (Normal.IsNearlyZero() || TravelDirection.IsNearlyZero())
	{
		Diagnostics.CurvatureResult = TEXT("InvalidBasis");
		return 0.0;
	}

	const double ProbeDistance = DriveConfig.GroundCurvatureProbeDistance;
	const double ProbeHeight = DriveConfig.GroundCurvatureProbeHeight;
	const FVector ProbeCenter = Contact.ImpactPoint + TravelDirection * ProbeDistance;

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(VehicleGroundCurvature),
		true,
		PawnOwner);

	FHitResult AheadHit;
	
	const bool bHit = World->LineTraceSingleByChannel(
		AheadHit,
		ProbeCenter + Normal * ProbeHeight,
		ProbeCenter - Normal * ProbeHeight,
		VehicleGroundChannel,
		QueryParams);

	if (!bHit || !AheadHit.IsValidBlockingHit())
	{
		Diagnostics.CurvatureResult = TEXT("NoAheadHit");
		// 추가 정보를 얻지 못하면 현재 면을 평면으로 근사한다.
		// 이것만으로 먼 도로에 접지시키지는 않는다.
		return 0.0;
	}

	const FVector AheadNormal = FVector(AheadHit.ImpactNormal).GetSafeNormal();
	Diagnostics.AheadNormal = AheadNormal;

	if (AheadNormal.IsNearlyZero())
	{
		Diagnostics.CurvatureResult = TEXT("InvalidAheadNormal");
		return 0.0;
	}

	const double MinContinuityDot = FMath::Cos(FMath::DegreesToRadians(static_cast<double>(DriveConfig.MaxGroundAngle)));

	// 현재 도로와 지나치게 다른 방향의 면은 곡률 추정에서 제외한다.
	if (FVector::DotProduct(Normal, AheadNormal) < MinContinuityDot)
	{
		Diagnostics.CurvatureResult = TEXT("DiscontinuousAheadNormal");
		return 0.0;
	}

	const double SampleDistance = FVector::DotProduct(AheadHit.ImpactPoint - Contact.ImpactPoint, TravelDirection);
	Diagnostics.SampleDistance = SampleDistance;

	if (SampleDistance <= 1.0)
	{
		Diagnostics.CurvatureResult = TEXT("InvalidSampleDistance");
		return 0.0;
	}

	// 오목한 루프 안쪽은 양수, 볼록한 언덕 정상은 음수가 된다.
	const double SignedCurvature = -FVector::DotProduct(AheadNormal - Normal, TravelDirection) / SampleDistance;
	Diagnostics.CurvatureResult = TEXT("Sampled");

	// 원형 경로에서는 v²/R에 해당한다.
	return SpeedSquared * SignedCurvature;
}

double UMWVehicleMovementComponent::CalculateGroundNormalLoadAcceleration(const FHitResult& Contact,
	const FVector& TangentVelocity, FGroundSupportDiagnostics& Diagnostics) const
{
	const FVector Normal = FVector(Contact.ImpactNormal).GetSafeNormal();
	const FVector Gravity(0.0, 0.0, GetGravityZ() * DriveConfig.GravityScale);
	
	const double RequiredNormalAcceleration = EstimateRequiredNormalAcceleration(Contact, TangentVelocity, Diagnostics);
	const double GravityNormalAcceleration = FVector::DotProduct(Gravity, Normal);
	const double DownforceAcceleration = CalculateDownforceAcceleration(TangentVelocity.Size());

	// 제한 전의 음수 하중도 보존해야 이탈 원인을 구분할 수 있다.
	Diagnostics.bEvaluated = true;
	Diagnostics.TangentSpeed = TangentVelocity.Size();
	Diagnostics.RequiredAcceleration = RequiredNormalAcceleration;
	Diagnostics.GravityNormalAcceleration = GravityNormalAcceleration;
	Diagnostics.DownforceAcceleration = DownforceAcceleration;
	Diagnostics.NormalLoadAcceleration = RequiredNormalAcceleration - GravityNormalAcceleration + DownforceAcceleration;
	return Diagnostics.NormalLoadAcceleration;
}

void UMWVehicleMovementComponent::LogGroundSupport(const TCHAR* Decision, bool bWasGrounded,
	const FHitResult* Candidate, const FGroundSupportDiagnostics& Diagnostics) const
{
	if (CVarVehicleSupportLog.GetValueOnGameThread() == 0)
	{
		return;
	}

	// 한 이동 단계에서도 자세/위치 보정 전후로 여러 번 검사한다.
	// Seq와 Time으로 호출을 구분하며, 로그를 위해 추가 충돌 검사를 수행하지 않는다.
	static uint64 QuerySequence = 0;
	const UWorld* World = GetWorld();
	const FVector Position = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
	const FVector Normal = Candidate ? FVector(Candidate->ImpactNormal) : GroundNormal;
	const double Limit = bWasGrounded ? -DriveConfig.GroundSupportTolerance : 0.0;
	UE_LOG(LogMWVehicleSupport, Log,
		TEXT("VehicleSupport Seq=%llu Time=%.6f Frame=%llu NetMode=%d Role=%d Actor=%s Decision=%s ")
		TEXT("WasGrounded=%d Grounded=%d Evaluated=%d Curvature=%s Speed=%.6f Required=%.6f ")
		TEXT("GravityNormal=%.6f Downforce=%.6f Load=%.6f Limit=%.6f Distance=%.6f SampleDistance=%.6f ")
		TEXT("PX=%.6f PY=%.6f PZ=%.6f NX=%.6f NY=%.6f NZ=%.6f ")
		TEXT("AheadNX=%.6f AheadNY=%.6f AheadNZ=%.6f VX=%.6f VY=%.6f VZ=%.6f"),
		++QuerySequence, World ? World->GetTimeSeconds() : 0.0, GFrameCounter,
		static_cast<int32>(GetNetMode()), PawnOwner ? static_cast<int32>(PawnOwner->GetLocalRole()) : -1,
		*GetPathNameSafe(PawnOwner), Decision, bWasGrounded, bGrounded, Diagnostics.bEvaluated,
		Diagnostics.CurvatureResult, Diagnostics.bEvaluated ? Diagnostics.TangentSpeed : Velocity.Size(),
		Diagnostics.RequiredAcceleration, Diagnostics.GravityNormalAcceleration, Diagnostics.DownforceAcceleration,
		Diagnostics.NormalLoadAcceleration, Limit, Candidate ? Candidate->Distance : -1.0,
		Diagnostics.SampleDistance, Position.X, Position.Y, Position.Z, Normal.X, Normal.Y, Normal.Z,
		Diagnostics.AheadNormal.X, Diagnostics.AheadNormal.Y, Diagnostics.AheadNormal.Z,
		Velocity.X, Velocity.Y, Velocity.Z);
}

void UMWVehicleMovementComponent::MoveVehicle(float DeltaTime)
{
	if (!bDriveConfigReady || !PawnOwner || !UpdatedComponent || DeltaTime <= 0.f)
	{
		return;
	}

	const FVector MoveDelta = Velocity * DeltaTime;

	FHitResult Hit(1.f);

	SafeMoveUpdatedComponent(MoveDelta, TargetRotation, true, Hit);

	if (Hit.IsValidBlockingHit())
	{
		HandleImpact(Hit, DeltaTime, MoveDelta);
		if (!IsValid(PawnOwner) || !IsValid(UpdatedComponent))
		{
			return;
		}

		if (TryResolveVehicleContact(Hit))
		{
			return;
		}

		ResolveContactVelocity(Hit);

		// 처음 요청한 이동 중 남은 부분을 표면을 따라 시도한다.
		SlideAlongSurface(MoveDelta, 1.f - Hit.Time, Hit.Normal, Hit, true);

		// 미끄러지는 도중 다른 면에도 막혔다면 속도를 정리한다.
		if (Hit.IsValidBlockingHit())
		{
			ResolveContactVelocity(Hit);
		}
	}
}

bool UMWVehicleMovementComponent::TryResolveVehicleContact(const FHitResult& Hit)
{
	if (!IsValid(PawnOwner) || !PawnOwner->HasAuthority() || !Hit.IsValidBlockingHit())
	{
		return false;
	}

	const AMWVehiclePawn* OtherPawn = Cast<AMWVehiclePawn>(Hit.GetActor());
	if (!IsValid(OtherPawn) || OtherPawn == PawnOwner)
	{
		return false;
	}

	auto* Other = Cast<UMWVehicleMovementComponent>(OtherPawn->GetMovementComponent());
	if (!IsValid(Other) ||
		!Other->bDriveConfigReady ||
		!IsValid(Other->UpdatedPrimitive) ||
		Hit.GetComponent() != Other->UpdatedPrimitive)
	{
		return false;
	}

	// B 표면에서 A를 향하는 수평 법선. 수직 접촉은 기존 처리로 보낸다.
	const FVector Normal = FVector(Hit.ImpactNormal).GetSafeNormal2D();
	if (Normal.IsNearlyZero())
	{
		return false;
	}

	const double SpeedA = FVector::DotProduct(Velocity, Normal);
	const double SpeedB = FVector::DotProduct(Other->Velocity, Normal);
	const double ClosingSpeed = SpeedB - SpeedA;
	if (ClosingSpeed <= KINDA_SMALL_NUMBER)
	{
		return true;
	}

	// 저속에서도 서로 밀 수 있다. 반발만 일정 접근 속도 이상에서 적용한다.
	const double MinBounceSpeed = FMath::Max(DriveConfig.MinBounceSpeed, Other->DriveConfig.MinBounceSpeed);
	const double Restitution = ClosingSpeed >= MinBounceSpeed
		? FMath::Min(DriveConfig.CollisionRestitution, Other->DriveConfig.CollisionRestitution)
		: 0.0;

	const double MassA = DriveConfig.VehicleMass;
	const double MassB = Other->DriveConfig.VehicleMass;
	const double InvMassA = 1.0 / MassA;
	const double InvMassB = 1.0 / MassB;
	const double Impulse = (1.0 + Restitution) * ClosingSpeed / (InvMassA + InvMassB);

	// 두 차량의 공통 속도를 같은 양만큼 감속해 밀림을 줄이고, 충돌 후 분리 속도는 유지한다.
	// 속도 차이가 작은 접촉에서는 나란히 달리던 차량을 갑자기 감속시키지 않는다.
	const double Retention = FMath::Min(DriveConfig.CollisionMomentumRetention, Other->DriveConfig.CollisionMomentumRetention);
	const double CommonSpeed = (SpeedA * MassA + SpeedB * MassB) / (MassA + MassB);
	const double MaxCommonSpeedLoss = ClosingSpeed * 0.5;
	const double CommonSpeedLoss = FMath::Clamp(CommonSpeed, -MaxCommonSpeedLoss, MaxCommonSpeedLoss) * (1.0 - Retention);
	
	const FVector DeltaVelocityA = Normal * (Impulse * InvMassA - CommonSpeedLoss);
	const FVector DeltaVelocityB = -Normal * (Impulse * InvMassB + CommonSpeedLoss);
	
	ApplyExternalVelocityChange(DeltaVelocityA);
	Other->ApplyExternalVelocityChange(DeltaVelocityB);

	return true;
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
	const FVector ProbeUp = GetGroundProbeUp();
	FHitResult Hit(1.f);
	
	SafeMoveUpdatedComponent(-ProbeUp * CorrectionDistance,
		UpdatedComponent->GetComponentQuat(), true, Hit);
	// 간격 보정은 주행 속도에 합산하지 않는다.
	UpdateGroundState();
}

FVector UMWVehicleMovementComponent::GetGroundProbeUp() const
{
	if (UpdatedComponent)
	{
		return UpdatedComponent->GetUpVector();
	}
	
	return FVector::UpVector;
}

FQuat UMWVehicleMovementComponent::CalculateGroundAlignRotation(const FQuat& CurrentRotation, float DeltaTime) const
{
	if (!bDriveConfigReady || !bGrounded || DeltaTime <= 0.f || DriveConfig.GroundAlignSpeed <= 0.f)
	{
		return CurrentRotation;
	}
	
	// 유효한 접지면의 법선을 차체의 목표 Up으로 사용한다.
	const FVector Up = GroundNormal.GetSafeNormal();

	if (Up.IsNearlyZero())
	{
		return CurrentRotation;
	}
	
	// 현재 전방을 노면에 투영해 진행 방향을 유지한다.
	FVector Forward = FVector::VectorPlaneProject(CurrentRotation.GetAxisX(), Up).GetSafeNormal();
	
	if (Forward.IsNearlyZero())
	{
		// 전방 투영이 불가능하면 현재 오른쪽 방향을 이용한다.
		const FVector Right = FVector::VectorPlaneProject(CurrentRotation.GetAxisY(), Up).GetSafeNormal();

		Forward = FVector::CrossProduct(Right, Up).GetSafeNormal();

		if (Forward.IsNearlyZero())
		{
			return CurrentRotation;
		}
	}
	
	// X는 전방, Z는 노면 법선인 목표 자세를 만든다.
	const FQuat DesiredRotation = FRotationMatrix::MakeFromXZ(Forward, Up).ToQuat();

	const double Angle = CurrentRotation.AngularDistance(DesiredRotation);
	
	if (Angle <= SMALL_NUMBER)
	{
		return DesiredRotation;
	}

	// 한 시뮬레이션 단계에서 허용할 회전량을 제한한다.
	const double MaxStepAngle = FMath::DegreesToRadians(
		static_cast<double>(DriveConfig.GroundAlignSpeed))
		* DeltaTime;

	const double Alpha = FMath::Min(1.0, MaxStepAngle / Angle);

	return FQuat::Slerp(CurrentRotation, DesiredRotation, Alpha).GetNormalized();
}
