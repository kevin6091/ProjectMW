

#include "Player/MWGameplayPlayerController.h"
#include "UI/Gameplay/MWArrivalScreen.h"
#include "Gameplay/Items/MWItemComponent.h"
#include "Algo/AllOf.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "Gameplay/Construction/MWTrackConstructionComponent.h"
#include "Gameplay/Construction/MWTrackPlacementViewComponent.h"
#include "Gameplay/Construction/MWTrackSelectionBoard.h"
#include "Data/MWInputData.h"
#include "System/GameState/MWGameplayGameState.h"
#include "System/GameMode/MWGameplayGameMode.h"
#include "System/MWPlayerState.h"
#include "System/MWAssetManager.h"
#include "LogChannel.h"
#include "MWGameplayTags.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/LevelStreaming.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "UI/MWTrackSelectionWidget.h"

AMWGameplayPlayerController::AMWGameplayPlayerController()
{
	bAutoManageActiveCameraTarget = true;
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
	PlacementView = CreateDefaultSubobject<UMWTrackPlacementViewComponent>(TEXT("TrackPlacementView"));
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
	ApplyGameplayInputMode();
}

void AMWGameplayPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	ApplyGameplayInputMode();
}

void AMWGameplayPlayerController::PostSeamlessTravel()
{
	Super::PostSeamlessTravel();
	LastReadyRoundEpoch = 0;
	LastReadyCourseSessionId = 0;
	LastReadyCourseRevision = 0;
	NextRaceReadyReportTime = 0.0;
	ApplyGameplayInputMode();
	RefreshDrivingInput();
}

// 현재 소유한 차량에만 아이템 사용 의도를 전달하고 서버 판정은 ItemComponent에 맡긴다.
void AMWGameplayPlayerController::HandleUseItem()
{
    if (AMWVehiclePawn* Vehicle = GetInputVehicle())
    {
        Vehicle->GetItemComponent()->RequestUseItem();
    }
}

void AMWGameplayPlayerController::RequestConfirmTrack(int32 SessionId, int32 Revision, int32 OfferId)
{
	if (!IsLocalController() || LastTrackSelectionRequestId == MAX_int32)
	{
		return;
	}

	ServerConfirmTrack(SessionId, Revision, OfferId, ++LastTrackSelectionRequestId);
}

void AMWGameplayPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	PlacementView->ResetView();
	SetTrackSelectionBoard(nullptr);
	ClearDrivingInput();
	if (IsValid(TrackSelectionWidget))
	{
		TrackSelectionWidget->RemoveFromParent();
		TrackSelectionWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

EMWRoomPhase AMWGameplayPlayerController::GetConstructionPresentationPhase() const
{
	const auto* GS = GetWorld()->GetGameState<AMWGameplayGameState>();
	const EMWRoomPhase Phase = GS ? GS->GetRoomPhase() : EMWRoomPhase::PreparingConstruction;
	const auto* PS = GetPlayerState<AMWPlayerState>();
	return Phase == EMWRoomPhase::TrackPlacement && PS && GS->GetConstructionComponent()->IsPlacementReselectionPendingFor(*PS)
		? EMWRoomPhase::PreparingConstruction : Phase;
}

void AMWGameplayPlayerController::ApplyGameplayInputMode(bool bRestoreConstructionFocus)
{
	if (!IsLocalController() || !HasActorBegunPlay())
	{
		return;
	}

	const EMWRoomPhase Phase = GetConstructionPresentationPhase();
	// 실패 단계의 안내는 경기 HUD가 소유한다. 종료된 건설판이 오류 배지를 덮지 않게 한다.
	const bool bShowConstruction = TrackSelectionWidgetClass &&
		(Phase == EMWRoomPhase::PreparingConstruction || Phase == EMWRoomPhase::TrackPlacement);
	const bool bResetConstructionWidget = PresentedPhase == EMWRoomPhase::TrackPlacement
		&& Phase == EMWRoomPhase::PreparingConstruction;
	PresentedPhase = Phase;
	if ((!bShowConstruction || bResetConstructionWidget) && IsValid(TrackSelectionWidget))
	{
		TrackSelectionWidget->RemoveFromParent();
		TrackSelectionWidget = nullptr;
	}

	if (Phase == EMWRoomPhase::Failed)
	{
		// 가림 중인 복구창은 자체 포커스를 유지하고, 공개된 화면에서는 기존 게임·UI 입력을 유지한다.
		if (!GetGameInstance()->GetSubsystem<UMWArrivalScreen>()->IsCovered())
		{
			FInputModeGameAndUI Mode;
			Mode.SetHideCursorDuringCapture(false);
			Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			SetInputMode(Mode);
			bShowMouseCursor = true;
		}
		return;
	}

	if (bShowConstruction)
	{
		if (!IsValid(TrackSelectionWidget))
		{
			TrackSelectionWidget = CreateWidget<UUserWidget>(this, TrackSelectionWidgetClass);
		}

		if (IsValid(TrackSelectionWidget))
		{
			const bool bAttached = !TrackSelectionWidget->IsInViewport();
			if (bAttached)
			{
				TrackSelectionWidget->AddToViewport();
			}
			if (GetGameInstance()->GetSubsystem<UMWArrivalScreen>()->IsCovered())
			{
				SetInputMode(FInputModeUIOnly());
				bShowMouseCursor = false;
			}
			else if (bAttached || !bShowMouseCursor || bRestoreConstructionFocus)
			{
				// 가림막 뒤에서 만든 위젯도 공개 상태에 들어오면 입력과 커서를 함께 복구한다.
				FInputModeGameAndUI Mode;
				if (bRestoreConstructionFocus)
				{
					TrackSelectionWidget->SetIsFocusable(true);
					Mode.SetWidgetToFocus(TrackSelectionWidget->TakeWidget());
				}
				Mode.SetHideCursorDuringCapture(false);
				Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
				SetInputMode(Mode);
				bShowMouseCursor = true;
			}
			return;
		}
	}

	if (GetGameInstance()->GetSubsystem<UMWArrivalScreen>()->IsCovered())
        SetInputMode(FInputModeUIOnly());
    else
        SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;
	UpdateRaceViewTarget();
}

void AMWGameplayPlayerController::UpdateRaceViewTarget()
{
	if (!IsLocalController()) return;
	const auto* GS = GetWorld()->GetGameState<AMWGameplayGameState>();
	const auto* PS = GetPlayerState<AMWPlayerState>();
	AActor* Target = GetPawn();
	if (GS && PS && GS->GetProgress().RaceMode == EMWRaceMode::SuddenDeath
		&& (!PS->GetRoundState().bRaceParticipant || PS->GetRoundState().RaceStatus == EMWRaceStatus::Eliminated))
	{
		for (const APlayerState* Base : GS->PlayerArray)
		{
			const auto* Racer = Cast<AMWPlayerState>(Base);
			if (Racer && Racer->GetRoundState().bRaceParticipant && Racer->GetRoundState().RoundEpoch == GS->GetProgress().RoundEpoch
				&& Racer->GetRoundState().RaceStatus != EMWRaceStatus::Eliminated && IsValid(Racer->GetPawn()))
			{
				Target = Racer->GetPawn();
				if (Racer->GetRoundState().RaceStatus == EMWRaceStatus::Active) break;
			}
		}
	}
	if (IsValid(Target) && GetViewTarget() != Target) SetViewTarget(Target);
}

void AMWGameplayPlayerController::RestoreArrivalInput()
{
	// 가림 해제와 단계 변경은 같은 커서·입력 경로를 사용한다.
	ApplyGameplayInputMode(true);
}

void AMWGameplayPlayerController::PlayerTick(float DeltaSeconds)
{
	Super::PlayerTick(DeltaSeconds);
	if (!IsLocalController())
	{
		return;
	}
	const auto* GS = GetWorld()->GetGameState<AMWGameplayGameState>();
	// 같은 건설 단계에서 가림이 풀렸거나 표시 플래그가 다시 꺼져도 다음 프레임에 복구한다.
	const bool bConstructionCursorHidden = !bShowMouseCursor && IsValid(TrackSelectionWidget)
		&& !GetGameInstance()->GetSubsystem<UMWArrivalScreen>()->IsCovered();
	if (GS && (GetConstructionPresentationPhase() != PresentedPhase || bConstructionCursorHidden))
	{
		ApplyGameplayInputMode();
	}
	PlacementView->Update(DeltaSeconds, Cast<UEnhancedInputComponent>(InputComponent));
	if (GS && (GS->GetRoomPhase() == EMWRoomPhase::PreparingRace || GS->GetRoomPhase() == EMWRoomPhase::Countdown
		|| GS->GetRoomPhase() == EMWRoomPhase::Racing)) UpdateRaceViewTarget();

	TryReportRaceReady();
}

bool AMWGameplayPlayerController::IsDrivingInputReady() const
{
	const auto* Vehicle = Cast<AMWVehiclePawn>(GetPawn());
	const auto* EnhancedInput = DrivingInputComponent.Get();
	const auto* Subsystem = DrivingInputSubsystem.Get();
	const UInputAction* UseItem = BoundDrivingInputData
		? BoundDrivingInputData->FindInputActionByTag(MWGameplayTags::Input_UseItem) : nullptr;
	const int32 ExpectedBindings = 12 + (UseItem && UseItem->ValueType == EInputActionValueType::Boolean ? 1 : 0);
	if (!IsLocalController() || !IsValid(Vehicle) || !Vehicle->InputEnabled() || InputVehicle != Vehicle
		|| !EnhancedInput || EnhancedInput != InputComponent || !Cast<UEnhancedPlayerInput>(PlayerInput)
		|| !Subsystem || !BoundDrivingInputData || BoundDrivingInputData != Vehicle->GetDrivingInputData()
		|| !DrivingMappingContext.IsValid() || !Subsystem->HasMappingContext(DrivingMappingContext.Get())
		|| DrivingBindingHandles.Num() != ExpectedBindings)
	{
		return false;
	}
	// 바인딩을 외부에서 제거한 경우 저장된 핸들 수만으로 준비 완료라고 판단하지 않는다.
	return Algo::AllOf(DrivingBindingHandles, [EnhancedInput](uint32 Handle)
	{
		return EnhancedInput->GetActionEventBindings().ContainsByPredicate([Handle](const auto& Binding)
		{
			return Binding->GetHandle() == Handle;
		});
	});
}

bool AMWGameplayPlayerController::IsLocalRaceReady() const
{
	const UWorld* World = GetWorld();
	const auto* GS = World ? World->GetGameState<AMWGameplayGameState>() : nullptr;
	const auto* PS = GetPlayerState<AMWPlayerState>();
	const auto* Vehicle = Cast<AMWVehiclePawn>(GetPawn());
	if (!IsLocalController() || !HasActorBegunPlay() || !World || !World->AreActorsInitialized()
		|| World->IsVisibilityRequestPending() || !GS || !PS || PS->IsInactive() || !PS->IsOwnedBy(this)
		|| !GS->PlayerArray.Contains(PS) || !IsValid(Vehicle) || Vehicle->GetController() != this
		|| Vehicle->GetPlayerState() != PS || GetViewTarget() != Vehicle || !IsDrivingInputReady())
	{
		return false;
	}
	for (const ULevelStreaming* Level : World->GetStreamingLevels())
	{
		if (Level && Level->ShouldBeVisible() && (!Level->IsLevelLoaded() || !Level->IsLevelVisible()))
		{
			return false;
		}
	}
	const FMWGameplayProgress& Progress = GS->GetProgressRef();
	const FMWPlayerRoundState Round = PS->GetRoundState();
	if (Progress.Phase != EMWRoomPhase::PreparingRace || Progress.RoundEpoch <= 0
		|| Round.RoundEpoch != Progress.RoundEpoch || Round.RaceStatus != EMWRaceStatus::Waiting
		|| !Vehicle->IsReadyForRace(Progress.RoundEpoch))
	{
		return false;
	}
	const auto* Construction = GS->GetConstructionComponent();
	if (!Construction)
	{
		return false;
	}
	if (Progress.CourseSessionId == 0 && Progress.CourseRevision == 0)
	{
		const auto& Session = Construction->GetPlacementSession();
		return Session.SessionId == 0 && !Session.bIsOpen && Session.PlacedTracks.IsEmpty();
	}
	return Construction->IsCourseReadyForRace(Progress.CourseSessionId, Progress.CourseRevision);
}

void AMWGameplayPlayerController::TryReportRaceReady()
{
	const auto* GS = GetWorld()->GetGameState<AMWGameplayGameState>();
	if (!IsLocalController() || !GS || GS->GetRoomPhase() != EMWRoomPhase::PreparingRace)
	{
		return;
	}
	if (!IsDrivingInputReady())
	{
		const auto* Vehicle = Cast<AMWVehiclePawn>(GetPawn());
		UMWInputData* Data = IsValid(Vehicle) ? Vehicle->GetDrivingInputData() : nullptr;
		TArray<FText> Errors;
		if (Data && Data->ValidateInputData(Errors))
		{
			// Pawn보다 로컬 InputComponent/Subsystem이 늦게 준비된 경우 기존 연결 함수를 재사용한다.
			RefreshDrivingInput();
		}
	}
	if (!IsLocalRaceReady())
	{
		return;
	}
	const FMWGameplayProgress& Progress = GS->GetProgressRef();
	const double Now = GetWorld()->GetTimeSeconds();
	if (LastReadyRoundEpoch == Progress.RoundEpoch && LastReadyCourseSessionId == Progress.CourseSessionId
		&& LastReadyCourseRevision == Progress.CourseRevision && Now < NextRaceReadyReportTime)
	{
		return;
	}
	LastReadyRoundEpoch = Progress.RoundEpoch;
	LastReadyCourseSessionId = Progress.CourseSessionId;
	LastReadyCourseRevision = Progress.CourseRevision;
	// Reliable은 서버 수락을 보장하지 않는다. 같은 준비 키도 Countdown에 진입할 때까지 재보고한다.
	NextRaceReadyReportTime = Now + 0.5;
	ServerReportRaceReady(Progress.RoundEpoch, Progress.CourseSessionId, Progress.CourseRevision);
}

void AMWGameplayPlayerController::ServerReportRaceReady_Implementation(int32 RoundEpoch, int32 CourseSessionId, int32 CourseRevision)
{
	if (auto* Mode = GetWorld()->GetAuthGameMode<AMWGameplayGameMode>())
	{
		Mode->ReportRaceReady(this, RoundEpoch, CourseSessionId, CourseRevision);
	}
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
		&& DrivingInputSubsystem == Subsystem && BoundDrivingInputData == Data && IsDrivingInputReady())
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
	const UInputAction* UseItem = Data->FindInputActionByTag(MWGameplayTags::Input_UseItem);
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
	if (UseItem && UseItem->ValueType == EInputActionValueType::Boolean)
	{
		DrivingBindingHandles.Add(EnhancedInput->BindAction(UseItem, ETriggerEvent::Started, this, &ThisClass::HandleUseItem).GetHandle());
	}
	Subsystem->AddMappingContext(DrivingMappingContext.Get(), 0);
	UE_LOG(LogTemp, Log, TEXT("%s: Driving input bound to %s (%d bindings)."), *GetName(), *Vehicle->GetName(), DrivingBindingHandles.Num());
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
		&& Vehicle->GetController() == this && Vehicle->InputEnabled() && Vehicle->CanDrive()
		? Vehicle : nullptr;
}

void AMWGameplayPlayerController::ServerConfirmTrack_Implementation(int32 SessionId, int32 Revision, int32 OfferId,
	int32 RequestId)
{
	FMWTrackSelectionReply Reply;
	Reply.SelectionSessionId = SessionId;
	Reply.OffersRevision = Revision;
	Reply.OfferId = OfferId;
	Reply.RequestId = RequestId;
	Reply.Reason = EMWTrackSelectionRejectReason::Unavailable;

	if (AMWGameplayGameState* GS = GetWorld()->GetGameState<AMWGameplayGameState>())
	{
		if (UMWTrackConstructionComponent* Construction = GS->GetConstructionComponent())
		{
			Reply = Construction->ConfirmSelection(this, SessionId, Revision, OfferId, RequestId);
		}
	}

	UE_LOG(LogMWServer, Log, TEXT("[MWTrackSelection] Session=%d Revision=%d Request=%d Offer=%d Accepted=%d Reason=%s"),
		Reply.SelectionSessionId, Reply.OffersRevision, Reply.RequestId, Reply.OfferId,
		Reply.bAccepted ? 1 : 0, *UEnum::GetValueAsString(Reply.Reason));
	ClientTrackSelectionReply(Reply);
}

void AMWGameplayPlayerController::ClientTrackSelectionReply_Implementation(const FMWTrackSelectionReply& Reply)
{
	if (auto* Widget = Cast<UMWTrackSelectionWidget>(TrackSelectionWidget))
	{
		Widget->HandleSelectionReply(Reply);
	}
	OnTrackSelectionReply(Reply);
}

void AMWGameplayPlayerController::SetTrackSelectionBoard(AMWTrackSelectionBoard* Board)
{
	if (!IsLocalController()) return;
	if (SelectionInputComponent.IsValid())
	{
		SelectionInputComponent->RemoveBindingByHandle(SelectionBindingHandle);
		if (auto* Inputs = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
			if (SelectionInputData) Inputs->RemoveMappingContext(SelectionInputData->GetMappingContext());
	}
	SelectionInputComponent.Reset();
	SelectionBoard = Board;
	if (!Board) return;
	auto* Input = Cast<UEnhancedInputComponent>(InputComponent);
	auto* Inputs = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	SelectionInputData = UMWAssetManager::Get().LoadAssetSynchronous<UMWInputData>(MWGameplayTags::Asset_Input_SelectConstruction);
	const auto* Action = SelectionInputData ? SelectionInputData->FindInputActionByTag(MWGameplayTags::Input_Select) : nullptr;
	if (!Input || !Inputs || !Action || !SelectionInputData->GetMappingContext()) return;
	// 기존 선택 IMC의 좌클릭 액션을 사용한다. 화면 버튼의 위치와 무관한 월드 선택이다.
	SelectionBindingHandle = Input->BindAction(Action, ETriggerEvent::Started, this, &ThisClass::HandleTrackSelectPressed).GetHandle();
	SelectionInputComponent = Input;
	FModifyContextOptions Options;
	Options.bIgnoreAllPressedKeysUntilRelease = true;
	Inputs->AddMappingContext(SelectionInputData->GetMappingContext(), 1, Options);
}

void AMWGameplayPlayerController::HandleTrackSelectPressed()
{
	if (!SelectionBoard.IsValid() || GetGameInstance()->GetSubsystem<UMWArrivalScreen>()->IsCovered()) return;
	if (const auto* GS = GetWorld()->GetGameState<AMWGameplayGameState>())
	{
		const auto Session = GS->GetConstructionComponent()->GetSelectionSession();
		if (Session.IsWaitingForReselectionOffers())
		{
			ServerRetryTrackSelection(Session.SelectionSessionId, Session.OffersRevision);
			return;
		}
	}
	if (auto* Widget = Cast<UMWTrackSelectionWidget>(TrackSelectionWidget)) Widget->ConfirmOffer(SelectionBoard->FindHoveredOffer());
}

void AMWGameplayPlayerController::ServerRetryTrackSelection_Implementation(int32 SessionId, int32 Revision)
{
	if (auto* GS = GetWorld()->GetGameState<AMWGameplayGameState>(); GS && GS->GetRoomPhase() == EMWRoomPhase::TrackPlacement)
	{
		GS->GetConstructionComponent()->RetryPlacementReselection(this, SessionId, Revision);
	}
}

void AMWGameplayPlayerController::RequestTrackHover(int32 SessionId, int32 Revision, int32 OfferId)
{
	if (IsLocalController()) ServerUpdateTrackHover(SessionId, Revision, OfferId);
}

void AMWGameplayPlayerController::RequestTrackCursor(int32 SessionId, int32 Revision, bool bVisible, const FVector& Position)
{
	if (IsLocalController() && NextCursorSequence < MAX_int32)
		ServerUpdateTrackCursor(SessionId, Revision, ++NextCursorSequence, bVisible, Position);
}

void AMWGameplayPlayerController::ServerUpdateTrackHover_Implementation(int32 SessionId, int32 Revision, int32 OfferId)
{
	if (auto* GS = GetWorld()->GetGameState<AMWGameplayGameState>())
		GS->GetConstructionComponent()->UpdateHover(this, SessionId, Revision, OfferId);
}

void AMWGameplayPlayerController::ServerUpdateTrackCursor_Implementation(int32 SessionId, int32 Revision,
	int32 Sequence, bool bVisible, FVector_NetQuantize Position)
{
	if (auto* GS = GetWorld()->GetGameState<AMWGameplayGameState>())
		GS->GetConstructionComponent()->UpdateCursor(this, SessionId, Revision, Sequence, bVisible, Position);
}

void AMWGameplayPlayerController::ServerUpdateTrackPlacementPreview_Implementation(int32 SessionId, int32 Sequence,
	const FMWTrackPlacementIntent& Intent)
{
	if (auto* GS = GetWorld()->GetGameState<AMWGameplayGameState>())
	{
		GS->GetConstructionComponent()->UpdatePlacementPreview(this, SessionId, Sequence, Intent);
	}
}

void AMWGameplayPlayerController::ServerPlaceTrack_Implementation(int32 SessionId, int32 CourseRevision,
	const FMWTrackPlacementIntent& Intent)
{
	EMWTrackPlacementResult Result = EMWTrackPlacementResult::Unavailable;
	if (auto* GS = GetWorld()->GetGameState<AMWGameplayGameState>())
	{
		Result = GS->GetConstructionComponent()->PlaceTrack(this, SessionId, CourseRevision, Intent);
	}
	ClientTrackPlacementReply(SessionId, Result);
}

void AMWGameplayPlayerController::ClientTrackPlacementReply_Implementation(int32 SessionId, EMWTrackPlacementResult Result)
{
	PlacementView->HandlePlacementReply(SessionId, Result);
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
