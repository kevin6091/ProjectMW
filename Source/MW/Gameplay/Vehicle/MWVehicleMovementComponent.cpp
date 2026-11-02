

#include "Gameplay/Vehicle/MWVehicleMovementComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"

UMWVehicleMovementComponent::UMWVehicleMovementComponent()
{
	MaxWalkSpeed = 1800.f;
	MaxAcceleration = 1600.f;
	GroundFriction = 1.5f; // 마찰력

	bUseSeparateBrakingFriction = true;
	BrakingFriction = 0.5f; // 브레이크 시 마찰력

	bOrientRotationToMovement = true;
	bUseControllerDesiredRotation = false;
}

void UMWVehicleMovementComponent::SetDriveInput(const FMWVehicleDriveInput& NewInput)
{
	// 각 입력을 허용 범위로 제한
	DriveInput.Throttle = FMath::IsFinite(NewInput.Throttle) ? FMath::Clamp(NewInput.Throttle, 0.f, 1.f) : 0.f;
	DriveInput.Reverse = FMath::IsFinite(NewInput.Reverse) ? FMath::Clamp(NewInput.Reverse, 0.f, 1.f) : 0.f;
	DriveInput.Steering = FMath::IsFinite(NewInput.Steering) ? FMath::Clamp(NewInput.Steering, -1.f, 1.f) : 0.f;
	DriveInput.bDriftRequested = NewInput.bDriftRequested;

	// 브레이크 강도에 따라 보간
	// BrakingDecelerationWalking = FMath::Lerp(CoastDeceleration, FullBrakeDeceleration, DriveInput.Reverse);
}

void UMWVehicleMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	if (HasValidData() && CharacterOwner->IsLocallyControlled())
	{
		float EffectiveThrottle = 0.f;
		float EffectiveBrake = 0.f;
		
		ResolveDriveControl(EffectiveThrottle, EffectiveBrake);
		
		if (!FMath::IsNearlyZero(EffectiveThrottle))
		{
			CharacterOwner->AddMovementInput(UpdatedComponent->GetForwardVector(), EffectiveThrottle);
		}
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UMWVehicleMovementComponent::PhysicsRotation(float DeltaTime)
{
	if (!HasValidData() || DeltaTime <= 0.f || !IsMovingOnGround()) return;

	// 전진 속도가 충분할 때만 회전
	const float ForwardSpeed = FVector::DotProduct(Velocity, UpdatedComponent->GetForwardVector());
	
	const float SpeedMagnitude = FMath::Abs(ForwardSpeed);
	
	if (SpeedMagnitude <= DriveConfig.MinSteeringSpeed || FMath::IsNearlyZero(DriveInput.Steering))
	{
		return;
	}

	// 저속에서는 회전량을 줄이고 FullSteeringSpeed 이상에서는 설정 회전 속도를 사용
	const float SpeedFactor = FMath::Clamp(SpeedMagnitude / FMath::Max(DriveConfig.FullSteeringSpeed, 1.f), 0.f, 1.f);
	const float TravelSign = ForwardSpeed >= 0.f ? 1.f : -1.f;
	
	const float TurnMultiplier = IsDriftControlActive() ? FMath::Max(DriveConfig.DriftTurnRateMultiplier, 0.f) : 1.f;
	const float DeltaYaw = DriveInput.Steering * DriveConfig.TurnRateDegPerSecond * TurnMultiplier * SpeedFactor * TravelSign * DeltaTime;
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
	
	if (DriveInput.Reverse > KINDA_SMALL_NUMBER)
	{
		return false;
	}
	
	if (FMath::Abs(DriveInput.Steering) <= DriveConfig.MinDriftSteering)
	{
		return false;
	}
	
	const float ForwardSpeed = FVector::DotProduct(Velocity, UpdatedComponent->GetForwardVector());

	return ForwardSpeed > DriveConfig.MinDriftSpeed;
}

void UMWVehicleMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	if (!IsMovingOnGround())
	{
		Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
		return;
	}
	
	float EffectiveThrottle = 0.f;
	float EffectiveBrake = 0.f;
	
	ResolveDriveControl(EffectiveThrottle, EffectiveBrake);
	
	const float EffectiveBrakingDeceleration = FMath::Lerp(DriveConfig.CoastDeceleration, DriveConfig.FullBrakeDeceleration, EffectiveBrake);
	
	float EffectiveFriction = Friction;
	
	if (IsDriftControlActive())
	{
		EffectiveFriction *= FMath::Clamp(DriveConfig.DriftFrictionMultiplier, 0.f, 1.f);
	}
	
	Super::CalcVelocity(DeltaTime, EffectiveFriction, bFluid, EffectiveBrakingDeceleration);
}

float UMWVehicleMovementComponent::GetMaxSpeed() const
{
	const float BaseMaxSpeed = Super::GetMaxSpeed();
	
	if (!HasValidData() || !IsMovingOnGround())
	{
		return BaseMaxSpeed;
	}
	
	const float ForwardSpeed = FVector::DotProduct(Velocity, UpdatedComponent->GetForwardVector());
	const float Threshold = FMath::Max(DriveConfig.DirectionChangeSpeedThreshold, 0.f);
	const float ReverseSpeedLimit = FMath::Min(BaseMaxSpeed, FMath::Max(DriveConfig.MaxReverseSpeed, 0.f));
	
	if (ForwardSpeed < -Threshold)
	{
		return ReverseSpeedLimit;
	}
	
	if (ForwardSpeed > Threshold)
	{
		return BaseMaxSpeed;
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
	
	return BaseMaxSpeed;
}

void UMWVehicleMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	
	if (!DriveData)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: DriveData is not assigned."), *GetNameSafe(GetOwner()));
		
		DisableMovement();
		SetComponentTickEnabled(false);
		return;
	}
	
	TArray<FText> Errors;
	
	if (!DriveData->ValidateConfig(Errors))
	{
		for (const FText& Error : Errors)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: %s"), *GetNameSafe(GetOwner()), *Error.ToString());
		}
		
		DisableMovement();
		SetComponentTickEnabled(false);
		return;
	}
	
	DriveConfig = DriveData->GetConfig();
	
	BrakingDecelerationWalking = DriveConfig.CoastDeceleration;
}

void UMWVehicleMovementComponent::ResolveDriveControl(float& OutThrottle, float& OutBrake) const
{
	// 기본 상태: 추진·강한 제동 없음
	OutThrottle = 0.f;
	OutBrake = 0.f;

	if (!HasValidData())
	{
		return;
	}
	
	const AController* Controller = CharacterOwner->GetController();
	
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
