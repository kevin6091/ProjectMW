

#include "Gameplay/Vehicle/MWVehicleMovementComponent.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "Gameplay/Items/MWItemComponent.h"
#include "Gameplay/Track/MWTrackRuntimeComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Components/PrimitiveComponent.h"
#include "Components/BoxComponent.h"
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

namespace
{
	constexpr float MaxImpactYawRate = 150.f;
	constexpr float ImpactYawDamping = MWVehicleImpactYawDamping;
	constexpr double ImpactYawScale = 0.35;

	// 각 Box 축이 노면 법선 방향으로 차지하는 길이를 합한다.
	double CalculateBoxSupportRadius(const FVector& Extent, const FQuat& Rotation, const FVector& Normal)
	{
		return Extent.X * FMath::Abs(FVector::DotProduct(Rotation.GetAxisX(), Normal))
			+ Extent.Y * FMath::Abs(FVector::DotProduct(Rotation.GetAxisY(), Normal))
			+ Extent.Z * FMath::Abs(FVector::DotProduct(Rotation.GetAxisZ(), Normal));
	}

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
			const FVector RelativeDelta = MoveDelta - Other.Velocity * DeltaTime - Other.CorrectionDelta;
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

	AMWVehiclePawn* Vehicle = Cast<AMWVehiclePawn>(PawnOwner);
	if (Vehicle)
	{
		// 복제 도착 순서나 늦은 소유권 연결도 다음 이동 계산 전에 다시 확인한다.
		Vehicle->RefreshDrivePermission();
	}
	if (GetNetMode() != NM_Standalone)
	{
		if (Vehicle)
		{
			Vehicle->TickNetworkDrive(DeltaTime);
			return;
		}
		if (!PawnOwner->IsLocallyControlled())
		{
			return;
		}
	}
	if (Vehicle && !Vehicle->CanDrive())
	{
		ResetDriveInput();
		StopMovementImmediately();
		return;
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
	const auto* Vehicle = Cast<AMWVehiclePawn>(PawnOwner);
	if (Vehicle && !Vehicle->CanDrive())
	{
		ResetDriveInput();
		StopMovementImmediately();
		return;
	}
	FVector OrientationNormal;
	UpdateGroundState(&OrientationNormal);

	// 접지 판정에서 얻은 평면을 재사용한다. 평면을 구하지 못한 경우 접촉 법선을 사용한다.
	const bool bHasGroundPlane = !OrientationNormal.IsNearlyZero();
	GroundOrientationNormal = bHasGroundPlane ? OrientationNormal : GroundNormal.GetSafeNormal();

	if (bGrounded)
	{
		// 기존 접지 로그와 같은 스텝의 목표 자세를 기록한다. 진단을 위한 추가 Trace는 하지 않는다.
		if (CVarVehicleSupportLog.GetValueOnGameThread() != 0)
		{
			UE_LOG(LogMWVehicleSupport, Log,
				TEXT("VehicleOrientation Time=%.6f Frame=%llu Actor=%s Plane=%d ")
				TEXT("HalfLength=%.6f HalfWidth=%.6f NX=%.6f NY=%.6f NZ=%.6f"),
				GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0, GFrameCounter, *GetPathNameSafe(PawnOwner),
				bHasGroundPlane, DriveConfig.GroundPlaneProbeHalfLength, DriveConfig.GroundPlaneProbeHalfWidth,
				GroundOrientationNormal.X, GroundOrientationNormal.Y, GroundOrientationNormal.Z);
		}
	}

	const bool bMovedForRotation = UpdateGroundRotation(DeltaTime, PredictedContacts);
	if (Vehicle && !Vehicle->CanDrive())
	{
		StopMovementImmediately();
		return;
	}

	// 움직이지 않은 접지 차량은 같은 위치와 자세에서 얻은 바닥 검사 결과를 재사용한다.
	if (bMovedForRotation || !bGrounded)
	{
		UpdateGroundState();
	}

	if (bGrounded)
	{
		UpdateDriveVelocity(DeltaTime);
	}
	else
	{
		UpdateAirVelocity(DeltaTime);
	}

	MoveVehicle(DeltaTime, PredictedContacts, OutImpact);
	if (Vehicle && !Vehicle->CanDrive())
	{
		StopMovementImmediately();
		return;
	}

	UpdateGroundState();
	AdjustGroundHeight(DeltaTime);
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
	OutSnapshot.GroundNormal = GroundNormal.GetSafeNormal();
}

void UMWVehicleMovementComponent::ApplyDriveSnapshot(const FMWVehicleDriveSnapshot& Snapshot)
{
	// 서버 위치를 복원할 때 Sweep하거나 Pawn의 텔레포트 콜백으로 입력을 초기화하지 않는다.
	UpdatedComponent->SetWorldLocationAndRotation(Snapshot.Location, Snapshot.Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	Velocity = Snapshot.Velocity;
	bGrounded = Snapshot.bGrounded;
	ImpactYawRate = Snapshot.ImpactYawRate;
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

// 기존 트랙 효과의 수명과 발판 틱 순서만 연결한다. main의 주행·접지 함수는 유지한다.
void UMWVehicleMovementComponent::ApplyTrackBoost(float MinimumThrottle, float Duration)
{
	TrackMinimumThrottle = FMath::Clamp(MinimumThrottle, 0.f, 1.f);
	TrackBoostEndTime = GetWorld()->GetTimeSeconds() + Duration;
}

void UMWVehicleMovementComponent::ApplyTrackSurfaceVelocity(const FVector& VelocityCmPerSecond, float Duration)
{
	TrackSurfaceVelocity = VelocityCmPerSecond;
	TrackSurfaceEndTime = GetWorld()->GetTimeSeconds() + Duration;
}

void UMWVehicleMovementComponent::SetTrackLiftBase(AActor* Base)
{
	if (UMWTrackRuntimeComponent* Current = TrackLiftBase.Get())
	{
		if (Current->GetOwner() == Base)
		{
			return;
		}
		RemoveTickPrerequisiteComponent(Current);
	}
	TrackLiftBase.Reset();
	if (IsValid(Base) && Base->GetWorld() == GetWorld())
	{
		if (UMWTrackRuntimeComponent* Runtime = Base->FindComponentByClass<UMWTrackRuntimeComponent>())
		{
			TrackLiftBase = Runtime;
			// 같은 프레임에서 발판 이동을 먼저 반영한 뒤 차량 이동과 지면 판정을 계산한다.
			AddTickPrerequisiteComponent(Runtime);
		}
	}
}

void UMWVehicleMovementComponent::ClearTrackLiftBase(const AActor* Base)
{
	if (const UMWTrackRuntimeComponent* Current = TrackLiftBase.Get(); Current && Current->GetOwner() == Base)
	{
		SetTrackLiftBase(nullptr);
	}
}

void UMWVehicleMovementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetTrackLiftBase(nullptr);
	Super::EndPlay(EndPlayReason);
}

bool UMWVehicleMovementComponent::IsTrackBoostActive() const
{
	return GetWorld() && GetWorld()->GetTimeSeconds() < TrackBoostEndTime;
}

const UMWItemComponent* UMWVehicleMovementComponent::GetActiveItemBooster() const
{
	const AMWVehiclePawn* Vehicle = Cast<AMWVehiclePawn>(PawnOwner);
	const UMWItemComponent* Items = Vehicle ? Vehicle->GetItemComponent() : nullptr;
	return IsValid(Items) && Items->IsBoosterActive() ? Items : nullptr;
}

FMWVehicleContactTuning UMWVehicleMovementComponent::GetContactTuning() const
{
	return {DriveConfig.VehicleMass, DriveConfig.CollisionRestitution,
		DriveConfig.CollisionMomentumRetention, DriveConfig.MinBounceSpeed};
}

FTransform UMWVehicleMovementComponent::LimitDrivePresentation(const FTransform& Desired, float DeltaTime,
	TArrayView<const FMWVehicleContactSample> Contacts) const
{
	FTransform Result = Desired;
	if (!UpdatedPrimitive || !GetWorld())
	{
		return Result;
	}
	const FCollisionShape Shape = UpdatedPrimitive->GetCollisionShape();
	const FVector Start = UpdatedPrimitive->GetComponentLocation();
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MWDrivePresentation), false, PawnOwner);
	QueryParams.AddIgnoredActors(UpdatedPrimitive->GetMoveIgnoreActors());
	for (const FMWVehicleContactSample& Contact : Contacts)
	{
		QueryParams.AddIgnoredActor(Cast<AActor>(Contact.Vehicle.ResolveObjectPtr()));
	}
	const FCollisionResponseParams Responses(UpdatedPrimitive->GetCollisionResponseToChannels());
	if (GetWorld()->OverlapBlockingTestByChannel(Start, Result.GetRotation(), UpdatedPrimitive->GetCollisionObjectType(),
		Shape, QueryParams, Responses))
	{
		Result.SetRotation(UpdatedPrimitive->GetComponentQuat());
	}

	// 짧은 표시 예측도 벽이나 상대를 통과하지 않도록 Sweep으로 이동 범위를 제한한다.
	const FVector Delta = Desired.GetLocation() - Start;
	double Fraction = 1.0;
	FHitResult Hit;
	if (GetWorld()->SweepSingleByChannel(Hit, Start, Desired.GetLocation(), Result.GetRotation(),
		UpdatedPrimitive->GetCollisionObjectType(), Shape, QueryParams, Responses))
	{
		Fraction = Hit.bStartPenetrating ? 0.0 : Hit.Time;
	}
	if (Shape.IsBox() && DeltaTime > 0.f)
	{
		double ContactFraction;
		FVector Normal;
		if (FindPredictedContact(Start, Result.GetRotation(), Shape.GetBox(), Delta, DeltaTime, Contacts, ContactFraction, Normal))
		{
			Fraction = FMath::Min(Fraction, ContactFraction);
		}
	}
	Result.SetLocation(Start + Delta * Fraction);
	return Result;
}

FTransform UMWVehicleMovementComponent::ConstrainContactPresentation(const FTransform& LocalPose,
	const FTransform& OtherPose, const FVector& OtherExtent, const FVector& Normal, bool bShowContact) const
{
	FTransform Result = OtherPose;
	if (!UpdatedPrimitive || !UpdatedPrimitive->GetCollisionShape().IsBox() || Normal.IsNearlyZero())
	{
		return Result;
	}
	FMWVehicleContactSample Other;
	Other.Location = OtherPose.GetLocation();
	Other.Rotation = OtherPose.GetRotation();
	Other.Extent = OtherExtent;
	const FVector OwnExtent = UpdatedPrimitive->GetCollisionShape().GetBox();
	double Fraction;
	FVector HitNormal;
	const bool bOverlapping = SweepContactBoxes(LocalPose.GetLocation(), LocalPose.GetRotation(), OwnExtent + FVector(1.5, 1.5, 0.0),
		Other, FVector::ZeroVector, Fraction, HitNormal);
	if (!bShowContact && !bOverlapping)
	{
		return Result;
	}
	// 기울어진 모서리 접촉도 실제 박스 축으로 맞춘다. 서버 충돌체는 움직이지 않는다.
	constexpr double SearchDistance = 80.0;
	Other.Location += Normal * SearchDistance;
	if (SweepContactBoxes(LocalPose.GetLocation(), LocalPose.GetRotation(), OwnExtent, Other,
		Normal * (SearchDistance * 2.0), Fraction, HitNormal))
	{
		// 아직 계산된 접촉이 없으면 아주 작은 여유를 남겨 표시 예측만으로 먼저 부딪혀 보이지 않게 한다.
		const double Skin = bShowContact ? 0.15 : 1.5;
		const double Adjustment = SearchDistance - Fraction * SearchDistance * 2.0 + Skin;
		if (FMath::Abs(Adjustment) <= 40.0)
		{
			Result.AddToTranslation(Normal * Adjustment);
		}
	}
	return Result;
}

void UMWVehicleMovementComponent::StopMovementImmediately()
{
	Super::StopMovementImmediately();
	ImpactYawRate = 0.f;
	TargetRotation = UpdatedComponent ? UpdatedComponent->GetComponentQuat() : FQuat::Identity;
}

void UMWVehicleMovementComponent::ResetDriveInput()
{
	DriveInput = FMWVehicleDriveInput{};
	ConsumeInputVector();
}

void UMWVehicleMovementComponent::OnTeleported()
{
	Super::OnTeleported();
	SetTrackLiftBase(nullptr);
	TrackBoostEndTime = -1.0;
	TrackSurfaceEndTime = -1.0;
	ResetDriveInput();
	bGrounded = false;
	ImpactYawRate = 0.f;
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
	// 후진 제한은 그대로 두고 기존 전진 부스터 배율만 적용한다.
	const UMWItemComponent* Booster = GetActiveItemBooster();
	const float BoostedForwardSpeedLimit =
		Booster ? ForwardSpeedLimit * Booster->GetBoosterSpeedMultiplier() : ForwardSpeedLimit;
	const float Threshold = FMath::Max(DriveConfig.DirectionChangeSpeedThreshold, 0.f);
	const float ReverseSpeedLimit = FMath::Min(ForwardSpeedLimit, FMath::Max(DriveConfig.MaxReverseSpeed, 0.f));

	if (ForwardSpeed < -Threshold)
	{
		return ReverseSpeedLimit;
	}

	if (ForwardSpeed > Threshold)
	{
		return BoostedForwardSpeedLimit;
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

	return BoostedForwardSpeedLimit;
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

	const float MinimumThrottle = GetActiveItemBooster() ? 1.f : IsTrackBoostActive() ? TrackMinimumThrottle : 0.f;
	const float ForwardRequest = FMath::Max(DriveInput.Throttle, MinimumThrottle);
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

	const FVector Up = GroundOrientationNormal.GetSafeNormal();

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

	const double Before = CalculateBoxSupportRadius(Extent, FromRotation, Normal);
	const double After = CalculateBoxSupportRadius(Extent, ToRotation, Normal);

	return Normal * (After - Before);
}

bool UMWVehicleMovementComponent::IsVehiclePoseBlocked(const FVector& Location, const FQuat& Rotation, float ShapeInflation,
	TArrayView<const FMWVehicleContactSample> PredictedContacts) const
{
	const UWorld* World = GetWorld();

	if (!World || !PawnOwner || !UpdatedPrimitive)
	{
		// 검사가 불가능한 상태에서는 자세 변경을 허용하지 않는다.
		return true;
	}

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(PawnOwner);
	QueryParams.AddIgnoredActors(UpdatedPrimitive->GetMoveIgnoreActors());

	const FCollisionResponseParams Responses(UpdatedPrimitive->GetCollisionResponseToChannels());

	const FCollisionShape Shape = UpdatedPrimitive->GetCollisionShape(FMath::Max(ShapeInflation, 0.f));
	if (World->OverlapBlockingTestByChannel(
		Location,
		Rotation,
		UpdatedPrimitive->GetCollisionObjectType(),
		UpdatedPrimitive->GetCollisionShape(FMath::Max(ShapeInflation, 0.f)),
		QueryParams,
		Responses))
	{
		return true;
	}
	if (Shape.IsBox())
	{
		for (const FMWVehicleContactSample& Other : PredictedContacts)
		{
			double Time;
			FVector Normal;
			if (SweepContactBoxes(Location, Rotation, Shape.GetBox(), Other, FVector::ZeroVector, Time, Normal))
			{
				return true;
			}
		}
	}
	return false;
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

	const double TravelSign = EffectiveThrottle < 0.f ? -1.0 : 1.0;
	const FVector DriveDirection = Forward * TravelSign;
	double DriveSpeedLimit = 0.0;
	double RequestedDriveSpeedIncrease = 0.0;

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
		const double SpeedInRequestedDirection = FVector::DotProduct(GroundVelocity, DriveDirection);
		DriveSpeedLimit = GetMaxSpeed();

		const UMWItemComponent* Booster = EffectiveThrottle > 0.f ? GetActiveItemBooster() : nullptr;
		const double EngineSpeedIncrease = DriveConfig.DriveAcceleration * (Booster ? Booster->GetBoosterAccelerationMultiplier() : 1.f) * FMath::Abs(EffectiveThrottle) * DeltaTime;

		const double RemainingSpeed = FMath::Max(DriveSpeedLimit - SpeedInRequestedDirection, 0.0);
		RequestedDriveSpeedIncrease = FMath::Min(EngineSpeedIncrease, RemainingSpeed);
		GroundVelocity += DriveDirection * RequestedDriveSpeedIncrease;

		// 부스터 종료 등으로 종방향 상한을 초과하면 기존 감속량으로 복귀한다.
		const double ExcessSpeed = FMath::Max(SpeedInRequestedDirection - DriveSpeedLimit, 0.0);
		const double SpeedReduction = FMath::Min(ExcessSpeed, static_cast<double>(DriveConfig.CoastDeceleration) * DeltaTime);
		GroundVelocity -= DriveDirection * SpeedReduction;
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

	const double SpeedLimitSquared = FMath::Square(DriveSpeedLimit);
	if (RequestedDriveSpeedIncrease <= 0.0 || Velocity.SizeSquared() <= SpeedLimitSquared)
	{
		return;
	}

	// 접지 한계를 통과한 종방향 추진만 분리해 중력과 횡그립의 결과를 보존한다.
	const double AppliedDriveSpeedIncrease = FMath::Max(FVector::DotProduct(AppliedTireDeltaVelocity, DriveDirection), 0.0);
	const FVector VelocityWithoutDrive = Velocity - DriveDirection * AppliedDriveSpeedIncrease;

	const double SpeedAlongDrive = FVector::DotProduct(VelocityWithoutDrive, DriveDirection);
	const double SideSpeed = FVector::DotProduct(VelocityWithoutDrive, Right);

	// 횡속도가 사용한 몫을 빼고, 상한 안에서 허용되는 종방향 속도를 계산한다.
	const double MaxSpeedAlongDrive = FMath::Sqrt(FMath::Max(SpeedLimitSquared - FMath::Square(SideSpeed), 0.0));
	const double AllowedDriveSpeedIncrease = FMath::Max(MaxSpeedAlongDrive - SpeedAlongDrive, 0.0);

	Velocity = VelocityWithoutDrive + DriveDirection * FMath::Min(AppliedDriveSpeedIncrease, AllowedDriveSpeedIncrease);
}

void UMWVehicleMovementComponent::UpdateGroundState(FVector* OutOrientationNormal)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(MWVehicle_UpdateGroundState);

	if (OutOrientationNormal)
	{
		*OutOrientationNormal = FVector::ZeroVector;
	}

	const bool bWasGrounded = bGrounded;
	const FVector PreviousNormal = GroundNormal.GetSafeNormal();
	FGroundSupportDiagnostics Diagnostics;

	// 이번 검사에서 바닥을 찾지 못하면 비접지 상태로 남긴다.
	bGrounded = false;
	GroundNormal = GetGroundProbeUp();
	GroundDistance = 0.f;
	GroundNormalLoadAcceleration = 0.0;

	FHitResult Candidate;
	FVector OrientationNormal = FVector::ZeroVector;
	if (!FindGroundCandidate(Candidate, PreviousNormal, bWasGrounded, OrientationNormal))
	{
		LogGroundSupport(TEXT("NoCandidate"), bWasGrounded, nullptr, Diagnostics);
		return;
	}
	Diagnostics.bUsedGroundPlane = !OrientationNormal.IsNearlyZero();

	const FVector CandidateNormal = FVector(Candidate.ImpactNormal).GetSafeNormal();

	if (CandidateNormal.IsNearlyZero())
	{
		LogGroundSupport(TEXT("InvalidNormal"), bWasGrounded, &Candidate, Diagnostics);
		return;
	}

	const double OutwardSpeed = FVector::DotProduct(Velocity, Candidate.ImpactNormal);

	// 새 면의 법선 변화는 노면 운반으로 처리하되, 이전 지지면에서 이미 분리하는 외부 속도는 보존한다.
	const bool bLeavingPreviousSurface = bWasGrounded
		&& FVector::DotProduct(Velocity, PreviousNormal) > DriveConfig.GroundDetachSpeed;
	if (bLeavingPreviousSurface || (!bWasGrounded && OutwardSpeed > DriveConfig.GroundDetachSpeed))
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
	
	const double NormalLoad = CalculateGroundNormalLoadAcceleration(Candidate, AcceptedVelocity, bWasGrounded, Diagnostics);

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

	// 고속으로 언덕 정상을 넘을 때, 이미 접지한 차량이 같은 트랙을 따라가는 경우에만 보조한다.
	// 하중 판정에만 적용하며 접촉 후보, 착지 조건, 타이어 하중은 기존 계산을 사용한다.
	const bool bCanAssistCrest = NormalLoad < MinimumAllowedLoad
		&& bWasGrounded
		&& Diagnostics.RequiredAcceleration < 0.0
		&& Diagnostics.bSameTrackAhead
		&& CandidateNormal.Z > 0.0
		&& Diagnostics.AheadNormal.Z > 0.0
		&& Diagnostics.TangentSpeed >= DriveConfig.DownforceReferenceSpeed;
	Diagnostics.CrestSupportAcceleration = bCanAssistCrest ? DriveConfig.MaxCrestSupportAcceleration : 0.0;
	if (NormalLoad + Diagnostics.CrestSupportAcceleration < MinimumAllowedLoad)
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
	LogGroundSupport(Diagnostics.CrestSupportAcceleration > 0.0 ? TEXT("AcceptedCrestSupport") : TEXT("Accepted"),
		bWasGrounded, &Candidate, Diagnostics);

	if (OutOrientationNormal)
	{
		// 접지가 확정된 평면만 이번 스텝의 차체 정렬에 전달한다.
		if (OrientationNormal.IsNearlyZero())
		{
			TryCalculateGroundPlane(Candidate, CandidateNormal, OrientationNormal);
		}
		*OutOrientationNormal = OrientationNormal;
	}
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

	FVector ResponseNormal = CollisionNormal;
	double Restitution = 0.0;
	// 다른 차량의 상대 속도와 질량은 서버의 전용 경로에서 처리한다.
	// 소유 클라이언트에서 그 접촉을 고정된 벽처럼 반사하지 않는다.
	if (!bSupportContact && bGrounded && !Cast<AMWVehiclePawn>(Hit.GetActor()))
	{
		const FVector GroundTangentNormal = FVector::VectorPlaneProject(
			CollisionNormal, GroundNormal).GetSafeNormal();

		if (!GroundTangentNormal.IsNearlyZero())
		{
			// 월드 Z를 지우지 않고 현재 노면 위로 반발 방향을 제한한다.
			// 경사로와 루프에서도 충돌 때문에 노면 바깥쪽 속도가 생기지 않는다.
			ResponseNormal = GroundTangentNormal;
			const double ClosingSpeed = -FVector::DotProduct(Velocity, ResponseNormal);
			if (ClosingSpeed >= DriveConfig.MinWallBounceSpeed)
			{
				Restitution = DriveConfig.WallRestitution;
			}
		}
	}

	const double IntoSurfaceSpeed = FVector::DotProduct(Velocity, ResponseNormal);
	if (IntoSurfaceSpeed >= 0.f)
	{
		return;
	}

	// 접근 속도를 없애는 양에 반발 속도를 더한다. 접선 방향 속도는 유지한다.
	const FVector DeltaVelocity = -ResponseNormal * IntoSurfaceSpeed * (1.0 + Restitution);

	// 접지 검사 사이에서 충돌 보정이 접지를 해제하는 경우도 추적한다.
	// Hit.Normal과 ImpactNormal이 다를 수 있으므로 둘을 구분해 기록한다.
	if (CVarVehicleSupportLog.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogMWVehicleSupport, Log,
			TEXT("VehicleContact Time=%.6f Frame=%llu Actor=%s HitActor=%s Component=%s Support=%d ")
			TEXT("Into=%.6f NX=%.6f NY=%.6f NZ=%.6f ImpactNX=%.6f ImpactNY=%.6f ImpactNZ=%.6f ")
			TEXT("BeforeVX=%.6f BeforeVY=%.6f BeforeVZ=%.6f DX=%.6f DY=%.6f DZ=%.6f ")
			TEXT("Restitution=%.6f ResponseNX=%.6f ResponseNY=%.6f ResponseNZ=%.6f"),
			GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0, GFrameCounter, *GetPathNameSafe(PawnOwner),
			*GetPathNameSafe(Hit.GetActor()), *GetPathNameSafe(HitComponent), bSupportContact,
			IntoSurfaceSpeed, CollisionNormal.X, CollisionNormal.Y, CollisionNormal.Z,
			SurfaceNormal.X, SurfaceNormal.Y, SurfaceNormal.Z,
			Velocity.X, Velocity.Y, Velocity.Z, DeltaVelocity.X, DeltaVelocity.Y, DeltaVelocity.Z,
			Restitution, ResponseNormal.X, ResponseNormal.Y, ResponseNormal.Z);
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

bool UMWVehicleMovementComponent::FindGroundCandidate(FHitResult& OutHit, const FVector& PreviousNormal,
	bool bWasGrounded, FVector& OutOrientationNormal) const
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

	bool bBlockingHit = SweepGround(Shape);

	// 이미 접지한 차량은 박스 가장자리가 노면을 가리는 경우에도 안쪽으로 다시 검사한다.
	if (bBlockingHit && Shape.IsBox() && (OutHit.bStartPenetrating || bWasGrounded) &&
		FVector::DotProduct(OutHit.ImpactNormal, ProbeUp) < MinGroundDot)
	{
		constexpr double SideInset = 0.5;

		FVector ProbeExtent = Shape.GetBox();

		ProbeExtent.X -= FMath::Min(SideInset, ProbeExtent.X * 0.5);
		ProbeExtent.Y -= FMath::Min(SideInset, ProbeExtent.Y * 0.5);

		OutHit.Reset();

		bBlockingHit = SweepGround(FCollisionShape::MakeBox(ProbeExtent));
	}

	// 스윕이 주행 가능한 면에 닿으면 실제 박스 접촉을 우선한다.
	if (bBlockingHit && OutHit.IsValidBlockingHit() && FVector::DotProduct(OutHit.ImpactNormal, ProbeUp) >= MinGroundDot)
	{
		return true;
	}

	// 기존 접지를 잃은 경우에만 차체 아래의 연속된 면으로 후보를 복원한다.
	// 공중에서의 첫 착지는 박스 충돌을 기준으로 처리한다.
	OutHit.Reset();
	return bWasGrounded && TryCalculateGroundPlane(OutHit, PreviousNormal, OutOrientationNormal);
}

bool UMWVehicleMovementComponent::UpdateGroundRotation(float DeltaTime, TArrayView<const FMWVehicleContactSample> PredictedContacts)
{
	if (!UpdatedComponent)
	{
		return false;
	}

	// 공중이거나 자세 변경이 실패해도 실제 회전과 일치시킨다.
	TargetRotation = UpdatedComponent->GetComponentQuat();

	if (!bDriveConfigReady || !PawnOwner || !UpdatedPrimitive || DeltaTime <= 0.f)
	{
		return false;
	}

	const FCollisionShape Shape = UpdatedPrimitive->GetCollisionShape();

	if (!Shape.IsBox())
	{
		return false;
	}

	// 충돌 회전은 공중에서도 감쇠한다. 입력 조향은 아래 노면 회전 경로에서 한 번만 계산한다.
	const float ImpactAngle = ImpactYawRate * DeltaTime;
	ImpactYawRate *= FMath::Exp(-ImpactYawDamping * DeltaTime);
	if (FMath::Abs(ImpactYawRate) < 0.05f)
	{
		ImpactYawRate = 0.f;
	}
	if (!bGrounded)
	{
		const FQuat Candidate = (FQuat(FVector::UpVector, FMath::DegreesToRadians(ImpactAngle)) * TargetRotation).GetNormalized();
		const FVector Location = UpdatedComponent->GetComponentLocation();
		if (!IsVehiclePoseBlocked(Location, FQuat::Slerp(TargetRotation, Candidate, 0.5), 0.f, PredictedContacts)
			&& !IsVehiclePoseBlocked(Location, Candidate, 0.f, PredictedContacts))
		{
			TargetRotation = Candidate;
		}
		// 공중에서는 목표 회전만 계산한다. 실제 이동은 MoveVehicle에서 수행한다.
		return false;
	}
	const FVector Normal = GroundNormal.GetSafeNormal();

	if (Normal.IsNearlyZero())
	{
		return false;
	}

	const FQuat StartRotation = TargetRotation;

	const FQuat AlignedRotation = CalculateGroundAlignRotation(StartRotation, DeltaTime);

	const FQuat SteeredRotation = CalculateSteeringRotation(AlignedRotation, DeltaTime);
	const FQuat DesiredRotation = (FQuat(FVector::UpVector, FMath::DegreesToRadians(ImpactAngle)) * SteeredRotation).GetNormalized();

	const double AngleDegrees = FMath::RadiansToDegrees(StartRotation.AngularDistance(DesiredRotation));

	if (AngleDegrees <= KINDA_SMALL_NUMBER)
	{
		return false;
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
            static_cast<float>(Padding), PredictedContacts);

        if (bRotationBlocked
            || !MoveTo(RaisedLocation, FromRotation)
            || !MoveTo(RaisedLocation, ToRotation)
            || !MoveTo(ToLocation, ToRotation))
        {
            // 간격 확보와 앞 구간의 회전까지 모두 되돌린다.
            ScopedMove.RevertMove();

            TargetRotation = UpdatedComponent->GetComponentQuat();
        	
            return false;
        }
    }

    TargetRotation = UpdatedComponent->GetComponentQuat();
    return true;
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

bool UMWVehicleMovementComponent::TryCalculateGroundPlane(FHitResult& InOutContact, const FVector& ReferenceNormal,
	FVector& OutNormal) const
{
	const UWorld* World = GetWorld();

	if (!bDriveConfigReady ||
		!World ||
		!PawnOwner ||
		!UpdatedPrimitive ||
		!UpdatedComponent ||
		ReferenceNormal.IsNearlyZero())
	{
		return false;
	}

	const FCollisionShape Shape = UpdatedPrimitive->GetCollisionShape();

	if (!Shape.IsBox())
	{
		return false;
	}

	const FVector Extent = Shape.GetBox();
	const FQuat Rotation = UpdatedComponent->GetComponentQuat();
	const FVector Forward = FVector::VectorPlaneProject(Rotation.GetAxisX(), ReferenceNormal).GetSafeNormal();
	const FVector Right = FVector::CrossProduct(ReferenceNormal, Forward).GetSafeNormal();

	if (Forward.IsNearlyZero() || Right.IsNearlyZero())
	{
		return false;
	}

	const bool bHasBoxContact = InOutContact.IsValidBlockingHit();
	const FVector Location = UpdatedComponent->GetComponentLocation();
	const double SupportRadius = CalculateBoxSupportRadius(Extent, Rotation, ReferenceNormal);
	const FVector FootCenter = Location - ReferenceNormal * SupportRadius;

	// 자세 조사에는 기존 범위를 사용한다. 지지를 복원할 때는 실제 차체 아래만 검사한다.
	const double X = bHasBoxContact ? DriveConfig.GroundPlaneProbeHalfLength
		: FMath::Min<double>(DriveConfig.GroundPlaneProbeHalfLength, Extent.X);
	const double Y = bHasBoxContact ? DriveConfig.GroundPlaneProbeHalfWidth
		: FMath::Min<double>(DriveConfig.GroundPlaneProbeHalfWidth, Extent.Y);

	// 앞왼쪽, 앞오른쪽, 뒤왼쪽, 뒤오른쪽.
	const FVector2D Offsets[] =
	{
		FVector2D( X, -Y),
		FVector2D( X,  Y),
		FVector2D(-X, -Y),
		FVector2D(-X,  Y)
	};

	// 보조 지지 검사에는 박스 스윕과 같은 충돌 형상을 사용한다.
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(VehicleGroundPlane), bHasBoxContact, PawnOwner);
	const FVector ProbeOffset = ReferenceNormal * DriveConfig.GroundPlaneProbeHalfHeight;
	const auto TraceGround = [&](const FVector& Center, FHitResult& OutHit)
	{
		return World->LineTraceSingleByChannel(OutHit, Center + ProbeOffset, Center - ProbeOffset,
			VehicleGroundChannel, Params) && OutHit.IsValidBlockingHit();
	};

	FHitResult Support = InOutContact;
	const FVector ProbeUp = Rotation.GetAxisZ();
	if (!bHasBoxContact)
	{
		// 중앙 지지가 있어야 한다. 양끝 샘플만으로 틈이나 낭떠러지를 연결하지 않는다.
		if (!TraceGround(FootCenter, Support))
		{
			return false;
		}

		const FVector SupportNormal = FVector(Support.ImpactNormal).GetSafeNormal();
		const double ProbeDot = FVector::DotProduct(ProbeUp, SupportNormal);
		const double MinGroundDot = FMath::Cos(FMath::DegreesToRadians(static_cast<double>(DriveConfig.MaxGroundAngle)));
		if (ProbeDot < MinGroundDot || FVector::DotProduct(ReferenceNormal, SupportNormal) < MinGroundDot)
		{
			return false;
		}

		// 선 검사에서 확인한 평면까지 박스가 아래로 이동할 거리를 계산한다.
		const double Radius = CalculateBoxSupportRadius(Extent, Rotation, SupportNormal);
		const double Distance = (FVector::DotProduct(Location - Support.ImpactPoint, SupportNormal) - Radius) / ProbeDot;
		// 자세가 바뀐 직후에는 연속된 노면이 스윕 거리 밖으로 조금 밀릴 수 있다.
		// 보조 검사에만 노면 높이 오차만큼 여유를 두고 간격 보정은 기존 한계를 사용한다.
		const double MaxDistance = static_cast<double>(DriveConfig.GroundProbeDistance) + DriveConfig.GroundPlaneMaxHeightError;
		if (Distance < -DriveConfig.GroundPlaneMaxFitError || Distance > MaxDistance)
		{
			return false;
		}

		Support.Distance = FMath::Max(0.0, Distance);
		Support.Location = Location - ProbeUp * Support.Distance;
		Support.TraceStart = Location;
		Support.TraceEnd = Location - ProbeUp * MaxDistance;
		Support.Time = Support.Distance / MaxDistance;
	}

	const FVector ContactNormal = FVector(Support.ImpactNormal).GetSafeNormal();

	TArray<FVector, TInlineAllocator<4>> Points;
	int32 FrontPointCount = 0;

	const double MinNormalDot = FMath::Cos(FMath::DegreesToRadians(static_cast<double>(DriveConfig.GroundPlaneMaxNormalAngle)));

	for (const FVector2D& Offset : Offsets)
	{
		const FVector Center = FootCenter + Forward * Offset.X + Right * Offset.Y;
		FHitResult Hit;

		if (!TraceGround(Center, Hit))
		{
			continue;
		}

		const FVector SampleNormal = FVector(Hit.ImpactNormal).GetSafeNormal();

		if (SampleNormal.IsNearlyZero())
		{
			continue;
		}

		if (FVector::DotProduct(ContactNormal, SampleNormal) < MinNormalDot)
		{
			continue;
		}

		const double HeightError = FMath::Abs(FVector::DotProduct(Hit.ImpactPoint - Support.ImpactPoint, ContactNormal));

		// 가까운 다른 높이의 면을 무리하게 함께 사용하지 않는다.
		if (HeightError > DriveConfig.GroundPlaneMaxHeightError)
		{
			continue;
		}

		Points.Add(Hit.ImpactPoint);
		FrontPointCount += Offset.X > 0.0 ? 1 : 0;
	}

	if (!bHasBoxContact)
	{
		// 중앙과 앞뒤 양쪽이 같은 주행면에 있어야 보조 접지를 허용한다.
		if (FrontPointCount == 0 || FrontPointCount == Points.Num())
		{
			return false;
		}
		if (Points.Num() == 2)
		{
			Points.Add(Support.ImpactPoint);
		}
	}

	if (Points.Num() < 3)
	{
		return false;
	}

	FVector Normal;

	if (Points.Num() == 4)
	{
		const FVector FrontToBack = (Points[0] + Points[1]) - (Points[2] + Points[3]);
		const FVector LeftToRight = (Points[1] + Points[3]) - (Points[0] + Points[2]);

		Normal = FVector::CrossProduct(FrontToBack, LeftToRight).GetSafeNormal();
	}
	else
	{
		Normal = FVector::CrossProduct(Points[1] - Points[0], Points[2] - Points[0]).GetSafeNormal();
	}

	if (Normal.IsNearlyZero())
	{
		return false;
	}

	if (FVector::DotProduct(Normal, ContactNormal) < 0.0)
	{
		Normal = -Normal;
	}

	if (FVector::DotProduct(Normal, ContactNormal) < MinNormalDot)
	{
		return false;
	}

	FVector Center = FVector::ZeroVector;

	for (const FVector& Point : Points)
	{
		Center += Point;
	}

	Center /= Points.Num();

	// 이어지는 곡면에는 평면 맞춤 오차보다 큰 편차가 생길 수 있다.
	// 보조 접지에서는 기존 높이 오차 범위까지 허용한다.
	const double MaxFitError = bHasBoxContact ? DriveConfig.GroundPlaneMaxFitError : DriveConfig.GroundPlaneMaxHeightError;
	for (const FVector& Point : Points)
	{
		const double FitError = FMath::Abs(FVector::DotProduct(Point - Center, Normal));

		if (FitError > MaxFitError)
		{
			return false;
		}
	}

	// 중앙의 지지가 앞뒤에서 구한 평면과 일치할 때만 접촉을 확정한다.
	if (!bHasBoxContact)
	{
		const double CenterError = FMath::Abs(FVector::DotProduct(Support.ImpactPoint - Center, Normal));
		if (CenterError > MaxFitError)
		{
			return false;
		}
		InOutContact = Support;
	}

	OutNormal = Normal;
	return true;
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
	const FVector& TangentVelocity, bool bWasGrounded, FGroundSupportDiagnostics& Diagnostics) const
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
	// 한 트랙의 노면이 여러 메시로 구성되어 있어도, 다음 트랙까지 보조를 이어가지는 않는다.
	Diagnostics.bSameTrackAhead = Contact.GetActor() && Contact.GetActor() == AheadHit.GetActor();
	if (bWasGrounded && !Diagnostics.bSameTrackAhead && SignedCurvature < 0.0 && Normal.Z > 0.0 && AheadNormal.Z > 0.0)
	{
		// 다음 조각의 기울기는 실제로 닿기 전까지 현재 노면의 볼록한 곡률로 보지 않는다.
		Diagnostics.CurvatureResult = TEXT("CrossTrackCrest");
		return 0.0;
	}

	// 원형 경로에서는 v²/R에 해당한다.
	return SpeedSquared * SignedCurvature;
}

double UMWVehicleMovementComponent::CalculateGroundNormalLoadAcceleration(const FHitResult& Contact,
	const FVector& TangentVelocity, bool bWasGrounded, FGroundSupportDiagnostics& Diagnostics) const
{
	const FVector Normal = FVector(Contact.ImpactNormal).GetSafeNormal();
	const FVector Gravity(0.0, 0.0, GetGravityZ() * DriveConfig.GravityScale);
	
	const double RequiredNormalAcceleration = EstimateRequiredNormalAcceleration(Contact, TangentVelocity, bWasGrounded, Diagnostics);
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
	const double Limit = bWasGrounded ? -DriveConfig.GroundSupportTolerance - Diagnostics.CrestSupportAcceleration : 0.0;
	UE_LOG(LogMWVehicleSupport, Log,
		TEXT("VehicleSupport Seq=%llu Time=%.6f Frame=%llu NetMode=%d Role=%d Actor=%s Decision=%s ")
		TEXT("WasGrounded=%d Grounded=%d Evaluated=%d Curvature=%s Speed=%.6f Required=%.6f ")
		TEXT("GravityNormal=%.6f Downforce=%.6f Load=%.6f Limit=%.6f Distance=%.6f SampleDistance=%.6f ")
		TEXT("PX=%.6f PY=%.6f PZ=%.6f NX=%.6f NY=%.6f NZ=%.6f ")
		TEXT("AheadNX=%.6f AheadNY=%.6f AheadNZ=%.6f VX=%.6f VY=%.6f VZ=%.6f Candidate=%s"),
		++QuerySequence, World ? World->GetTimeSeconds() : 0.0, GFrameCounter,
		static_cast<int32>(GetNetMode()), PawnOwner ? static_cast<int32>(PawnOwner->GetLocalRole()) : -1,
		*GetPathNameSafe(PawnOwner), Decision, bWasGrounded, bGrounded, Diagnostics.bEvaluated,
		Diagnostics.CurvatureResult, Diagnostics.bEvaluated ? Diagnostics.TangentSpeed : Velocity.Size(),
		Diagnostics.RequiredAcceleration, Diagnostics.GravityNormalAcceleration, Diagnostics.DownforceAcceleration,
		Diagnostics.NormalLoadAcceleration, Limit, Candidate ? Candidate->Distance : -1.0,
		Diagnostics.SampleDistance, Position.X, Position.Y, Position.Z, Normal.X, Normal.Y, Normal.Z,
		Diagnostics.AheadNormal.X, Diagnostics.AheadNormal.Y, Diagnostics.AheadNormal.Z,
		Velocity.X, Velocity.Y, Velocity.Z,
		Candidate ? (Diagnostics.bUsedGroundPlane ? TEXT("GroundPlane") : TEXT("BoxSweep")) : TEXT("None"));
}

void UMWVehicleMovementComponent::MoveVehicle(float DeltaTime, TArrayView<const FMWVehicleContactSample> PredictedContacts,
	FMWVehiclePredictedImpact* OutImpact)
{
	if (!bDriveConfigReady || !PawnOwner || !UpdatedComponent || DeltaTime <= 0.f)
	{
		return;
	}

	const FVector SurfaceVelocity = GetWorld()->GetTimeSeconds() < TrackSurfaceEndTime
		? TrackSurfaceVelocity : FVector::ZeroVector;
	const FVector MoveDelta = (Velocity + SurfaceVelocity) * DeltaTime;
	const UBoxComponent* Box = Cast<UBoxComponent>(UpdatedComponent);
	double ContactFraction = 1.0;
	FVector ContactNormal = FVector::ZeroVector;
	const FMWVehicleContactSample* PredictedHit = nullptr;
	if (Box && !PredictedContacts.IsEmpty())
	{
		PredictedHit = FindPredictedContact(Box->GetComponentLocation(), TargetRotation, Box->GetScaledBoxExtent(),
			MoveDelta, DeltaTime, PredictedContacts, ContactFraction, ContactNormal);
	}

	// 벽이 예측 차량보다 먼저 닿으면 기존 월드 충돌을 우선한다.
	if (!MoveWithWorldCollision(MoveDelta * ContactFraction, DeltaTime * ContactFraction, SurfaceVelocity))
	{
		// 벽 반발로 기존 차량 접촉 예측이 취소돼도 스텝의 나머지 시간을 잃지 않는다.
		const float RemainingTime = DeltaTime * (1.0 - ContactFraction);
		if (RemainingTime > SMALL_NUMBER && IsValid(PawnOwner) && IsValid(UpdatedComponent))
		{
			MoveWithWorldCollision((Velocity + SurfaceVelocity) * RemainingTime, RemainingTime, SurfaceVelocity);
		}
		return;
	}
	if (PredictedHit && IsValid(Box))
	{
		// 예측에서는 내 차량만 바꾼다. 상대에게 적용될 속도는 다음 예측 Sweep에만 사용한다.
		const FMWVehicleContactTuning Tuning = GetContactTuning();
		const FContactResponse Response = CalculateContactResponse(Velocity, PredictedHit->Velocity, ContactNormal, Tuning, PredictedHit->Tuning);
		const FVector OtherDelta = PredictedHit->Velocity * DeltaTime + PredictedHit->CorrectionDelta;
		const FVector CenterOffset = PredictedHit->Location + OtherDelta * ContactFraction - Box->GetComponentLocation();
		FVector ArmA, ArmB;
		GetContactArms(ContactNormal, CenterOffset, Box->GetComponentQuat(), Box->GetScaledBoxExtent(),
			PredictedHit->Rotation, PredictedHit->Extent, ArmA, ArmB);
		ApplyExternalVelocityChange(Response.DeltaA);
		ApplyContactYaw(ImpactYawRate, ArmA, ContactNormal * Response.Impulse, Tuning.Mass, Box->GetScaledBoxExtent());
		if (OutImpact)
		{
			OutImpact->SampleIndex = PredictedHit - PredictedContacts.GetData();
			OutImpact->Fraction = ContactFraction;
			OutImpact->OtherVelocityDelta = Response.DeltaB;
			OutImpact->Normal = ContactNormal;
			ApplyContactYaw(OutImpact->OtherYawRateDelta, ArmB, -ContactNormal * Response.Impulse,
				PredictedHit->Tuning.Mass, PredictedHit->Extent);
		}
		// 접촉이 스텝 중간에 일어나면 바뀐 속도로 남은 시간도 이동한다.
		// 두 차량의 공통 이동까지 중단하면 붙어서 달릴 때 내 차만 제자리에 막힌다.
		const float RemainingTime = DeltaTime * (1.0 - ContactFraction);
		MoveWithWorldCollision((Velocity + SurfaceVelocity) * RemainingTime, RemainingTime, SurfaceVelocity);
	}
}

bool UMWVehicleMovementComponent::MoveWithWorldCollision(const FVector& Delta, float DeltaTime, const FVector& InSurfaceVelocity)
{
	// 첫 구간은 예측 차량 접촉 시각까지다. 벽에 먼저 닿으면 기존 접촉 예측은 폐기한다.
	FVector SurfaceVelocity = InSurfaceVelocity;
	FVector MoveDelta = Delta;
	float RemainingTime = DeltaTime;
	bool bUnblocked = true;
	constexpr int32 MaxContactIterations = 4;
	for (int32 Iteration = 0; Iteration < MaxContactIterations; ++Iteration)
	{
		FHitResult Hit(1.f);
		SafeMoveUpdatedComponent(MoveDelta, TargetRotation, true, Hit);
		if (!IsValid(PawnOwner) || !IsValid(UpdatedComponent))
		{
			return false;
		}
		if (const auto* Vehicle = Cast<AMWVehiclePawn>(PawnOwner); Vehicle && !Vehicle->CanDrive())
		{
			StopMovementImmediately();
			return false;
		}
		if (!Hit.IsValidBlockingHit())
		{
			return bUnblocked;
		}
		bUnblocked = false;
		HandleImpact(Hit, RemainingTime, MoveDelta);
		if (!IsValid(PawnOwner) || !IsValid(UpdatedComponent) || TryResolveVehicleContact(Hit))
		{
			return false;
		}
		RemainingTime *= 1.f - Hit.Time;
		const FVector PreviousMoveVelocity = Velocity + SurfaceVelocity;
		ResolveContactVelocity(Hit);
		const double SurfaceSpeedAlongNormal = FVector::DotProduct(SurfaceVelocity, Hit.Normal);
		if (SurfaceSpeedAlongNormal < 0.0)
		{
			SurfaceVelocity -= Hit.Normal * SurfaceSpeedAlongNormal;
		}
		if (RemainingTime <= SMALL_NUMBER || (Hit.Time <= SMALL_NUMBER
			&& (Velocity + SurfaceVelocity).Equals(PreviousMoveVelocity, KINDA_SMALL_NUMBER)))
		{
			return false;
		}
		MoveDelta = (Velocity + SurfaceVelocity) * RemainingTime;
	}
	return false;
}

bool UMWVehicleMovementComponent::TryResolveVehicleContact(const FHitResult& Hit)
{
	if (!IsValid(PawnOwner) || !PawnOwner->HasAuthority() || !Hit.IsValidBlockingHit())
	{
		return false;
	}

	AMWVehiclePawn* OwnPawn = Cast<AMWVehiclePawn>(PawnOwner);
	AMWVehiclePawn* OtherPawn = Cast<AMWVehiclePawn>(Hit.GetActor());
	if (!OwnPawn || !IsValid(OtherPawn) || OtherPawn == OwnPawn || !OwnPawn->CanDrive() || !OtherPawn->CanDrive())
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

	ApplyExternalVelocityChange(Response.DeltaA);
	Other->ApplyExternalVelocityChange(Response.DeltaB);
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
	OwnPawn->NotifyDriveContact(OtherPawn, Normal);
	OtherPawn->NotifyDriveContact(OwnPawn, -Normal);

	return true;
}

void UMWVehicleMovementComponent::AdjustGroundHeight(float DeltaTime)
{
	if (!bDriveConfigReady ||
		!PawnOwner ||
		!UpdatedComponent ||
		!bGrounded ||
		!FMath::IsFinite(DeltaTime) ||
		DeltaTime <= 0.f)
	{
		return;
	}

	const double Distance = GroundDistance;
    const double Target = DriveConfig.GroundClearance;
    const double Tolerance = DriveConfig.GroundHeightTolerance;

    const double Lower = Target - Tolerance;
    const double Upper = Target + Tolerance;

	double Error = 0.0;

	if (Distance < Lower)
	{
		// 너무 가까우므로 노면에서 멀어지는 방향으로 보정한다.
		Error = Distance - Lower;
	}
	else if (Distance > Upper)
	{
		// 너무 멀므로 노면에 가까워지는 방향으로 보정한다.
		Error = Distance - Upper;
	}
	else
	{
		// 허용 범위 안의 작은 변화에는 반응하지 않는다.
		return;
	}

	// 이번 이동 단계에서 일반적으로 허용할 보정량.
	const double MaxStep = static_cast<double>(DriveConfig.GroundHeightCorrectionSpeed) * DeltaTime;
	double Step = FMath::Clamp(Error, -MaxStep, MaxStep);
	const double MinDistance = DriveConfig.GroundMinClearance;
	const double MaxDistance = DriveConfig.GroundProbeDistance - DriveConfig.GroundProbeSafetyMargin;
	const double PredictedDistance = Distance - Step;

	// 보정 속도보다 안전한 간격 확보를 우선한다.
	if (PredictedDistance < MinDistance)
	{
		Step = Distance - MinDistance;
	}
	else if (PredictedDistance > MaxDistance)
	{
		Step = Distance - MaxDistance;
	}

	if (FMath::Abs(Step) <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// 스윕과 보조 지지 검사에서 GroundDistance를 계산한 방향과 일치시킨다.
	const FVector ProbeUp = GetGroundProbeUp();

	FHitResult Hit(1.f);

	SafeMoveUpdatedComponent(
		-ProbeUp * Step,
		UpdatedComponent->GetComponentQuat(),
		true,
		Hit);

	// 충돌로 계획한 만큼 움직이지 못했을 수도 있으므로
	// 실제 이동 결과를 기준으로 다시 검사한다.
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
	const FVector Up = GroundOrientationNormal.GetSafeNormal();

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
