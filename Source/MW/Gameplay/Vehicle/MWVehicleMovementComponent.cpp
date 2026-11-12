

#include "Gameplay/Vehicle/MWVehicleMovementComponent.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Components/PrimitiveComponent.h"
#include "Components/BoxComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "CollisionShape.h"

namespace
{
	constexpr float MaxImpactYawRate = 150.f;
	constexpr float ImpactYawDamping = 8.f;
	constexpr double ImpactYawScale = 0.35;

	struct FContactResponse
	{
		FVector DeltaA = FVector::ZeroVector;
		FVector DeltaB = FVector::ZeroVector;
		double Impulse = 0.0;
	};

	FContactResponse CalculateContactResponse(const FVector& VelocityA, const FVector& VelocityB,
		const FVector& Normal, const FMWVehicleContactTuning& A, const FMWVehicleContactTuning& B)
	{
		// 떨어지는 중인 차량은 다시 밀지 않는다. 반발은 최소 접근 속도 이상에서만 적용한다.
		const double SpeedA = FVector::DotProduct(VelocityA, Normal);
		const double SpeedB = FVector::DotProduct(VelocityB, Normal);
		const double ClosingSpeed = SpeedB - SpeedA;
		if (ClosingSpeed <= KINDA_SMALL_NUMBER)
		{
			return {};
		}
		const double Restitution = ClosingSpeed >= FMath::Max(A.MinBounceSpeed, B.MinBounceSpeed)
			? FMath::Min(A.Restitution, B.Restitution) : 0.0;
		const double InvMassA = 1.0 / A.Mass;
		const double InvMassB = 1.0 / B.Mass;
		const double Impulse = (1.0 + Restitution) * ClosingSpeed / (InvMassA + InvMassB);
		const double CommonSpeed = (SpeedA * A.Mass + SpeedB * B.Mass) / (A.Mass + B.Mass);
		const double MaxLoss = ClosingSpeed * 0.5;
		const double Loss = FMath::Clamp(CommonSpeed, -MaxLoss, MaxLoss)
			* (1.0 - FMath::Min(A.MomentumRetention, B.MomentumRetention));
		return {Normal * (Impulse * InvMassA - Loss), -Normal * (Impulse * InvMassB + Loss), Impulse};
	}

	double ProjectBoxRadius(const FVector& Axis, const FQuat& Rotation, const FVector& Extent)
	{
		return FMath::Abs(Axis | Rotation.GetAxisX()) * Extent.X
			+ FMath::Abs(Axis | Rotation.GetAxisY()) * Extent.Y
			+ FMath::Abs(Axis | Rotation.GetAxisZ()) * Extent.Z;
	}

	void GetContactArms(const FVector& Normal, const FVector& CenterOffset, const FQuat& RotationA,
		const FVector& ExtentA, const FQuat& RotationB, const FVector& ExtentB, FVector& OutA, FVector& OutB)
	{
		// 면 전체가 맞닿으면 접촉 구간의 가운데를 쓴다. 정중앙 추돌이 모서리 한 점을 기준으로 회전하는 것을 막는다.
		const FVector Tangent(-Normal.Y, Normal.X, 0.0);
		const double RadiusA = ProjectBoxRadius(Tangent, RotationA, ExtentA);
		const double RadiusB = ProjectBoxRadius(Tangent, RotationB, ExtentB);
		const double CenterB = CenterOffset | Tangent;
		const double Contact = 0.5 * (FMath::Max(-RadiusA, CenterB - RadiusB) + FMath::Min(RadiusA, CenterB + RadiusB));
		OutA = Tangent * FMath::Clamp(Contact, -RadiusA, RadiusA);
		OutB = Tangent * FMath::Clamp(Contact - CenterB, -RadiusB, RadiusB);
	}

	void ApplyContactYaw(float& Rate, const FVector& Offset, const FVector& Impulse, double Mass, const FVector& Extent)
	{
		const double RadiusSquared = Extent.SizeSquared2D();
		const double Inertia = Mass * RadiusSquared / 3.0;
		if (Inertia > UE_SMALL_NUMBER)
		{
			const FVector Arm = FVector(Offset.X, Offset.Y, 0.0).GetClampedToMaxSize(FMath::Sqrt(RadiusSquared));
			const double DeltaYawRate = FMath::RadiansToDegrees(FVector::CrossProduct(Arm, Impulse).Z / Inertia) * ImpactYawScale;
			Rate = FMath::Clamp(Rate + DeltaYawRate, -MaxImpactYawRate, MaxImpactYawRate);
		}
	}

	bool SweepContactBoxes(const FVector& Location, const FQuat& Rotation, const FVector& Extent,
		const FMWVehicleContactSample& Other, const FVector& RelativeDelta, double& OutTime, FVector& OutNormal)
	{
		const FVector CenterOffset = Other.Location - Location;
		const FVector OwnAxes[] = {Rotation.GetAxisX(), Rotation.GetAxisY(), Rotation.GetAxisZ()};
		const FVector OtherAxes[] = {Other.Rotation.GetAxisX(), Other.Rotation.GetAxisY(), Other.Rotation.GetAxisZ()};
		double EnterTime = 0.0;
		double ExitTime = 1.0;
		double MinPenetration = TNumericLimits<double>::Max();
		FVector EnterNormal = FVector::ZeroVector;
		FVector OverlapNormal = FVector::ZeroVector;

		// 각 축에 투영한 두 박스가 겹치는 시간 구간을 구한다. 모든 축에서 겹치는 첫 시각이 접촉이다.
		const auto TestAxis = [&](const FVector& Axis)
		{
			const double Offset = CenterOffset | Axis;
			const double Radius = ProjectBoxRadius(Axis, Rotation, Extent) + ProjectBoxRadius(Axis, Other.Rotation, Other.Extent);
			const double Penetration = Radius - FMath::Abs(Offset);
			if (Penetration < MinPenetration)
			{
				MinPenetration = Penetration;
				OverlapNormal = Offset >= 0.0 ? -Axis : Axis;
			}

			const double Travel = RelativeDelta | Axis;
			if (FMath::IsNearlyZero(Travel))
			{
				return Penetration >= 0.0;
			}
			const double Start = (Offset - Radius) / Travel;
			const double End = (Offset + Radius) / Travel;
			const double AxisEnter = FMath::Min(Start, End);
			if (AxisEnter > EnterTime)
			{
				EnterTime = AxisEnter;
				EnterNormal = Travel > 0.0 ? -Axis : Axis;
			}
			ExitTime = FMath::Min(ExitTime, FMath::Max(Start, End));
			return EnterTime <= ExitTime;
		};

		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			if (!TestAxis(OwnAxes[Axis]) || !TestAxis(OtherAxes[Axis]))
			{
				return false;
			}
		}
		for (const FVector& OwnAxis : OwnAxes)
		{
			for (const FVector& OtherAxis : OtherAxes)
			{
				const FVector CrossAxis = FVector::CrossProduct(OwnAxis, OtherAxis).GetSafeNormal();
				if (!CrossAxis.IsZero() && !TestAxis(CrossAxis))
				{
					return false;
				}
			}
		}
		OutTime = EnterTime;
		OutNormal = EnterTime > 0.0 ? EnterNormal : OverlapNormal;
		return true;
	}

	const FMWVehicleContactSample* FindPredictedContact(const FVector& Location, const FQuat& Rotation,
		const FVector& Extent, const FVector& MoveDelta, float DeltaTime, TArrayView<const FMWVehicleContactSample> Contacts,
		double& OutFraction, FVector& OutNormal)
	{
		const FMWVehicleContactSample* Closest = nullptr;
		OutFraction = 1.0;
		for (const FMWVehicleContactSample& Other : Contacts)
		{
			// 상대 차량도 이 스텝 동안 움직인다고 보고 두 박스를 상대 이동량으로 검사한다.
			const FVector RelativeDelta = MoveDelta - Other.Velocity * DeltaTime;
			const double Length = RelativeDelta.Size();
			if (Length <= UE_KINDA_SMALL_NUMBER ||
				FVector::DistSquared(Location, Other.Location) > FMath::Square(Extent.Size() + Other.Extent.Size() + Length))
			{
				continue;
			}
			double Fraction = 0.0;
			FVector Normal;
			if (!SweepContactBoxes(Location, Rotation, Extent, Other, RelativeDelta, Fraction, Normal))
			{
				continue;
			}
			const FVector HorizontalNormal = Normal.GetSafeNormal2D();
			if (!HorizontalNormal.IsNearlyZero() && (RelativeDelta | HorizontalNormal) < -UE_KINDA_SMALL_NUMBER
				&& (!Closest || Fraction < OutFraction))
			{
				Closest = &Other;
				OutFraction = Fraction;
				OutNormal = HorizontalNormal;
			}
		}
		return Closest;
	}
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
void UMWVehicleMovementComponent::SimulateDriveStep(float DeltaTime, TArrayView<const FMWVehicleContactSample> PredictedContacts,
	FMWVehiclePredictedImpact* OutImpact)
{
	UpdateGroundState();
	UpdateSteering(DeltaTime, PredictedContacts);
	if (bGrounded)
	{
		UpdateDriveVelocity(DeltaTime);
	}
	else
	{
		UpdateAirVelocity(DeltaTime);
	}
	MoveVehicle(DeltaTime, PredictedContacts, OutImpact);
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
	OutSnapshot.ImpactYawRate = ImpactYawRate;
}

void UMWVehicleMovementComponent::ApplyDriveSnapshot(const FMWVehicleDriveSnapshot& Snapshot)
{
	// 서버 위치를 복원할 때 Sweep하거나 Pawn의 텔레포트 콜백으로 입력을 초기화하지 않는다.
	UpdatedComponent->SetWorldLocationAndRotation(Snapshot.Location, Snapshot.Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	Velocity = Snapshot.Velocity;
	bGrounded = Snapshot.bGrounded;
	ImpactYawRate = Snapshot.ImpactYawRate;
	TargetRotation = UpdatedComponent->GetComponentQuat();
	UpdateComponentVelocity();
}

FMWVehicleContactTuning UMWVehicleMovementComponent::GetContactTuning() const
{
	return {DriveConfig.VehicleMass, DriveConfig.CollisionRestitution,
		DriveConfig.CollisionMomentumRetention, DriveConfig.MinBounceSpeed};
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
	ImpactYawRate = 0.f;
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

void UMWVehicleMovementComponent::UpdateSteering(float DeltaTime, TArrayView<const FMWVehicleContactSample> PredictedContacts)
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

	// 충돌로 생긴 회전은 입력이 없거나 공중에 있어도 감쇠하면서 남는다.
	float DeltaYaw = ImpactYawRate * DeltaTime;
	ImpactYawRate *= FMath::Exp(-ImpactYawDamping * DeltaTime);
	if (FMath::Abs(ImpactYawRate) < 0.05f)
	{
		ImpactYawRate = 0.f;
	}
	const AController* Controller = PawnOwner->GetController();
	if (bGrounded && Controller && !Controller->IsMoveInputIgnored() && !FMath::IsNearlyZero(DriveInput.Steering))
	{
		const float ForwardSpeed = FVector::DotProduct(Velocity, UpdatedComponent->GetForwardVector());
		const float SpeedMagnitude = FMath::Abs(ForwardSpeed);
		if (SpeedMagnitude > DriveConfig.MinSteeringSpeed)
		{
			const float SpeedFactor = FMath::Clamp(SpeedMagnitude / FMath::Max(DriveConfig.FullSteeringSpeed, 1.f), 0.f, 1.f);
			const float TravelSign = ForwardSpeed >= 0.f ? 1.f : -1.f;
			const float TurnMultiplier = IsDriftControlActive() ? FMath::Max(DriveConfig.DriftTurnRateMultiplier, 0.f) : 1.f;
			DeltaYaw += DriveInput.Steering * DriveConfig.TurnRateDegPerSecond * TurnMultiplier * SpeedFactor * TravelSign * DeltaTime;
		}
	}
	if (FMath::IsNearlyZero(DeltaYaw))
	{
		return;
	}

	FRotator NewRotation = UpdatedComponent->GetComponentRotation();

	NewRotation.Yaw += DeltaYaw;

	const FQuat CandidateRotation = NewRotation.Quaternion();
	if (UpdatedPrimitive && GetWorld())
	{
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(PawnOwner);
		QueryParams.AddIgnoredActors(UpdatedPrimitive->GetMoveIgnoreActors());
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
		if (!PredictedContacts.IsEmpty() && Shape.IsBox())
		{
			const FVector Extent = Shape.GetBox();
			for (const FMWVehicleContactSample& Other : PredictedContacts)
			{
				if (FVector::DistSquared(Location, Other.Location) > FMath::Square(Extent.Size() + Other.Extent.Size()))
				{
					continue;
				}
				double Time;
				FVector Normal;
				if (SweepContactBoxes(Location, MidRotation, Extent, Other, FVector::ZeroVector, Time, Normal) ||
					SweepContactBoxes(Location, CandidateRotation, Extent, Other, FVector::ZeroVector, Time, Normal))
				{
					return;
				}
			}
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
	QueryParams.AddIgnoredActors(Collision->GetMoveIgnoreActors());
	const FCollisionResponseParams ResponseParams(Collision->GetCollisionResponseToChannels());

	const float MinGroundDot = FMath::Cos(FMath::DegreesToRadians(DriveConfig.MaxGroundAngle));
	const FCollisionShape Shape = Collision->GetCollisionShape();
	FHitResult Hit;
	const auto SweepGround = [&](const FCollisionShape& ProbeShape)
	{
		return World->SweepSingleByChannel(Hit, Start, End, Collision->GetComponentQuat(),
			Collision->GetCollisionObjectType(), ProbeShape, QueryParams, ResponseParams);
	};
	if (!SweepGround(Shape))
	{
		return;
	}

	if (Hit.bStartPenetrating && Shape.IsBox()
		&& FVector::DotProduct(Hit.ImpactNormal, FVector::UpVector) < MinGroundDot)
	{
		// 차체 옆벽과의 초기 겹침이 아래쪽 바닥을 가릴 수 있다.
		// 검사에만 가로 여유를 주고 높이를 유지해 실제 바닥까지의 거리를 구한다.
		constexpr double SideInset = 0.5;
		FVector ProbeExtent = Shape.GetBox();
		ProbeExtent.X -= FMath::Min(SideInset, ProbeExtent.X * 0.5);
		ProbeExtent.Y -= FMath::Min(SideInset, ProbeExtent.Y * 0.5);
		Hit.Reset();
		if (!SweepGround(FCollisionShape::MakeBox(ProbeExtent)))
		{
			return;
		}
	}

	if (!Hit.IsValidBlockingHit() || FVector::DotProduct(Hit.ImpactNormal, FVector::UpVector) < MinGroundDot)
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

void UMWVehicleMovementComponent::MoveVehicle(float DeltaTime, TArrayView<const FMWVehicleContactSample> PredictedContacts,
	FMWVehiclePredictedImpact* OutImpact)
{
	if (!bDriveConfigReady || !PawnOwner || !UpdatedComponent || DeltaTime <= 0.f)
	{
		return;
	}

	const FVector MoveDelta = Velocity * DeltaTime;
	const UBoxComponent* Box = Cast<UBoxComponent>(UpdatedComponent);
	double ContactFraction = 1.0;
	FVector ContactNormal = FVector::ZeroVector;
	const FMWVehicleContactSample* PredictedHit = nullptr;
	if (Box && !PredictedContacts.IsEmpty())
	{
		PredictedHit = FindPredictedContact(Box->GetComponentLocation(), TargetRotation, Box->GetScaledBoxExtent(),
			MoveDelta, DeltaTime, PredictedContacts, ContactFraction, ContactNormal);
	}

	FHitResult Hit(1.f);
	// 벽이 예측 차량보다 먼저 닿으면 기존 월드 충돌을 우선한다.
	const FVector SweepDelta = MoveDelta * ContactFraction;
	SafeMoveUpdatedComponent(SweepDelta, TargetRotation, true, Hit);

	if (Hit.IsValidBlockingHit())
	{
		HandleImpact(Hit, DeltaTime, SweepDelta);
		if (!IsValid(PawnOwner) || !IsValid(UpdatedComponent))
		{
			return;
		}

		if (TryResolveVehicleContact(Hit))
		{
			return;
		}

		// 충돌면 안쪽으로 향하는 속도만 제거한다.
		const float IntoSurfaceSpeed = FVector::DotProduct(Velocity, Hit.Normal);

		if (IntoSurfaceSpeed < 0.f)
		{
			Velocity -= Hit.Normal * IntoSurfaceSpeed;
		}

		// 처음 요청한 이동 중 남은 부분을 표면을 따라 시도한다.
		SlideAlongSurface(SweepDelta, 1.f - Hit.Time, Hit.Normal, Hit, true);

		// 미끄러지는 도중 다른 면에도 막혔다면 속도를 정리한다.
		if (Hit.IsValidBlockingHit())
		{
			if (TryResolveVehicleContact(Hit))
			{
				return;
			}
			const float IntoSecondSurfaceSpeed = FVector::DotProduct(Velocity, Hit.Normal);

			if (IntoSecondSurfaceSpeed < 0.f)
			{
				Velocity -= Hit.Normal * IntoSecondSurfaceSpeed;
			}
		}
	}
	else if (PredictedHit && IsValid(Box))
	{
		// 예측에서는 내 차량만 바꾼다. 상대에게 적용될 속도는 다음 예측 Sweep에만 사용한다.
		const FMWVehicleContactTuning Tuning = GetContactTuning();
		const FContactResponse Response = CalculateContactResponse(Velocity, PredictedHit->Velocity, ContactNormal, Tuning, PredictedHit->Tuning);
		const FVector CenterOffset = PredictedHit->Location + PredictedHit->Velocity * (DeltaTime * ContactFraction) - Box->GetComponentLocation();
		FVector ArmA, ArmB;
		GetContactArms(ContactNormal, CenterOffset, Box->GetComponentQuat(), Box->GetScaledBoxExtent(),
			PredictedHit->Rotation, PredictedHit->Extent, ArmA, ArmB);
		Velocity += Response.DeltaA;
		ApplyContactYaw(ImpactYawRate, ArmA, ContactNormal * Response.Impulse, Tuning.Mass, Box->GetScaledBoxExtent());
		if (OutImpact && Response.Impulse > 0.0)
		{
			OutImpact->SampleIndex = PredictedHit - PredictedContacts.GetData();
			OutImpact->Fraction = ContactFraction;
			OutImpact->OtherVelocityDelta = Response.DeltaB;
		}
	}
}

bool UMWVehicleMovementComponent::TryResolveVehicleContact(const FHitResult& Hit)
{
	if (!IsValid(PawnOwner) || !PawnOwner->HasAuthority() || !Hit.IsValidBlockingHit())
	{
		return false;
	}

	AMWVehiclePawn* OwnPawn = Cast<AMWVehiclePawn>(PawnOwner);
	AMWVehiclePawn* OtherPawn = Cast<AMWVehiclePawn>(Hit.GetActor());
	if (!OwnPawn || !IsValid(OtherPawn) || OtherPawn == OwnPawn)
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

	const FMWVehicleContactTuning TuningA = GetContactTuning();
	const FMWVehicleContactTuning TuningB = Other->GetContactTuning();
	const FContactResponse Response = CalculateContactResponse(Velocity, Other->Velocity, Normal, TuningA, TuningB);
	if (Response.Impulse <= 0.0)
	{
		return true;
	}

	Velocity += Response.DeltaA;
	Other->Velocity += Response.DeltaB;
	if (const UBoxComponent* OtherBox = Cast<UBoxComponent>(Other->UpdatedPrimitive))
	{
		const FVector ExtentA = OwnPawn->CollisionRoot->GetScaledBoxExtent();
		const FVector ExtentB = OtherBox->GetScaledBoxExtent();
		const FVector CenterOffset = OtherBox->GetComponentLocation() - UpdatedComponent->GetComponentLocation();
		FVector ArmA, ArmB;
		GetContactArms(Normal, CenterOffset, UpdatedComponent->GetComponentQuat(), ExtentA,
			OtherBox->GetComponentQuat(), ExtentB, ArmA, ArmB);
		ApplyContactYaw(ImpactYawRate, ArmA, Normal * Response.Impulse, TuningA.Mass, ExtentA);
		ApplyContactYaw(Other->ImpactYawRate, ArmB, -Normal * Response.Impulse, TuningB.Mass, ExtentB);
	}
	Other->UpdateComponentVelocity();
	OwnPawn->bContactSnapshotPending = true;
	OtherPawn->bContactSnapshotPending = true;

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
	FHitResult Hit(1.f);
	SafeMoveUpdatedComponent(-FVector::UpVector * CorrectionDistance,
		UpdatedComponent->GetComponentQuat(), true, Hit);
	// 간격 보정은 주행 속도에 합산하지 않는다.
	UpdateGroundState();
}
