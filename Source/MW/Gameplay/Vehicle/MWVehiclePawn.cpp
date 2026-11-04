

#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "Gameplay/Vehicle/MWVehicleMovementComponent.h"
#include "Data/MWInputData.h"
#include "MWGameplayTags.h"

#include "Components/BoxComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"

#include "InputAction.h"
#include "InputMappingContext.h"

// 충돌 루트와 Pawn 전용 이동 컴포넌트를 연결한다.
AMWVehiclePawn::AMWVehiclePawn()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	bUseControllerRotationYaw = false;

	CollisionRoot = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionRoot"));
	SetRootComponent(CollisionRoot);

	CollisionRoot->InitBoxExtent(FVector(100.f, 50.f, 30.f));
	CollisionRoot->SetCollisionProfileName(TEXT("Pawn"));
	CollisionRoot->SetSimulatePhysics(false);

	VehicleMovement = CreateDefaultSubobject<UMWVehicleMovementComponent>(TEXT("VehicleMovement"));
	VehicleMovement->SetUpdatedComponent(CollisionRoot);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 550.f;
	CameraBoom->SetRelativeRotation(FRotator(-15.f, 0.f, 0.f));
	CameraBoom->SocketOffset = FVector(0.f, 0.f, 100.f);
	CameraBoom->bDoCollisionTest = false;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
}

void AMWVehiclePawn::SubmitDriveInput(const FMWVehicleDriveInput& NewInput)
{
	DriveInput = NewInput;
	if (UMWVehicleMovementComponent* Movement = Cast<UMWVehicleMovementComponent>(GetMovementComponent()))
	{
		Movement->SetDriveInput(NewInput);
	}
}

UPawnMovementComponent* AMWVehiclePawn::GetMovementComponent() const
{
	return VehicleMovement;
}

void AMWVehiclePawn::UnPossessed()
{
	DriveInput = FMWVehicleDriveInput{};
	VehicleMovement->ResetDriveInput();
	RemoveDrivingMappingContext();
	Super::UnPossessed();
}

void AMWVehiclePawn::TeleportSucceeded(bool bIsATest)
{
	Super::TeleportSucceeded(bIsATest);
	if (!bIsATest)
	{
		DriveInput = FMWVehicleDriveInput{};
	}
}

// Called when the game starts or when spawned
void AMWVehiclePawn::BeginPlay()
{
	Super::BeginPlay();

}

// Called to bind functionality to input
void AMWVehiclePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	APlayerController* PC = Cast<APlayerController>(GetController());

	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("PlayerController is not valid on %s"), *GetName());
		return;
	}

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);

	if (!EIC)
	{
		UE_LOG(LogTemp, Warning, TEXT("EnhancedInputComponent is not valid on %s"), *GetName());
		return;
	}

	if (!DrivingInputData)
	{
		UE_LOG(LogTemp, Warning, TEXT("DrivingInputData is not set on %s"), *GetName());
		return;
	}

	const UInputMappingContext* MappingContext = DrivingInputData->GetMappingContext();

	const UInputAction* Accel = DrivingInputData->FindInputActionByTag(MWGameplayTags::Input_Accel);
	const UInputAction* Brake = DrivingInputData->FindInputActionByTag(MWGameplayTags::Input_Reverse);
	const UInputAction* Steer = DrivingInputData->FindInputActionByTag(MWGameplayTags::Input_Steer);
	const UInputAction* Drift = DrivingInputData->FindInputActionByTag(MWGameplayTags::Input_Drift);
	// const UInputAction* UseItem = DrivingInputData->FindInputActionByTag(MWGameplayTags::Input_UseItem);

	if (!MappingContext ||
		!Accel ||
		!Brake ||
		!Steer ||
		!Drift)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"%s: Driving input is incomplete. "
				"IMC=%s Accel=%s Brake=%s Steer=%s Drift=%s"),
			*GetNameSafe(this),
			*GetNameSafe(MappingContext),
			*GetNameSafe(Accel),
			*GetNameSafe(Brake),
			*GetNameSafe(Steer),
			*GetNameSafe(Drift));

		return;
	}

	if (Accel->ValueType != EInputActionValueType::Axis1D ||
		Brake->ValueType != EInputActionValueType::Axis1D ||
		Steer->ValueType != EInputActionValueType::Axis1D)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"%s: Accel, Brake and Steer "
				"must use Axis1D."),
			*GetNameSafe(this));

		return;
	}

	if (Drift->ValueType != EInputActionValueType::Boolean)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s: Drift must use Boolean."),
			*GetNameSafe(this));

		return;
	}

	ULocalPlayer* LocalPlayer = PC->GetLocalPlayer();

	if (!LocalPlayer)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s: LocalPlayer is unavailable."),
			*GetNameSafe(this));

		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();

	if (!InputSubsystem)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s: Enhanced Input subsystem is unavailable."),
			*GetNameSafe(this));

		return;
	}

	DriveInput = FMWVehicleDriveInput{};
	SubmitDriveInput(DriveInput);

	EIC->BindAction(Accel, ETriggerEvent::Triggered, this, &ThisClass::HandleAccel);
	EIC->BindAction(Accel, ETriggerEvent::Completed, this, &ThisClass::StopAccel);
	EIC->BindAction(Accel, ETriggerEvent::Canceled, this, &ThisClass::StopAccel);

	EIC->BindAction(Brake, ETriggerEvent::Triggered, this, &ThisClass::HandleReverse);
	EIC->BindAction(Brake, ETriggerEvent::Completed, this, &ThisClass::StopReverse);
	EIC->BindAction(Brake, ETriggerEvent::Canceled, this, &ThisClass::StopReverse);

	EIC->BindAction(Steer, ETriggerEvent::Triggered, this, &ThisClass::HandleSteer);
	EIC->BindAction(Steer, ETriggerEvent::Completed, this, &ThisClass::StopSteer);
	EIC->BindAction(Steer, ETriggerEvent::Canceled, this, &ThisClass::StopSteer);

	EIC->BindAction(Drift, ETriggerEvent::Triggered, this, &ThisClass::StartDrift);
	EIC->BindAction(Drift, ETriggerEvent::Completed, this, &ThisClass::StopDrift);
	EIC->BindAction(Drift, ETriggerEvent::Canceled, this, &ThisClass::StopDrift);

	InputSubsystem->AddMappingContext(MappingContext, 0);

	UE_LOG(
		LogTemp,
		Log,
		TEXT("%s: Driving input setup completed."),
		*GetNameSafe(this));
}

void AMWVehiclePawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DriveInput = FMWVehicleDriveInput();
	SubmitDriveInput(DriveInput);
	RemoveDrivingMappingContext();
	Super::EndPlay(EndPlayReason);
}

void AMWVehiclePawn::RemoveDrivingMappingContext()
{
	if (DrivingInputData)
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			if (ULocalPlayer* LP = PC->GetLocalPlayer())
			{
				if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
				{
					if (const UInputMappingContext* Context = DrivingInputData->GetMappingContext())
						Subsystem->RemoveMappingContext(Context);
				}
			}
		}
	}

}

void AMWVehiclePawn::HandleAccel(const FInputActionValue& Value)
{
	DriveInput.Throttle = Value.Get<float>();
	SubmitDriveInput(DriveInput);
}

void AMWVehiclePawn::HandleReverse(const FInputActionValue& Value)
{
	DriveInput.Reverse = Value.Get<float>();
	SubmitDriveInput(DriveInput);
}

void AMWVehiclePawn::HandleSteer(const FInputActionValue& Value)
{
	DriveInput.Steering = Value.Get<float>();
	SubmitDriveInput(DriveInput);
}

void AMWVehiclePawn::StopAccel()
{
	DriveInput.Throttle = 0.0f;
	SubmitDriveInput(DriveInput);
}

void AMWVehiclePawn::StopReverse()
{
	DriveInput.Reverse = 0.0f;
	SubmitDriveInput(DriveInput);
}

void AMWVehiclePawn::StopSteer()
{
	DriveInput.Steering = 0.0f;
	SubmitDriveInput(DriveInput);
}

void AMWVehiclePawn::StartDrift()
{
	DriveInput.bDriftRequested = true;
	SubmitDriveInput(DriveInput);
}

void AMWVehiclePawn::StopDrift()
{
	DriveInput.bDriftRequested = false;
	SubmitDriveInput(DriveInput);
}

