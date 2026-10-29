

#include "Gameplay/Vehicle/MWVehicleCharacter.h"
#include "Gameplay/Vehicle/MWVehicleMovementComponent.h"
#include "Data/MWInputData.h"
#include "MWGameplayTags.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"

#include "InputAction.h"
#include "InputMappingContext.h"

// 기본 CharacterMovement를 차량용 이동 컴포넌트로 교체
AMWVehicleCharacter::AMWVehicleCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UMWVehicleMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	bUseControllerRotationYaw = false;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 550.f;
	CameraBoom->SetRelativeRotation(FRotator(-15.f, 0.f, 0.f));
	CameraBoom->SocketOffset = FVector(0.f, 0.f, 100.f);
	CameraBoom->bDoCollisionTest = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
}

void AMWVehicleCharacter::SubmitDriveInput(const FMWVehicleDriveInput& NewInput)
{
	if (UMWVehicleMovementComponent* Movement = Cast<UMWVehicleMovementComponent>(GetCharacterMovement()))
	{
		Movement->SetDriveInput(NewInput);
	}
}

// Called when the game starts or when spawned
void AMWVehicleCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called to bind functionality to input
void AMWVehicleCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
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
	const UInputAction* Brake = DrivingInputData->FindInputActionByTag(MWGameplayTags::Input_Brake);
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

	EIC->BindAction(Brake, ETriggerEvent::Triggered, this, &ThisClass::HandleBrake);
	EIC->BindAction(Brake, ETriggerEvent::Completed, this, &ThisClass::StopBrake);
	EIC->BindAction(Brake, ETriggerEvent::Canceled, this, &ThisClass::StopBrake);

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

void AMWVehicleCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DriveInput = FMWVehicleDriveInput();
	SubmitDriveInput(DriveInput);

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
	
	Super::EndPlay(EndPlayReason);
}

void AMWVehicleCharacter::HandleAccel(const FInputActionValue& Value)
{
	DriveInput.Throttle = Value.Get<float>();
	SubmitDriveInput(DriveInput);
}

void AMWVehicleCharacter::HandleBrake(const FInputActionValue& Value)
{
	DriveInput.Brake = Value.Get<float>();
	SubmitDriveInput(DriveInput);
}

void AMWVehicleCharacter::HandleSteer(const FInputActionValue& Value)
{
	DriveInput.Steering = Value.Get<float>();
	SubmitDriveInput(DriveInput);
}

void AMWVehicleCharacter::StopAccel()
{
	DriveInput.Throttle = 0.0f;
	SubmitDriveInput(DriveInput);
}

void AMWVehicleCharacter::StopBrake()
{
	DriveInput.Brake = 0.0f;
	SubmitDriveInput(DriveInput);
}

void AMWVehicleCharacter::StopSteer()
{
	DriveInput.Steering = 0.0f;
	SubmitDriveInput(DriveInput);
}

void AMWVehicleCharacter::StartDrift()
{
	DriveInput.bDriftRequested = true;
	SubmitDriveInput(DriveInput);
}

void AMWVehicleCharacter::StopDrift()
{
	DriveInput.bDriftRequested = false;
	SubmitDriveInput(DriveInput);
}

