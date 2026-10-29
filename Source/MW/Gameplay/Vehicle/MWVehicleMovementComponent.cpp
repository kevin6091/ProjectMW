

#include "Gameplay/Vehicle/MWVehicleMovementComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"

UMWVehicleMovementComponent::UMWVehicleMovementComponent()
{
	MaxWalkSpeed = 1800.f;
	MaxAcceleration = 1600.f;
	GroundFriction = 1.5f; // 마찰력

	BrakingDecelerationWalking = CoastDeceleration;
	bUseSeparateBrakingFriction = true;
	BrakingFriction = 0.5f; // 브레이크 시 마찰력

	bOrientRotationToMovement = true;
	bUseControllerDesiredRotation = false;
}

void UMWVehicleMovementComponent::SetDriveInput(const FMWVehicleDriveInput& NewInput)
{
	// 각 입력을 허용 범위로 제한
	DriveInput.Throttle = FMath::IsFinite(NewInput.Throttle) ? FMath::Clamp(NewInput.Throttle, 0.f, 1.f) : 0.f;
	DriveInput.Brake = FMath::IsFinite(NewInput.Brake) ? FMath::Clamp(NewInput.Brake, 0.f, 1.f) : 0.f;
	DriveInput.Steering = FMath::IsFinite(NewInput.Steering) ? FMath::Clamp(NewInput.Steering, -1.f, 1.f) : 0.f;
	DriveInput.bDriftRequested = NewInput.bDriftRequested;

	// 브레이크 강도에 따라 보간
	BrakingDecelerationWalking = FMath::Lerp(CoastDeceleration, FullBrakeDeceleration, DriveInput.Brake);
}

void UMWVehicleMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	if (HasValidData() && CharacterOwner->IsLocallyControlled())
	{
		// 제동을 우선하며, 차량의 전진 입력을 이동 처리 전에 누적
		if (DriveInput.Brake <= KINDA_SMALL_NUMBER && DriveInput.Throttle > KINDA_SMALL_NUMBER)
		{
			CharacterOwner->AddMovementInput(UpdatedComponent->GetForwardVector(), DriveInput.Throttle);
		}
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UMWVehicleMovementComponent::PhysicsRotation(float DeltaTime)
{
	if (!HasValidData() || DeltaTime <= 0.f || !IsMovingOnGround()) return;

	// 전진 속도가 충분할 때만 회전
	const float ForwardSpeed = FVector::DotProduct(Velocity, UpdatedComponent->GetForwardVector());
	if (ForwardSpeed <= MinSteeringSpeed || FMath::IsNearlyZero(DriveInput.Steering)) return;

	// 저속에서는 회전량을 줄이고 FullSteeringSpeed 이상에서는 설정 회전 속도를 사용
	const float SpeedFactor = FMath::Clamp(ForwardSpeed / FMath::Max(FullSteeringSpeed, 1.f), 0.f, 1.f);
	const float TurnMultiplier = IsDriftControlActive() ? FMath::Max(DriftTurnRateMultiplier, 0.f) : 1.f;
	const float DeltaYaw = DriveInput.Steering * TurnRateDegPerSecond * TurnMultiplier * SpeedFactor * DeltaTime;
	const float NewYaw = UpdatedComponent->GetComponentRotation().Yaw + DeltaYaw;

	MoveUpdatedComponent(FVector::ZeroVector, FRotator(0.f, NewYaw, 0.f).Quaternion(), true);
}

bool UMWVehicleMovementComponent::IsDriftControlActive() const
{
	if (!HasValidData() || !IsMovingOnGround())
	{
		return false;
	}
	
	const AController* Controller = CharacterOwner->GetController();
	
	if (!Controller || Controller->IsMoveInputIgnored())
	{
		return false;
	}
	
	if (!DriveInput.bDriftRequested)
	{
		return false;
	}
	
	if (DriveInput.Brake > KINDA_SMALL_NUMBER)
	{
		return false;
	}
	
	if (FMath::Abs(DriveInput.Steering) <= MinDriftSteering)
	{
		return false;
	}
	
	const float ForwardSpeed = FVector::DotProduct(Velocity, UpdatedComponent->GetForwardVector());

	return ForwardSpeed > MinDriftSpeed;
}

void UMWVehicleMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	float EffectiveFriction = Friction;
	
	if (IsDriftControlActive())
	{
		EffectiveFriction *= FMath::Clamp(DriftFrictionMultiplier, 0.f, 1.f);
	}
	
	Super::CalcVelocity(DeltaTime, EffectiveFriction, bFluid, BrakingDeceleration);
}
