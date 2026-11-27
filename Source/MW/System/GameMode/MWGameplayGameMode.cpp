


#include "System/GameMode/MWGameplayGameMode.h"

#include "LogChannel.h"
#include "System/GameState/MWGameplayGameState.h"
#include "System/MWPlayerState.h"
#include "Player/MWGameplayPlayerController.h"
#include "System/MWGameInstance.h"
#include "Gameplay/Construction/MWTrackConstructionComponent.h"
#include "Gameplay/Construction/MWTrackSelectionBoard.h"
#include "Gameplay/Items/MWCoursePickupComponent.h"
#include "Gameplay/PlayerStart/MWPlayerStartManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Data/MWVehicleDefinitionData.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerStart.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "Gameplay/Track/MWTrackRuntimeSubsystem.h"
#include "Gameplay/Track/MWTrackRuntimeSettings.h"
#include "System/MWAssetManager.h"
#include "TimerManager.h"
#include "Algo/Count.h"

AMWGameplayGameMode::AMWGameplayGameMode()
{
	GameStateClass = AMWGameplayGameState::StaticClass();
	PlayerControllerClass = AMWGameplayPlayerController::StaticClass();
	PlayerStateClass = AMWPlayerState::StaticClass();

	CoursePickups = CreateDefaultSubobject<UMWCoursePickupComponent>(TEXT("CoursePickups"));
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
    if (!bEnableTrackConstruction)
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

bool AMWGameplayGameMode::IsMatchMember(int32 MemberId) const
{
	const auto* App = GetGameInstance<UMWGameInstance>();
	return bMatchInitialized ? MatchMembers.Contains(MemberId) : App && App->GetStartingPlayers().Contains(MemberId);
}

bool AMWGameplayGameMode::CollectStartingControllers(TArray<AMWGameplayPlayerController*>& OutControllers, bool bRaceOnly) const
{
	OutControllers.Reset();

	UWorld* World = GetWorld();
	const auto* App = GetGameInstance<UMWGameInstance>();
	const auto* GS = GetGameState<AMWGameplayGameState>();

	if (!World || !App || !GS)
		return false;

	TArray<int32> StartingPlayers = App->GetStartingPlayers();
	if (bMatchInitialized) StartingPlayers.RemoveAll([this](int32 MemberId) { return !MatchMembers.Contains(MemberId); });

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

	if (OutControllers.Contains(nullptr)) return false;
	if (bRaceOnly) OutControllers.RemoveAll([](const AMWGameplayPlayerController* PC) { return !PC->GetPlayerState<AMWPlayerState>()->GetRoundState().bRaceParticipant; });
	return !OutControllers.IsEmpty();
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
		return !bEnableTrackConstruction && Session.SessionId == 0
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
	if (!CollectStartingControllers(Controllers, true))
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
	const bool bNextNormalRound = NewPhase == EMWRoomPhase::PreparingConstruction && Current.Phase == EMWRoomPhase::RoundResults;
	const bool bStartSuddenDeath = NewPhase == EMWRoomPhase::PreparingRace && Current.Phase == EMWRoomPhase::RoundResults;

	// 전환 승인 전에 기존 대기 상태를 지우지 않는다.
	switch (NewPhase)
	{
	case EMWRoomPhase::PreparingConstruction:
		{
			if ((!bInitialEntry && !bNextNormalRound) || !bEnableTrackConstruction
				|| (bNextNormalRound && (Current.RaceMode != EMWRaceMode::Normal || CountQualifiedPlayers() != 0)))
			{
				return;
			}

			if (!Construction)
			{
				FailGameplay(TEXT("트랙 건설 컴포넌트가 없습니다."));
				return;
			}

			if (Construction->GetSelectionSession().bIsOpen || (bInitialEntry && Construction->GetSelectionSession().SelectionSessionId != 0))
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
				bInitialEntry && !bEnableTrackConstruction;

			if (!bFromPlacement && !bSkipConstruction && !bStartSuddenDeath)
			{
				return;
			}
			if (bStartSuddenDeath && (Current.RaceMode != EMWRaceMode::Normal || CountQualifiedPlayers() < 2 || !IsRaceCourseCurrent())) return;

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
			if (bStartSuddenDeath)
			{
				// 자격은 확정 결과에서, 실제 다음 참가자는 현재 접속 명단에서 가져온다.
				Controllers.RemoveAll([&Current](const AMWGameplayPlayerController* PC)
				{
					const int32 MemberId = PC->GetPlayerState<AMWPlayerState>()->GetLobbyMemberId();
					return !Current.Results.ContainsByPredicate([&Current, MemberId](const FMWPlayerRoundState& Entry)
						{ return Entry.MemberId == MemberId && Entry.TotalScore >= Current.TargetScore; });
				});
				if (Controllers.IsEmpty())
				{
					FailGameplay(TEXT("서든데스에 참가할 목표점수 도달자가 남아 있지 않습니다."));
					return;
				}
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

	case EMWRoomPhase::RoundResults:
		{
			if (Current.Phase != EMWRoomPhase::Racing || !AreAllRacersDone()) return;
			if (!FMath::IsFinite(RoundResultsSeconds) || RoundResultsSeconds <= 0.f)
			{
				FailGameplay(TEXT("결과 대기 시간은 0보다 커야 합니다."));
				return;
			}
			break;
		}
	case EMWRoomPhase::MatchResults:
		{
			if (Current.Phase != EMWRoomPhase::RoundResults
				|| (Current.RaceMode == EMWRaceMode::Normal && CountQualifiedPlayers() != 1)) return;
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
		bMatchInitialized = true;
		for (AMWGameplayPlayerController* PC : Controllers) MatchMembers.Add(PC->GetPlayerState<AMWPlayerState>()->GetLobbyMemberId());
		Next.RoundIndex = 1;
		Next.RoundEpoch = 1;
		Next.RaceMode = EMWRaceMode::Normal;
	}
	else if (bNextNormalRound || bStartSuddenDeath)
	{
		if (Current.RoundEpoch == MAX_int32 || Current.RoundIndex == MAX_int32)
		{
			FailGameplay(TEXT("라운드 번호 범위를 초과했습니다."));
			return;
		}
		++Next.RoundIndex;
		++Next.RoundEpoch;
		Next.RaceMode = bStartSuddenDeath ? EMWRaceMode::SuddenDeath : EMWRaceMode::Normal;
	}
	Next.TargetScore = TargetScore;

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
	else if (NewPhase == EMWRoomPhase::RoundResults)
	{
		Next.PhaseEndServerTime = GetWorld()->GetTimeSeconds() + RoundResultsSeconds;
	}
	if (NewPhase == EMWRoomPhase::RoundResults || NewPhase == EMWRoomPhase::MatchResults)
	{
		Next.Results = BuildResults(NewPhase == EMWRoomPhase::MatchResults);
		Next.ResultsEpoch = Next.RoundEpoch;
	}

	UE_LOG(LogMWServer, Log, TEXT("[MWProgress] %s -> %s Round=%d Epoch=%d"),
		*UEnum::GetValueAsString(Current.Phase), *UEnum::GetValueAsString(NewPhase), Next.RoundIndex, Next.RoundEpoch);

	if (NewPhase == EMWRoomPhase::Racing)
	{
		NextFinishOrder = 1;
		NextEliminationOrder = 1;
	}
	GS->ApplyProgressFromServer(Next);

	if (bInitialEntry || bNextNormalRound || bStartSuddenDeath)
	{
		for (APlayerState* Base : GS->PlayerArray)
		{
			auto* PS = Cast<AMWPlayerState>(Base);
			if (!IsValid(PS) || !IsMatchMember(PS->GetLobbyMemberId())) continue;

			FMWPlayerRoundState NewRound;
			NewRound.MemberId = PS->GetLobbyMemberId();
			NewRound.TotalScore = bInitialEntry ? 0 : PS->GetRoundState().TotalScore;
			NewRound.RoundEpoch = Next.RoundEpoch;
			if (bStartSuddenDeath)
			{
				const auto* Result = Current.Results.FindByPredicate([&NewRound](const FMWPlayerRoundState& Entry)
					{ return Entry.MemberId == NewRound.MemberId; });
				if (Result) NewRound.TotalScore = Result->TotalScore;
				NewRound.bRaceParticipant = Result && Result->TotalScore >= Current.TargetScore;
			}
			NewRound.RaceStatus = NewRound.bRaceParticipant ? EMWRaceStatus::Waiting : EMWRaceStatus::Spectating;
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
			if (PlayerRound.RoundEpoch != Next.RoundEpoch || !PlayerRound.bRaceParticipant)
			{
				continue;
			}

			PlayerRound.RaceStatus = NewPhase == EMWRoomPhase::Racing
				? EMWRaceStatus::Active : EMWRaceStatus::Waiting;
			PlayerRound.FinishOrder = 0;
			if (NewPhase == EMWRoomPhase::Racing)
			{
				if (auto* Vehicle = Cast<AMWVehiclePawn>(PS->GetPawn()))
				{
					GetWorld()->GetSubsystem<UMWTrackRuntimeSubsystem>()->BeginPlayerRace(*Vehicle, Next.RoundEpoch);
				}
			}
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
		if (!Construction->BeginSelection(Players, Error))
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
	if (NewPhase == EMWRoomPhase::RoundResults)
	{
		CoursePickups->Clear();
		WaitForRacePhaseDeadline();
		return;
	}
	if (NewPhase == EMWRoomPhase::MatchResults) return;

	if (NewPhase == EMWRoomPhase::Racing)
	{
		// Progress와 PlayerRound의 알림에서 Pawn이 주행 허용을 다시 계산한다.
		return;
	}

	// 허용된 전환 중 여기까지 내려오는 경우는 Failed뿐이다.
	CoursePickups->Clear();
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
	if (!GS->GetConstructionComponent()->BeginPlacement(StartManager->GetActorTransform(),
		SelectionBoard->GetLayoutScale(), Error))
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
	if (Expected.Phase == EMWRoomPhase::RoundResults
		&& (Expected.ResultsEpoch != Expected.RoundEpoch || Expected.Results.IsEmpty()))
	{
		FailGameplay(TEXT("현재 라운드의 확정 결과가 없습니다."));
		return;
	}
	if ((Expected.Phase != EMWRoomPhase::PreparingRace && Expected.Phase != EMWRoomPhase::Countdown && Expected.Phase != EMWRoomPhase::RoundResults)
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
		else if (Expected.Phase == EMWRoomPhase::Countdown)
		{
			EnterPhase(EMWRoomPhase::Racing);
		}
		else if (Expected.RaceMode == EMWRaceMode::SuddenDeath || CountQualifiedPlayers() == 1)
		{
			EnterPhase(EMWRoomPhase::MatchResults);
		}
		else if (CountQualifiedPlayers() >= 2)
		{
			EnterPhase(EMWRoomPhase::PreparingRace);
		}
		else
		{
			EnterPhase(EMWRoomPhase::PreparingConstruction);
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
	if (!IsValid(Player) || Player->GetWorld() != GetWorld() || !CollectStartingControllers(Controllers, true)
		|| !Controllers.Contains(Player))
	{
		return;
	}
	const auto* PS = Player->GetPlayerState<AMWPlayerState>();
	const auto* Vehicle = Cast<AMWVehiclePawn>(Player->GetPawn());
	if (!PS || PS->GetRoundState().RoundEpoch != RoundEpoch || PS->GetRoundState().RaceStatus != EMWRaceStatus::Waiting
		|| !IsValid(Vehicle) || Vehicle->GetController() != Player || Vehicle->GetPlayerState() != PS
		|| !Vehicle->IsReadyForRace(RoundEpoch))
	{
		return;
	}
	if (!RaceReadyMembers.Contains(PS->GetLobbyMemberId()))
	{
		RaceReadyMembers.Add(PS->GetLobbyMemberId());
		UE_LOG(LogMWServer, Log, TEXT("[MWRaceReady] Member=%d Epoch=%d Course=%d Revision=%d Ready=%d/%d"),
			PS->GetLobbyMemberId(), RoundEpoch, CourseSessionId, CourseRevision, RaceReadyMembers.Num(), Controllers.Num());
	}
	// 전원 보고 후 일시적인 서버 준비 상태가 회복된 경우 중복 보고로도 출발을 재평가한다.
	if (AreAllRaceReady())
	{
		EnterPhase(EMWRoomPhase::Countdown);
	}
}

AMWPlayerState* AMWGameplayGameMode::FindActiveRacePlayer(AMWVehiclePawn& Vehicle, int32 RoundEpoch) const
{
	const auto* GS = GetGameState<AMWGameplayGameState>();
	const auto* App = GetGameInstance<UMWGameInstance>();
	if (!HasAuthority() || !GS || !App || !Vehicle.HasAuthority()
		|| Vehicle.GetWorld() != GetWorld() || Vehicle.IsActorBeingDestroyed())
	{
		return nullptr;
	}
	const FMWGameplayProgress Progress = GS->GetProgress();
	auto* PC = Cast<AMWGameplayPlayerController>(Vehicle.GetController());
	auto* PS = Vehicle.GetPlayerState<AMWPlayerState>();
	if (Progress.Phase != EMWRoomPhase::Racing || RoundEpoch <= 0 || RoundEpoch != Progress.RoundEpoch
		|| !IsValid(PC) || !IsValid(PS) || PS->IsInactive() || PS->GetOwner() != PC
		|| PC->GetPawn() != &Vehicle || PC->GetPlayerState<AMWPlayerState>() != PS || !GS->PlayerArray.Contains(PS)
		|| !IsMatchMember(PS->GetLobbyMemberId()) || !PS->GetRoundState().bRaceParticipant
		|| PS->GetRoundState().RoundEpoch != RoundEpoch || PS->GetRoundState().RaceStatus != EMWRaceStatus::Active)
	{
		return nullptr;
	}
	return PS;
}

bool AMWGameplayGameMode::TryPassPlayerCheckpoint(AMWVehiclePawn& Vehicle, AActor& Track, int32 RoundEpoch)
{
	auto* PS = FindActiveRacePlayer(Vehicle, RoundEpoch);
	const auto* GS = GetGameState<AMWGameplayGameState>();
	if (!PS || Track.GetWorld() != GetWorld() || !IsRaceCourseCurrent()) return false;
	int32 Ordinal = 0;
	bool bExpected = false;
	for (const auto& Piece : GS->GetConstructionComponent()->GetPlacementSession().PlacedTracks)
	{
		if (!Piece.bCheckpoint) continue;
		++Ordinal;
		if (Piece.Actor == &Track)
		{
			bExpected = Ordinal == PS->GetRoundState().CheckpointsPassed + 1;
			break;
		}
	}
	if (!bExpected) return false;
	FMWPlayerRoundState Round = PS->GetRoundState();
	Round.MemberId = PS->GetLobbyMemberId();
	++Round.CheckpointsPassed;
	if (Round.bCheckpointEligible && GS->GetProgress().RaceMode == EMWRaceMode::Normal)
	{
		Round.CheckpointScore += 100;
		Round.TotalScore += 100;
	}
	Round.bCheckpointEligible = true;
	PS->ApplyRoundStateFromServer(Round);
	return true;
}

bool AMWGameplayGameMode::TryFinishPlayerRace(AMWVehiclePawn& Vehicle, AActor& FinishTrack, int32 RoundEpoch)
{
	auto* PS = FindActiveRacePlayer(Vehicle, RoundEpoch);
	const auto* GS = GetGameState<AMWGameplayGameState>();
	if (!PS || !FinishTrack.HasAuthority() || FinishTrack.GetWorld() != GetWorld() || FinishTrack.IsActorBeingDestroyed()) return false;
	const FMWGameplayProgress Progress = GS->GetProgress();
	const auto* Construction = GS->GetConstructionComponent();
	if (!Construction || Progress.CourseSessionId <= 0 || Progress.CourseRevision <= 0 || !IsRaceCourseCurrent())
	{
		return false;
	}
	const auto& Tracks = Construction->GetPlacementSession().PlacedTracks;
	if (Tracks.IsEmpty() || Tracks.Last().Actor != &FinishTrack
		|| Tracks.Last().TrackClass != GetDefault<UMWTrackRuntimeSettings>()->FinishTrackClass || NextFinishOrder == MAX_int32
		|| PS->GetRoundState().CheckpointsPassed != Algo::CountIf(Tracks, [](const FMWPlacedTrack& Track) { return Track.bCheckpoint; }))
	{
		return false;
	}

	FMWPlayerRoundState Finished = PS->GetRoundState();
	Finished.RaceStatus = EMWRaceStatus::Finished;
	Finished.MemberId = PS->GetLobbyMemberId();
	Finished.FinishOrder = NextFinishOrder++;
	if (Progress.RaceMode == EMWRaceMode::Normal)
	{
		Finished.FinishScore = Finished.FinishOrder == 1 ? 300 : Finished.FinishOrder == 2 ? 150 : Finished.FinishOrder == 3 ? 50 : 0;
		Finished.TotalScore += Finished.FinishScore;
	}
	// 기존 공용 Notify가 서버 차량 정지와 충돌 해제·이동 스냅샷 갱신까지 실행한다.
	PS->ApplyRoundStateFromServer(Finished);
	UE_LOG(LogMWServer, Log, TEXT("[MWRace] Finished Member=%d Epoch=%d Order=%d Vehicle=%s"),
		PS->GetLobbyMemberId(), RoundEpoch, Finished.FinishOrder, *Vehicle.GetName());
	TryEndRound();
	return true;
}

bool AMWGameplayGameMode::HandlePlayerRaceFailure(AMWVehiclePawn& Vehicle, int32 RoundEpoch)
{
	auto* PS = FindActiveRacePlayer(Vehicle, RoundEpoch);
	if (!PS) return false;
	FMWPlayerRoundState Round = PS->GetRoundState();
	Round.MemberId = PS->GetLobbyMemberId();
	Round.bCheckpointEligible = false;
	if (GetGameState<AMWGameplayGameState>()->GetProgress().RaceMode == EMWRaceMode::SuddenDeath)
	{
		Round.RaceStatus = EMWRaceStatus::Eliminated;
		Round.EliminationOrder = NextEliminationOrder++;
	}
	PS->ApplyRoundStateFromServer(Round);
	TryEndRound();
	return true;
}

void AMWGameplayGameMode::CheckPlayerRaceFailure(AMWVehiclePawn& Vehicle)
{
	const auto* GS = GetGameState<AMWGameplayGameState>();
	if (!HasAuthority() || !GS || !Vehicle.CanDrive()) return;
	const FMWTrackPlacementSession& Course = GS->GetConstructionComponent()->GetPlacementSession();
	if (!Course.CourseBounds.IsValid || !FMath::IsFinite(FallRecoveryDepth) || FallRecoveryDepth <= 0.f) return;

	const double FallbackZ = Course.CourseBounds.Min.Z - FallRecoveryDepth;
	auto* Runtime = GetWorld()->GetSubsystem<UMWTrackRuntimeSubsystem>();
	if (Vehicle.GetActorLocation().Z < FallbackZ
		|| Runtime->ShouldRecoverUnsupportedFall(Vehicle, Course, FallbackZ, FallRecoveryProbeInterval))
	{
		Runtime->RespawnAtTrackCheckpoint(&Vehicle);
	}
}

bool AMWGameplayGameMode::AreAllRacersDone() const
{
	const auto* GS = GetGameState<AMWGameplayGameState>();
	const auto* App = GetGameInstance<UMWGameInstance>();
	if (!GS || !App) return false;
	int32 Racers = 0;
	for (int32 Member : App->GetStartingPlayers())
	{
		if (!IsMatchMember(Member)) continue;
		const auto* Found = GS->PlayerArray.FindByPredicate([Member](const APlayerState* Base)
			{ const auto* PS = Cast<AMWPlayerState>(Base); return PS && !PS->IsInactive() && PS->GetLobbyMemberId() == Member; });
		if (!Found) return false;
		const auto Round = CastChecked<AMWPlayerState>(Found->Get())->GetRoundState();
		if (Round.RoundEpoch != GS->GetProgress().RoundEpoch) return false;
		if (!Round.bRaceParticipant) continue;
		++Racers;
		if (Round.RaceStatus != EMWRaceStatus::Finished && Round.RaceStatus != EMWRaceStatus::Eliminated) return false;
	}
	return Racers > 0;
}

void AMWGameplayGameMode::TryEndRound()
{
	const auto* GS = GetGameState<AMWGameplayGameState>();
	if (!HasAuthority() || !GS || GS->GetRoomPhase() != EMWRoomPhase::Racing) return;
	bool bHasRacer = false;
	for (const APlayerState* Base : GS->PlayerArray)
	{
		const auto* PS = Cast<AMWPlayerState>(Base);
		if (PS && !PS->IsInactive() && IsMatchMember(PS->GetLobbyMemberId())
			&& PS->GetRoundState().RoundEpoch == GS->GetProgress().RoundEpoch && PS->GetRoundState().bRaceParticipant) bHasRacer = true;
	}
	if (!bHasRacer) { FailGameplay(TEXT("주행 참가자가 없습니다.")); return; }
	if (AreAllRacersDone()) EnterPhase(EMWRoomPhase::RoundResults);
}

int32 AMWGameplayGameMode::CountQualifiedPlayers() const
{
	const auto* GS = GetGameState<AMWGameplayGameState>();
	if (!GS) return 0;
	const auto& Progress = GS->GetProgressRef();
	if (Progress.ResultsEpoch != Progress.RoundEpoch) return 0;
	// 결과 대기 중 이탈해도 라운드 종료 시 확정한 종료/서든데스 경로는 바뀌지 않는다.
	return Algo::CountIf(Progress.Results, [&Progress](const FMWPlayerRoundState& Entry)
		{ return Entry.TotalScore >= Progress.TargetScore; });
}

TArray<FMWPlayerRoundState> AMWGameplayGameMode::BuildResults(bool bFinal)
{
	auto* GS = GetGameState<AMWGameplayGameState>();
	const auto& Progress = GS->GetProgressRef();
	TArray<FMWPlayerRoundState> Results;
	if (bFinal)
	{
		// 최종 순위는 확정 결과의 재정렬이다. 이미 완료한 이탈자의 기록도 보존한다.
		Results = Progress.Results;
	}
	else
	{
		for (APlayerState* Base : GS->PlayerArray)
		{
			auto* PS = Cast<AMWPlayerState>(Base);
			if (!IsValid(PS) || PS->IsInactive() || !IsMatchMember(PS->GetLobbyMemberId())) continue;
			FMWPlayerRoundState Round = PS->GetRoundState();
			if (Round.RoundEpoch != Progress.RoundEpoch) continue;
			Round.MemberId = PS->GetLobbyMemberId();
			Results.Add(Round);
		}
	}
	const bool bScoreRanking = bFinal && Progress.RaceMode == EMWRaceMode::Normal;
	Results.Sort([bScoreRanking](const FMWPlayerRoundState& A, const FMWPlayerRoundState& B)
	{
		if (bScoreRanking)
		{
			if (A.TotalScore != B.TotalScore) return A.TotalScore > B.TotalScore;
			if (A.RoundRank != B.RoundRank) return A.RoundRank < B.RoundRank;
		}
		else
		{
			if (A.bRaceParticipant != B.bRaceParticipant) return A.bRaceParticipant;
			if (A.bRaceParticipant)
			{
				if ((A.FinishOrder > 0) != (B.FinishOrder > 0)) return A.FinishOrder > 0;
				if (A.FinishOrder != B.FinishOrder) return A.FinishOrder < B.FinishOrder;
				if (A.EliminationOrder != B.EliminationOrder) return A.EliminationOrder > B.EliminationOrder;
			}
			else if (A.TotalScore != B.TotalScore) return A.TotalScore > B.TotalScore;
		}
		return A.MemberId < B.MemberId;
	});
	for (int32 Index = 0; Index < Results.Num(); ++Index)
	{
		auto& Entry = Results[Index];
		if (bFinal) Entry.FinalRank = Index + 1;
		else Entry.RoundRank = Entry.bRaceParticipant ? Index + 1 : 0;
		for (APlayerState* Base : GS->PlayerArray)
		{
			auto* PS = Cast<AMWPlayerState>(Base);
			if (IsValid(PS) && !PS->IsInactive() && IsMatchMember(PS->GetLobbyMemberId())
				&& PS->GetLobbyMemberId() == Entry.MemberId) { PS->ApplyRoundStateFromServer(Entry); break; }
		}
	}
	return Results;
}

void AMWGameplayGameMode::PrepareRace(const TArray<AMWGameplayPlayerController*>& Players)
{
	auto* GS = GetGameState<AMWGameplayGameState>();
	if (!HasAuthority() || !GS || GS->GetRoomPhase() != EMWRoomPhase::PreparingRace)
	{
		return;
	}
	if (GS->GetProgress().CourseSessionId > 0)
	{
		const auto& Tracks = GS->GetConstructionComponent()->GetPlacementSession().PlacedTracks;
		AActor* FinishTrack = Tracks.IsEmpty() ? nullptr : Tracks.Last().Actor.Get();
		if (!IsValid(FinishTrack) || !GetWorld()->GetSubsystem<UMWTrackRuntimeSubsystem>()->PrepareCourseFinish(*FinishTrack, FinishTriggerHalfExtent))
		{
			FailGameplay(TEXT("현재 코스의 종료 트랙에 결승 영역을 준비하지 못했습니다."));
			return;
		}
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

		// 기존 차량은 출발 슬롯으로 옮긴 다음 완주 때 껐던 충돌을 복원한다.
		const bool bExistingVehicle = IsValid(PC->GetPawn());
		if (!bExistingVehicle)
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
		if (bExistingVehicle && !Vehicle->TeleportTo(Start->GetActorLocation(), Start->GetActorRotation()))
		{
			FailRace(FString::Printf(TEXT("기존 차량을 출발 위치로 옮기지 못했습니다. Slot=%d"), Index + 1));
			return;
		}
		Vehicle->PrepareForRaceFromServer();

		UE_LOG(LogMWServer, Log, TEXT("[MWRace] 차량 소유 확인. Member=%d Slot=%d Start=%s Pawn=%s"),
			PC->GetPlayerState<AMWPlayerState>()->GetLobbyMemberId(), Index + 1, *GetNameSafe(Start), *GetNameSafe(Vehicle));
	}
	
	if (GS->GetProgress().CourseSessionId > 0)
	{
		// 코스와 출발 슬롯이 확정된 뒤에만 이번 라운드의 상자를 배치한다.
		CoursePickups->Prepare(GS->GetConstructionComponent()->GetPlacementSession(), PlayerStarts);
	}

	UE_LOG(LogMWServer, Log, TEXT("[MWRace] 전원 차량 소유 확인 완료. 클라이언트 출발 준비 보고를 기다립니다."));
}

void AMWGameplayGameMode::FailGameplay(const FString& Error)
{
	EnterPhase(EMWRoomPhase::Failed, Error);
}

void AMWGameplayGameMode::Logout(AController* Exiting)
{
	auto* PS = Exiting->GetPlayerState<AMWPlayerState>();
	const auto* GS = GetGameState<AMWGameplayGameState>();
	const bool bMember = PS && GS && IsMatchMember(PS->GetLobbyMemberId());
	const bool bEnding = GetWorld()->bIsTearingDown || (GS && (GS->GetRoomPhase() == EMWRoomPhase::MatchResults || GS->GetRoomPhase() == EMWRoomPhase::Failed));
	const bool bAbortPreparation = bMember && (GS->GetRoomPhase() == EMWRoomPhase::PreparingConstruction
		|| GS->GetRoomPhase() == EMWRoomPhase::TrackPlacement || (PS->GetRoundState().bRaceParticipant
			&& (GS->GetRoomPhase() == EMWRoomPhase::PreparingRace || GS->GetRoomPhase() == EMWRoomPhase::Countdown)));
	if (bMember)
	{
		MatchMembers.Remove(PS->GetLobbyMemberId());
		RaceReadyMembers.Remove(PS->GetLobbyMemberId());
	}
	Super::Logout(Exiting);
	if (bEnding) return;
	if (bMember && bMatchInitialized && MatchMembers.IsEmpty()) FailGameplay(TEXT("남아 있는 경기 참가자가 없습니다."));
	else if (bAbortPreparation) FailGameplay(TEXT("건설 또는 출발 준비 중 참가자가 나갔습니다."));
	else if (bMember) TryEndRound();
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
		if (!World->bIsTearingDown) CoursePickups->Clear();
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
