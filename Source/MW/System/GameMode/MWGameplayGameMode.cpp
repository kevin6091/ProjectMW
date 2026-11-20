


#include "System/GameMode/MWGameplayGameMode.h"

#include "LogChannel.h"
#include "System/GameState/MWGameplayGameState.h"
#include "System/MWPlayerState.h"
#include "Player/MWGameplayPlayerController.h"
#include "System/MWGameInstance.h"
#include "Gameplay/Construction/MWTrackConstructionComponent.h"
#include "Gameplay/Construction/MWTrackSelectionBoard.h"
#include "Gameplay/PlayerStart/MWPlayerStartManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Data/MWVehicleDefinitionData.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerStart.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "System/MWAssetManager.h"
#include "TimerManager.h"

AMWGameplayGameMode::AMWGameplayGameMode()
{
	GameStateClass = AMWGameplayGameState::StaticClass();
	PlayerControllerClass = AMWGameplayPlayerController::StaticClass();
	PlayerStateClass = AMWPlayerState::StaticClass();

	DefaultPawnClass = nullptr;
	bUseSeamlessTravel = true;
}

void AMWGameplayGameMode::InitGameState()
{
	Super::InitGameState();
	TryInitializeGameplay();
	
	auto* GS = GetGameState<AMWGameplayGameState>();
	if (!GS) return;

	auto* Construction = GS->GetConstructionComponent();
	if (!Construction) return;

	Construction->OnAllSelectionsConfirmed.AddUObject(this, &ThisClass::HandleAllSelectionsConfirmed);
	Construction->OnPlacementFinished.AddUObject(this, &ThisClass::HandlePlacementFinished);
	Construction->OnConstructionFailed.AddUObject(this, &ThisClass::HandleConstructionFailed);
}

void AMWGameplayGameMode::StartPlay()
{
	Super::StartPlay();
	TryInitializeGameplay();
	TryStartGameplay();
}

void AMWGameplayGameMode::TryInitializeGameplay()
{
	UWorld* World = GetWorld();

	if (!HasAuthority() || bRoomSettingsApplied || !World || !World->IsGameWorld())
	{
		return;
	}

	auto* App = GetGameInstance<UMWGameInstance>();
	auto* GS = GetGameState<AMWGameplayGameState>();

	FMWRoomSettings Settings;

	if (!App || !GS || !GameSession || !App->GetRoomSettingsForWorld(World, Settings))
	{
		return;
	}

	GS->ApplyRoomSettingsFromServer(Settings);

	GameSession->MaxPlayers = Settings.MaxPlayers;
	GameSession->MaxSpectators = 0;

	bRoomSettingsApplied = true;
}

void AMWGameplayGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& Error)
{
	Super::PreLogin(Options, Address, UniqueId, Error);
	// 기존 테마 월드처럼 새 접속은 닫아 둡니다. 기존 참가자의 Seamless Travel 도착과는 별도 경로입니다.
	if (Error.IsEmpty()) Error = TEXT("방을 준비 중이거나 입장이 닫혔습니다.");
}


void AMWGameplayGameMode::TryStartGameplay()
{
	UWorld* World = GetWorld();
    auto* GS = GetGameState<AMWGameplayGameState>();

    if (!HasAuthority() || !World || !World->HasBegunPlay() || !GS || !bRoomSettingsApplied)
    {
        return;
    }

    // Epoch 0에서만 최초 시작을 준비한다. 발급 후에는 다시 시작하지 않는다.
    const FMWGameplayProgress Progress = GS->GetProgress();
    if (Progress.RoundEpoch != 0 || Progress.Phase != EMWRoomPhase::PreparingConstruction)
    {
        return;
    }

    auto* Construction = GS->GetConstructionComponent();

    if (!Construction)
    {
        FailGameplay(TEXT("트랙 건설 컴포넌트가 없습니다."));
        return;
    }

    // 이미 선택 세션을 시작했다면 다시 시작하지 않는다.
    if (Construction->GetSelectionSession().SelectionSessionId != 0)
    {
        return;
    }

    TArray<AMWGameplayPlayerController*> Controllers;

    // 참가자가 아직 도착 중이라면 다음 도착 이벤트를 기다린다.
    if (!CollectStartingControllers(Controllers))
    {
        return;
    }

    TActorIterator<AMWPlayerStartManager> Grid(World);

    if (!Grid)
    {
        FailGameplay(TEXT("테마 레벨에서 MWPlayerStartManager를 찾지 못했습니다."));
        return;
    }

    StartManager = *Grid;
    ++Grid;

    if (Grid)
    {
        FailGameplay(TEXT("테마 레벨에 MWPlayerStartManager를 하나만 배치하세요."));
        return;
    }

    // 건설을 생략하는 경우에도 주행 준비 진입은 EnterPhase가 담당한다.
    if (ConstructionTrackDefinitionIds.IsEmpty())
    {
        EnterPhase(EMWRoomPhase::PreparingRace);
        return;
    }

    TActorIterator<AMWTrackSelectionBoard> Board(World);

    if (!Board)
    {
        FailGameplay(TEXT("테마 레벨에서 트랙 선택판을 찾지 못했습니다."));
        return;
    }

    SelectionBoard = *Board;
    ++Board;

    if (Board)
    {
        FailGameplay(TEXT("테마 레벨에 트랙 선택판을 하나만 배치하세요."));
        return;
    }

    // 최초 Epoch와 개인 상태를 확정한 뒤 EnterPhase의 진입 작업에서 선택을 연다.
    EnterPhase(EMWRoomPhase::PreparingConstruction);
}

bool AMWGameplayGameMode::CollectStartingControllers(TArray<AMWGameplayPlayerController*>& OutControllers) const
{
	OutControllers.Reset();

	UWorld* World = GetWorld();
	const auto* App = GetGameInstance<UMWGameInstance>();
	const auto* GS = GetGameState<AMWGameplayGameState>();

	if (!World || !App || !GS)
		return false;

	const TArray<int32>& StartingPlayers = App->GetStartingPlayers();

	if (StartingPlayers.IsEmpty())
		return false;

	OutControllers.SetNumZeroed(StartingPlayers.Num());

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		auto* PC = Cast<AMWGameplayPlayerController>(It->Get());

		if (!IsValid(PC) || !PC->HasClientLoadedCurrentWorld())
			continue;

		auto* PS = PC->GetPlayerState<AMWPlayerState>();

		if (!IsValid(PS) || PS->IsInactive() || PS->GetOwner() != PC || !GS->PlayerArray.Contains(PS) || !PS->GetVehicleAssetID().IsValid())
		{
			continue;
		}

		const int32 Index = StartingPlayers.IndexOfByKey(PS->GetLobbyMemberId());

		if (Index == INDEX_NONE)
			continue;

		if (OutControllers[Index] != nullptr)
			return false;

		OutControllers[Index] = PC;
	}

	return !OutControllers.Contains(nullptr);
}

bool AMWGameplayGameMode::IsRaceCourseCurrent() const
{
	const auto* GS = GetGameState<AMWGameplayGameState>();
	if (!GS)
	{
		return false;
	}
	const FMWGameplayProgress Progress = GS->GetProgress();
	const auto* Construction = GS->GetConstructionComponent();
	if (!Construction)
	{
		return false;
	}
	const auto& Session = Construction->GetPlacementSession();
	if (Progress.CourseSessionId == 0 && Progress.CourseRevision == 0)
	{
		// 고정 코스 개발 경로만 0/0을 허용한다. 동적 코스 미수신의 일반 예외가 아니다.
		return ConstructionTrackDefinitionIds.IsEmpty() && Session.SessionId == 0
			&& !Session.bIsOpen && Session.PlacedTracks.IsEmpty();
	}
	return Construction->IsCourseReadyForRace(Progress.CourseSessionId, Progress.CourseRevision);
}


bool AMWGameplayGameMode::AreAllRaceReady() const
{
	const auto* GS = GetGameState<AMWGameplayGameState>();
	if (!GS || (GS->GetRoomPhase() != EMWRoomPhase::PreparingRace && GS->GetRoomPhase() != EMWRoomPhase::Countdown)
		|| !IsRaceCourseCurrent())
	{
		return false;
	}
	TArray<AMWGameplayPlayerController*> Controllers;
	if (!CollectStartingControllers(Controllers))
	{
		return false;
	}
	const int32 Epoch = GS->GetProgress().RoundEpoch;
	for (const AMWGameplayPlayerController* PC : Controllers)
	{
		const auto* PS = PC->GetPlayerState<AMWPlayerState>();
		const auto* Vehicle = Cast<AMWVehiclePawn>(PC->GetPawn());
		if (!RaceReadyMembers.Contains(PS->GetLobbyMemberId()) || PS->GetRoundState().RoundEpoch != Epoch
			|| PS->GetRoundState().RaceStatus != EMWRaceStatus::Waiting || !IsValid(Vehicle)
			|| Vehicle->GetController() != PC || Vehicle->GetPlayerState() != PS || !Vehicle->IsReadyForRace(Epoch))
		{
			return false;
		}
	}
	return true;
}

void AMWGameplayGameMode::EnterPhase(EMWRoomPhase NewPhase, const FString& FailureReason)
{
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}

	auto* GS = GetGameState<AMWGameplayGameState>();

	if (!GS)
	{
		return;
	}

	const FMWGameplayProgress Current = GS->GetProgress();

	// 기본 Phase와 같아도 Epoch 0의 최초 진입 작업은 한 번 실행한다.
	const bool bInitialEntry = Current.RoundEpoch == 0
		&& Current.Phase == EMWRoomPhase::PreparingConstruction
		&& (NewPhase == EMWRoomPhase::PreparingConstruction || NewPhase == EMWRoomPhase::PreparingRace);

	if (Current.Phase == NewPhase && !bInitialEntry)
	{
		return;
	}

	auto* Construction = GS->GetConstructionComponent();

	TArray<AMWGameplayPlayerController*> Controllers;

	// 전환 승인 전에 기존 대기 상태를 지우지 않는다.
	switch (NewPhase)
	{
	case EMWRoomPhase::PreparingConstruction:
		{
			if (!bInitialEntry || ConstructionTrackDefinitionIds.IsEmpty())
			{
				return;
			}

			if (!Construction)
			{
				FailGameplay(TEXT("트랙 건설 컴포넌트가 없습니다."));
				return;
			}

			if (Construction->GetSelectionSession().SelectionSessionId != 0)
			{
				return;
			}

			if (!StartManager.IsValid() || !SelectionBoard.IsValid())
			{
				FailGameplay(TEXT("출발 슬롯 또는 트랙 선택판이 유효하지 않습니다."));
				return;
			}

			if (!CollectStartingControllers(Controllers))
			{
				FailGameplay(TEXT("시작 참가자를 확인하지 못했습니다."));
				return;
			}

			break;
		}

	case EMWRoomPhase::TrackPlacement:
		{
			if (Current.Phase != EMWRoomPhase::PreparingConstruction || Current.RoundEpoch <= 0)
			{
				return;
			}

			if (!Construction)
			{
				FailGameplay(TEXT("트랙 건설 컴포넌트가 없습니다."));
				return;
			}

			const FMWTrackSelectionSession Selection =
				Construction->GetSelectionSession();

			if (Selection.SelectionSessionId <= 0
				|| !Selection.bIsOpen
				|| !Construction->IsAllSelectionsConfirmed())
			{
				return;
			}

			if (!StartManager.IsValid() || !SelectionBoard.IsValid())
			{
				FailGameplay(
					TEXT("출발 슬롯 또는 트랙 선택판이 유효하지 않습니다."));
				return;
			}

			break;
		}

	case EMWRoomPhase::PreparingRace:
		{
			if (!Construction)
			{
				FailGameplay(TEXT("주행 준비 중 건설 컴포넌트가 없습니다."));
				return;
			}
			const bool bFromPlacement =
				Current.Phase == EMWRoomPhase::TrackPlacement && Current.RoundEpoch > 0;

			const bool bSkipConstruction =
				bInitialEntry && ConstructionTrackDefinitionIds.IsEmpty();

			if (!bFromPlacement && !bSkipConstruction)
			{
				return;
			}

			// 클라이언트 수신 대기는 PreparingRace에서 한다. 여기서는 서버의 확정 결과를 검사한다.
			if (bFromPlacement && (!Construction || Construction->GetPlacementSession().SessionId <= 0
				|| Construction->GetPlacementSession().CourseRevision <= 0 || Construction->GetPlacementSession().bIsOpen
				|| Construction->GetPlacementSession().PlacedTracks.IsEmpty()))
			{
				return;
			}
			if (!FMath::IsFinite(RacePreparationTimeoutSeconds) || RacePreparationTimeoutSeconds <= 0.f)
			{
				FailGameplay(TEXT("출발 준비 제한시간은 0보다 큰 유한값이어야 합니다."));
				return;
			}

			if (!StartManager.IsValid())
			{
				FailGameplay(TEXT("출발 슬롯 관리자가 유효하지 않습니다."));
				return;
			}

			if (!CollectStartingControllers(Controllers))
			{
				FailGameplay(TEXT("주행을 시작할 참가자 Controller를 확인하지 못했습니다."));
				return;
			}

			break;
		}

	case EMWRoomPhase::Countdown:
		{
			if (Current.Phase != EMWRoomPhase::PreparingRace || Current.RoundEpoch <= 0
				|| Current.PhaseEndServerTime <= GetWorld()->GetTimeSeconds() || !AreAllRaceReady())
			{
				return;
			}
			if (!FMath::IsFinite(CountdownSeconds) || CountdownSeconds <= 0.f)
			{
				FailGameplay(TEXT("카운트다운 시간은 0보다 큰 유한값이어야 합니다."));
				return;
			}
			break;
		}

	case EMWRoomPhase::Racing:
		{
			if (Current.Phase != EMWRoomPhase::Countdown || Current.RoundEpoch <= 0
				|| Current.PhaseEndServerTime <= 0.0 || GetWorld()->GetTimeSeconds() < Current.PhaseEndServerTime)
			{
				return;
			}
			if (!AreAllRaceReady())
			{
				FailGameplay(TEXT("출발 전 참가자 또는 코스·차량 준비 상태가 변경되었습니다."));
				return;
			}
			break;
		}

	case EMWRoomPhase::Failed:
		{
			break;
		}

	default:
		{
			UE_LOG(LogMWServer, Warning, TEXT("[MWProgress] 아직 지원하지 않는 전환입니다: %s"), *UEnum::GetValueAsString(NewPhase));
			return;
		}
	}

	// 승인된 전환만 기존 타이머를 종료한다. Countdown 동안에는 준비 보고를 보존한다.
	GetWorldTimerManager().ClearTimer(RacePhaseTimer);
	if (NewPhase != EMWRoomPhase::Countdown)
	{
		RaceReadyMembers.Reset();
	}

	FMWGameplayProgress Next = Current;
	if (bInitialEntry)
	{
		Next.RoundIndex = 1;
		Next.RoundEpoch = 1;
		Next.RaceMode = EMWRaceMode::Normal;
	}

	Next.Phase = NewPhase;
	Next.PhaseEndServerTime = 0.0;
	if (NewPhase == EMWRoomPhase::PreparingRace)
	{
		const auto& Session = Construction->GetPlacementSession();
		Next.CourseSessionId = bInitialEntry ? 0 : Session.SessionId;
		Next.CourseRevision = bInitialEntry ? 0 : Session.CourseRevision;
		Next.PhaseEndServerTime = GetWorld()->GetTimeSeconds() + RacePreparationTimeoutSeconds;
	}
	else if (NewPhase == EMWRoomPhase::Countdown)
	{
		Next.PhaseEndServerTime = GetWorld()->GetTimeSeconds() + CountdownSeconds;
	}

	UE_LOG(LogMWServer, Log, TEXT("[MWProgress] %s -> %s Round=%d Epoch=%d"),
		*UEnum::GetValueAsString(Current.Phase), *UEnum::GetValueAsString(NewPhase), Next.RoundIndex, Next.RoundEpoch);

	GS->ApplyProgressFromServer(Next);

	if (bInitialEntry)
	{
		for (AMWGameplayPlayerController* PC : Controllers)
		{
			auto* PS = IsValid(PC) ? PC->GetPlayerState<AMWPlayerState>() : nullptr;
			if (!IsValid(PS))
			{
				FailGameplay(TEXT("초기 라운드 상태를 적용할 참가자가 유효하지 않습니다."));
				return;
			}

			FMWPlayerRoundState NewRound;
			NewRound.RoundEpoch = Next.RoundEpoch;
			NewRound.RaceStatus = EMWRaceStatus::Waiting;
			PS->ApplyRoundStateFromServer(NewRound);
		}
	}

	if (Next.RoundEpoch > 0 && (NewPhase == EMWRoomPhase::PreparingRace || NewPhase == EMWRoomPhase::Countdown
		|| NewPhase == EMWRoomPhase::Racing || NewPhase == EMWRoomPhase::Failed))
	{
		for (APlayerState* BasePS : GS->PlayerArray)
		{
			auto* PS = Cast<AMWPlayerState>(BasePS);
			if (!IsValid(PS) || PS->IsInactive())
			{
				continue;
			}

			FMWPlayerRoundState PlayerRound = PS->GetRoundState();
			if (PlayerRound.RoundEpoch != Next.RoundEpoch)
			{
				continue;
			}

			PlayerRound.RaceStatus = NewPhase == EMWRoomPhase::Racing
				? EMWRaceStatus::Active : EMWRaceStatus::Waiting;
			PS->ApplyRoundStateFromServer(PlayerRound);
		}
	}

	if (NewPhase == EMWRoomPhase::PreparingConstruction)
	{
		TArray<AMWPlayerState*> Players;
		for (AMWGameplayPlayerController* PC : Controllers)
		{
			Players.Add(IsValid(PC) ? PC->GetPlayerState<AMWPlayerState>() : nullptr);
		}

		FText Error;
		if (!Construction->BeginSelection(ConstructionTrackDefinitionIds, Players, Error))
		{
			FailGameplay(Error.ToString());
		}
		return;
	}

	if (NewPhase == EMWRoomPhase::TrackPlacement)
	{
		StartTrackPlacement();
		return;
	}

	if (NewPhase == EMWRoomPhase::PreparingRace)
	{
		PrepareRace(Controllers);
		if (GS->GetRoomPhase() == EMWRoomPhase::PreparingRace)
		{
			WaitForRacePhaseDeadline();
		}
		return;
	}
	if (NewPhase == EMWRoomPhase::Countdown)
	{
		WaitForRacePhaseDeadline();
		return;
	}

	if (NewPhase == EMWRoomPhase::Racing)
	{
		// Progress와 PlayerRound의 알림에서 Pawn이 주행 허용을 다시 계산한다.
		return;
	}

	// 허용된 전환 중 여기까지 내려오는 경우는 Failed뿐이다.
	if (Construction)
	{
		Construction->EndSelection();
		Construction->CancelPlacement();
	}

	UE_LOG(LogMWServer, Error, TEXT("[MWGameplay] %s"), FailureReason.IsEmpty() ? TEXT("경기 진행을 중단했습니다.") : *FailureReason);
}

void AMWGameplayGameMode::StartTrackPlacement()
{
	auto* GS = GetGameState<AMWGameplayGameState>();
	if (!GS || GS->GetRoomPhase() != EMWRoomPhase::TrackPlacement)
		return;
	
	if (!StartManager.IsValid() || !SelectionBoard.IsValid())
	{
		FailGameplay(TEXT("출발 슬롯 또는 트랙 선택판이 유효하지 않습니다."));
		return;
	}
	
	auto* Construction = GS->GetConstructionComponent();
	if (!Construction)
	{
		FailGameplay(TEXT("트랙 건설 컴포넌트가 없습니다."));
		return;
	}

	// 선택판의 위치와 독립적으로 출발 그리드의 원점과 전방에 기준 도로를 맞춘다.
	FText Error;
	if (!GS->GetConstructionComponent()->BeginPlacement(CourseEnds, StartManager->GetActorTransform(), SelectionBoard->GetLayoutScale(), Error))
	{
		FailGameplay(Error.ToString());
	}
}

void AMWGameplayGameMode::WaitForRacePhaseDeadline()
{
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}
	const auto* GS = GetGameState<AMWGameplayGameState>();
	if (!GS)
	{
		return;
	}
	const FMWGameplayProgress Expected = GS->GetProgress();
	if ((Expected.Phase != EMWRoomPhase::PreparingRace && Expected.Phase != EMWRoomPhase::Countdown)
		|| Expected.RoundEpoch <= 0 || Expected.PhaseEndServerTime <= 0.0)
	{
		return;
	}
	const double Remaining = Expected.PhaseEndServerTime - GetWorld()->GetTimeSeconds();
	if (Remaining <= 0.0)
	{
		if (Expected.Phase == EMWRoomPhase::PreparingRace)
		{
			FailGameplay(TEXT("제한 시간 안에 전원의 출발 준비가 완료되지 않았습니다."));
		}
		else
		{
			EnterPhase(EMWRoomPhase::Racing);
		}
		return;
	}
	GetWorldTimerManager().SetTimer(RacePhaseTimer, FTimerDelegate::CreateWeakLambda(this, [this, Expected]
	{
		const auto* CurrentGS = GetGameState<AMWGameplayGameState>();
		if (!CurrentGS)
		{
			return;
		}
		const FMWGameplayProgress Current = CurrentGS->GetProgress();
		if (Current.Phase != Expected.Phase || Current.RoundEpoch != Expected.RoundEpoch
			|| Current.PhaseEndServerTime != Expected.PhaseEndServerTime
			|| Current.CourseSessionId != Expected.CourseSessionId || Current.CourseRevision != Expected.CourseRevision)
		{
			return;
		}
		// float 타이머가 조금 일찍 실행돼도 같은 절대 기한을 유지한다.
		WaitForRacePhaseDeadline();
	}), FMath::Max(0.001f, static_cast<float>(Remaining)), false);
}

void AMWGameplayGameMode::ReportRaceReady(AMWGameplayPlayerController* Player, int32 RoundEpoch,
	int32 CourseSessionId, int32 CourseRevision)
{
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}
	auto* GS = GetGameState<AMWGameplayGameState>();
	if (!GS || GS->GetRoomPhase() != EMWRoomPhase::PreparingRace)
	{
		return;
	}
	const FMWGameplayProgress Progress = GS->GetProgress();
	if (RoundEpoch <= 0 || RoundEpoch != Progress.RoundEpoch || CourseSessionId != Progress.CourseSessionId
		|| CourseRevision != Progress.CourseRevision || Progress.PhaseEndServerTime <= 0.0
		|| GetWorld()->GetTimeSeconds() >= Progress.PhaseEndServerTime || !IsRaceCourseCurrent())
	{
		return;
	}
	TArray<AMWGameplayPlayerController*> Controllers;
	if (!IsValid(Player) || Player->GetWorld() != GetWorld() || !CollectStartingControllers(Controllers)
		|| !Controllers.Contains(Player))
	{
		return;
	}
	const auto* PS = Player->GetPlayerState<AMWPlayerState>();
	const auto* Vehicle = Cast<AMWVehiclePawn>(Player->GetPawn());
	if (!PS || PS->GetRoundState().RoundEpoch != RoundEpoch || PS->GetRoundState().RaceStatus != EMWRaceStatus::Waiting
		|| !IsValid(Vehicle) || Vehicle->GetController() != Player || Vehicle->GetPlayerState() != PS
		|| !Vehicle->IsReadyForRace(RoundEpoch) || RaceReadyMembers.Contains(PS->GetLobbyMemberId()))
	{
		return;
	}
	RaceReadyMembers.Add(PS->GetLobbyMemberId());
	UE_LOG(LogMWServer, Log, TEXT("[MWRaceReady] Member=%d Epoch=%d Course=%d Revision=%d Ready=%d/%d"),
		PS->GetLobbyMemberId(), RoundEpoch, CourseSessionId, CourseRevision, RaceReadyMembers.Num(), Controllers.Num());
	if (AreAllRaceReady())
	{
		EnterPhase(EMWRoomPhase::Countdown);
	}
}

void AMWGameplayGameMode::PrepareRace(const TArray<AMWGameplayPlayerController*>& Players)
{
	auto* GS = GetGameState<AMWGameplayGameState>();
	if (!HasAuthority() || !GS || GS->GetRoomPhase() != EMWRoomPhase::PreparingRace)
	{
		return;
	}

	TArray<APlayerStart*> PlayerStarts;
	
	if (!StartManager.IsValid() || !StartManager->PreparePlayerStarts(Players.Num(), PlayerStarts))
	{
		FailGameplay(FString::Printf(TEXT("참가자 %d명의 출발 위치를 준비하지 못했습니다."), Players.Num()));
		return;
	}

	TArray<APawn*, TInlineAllocator<4>> SpawnedVehicles;
	const auto FailRace = [&](const FString& Error)
	{
		// 일부 슬롯만 차량 생성에 성공했으면 이번 시도에서 만든 차량을 모두 정리한다.
		for (APawn* Pawn : SpawnedVehicles)
		{
			Pawn->Destroy();
		}
		FailGameplay(Error);
	};
	for (int32 Index = 0; Index < Players.Num(); ++Index)
	{
		auto* PC = Players[Index];
		APlayerStart* Start = PlayerStarts[Index];
		if (!IsValid(PC) || !IsValid(Start))
		{
			FailRace(TEXT("Controller 또는 출발 위치가 유효하지 않습니다."));
			return;
		}

		// 다시 호출된 경우에도 이미 소유한 차량은 유지합니다.
		if (!IsValid(PC->GetPawn()))
		{
			RestartPlayerAtPlayerStart(PC, Start);
			if (APawn* Spawned = PC->GetPawn())
			{
				SpawnedVehicles.Add(Spawned);
			}
		}

		auto* Vehicle = Cast<AMWVehiclePawn>(PC->GetPawn());
		if (!IsValid(Vehicle) || Vehicle->GetController() != PC)
		{
			FailRace(FString::Printf(TEXT("차량 생성 또는 Possess 실패. Slot=%d"), Index + 1));
			return;
		}
		Vehicle->PrepareForRaceFromServer();

		UE_LOG(LogMWServer, Log, TEXT("[MWRace] 차량 소유 확인. Member=%d Slot=%d Start=%s Pawn=%s"),
			PC->GetPlayerState<AMWPlayerState>()->GetLobbyMemberId(), Index + 1, *GetNameSafe(Start), *GetNameSafe(Vehicle));
	}
	
	UE_LOG(LogMWServer, Log, TEXT("[MWRace] 전원 차량 소유 확인 완료. 클라이언트 출발 준비 보고를 기다립니다."));
}

void AMWGameplayGameMode::FailGameplay(const FString& Error)
{
	EnterPhase(EMWRoomPhase::Failed, Error);
}

void AMWGameplayGameMode::Logout(AController* Exiting)
{
	const auto* PS = Exiting->GetPlayerState<AMWPlayerState>();
	const auto* GS = GetGameState<AMWGameplayGameState>();
	const auto* App = GetGameInstance<UMWGameInstance>();
	if (PS && GS && App && App->GetStartingPlayers().Contains(PS->GetLobbyMemberId())
		&& (GS->GetRoomPhase() == EMWRoomPhase::PreparingConstruction || GS->GetRoomPhase() == EMWRoomPhase::TrackPlacement
			|| GS->GetRoomPhase() == EMWRoomPhase::PreparingRace || GS->GetRoomPhase() == EMWRoomPhase::Countdown))
	{
		FailGameplay(TEXT("건설 또는 출발 준비 중 시작 참가자가 나갔습니다."));
	}
	Super::Logout(Exiting);
}

void AMWGameplayGameMode::HandleStartingNewPlayer_Implementation(APlayerController*)
{
	// 참가자 한 명의 테마 진입 처리가 끝났으므로, 전원을 다시 확인합니다.
	TryStartGameplay();
}

UClass* AMWGameplayGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	const auto* PS = IsValid(InController)
	? InController->GetPlayerState<AMWPlayerState>()
	: nullptr;

	if (!IsValid(PS) || !PS->GetVehicleAssetID().IsValid())
	{
		return nullptr;
	}

	const FGameplayTag VehicleID = PS->GetVehicleAssetID();

	const auto* Definition = UMWAssetManager::Get().LoadAssetSynchronous<UMWVehicleDefinitionData>(VehicleID);

	if (!Definition)
	{
		UE_LOG(LogMWServer, Error, TEXT("[MWRace] 차량 Definition 로드 실패. Member=%d Vehicle=%s"), 
			PS->GetLobbyMemberId(), *VehicleID.ToString());
		return nullptr;
	}

	// Definition이 가진 BP 클래스 경로를 실제 생성 가능한 클래스로 로드합니다.
	UClass* VehicleClass = Definition->GetPawnClass().LoadSynchronous();

	if (!VehicleClass || VehicleClass->HasAnyClassFlags(CLASS_Abstract))
	{
		UE_LOG(LogMWServer, Error, TEXT("[MWRace] 차량 PawnClass 로드 실패. Member=%d Vehicle=%s"),
			PS->GetLobbyMemberId(), *VehicleID.ToString());
		return nullptr;
	}

	return VehicleClass;
}

void AMWGameplayGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RacePhaseTimer);
	}

	if (auto* GS = GetGameState<AMWGameplayGameState>())
	{
		if (auto* Construction = GS->GetConstructionComponent())
		{
			Construction->OnAllSelectionsConfirmed.RemoveAll(this);
			Construction->OnPlacementFinished.RemoveAll(this);
			Construction->OnConstructionFailed.RemoveAll(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AMWGameplayGameMode::HandleAllSelectionsConfirmed(int32 SessionId)
{
	if (!HasAuthority())
		return;

	auto* GS = GetGameState<AMWGameplayGameState>();

	if (!GS || GS->GetRoomPhase() != EMWRoomPhase::PreparingConstruction) return;
	

	auto* Construction = GS->GetConstructionComponent();
	if (!Construction) return;

	const FMWTrackSelectionSession Session = Construction->GetSelectionSession();

	if (SessionId <= 0 || Session.SelectionSessionId != SessionId || !Construction->IsAllSelectionsConfirmed())	return;

	// 이번 연결 단위에서는 기존 시작 함수를 사용한다.
	EnterPhase(EMWRoomPhase::TrackPlacement);
}

void AMWGameplayGameMode::HandlePlacementFinished(int32 SessionId, int32 CourseRevision)
{
	if (!HasAuthority() || !GetWorld())
		return;

	auto* GS = GetGameState<AMWGameplayGameState>();

	if (!GS || GS->GetRoomPhase() != EMWRoomPhase::TrackPlacement)
		return;

	auto* Construction = GS->GetConstructionComponent();
	if (!Construction)
		return;

	const auto& Session = Construction->GetPlacementSession();

	if (SessionId <= 0
		|| CourseRevision <= 0
		|| Session.SessionId != SessionId
		|| Session.CourseRevision != CourseRevision
		|| Session.bIsOpen
		|| Session.PlacedTracks.IsEmpty())
	{
		return;
	}

	// 확정 코스를 PreparingRace의 준비 키로 고정한 뒤 차량 생성과 수신 대기를 시작한다.
	EnterPhase(EMWRoomPhase::PreparingRace);
}

void AMWGameplayGameMode::HandleConstructionFailed(int32 SessionId, const FString& Error)
{
	if (!HasAuthority())
		return;

	auto* GS = GetGameState<AMWGameplayGameState>();
	if (!GS)
		return;

	auto* Construction = GS->GetConstructionComponent();
	if (!Construction)
		return;

	const EMWRoomPhase Phase = GS->GetRoomPhase();

	int32 CurrentSessionId = 0;

	if (Phase == EMWRoomPhase::PreparingConstruction)
	{
		CurrentSessionId = Construction->GetSelectionSession().SelectionSessionId;
	}
	else if (Phase == EMWRoomPhase::TrackPlacement)
	{
		CurrentSessionId = Construction->GetPlacementSession().SessionId;
	}
	else
	{
		return;
	}

	if (SessionId <= 0 || SessionId != CurrentSessionId)
		return;

	FailGameplay(Error);
}
