


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
}

void AMWGameplayGameMode::StartPlay()
{
	Super::StartPlay();
	TryInitializeGameplay();
	TryAdvanceGameplay();
}

void AMWGameplayGameMode::TryInitializeGameplay()
{
	if (bRoomSettingsApplied || !GetWorld() || !GetWorld()->IsGameWorld()) return;
	auto* App = GetGameInstance<UMWGameInstance>();
	auto* GS = GetGameState<AMWGameplayGameState>();
	FMWRoomSettings Settings;
	if (!App || !GS || !GameSession || !App->GetRoomSettingsForWorld(GetWorld(), Settings)) return;

	// GameInstance는 이동 동안 서버 설정을 보관하고, 새 GameState는 이를 클라이언트에 복제합니다.
	GS->ApplyRoomSettingsFromServer(Settings);
	GS->SetRoomPhaseFromServer(ConstructionTrackDefinitionIds.IsEmpty()
		? EMWRoomPhase::PreparingRace : EMWRoomPhase::PreparingConstruction);
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


void AMWGameplayGameMode::TryAdvanceGameplay()
{
	UWorld* World = GetWorld();
	auto* GS = GetGameState<AMWGameplayGameState>();
	const auto* App = GetGameInstance<UMWGameInstance>();

	if (!World || !World->HasBegunPlay() || !GS || !App || !bRoomSettingsApplied)
	{
		return;
	}
	const EMWRoomPhase Phase = GS->GetRoomPhase();
	if (Phase != EMWRoomPhase::PreparingConstruction && Phase != EMWRoomPhase::TrackPlacement
		&& Phase != EMWRoomPhase::PreparingRace)
	{
		return;
	}

	auto* Construction = GS->GetConstructionComponent();
	if (Phase == EMWRoomPhase::PreparingConstruction && Construction->GetSelectionSession().bIsOpen)
	{
		return;
	}

	// 호스트가 게임 시작을 눌렀을 때 서버가 고정한 참가자 ID입니다.
	const TArray<int32>& StartingPlayers = App->GetStartingPlayers();
	if (StartingPlayers.IsEmpty())
	{
		return;
	}

	TArray<AMWGameplayPlayerController*> ReadyControllers;
	ReadyControllers.SetNumZeroed(StartingPlayers.Num());

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		auto* PC = Cast<AMWGameplayPlayerController>(It->Get());

		// 목적지의 Gameplay Controller이며, 클라이언트가 이 맵을 로드했어야 합니다.
		if (!IsValid(PC) || !PC->HasClientLoadedCurrentWorld())
		{
			continue;
		}

		const auto* PS = PC->GetPlayerState<AMWPlayerState>();
		if (!IsValid(PS) || PS->IsInactive())
		{
			continue;
		}

		const int32 Index = StartingPlayers.IndexOfByKey(PS->GetLobbyMemberId());
		if (Index == INDEX_NONE || !PS->GetVehicleAssetID().IsValid())
		{
			continue;
		}

		ReadyControllers[Index] = PC;
	}

	// 로비 순서를 유지해 같은 명단을 트랙 선택과 차량 출발 슬롯에 사용한다.
	if (ReadyControllers.Contains(nullptr))
	{
		return;
	}

	if (Phase == EMWRoomPhase::TrackPlacement)
	{
		// 닫힌 세션의 최종 코스를 전원이 받은 뒤에 차량을 생성한다.
		if (Construction->GetPlacementSession().bIsOpen || CourseReadyMembers.Num() != StartingPlayers.Num())
		{
			return;
		}
		GS->SetRoomPhaseFromServer(EMWRoomPhase::PreparingRace);
		PrepareRace(ReadyControllers);
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

	if (Phase == EMWRoomPhase::PreparingRace)
	{
		PrepareRace(ReadyControllers);
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

	TArray<AMWPlayerState*> Players;
	for (AMWGameplayPlayerController* PC : ReadyControllers)
	{
		Players.Add(PC->GetPlayerState<AMWPlayerState>());
	}
	FText Error;
	if (!Construction->BeginSelection(ConstructionTrackDefinitionIds, Players, Error))
	{
		FailGameplay(Error.ToString());
		return;
	}
	Construction->OnAllSelectionsConfirmed.AddUObject(this, &ThisClass::StartTrackPlacement);
	Construction->OnPlacementFinished.AddUObject(this, &ThisClass::WaitForCourseDelivery);
}

void AMWGameplayGameMode::StartTrackPlacement()
{
	auto* GS = GetGameState<AMWGameplayGameState>();
	if (!GS || GS->GetRoomPhase() != EMWRoomPhase::PreparingConstruction)
	{
		return;
	}
	if (!StartManager.IsValid() || !SelectionBoard.IsValid())
	{
		FailGameplay(TEXT("출발 슬롯 또는 트랙 선택판이 유효하지 않습니다."));
		return;
	}

	// 선택판의 위치와 독립적으로 출발 그리드의 원점과 전방에 기준 도로를 맞춘다.
	FText Error;
	if (!GS->GetConstructionComponent()->BeginPlacement(StartManager->GetActorTransform(), SelectionBoard->GetLayoutScale(), Error))
	{
		FailGameplay(Error.ToString());
	}
}

void AMWGameplayGameMode::WaitForCourseDelivery()
{
	// 설치 Actor를 받지 못한 참가자가 있으면 차량 생성을 막고 실패 단계로 종료한다.
	GetWorldTimerManager().SetTimer(CourseDeliveryTimeout, FTimerDelegate::CreateWeakLambda(this, [this]
	{
		FailGameplay(TEXT("제한 시간 안에 전원에게 설치된 코스를 전달하지 못했습니다."));
	}), 30.0f, false);
}

void AMWGameplayGameMode::ConfirmTrackCourseReady(AMWGameplayPlayerController* Player, int32 SessionId, int32 CourseRevision)
{
	const auto* GS = GetGameState<AMWGameplayGameState>();
	const auto* App = GetGameInstance<UMWGameInstance>();
	const auto* PS = IsValid(Player) ? Player->GetPlayerState<AMWPlayerState>() : nullptr;
	if (!GS || GS->GetRoomPhase() != EMWRoomPhase::TrackPlacement || !App || !PS
		|| PS->GetOwner() != Player || !App->GetStartingPlayers().Contains(PS->GetLobbyMemberId()))
	{
		return;
	}
	const auto& Session = GS->GetConstructionComponent()->GetPlacementSession();
	if (Session.bIsOpen || Session.SessionId != SessionId || SessionId <= 0 || Session.CourseRevision != CourseRevision)
	{
		return;
	}

	CourseReadyMembers.Add(PS->GetLobbyMemberId());
	TryAdvanceGameplay();
}

void AMWGameplayGameMode::PrepareRace(const TArray<AMWGameplayPlayerController*>& Players)
{
	auto* GS = GetGameState<AMWGameplayGameState>();
	GetWorldTimerManager().ClearTimer(CourseDeliveryTimeout);

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

		const auto* Vehicle = Cast<AMWVehiclePawn>(PC->GetPawn());
		if (!IsValid(Vehicle) || Vehicle->GetController() != PC)
		{
			FailRace(FString::Printf(TEXT("차량 생성 또는 Possess 실패. Slot=%d"), Index + 1));
			return;
		}

		UE_LOG(LogMWServer, Log, TEXT("[MWRace] 차량 소유 확인. Member=%d Slot=%d Start=%s Pawn=%s"),
			PC->GetPlayerState<AMWPlayerState>()->GetLobbyMemberId(), Index + 1, *GetNameSafe(Start), *GetNameSafe(Vehicle));
	}

	GS->SetRoomPhaseFromServer(EMWRoomPhase::Racing);
	UE_LOG(LogMWServer, Log, TEXT("[MWRace] 전원 차량 소유 확인 완료."));
}

void AMWGameplayGameMode::FailGameplay(const FString& Error)
{
	GetWorldTimerManager().ClearTimer(CourseDeliveryTimeout);
	if (auto* GS = GetGameState<AMWGameplayGameState>())
	{
		GS->SetRoomPhaseFromServer(EMWRoomPhase::Failed);
		auto* Construction = GS->GetConstructionComponent();
		Construction->EndSelection();
		Construction->CancelPlacement();
	}
	UE_LOG(LogMWServer, Error, TEXT("[MWGameplay] %s"), *Error);
}

void AMWGameplayGameMode::Logout(AController* Exiting)
{
	const auto* PS = Exiting->GetPlayerState<AMWPlayerState>();
	const auto* GS = GetGameState<AMWGameplayGameState>();
	const auto* App = GetGameInstance<UMWGameInstance>();
	if (PS && GS && App && App->GetStartingPlayers().Contains(PS->GetLobbyMemberId())
		&& (GS->GetRoomPhase() == EMWRoomPhase::PreparingConstruction || GS->GetRoomPhase() == EMWRoomPhase::TrackPlacement))
	{
		FailGameplay(TEXT("건설 중 시작 참가자가 나갔습니다."));
	}
	Super::Logout(Exiting);
}

void AMWGameplayGameMode::HandleStartingNewPlayer_Implementation(APlayerController*)
{
	// 참가자 한 명의 테마 진입 처리가 끝났으므로, 전원을 다시 확인합니다.
	TryAdvanceGameplay();
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
