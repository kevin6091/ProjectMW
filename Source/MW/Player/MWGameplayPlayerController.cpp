


#include "Player/MWGameplayPlayerController.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "Data/MWInputData.h"
#include "MWGameplayTags.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputMappingContext.h"

AMWGameplayPlayerController::AMWGameplayPlayerController()
{
	bAutoManageActiveCameraTarget = true;
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
}

void AMWGameplayPlayerController::BeginPlay()
{
	Super::BeginPlay();
	ApplyGameplayInputMode();
	RefreshDrivingInput();
}

void AMWGameplayPlayerController::ReceivedPlayer()
{
	Super::ReceivedPlayer();
	ApplyGameplayInputMode();
	RefreshDrivingInput();
}

void AMWGameplayPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	RefreshDrivingInput();
}

void AMWGameplayPlayerController::SetPawn(APawn* InPawn)
{
	Super::SetPawn(InPawn);
	// 서버의 Possess와 소유 클라이언트의 Pawn 복제 수신 경로를 함께 처리한다.
	RefreshDrivingInput();
}

void AMWGameplayPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	ApplyGameplayInputMode();
}

void AMWGameplayPlayerController::PostSeamlessTravel()
{
	Super::PostSeamlessTravel();
	ApplyGameplayInputMode();
	RefreshDrivingInput();
}

void AMWGameplayPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearDrivingInput();
	Super::EndPlay(EndPlayReason);
}

void AMWGameplayPlayerController::ApplyGameplayInputMode()
{
	if (!IsLocalController() || !HasActorBegunPlay()) return;
	// 기존 Controller의 테마 진입 처리를 옮겼습니다. 원격 참가자의 입력 모드는 변경하지 않습니다.
	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
}

void AMWGameplayPlayerController::RefreshDrivingInput()
{
	AMWVehiclePawn* Vehicle = Cast<AMWVehiclePawn>(GetPawn());
	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer
		? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	UMWInputData* Data = Vehicle ? Vehicle->GetDrivingInputData() : nullptr;
	if (IsLocalController() && IsValid(Vehicle) && EnhancedInput && Subsystem && Data
		&& InputVehicle == Vehicle && DrivingInputComponent == EnhancedInput
		&& DrivingInputSubsystem == Subsystem && BoundDrivingInputData == Data)
	{
		return;
	}

	// Pawn 교체, 입력 컴포넌트 재생성 시 이전 차량의 입력과 이 Controller가 등록한 항목만 정리한다.
	ClearDrivingInput();
	if (!IsLocalController() || !IsValid(Vehicle) || !EnhancedInput || !Subsystem)
	{
		return;
	}
	if (!Data)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: DrivingInputData is not set on %s."), *GetName(), *Vehicle->GetName());
		return;
	}

	TArray<FText> Errors;
	if (!Data->ValidateInputData(Errors))
	{
		for (const FText& Error : Errors)
		{
			UE_LOG(LogTemp, Error, TEXT("%s: %s"), *GetName(), *Error.ToString());
		}
		return;
	}

	const UInputAction* Accel = Data->FindInputActionByTag(MWGameplayTags::Input_Accel);
	const UInputAction* Reverse = Data->FindInputActionByTag(MWGameplayTags::Input_Reverse);
	const UInputAction* Steer = Data->FindInputActionByTag(MWGameplayTags::Input_Steer);
	const UInputAction* Drift = Data->FindInputActionByTag(MWGameplayTags::Input_Drift);
	if (!Accel || !Reverse || !Steer || !Drift
		|| Accel->ValueType != EInputActionValueType::Axis1D
		|| Reverse->ValueType != EInputActionValueType::Axis1D
		|| Steer->ValueType != EInputActionValueType::Axis1D
		|| Drift->ValueType != EInputActionValueType::Boolean)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: Driving actions require Accel/Reverse/Steer Axis1D and Drift Boolean."), *GetName());
		return;
	}

	InputVehicle = Vehicle;
	DrivingInputComponent = EnhancedInput;
	DrivingInputSubsystem = Subsystem;
	BoundDrivingInputData = Data;
	DrivingMappingContext = Data->GetMappingContext();
	Vehicle->SubmitDriveInput(FMWVehicleDriveInput{});

	DrivingBindingHandles.Add(EnhancedInput->BindAction(Accel, ETriggerEvent::Triggered, this, &ThisClass::HandleAccel).GetHandle());
	DrivingBindingHandles.Add(EnhancedInput->BindAction(Accel, ETriggerEvent::Completed, this, &ThisClass::StopAccel).GetHandle());
	DrivingBindingHandles.Add(EnhancedInput->BindAction(Accel, ETriggerEvent::Canceled, this, &ThisClass::StopAccel).GetHandle());
	DrivingBindingHandles.Add(EnhancedInput->BindAction(Reverse, ETriggerEvent::Triggered, this, &ThisClass::HandleReverse).GetHandle());
	DrivingBindingHandles.Add(EnhancedInput->BindAction(Reverse, ETriggerEvent::Completed, this, &ThisClass::StopReverse).GetHandle());
	DrivingBindingHandles.Add(EnhancedInput->BindAction(Reverse, ETriggerEvent::Canceled, this, &ThisClass::StopReverse).GetHandle());
	DrivingBindingHandles.Add(EnhancedInput->BindAction(Steer, ETriggerEvent::Triggered, this, &ThisClass::HandleSteer).GetHandle());
	DrivingBindingHandles.Add(EnhancedInput->BindAction(Steer, ETriggerEvent::Completed, this, &ThisClass::StopSteer).GetHandle());
	DrivingBindingHandles.Add(EnhancedInput->BindAction(Steer, ETriggerEvent::Canceled, this, &ThisClass::StopSteer).GetHandle());
	DrivingBindingHandles.Add(EnhancedInput->BindAction(Drift, ETriggerEvent::Triggered, this, &ThisClass::StartDrift).GetHandle());
	DrivingBindingHandles.Add(EnhancedInput->BindAction(Drift, ETriggerEvent::Completed, this, &ThisClass::StopDrift).GetHandle());
	DrivingBindingHandles.Add(EnhancedInput->BindAction(Drift, ETriggerEvent::Canceled, this, &ThisClass::StopDrift).GetHandle());
	Subsystem->AddMappingContext(DrivingMappingContext.Get(), 0);
	UE_LOG(LogTemp, Log, TEXT("%s: Driving input bound to %s (12 bindings)."), *GetName(), *Vehicle->GetName());
}

void AMWGameplayPlayerController::ClearDrivingInput()
{
	if (AMWVehiclePawn* Vehicle = InputVehicle.Get())
	{
		Vehicle->SubmitDriveInput(FMWVehicleDriveInput{});
	}
	if (UEnhancedInputComponent* EnhancedInput = DrivingInputComponent.Get())
	{
		for (const uint32 Handle : DrivingBindingHandles)
		{
			EnhancedInput->RemoveBindingByHandle(Handle);
		}
	}
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = DrivingInputSubsystem.Get())
	{
		if (const UInputMappingContext* Context = DrivingMappingContext.Get())
		{
			Subsystem->RemoveMappingContext(Context);
		}
	}
	DrivingBindingHandles.Reset();
	InputVehicle.Reset();
	DrivingInputComponent.Reset();
	DrivingInputSubsystem.Reset();
	DrivingMappingContext.Reset();
	BoundDrivingInputData = nullptr;
}

AMWVehiclePawn* AMWGameplayPlayerController::GetInputVehicle() const
{
	AMWVehiclePawn* Vehicle = InputVehicle.Get();
	return IsLocalController() && Vehicle == GetPawn() && IsValid(Vehicle)
		&& Vehicle->GetController() == this && Vehicle->InputEnabled()
		? Vehicle : nullptr;
}

void AMWGameplayPlayerController::HandleAccel(const FInputActionValue& Value)
{
	if (AMWVehiclePawn* Vehicle = GetInputVehicle())
	{
		FMWVehicleDriveInput Input = Vehicle->GetDriveInput();
		Input.Throttle = Value.Get<float>();
		Vehicle->SubmitDriveInput(Input);
	}
}

void AMWGameplayPlayerController::HandleReverse(const FInputActionValue& Value)
{
	if (AMWVehiclePawn* Vehicle = GetInputVehicle())
	{
		FMWVehicleDriveInput Input = Vehicle->GetDriveInput();
		Input.Reverse = Value.Get<float>();
		Vehicle->SubmitDriveInput(Input);
	}
}

void AMWGameplayPlayerController::HandleSteer(const FInputActionValue& Value)
{
	if (AMWVehiclePawn* Vehicle = GetInputVehicle())
	{
		// Pawn의 현재 입력을 읽으므로 텔레포트로 초기화한 이전 가속을 되살리지 않는다.
		FMWVehicleDriveInput Input = Vehicle->GetDriveInput();
		Input.Steering = Value.Get<float>();
		Vehicle->SubmitDriveInput(Input);
	}
}

void AMWGameplayPlayerController::StopAccel()
{
	HandleAccel(FInputActionValue(0.f));
}

void AMWGameplayPlayerController::StopReverse()
{
	HandleReverse(FInputActionValue(0.f));
}

void AMWGameplayPlayerController::StopSteer()
{
	HandleSteer(FInputActionValue(0.f));
}

void AMWGameplayPlayerController::StartDrift()
{
	if (AMWVehiclePawn* Vehicle = GetInputVehicle())
	{
		FMWVehicleDriveInput Input = Vehicle->GetDriveInput();
		Input.bDriftRequested = true;
		Vehicle->SubmitDriveInput(Input);
	}
}

void AMWGameplayPlayerController::StopDrift()
{
	if (AMWVehiclePawn* Vehicle = GetInputVehicle())
	{
		FMWVehicleDriveInput Input = Vehicle->GetDriveInput();
		Input.bDriftRequested = false;
		Vehicle->SubmitDriveInput(Input);
	}
}

