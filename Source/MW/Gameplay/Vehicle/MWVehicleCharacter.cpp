

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

	if (!DrivingInputData) return;

	APlayerController* PC = Cast<APlayerController>(GetController());
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	
	if (!PC || !EIC) return;

	if (ULocalPlayer* LP = PC->GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			if (const UInputMappingContext* Context = DrivingInputData->GetMappingContext())
				Subsystem->AddMappingContext(Context, 0);
		}
	}
	
	const UInputAction* Accel = DrivingInputData->FindInputActionByTag(MWGameplayTags::Input_Accel);
	const UInputAction* Brake = DrivingInputData->FindInputActionByTag(MWGameplayTags::Input_Brake);
	const UInputAction* Steer = DrivingInputData->FindInputActionByTag(MWGameplayTags::Input_Steer);
	// const UInputAction* Drift = DrivingInputData->FindInputActionByTag(MWGameplayTags::Input_Drift);
	// const UInputAction* UseItem = DrivingInputData->FindInputActionByTag(MWGameplayTags::Input_UseItem);
	
	EIC->BindAction(Accel, ETriggerEvent::Triggered, this, &ThisClass::HandleAccel);
	EIC->BindAction(Accel, ETriggerEvent::Completed, this, &ThisClass::StopAccel);
	EIC->BindAction(Accel, ETriggerEvent::Canceled, this, &ThisClass::StopAccel);

	EIC->BindAction(Brake, ETriggerEvent::Triggered, this, &ThisClass::HandleBrake);
	EIC->BindAction(Brake, ETriggerEvent::Completed, this, &ThisClass::StopBrake);
	EIC->BindAction(Brake, ETriggerEvent::Canceled, this, &ThisClass::StopBrake);

	EIC->BindAction(Steer, ETriggerEvent::Triggered, this, &ThisClass::HandleSteer);
	EIC->BindAction(Steer, ETriggerEvent::Completed, this, &ThisClass::StopSteer);
	EIC->BindAction(Steer, ETriggerEvent::Canceled, this, &ThisClass::StopSteer);
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

