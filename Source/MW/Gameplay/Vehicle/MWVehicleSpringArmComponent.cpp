#include "Gameplay/Vehicle/MWVehicleSpringArmComponent.h"

#include "Engine/World.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "DrawDebugHelpers.h"

namespace
{
	double GetMovingTargetWeight(double TimeScale, double Decay)
	{
		// 아주 짧은 프레임에서도 1-exp(-x)의 소수점 손실과 0으로 나누는 일을 피한다.
		if (TimeScale < 0.0001)
		{
			return 1.0 - TimeScale * 0.5 + TimeScale * TimeScale / 6.0;
		}
		return (1.0 - Decay) / TimeScale;
	}

	FQuat ShortestRotation(const FQuat& Rotation)
	{
		const FQuat Normalized = Rotation.GetNormalized();
		return Normalized.W < 0.0 ? Normalized * -1.0 : Normalized;
	}
}

void UMWVehicleSpringArmComponent::ResetCameraLag()
{
	bHasCameraHistory = false;
}

void UMWVehicleSpringArmComponent::UpdateDesiredArmLocation(bool bDoTrace, bool bDoLocationLag,
	bool bDoRotationLag, float DeltaTime)
{
	if (bClampToMaxPhysicsDeltaTime)
	{
		DeltaTime = FMath::Min(DeltaTime, UPhysicsSettings::Get()->MaxPhysicsDeltaTime);
	}
	const FVector ArmOrigin = GetComponentLocation() + TargetOffset;
	const FQuat TargetRotation = GetTargetRotation().Quaternion();
	FVector DesiredLocation = ArmOrigin;
	FQuat DesiredRotation = TargetRotation;

	if (bHasCameraHistory && DeltaTime <= 0.f)
	{
		// 일시 정지처럼 시간이 진행되지 않은 틱에서 Lag가 갑자기 사라지지 않게 한다.
		DesiredLocation = bDoLocationLag && CameraLagSpeed > 0.f ? PreviousDesiredLoc : ArmOrigin;
		DesiredRotation = bDoRotationLag && CameraRotationLagSpeed > 0.f
			? PreviousDesiredRot.Quaternion() : TargetRotation;
	}
	else if (bHasCameraHistory)
	{
		// 이전 목표와 현재 목표 사이가 직선으로 움직였다고 가정한 지수 감쇠의 해를 사용한다.
		// 등속 주행 시 뒤따르는 거리가 FPS에 따라 달라지지 않으므로 차체와 카메라가 흔들리지 않는다.
		if (bDoLocationLag && CameraLagSpeed > 0.f)
		{
			const double TimeScale = CameraLagSpeed * static_cast<double>(DeltaTime);
			const double Decay = FMath::Exp(-TimeScale);
			const double TargetWeight = GetMovingTargetWeight(TimeScale, Decay);
			DesiredLocation += (PreviousDesiredLoc - PreviousArmOrigin) * Decay
				- (ArmOrigin - PreviousArmOrigin) * TargetWeight;
		}
		if (bDoRotationLag && CameraRotationLagSpeed > 0.f)
		{
			const double TimeScale = CameraRotationLagSpeed * static_cast<double>(DeltaTime);
			const double Decay = FMath::Exp(-TimeScale);
			const double TargetWeight = GetMovingTargetWeight(TimeScale, Decay);
			const FQuat PreviousError = ShortestRotation(PreviousDesiredRot.Quaternion() * PreviousTargetRotation.Inverse());
			const FQuat TargetDelta = ShortestRotation(TargetRotation * PreviousTargetRotation.Inverse());
			// 회전 차이를 가장 짧은 방향으로 풀어 Yaw가 -180/180도를 지날 때도 연속성을 유지한다.
			const FQuat ErrorLog = PreviousError.Log() * Decay - TargetDelta.Log() * TargetWeight;
			DesiredRotation = (ErrorLog.Exp() * TargetRotation).GetNormalized();
		}
	}

	if (bDoLocationLag && CameraLagMaxDistance > 0.f)
	{
		DesiredLocation = ArmOrigin + (DesiredLocation - ArmOrigin).GetClampedToMaxSize(CameraLagMaxDistance);
	}
	PreviousArmOrigin = ArmOrigin;
	PreviousTargetRotation = TargetRotation;
	PreviousDesiredLoc = DesiredLocation;
	PreviousDesiredRot = DesiredRotation.Rotator();
	bHasCameraHistory = true;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (bDrawDebugLagMarkers)
	{
		DrawDebugSphere(GetWorld(), ArmOrigin, 5.f, 8, FColor::Green);
		DrawDebugSphere(GetWorld(), DesiredLocation, 5.f, 8, FColor::Yellow);
	}
#endif

	// 팔 길이, 소켓 오프셋과 카메라 충돌 검사는 기존 SpringArm 방식으로 유지한다.
	DesiredLocation -= DesiredRotation.GetForwardVector() * TargetArmLength;
	DesiredLocation += DesiredRotation.RotateVector(SocketOffset);
	FVector ResultLocation = DesiredLocation;
	UnfixedCameraPosition = DesiredLocation;
	bIsCameraFixed = false;
	if (bDoTrace && TargetArmLength != 0.f)
	{
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MWVehicleCamera), false, GetOwner());
		FHitResult Hit;
		GetWorld()->SweepSingleByChannel(Hit, ArmOrigin, DesiredLocation, FQuat::Identity, ProbeChannel,
			FCollisionShape::MakeSphere(ProbeSize), QueryParams);
		ResultLocation = BlendLocations(DesiredLocation, Hit.Location, Hit.bBlockingHit, DeltaTime);
		bIsCameraFixed = ResultLocation != DesiredLocation;
	}
	const FTransform CameraTransform(DesiredRotation, ResultLocation);
	const FTransform RelativeCameraTransform = CameraTransform.GetRelativeTransform(GetComponentTransform());
	RelativeSocketLocation = RelativeCameraTransform.GetLocation();
	RelativeSocketRotation = RelativeCameraTransform.GetRotation();
	UpdateChildTransforms();
}
