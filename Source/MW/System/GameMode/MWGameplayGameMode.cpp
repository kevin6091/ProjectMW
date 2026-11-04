


#include "System/GameMode/MWGameplayGameMode.h"

#include "LogChannel.h"
#include "System/GameState/MWGameplayGameState.h"
#include "System/MWPlayerState.h"
#include "Player/MWGameplayPlayerController.h"
#include "System/MWGameInstance.h"
#include "Gameplay/PlayerStart/MWPlayerStartManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Data/MWVehicleDefinitionData.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerStart.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "System/MWAssetManager.h"

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
	TryPrepareRace();
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
	//GS->SetRoomPhaseFromServer(EMWRoomPhase::PreparingConstruction);
	GS->SetRoomPhaseFromServer(EMWRoomPhase::PreparingRace);
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


void AMWGameplayGameMode::TryPrepareRace()
{
	UWorld* World = GetWorld();
	const auto* GS = GetGameState<AMWGameplayGameState>();
	const auto* App = GetGameInstance<UMWGameInstance>();

	// 서버 월드와 방 설정이 준비됐고, 차량 생성 단계에 들어왔는지 확인합니다.
	if (!World || !World->HasBegunPlay() || !GS || !App ||
		!bRoomSettingsApplied || GS->GetRoomPhase() != EMWRoomPhase::PreparingRace)
	{
		return;
	}

	// 호스트가 게임 시작을 눌렀을 때 서버가 고정한 참가자 ID입니다.
	const TArray<int32>& StartingPlayers = App->GetStartingPlayers();
	if (StartingPlayers.IsEmpty())
	{
		return;
	}

	TMap<int32, AMWGameplayPlayerController*> ReadyControllers;

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

		const int32 MemberId = PS->GetLobbyMemberId();
		if (!StartingPlayers.Contains(MemberId) || !PS->GetVehicleAssetID().IsValid())
		{
			continue;
		}

		ReadyControllers.Add(MemberId, PC);
	}

	// 로비에서 확정한 대상만 담았으므로, ID 개수가 같으면 전원 준비입니다.
	if (ReadyControllers.Num() != StartingPlayers.Num())
	{
		UE_LOG(LogMWServer, Log, TEXT("[MWRace] 참가자 대기 %d/%d"),
			ReadyControllers.Num(), StartingPlayers.Num());
		return;
	}

	AMWPlayerStartManager* StartManager = nullptr;
	for (TActorIterator<AMWPlayerStartManager> It(World); It; ++It)
	{
		if (StartManager)
		{
			UE_LOG(LogMWServer, Error, TEXT("[MWRace] 테마 레벨에 MWPlayerStartManager를 하나만 배치하세요."));
			return;
		}
		StartManager = *It;
	}
	if (!StartManager)
	{
		UE_LOG(LogMWServer, Error, TEXT("[MWRace] 테마 레벨에서 MWPlayerStartManager를 찾지 못했습니다."));
		return;
	}

	TArray<APlayerStart*> PlayerStarts;
	if (!StartManager->PreparePlayerStarts(StartingPlayers.Num(), PlayerStarts))
	{
		UE_LOG(LogMWServer, Error, TEXT("[MWRace] 참가자 %d명의 출발 위치를 준비하지 못했습니다."), StartingPlayers.Num());
		return;
	}

	for (int32 Index = 0; Index < StartingPlayers.Num(); ++Index)
	{
		const int32 MemberId = StartingPlayers[Index];
		auto* PC = ReadyControllers.FindRef(MemberId);
		APlayerStart* Start = PlayerStarts[Index];

		if (!IsValid(PC) || !IsValid(Start))
		{
			UE_LOG(LogMWServer, Error, TEXT("[MWRace] Controller 또는 출발 위치가 유효하지 않습니다. Member=%d Slot=%d"),
				MemberId, Index + 1);
			return;
		}

		// 다시 호출된 경우에도 이미 소유한 차량은 유지합니다.
		if (!IsValid(PC->GetPawn()))
		{
			RestartPlayerAtPlayerStart(PC, Start);
		}

		const auto* Vehicle = Cast<AMWVehiclePawn>(PC->GetPawn());
		if (!IsValid(Vehicle) || Vehicle->GetController() != PC)
		{
			UE_LOG(LogMWServer, Error, TEXT("[MWRace] 차량 생성 또는 Possess 실패. Member=%d Slot=%d"),
				MemberId, Index + 1);
			return;
		}

		UE_LOG(LogMWServer, Log, TEXT("[MWRace] 차량 소유 확인. Member=%d Slot=%d Start=%s Pawn=%s"),
			MemberId, Index + 1, *GetNameSafe(Start), *GetNameSafe(Vehicle));
	}

	UE_LOG(LogMWServer, Log, TEXT("[MWRace] 전원 차량 소유 확인 완료."));
}

void AMWGameplayGameMode::HandleStartingNewPlayer_Implementation(APlayerController*)
{
	// 참가자 한 명의 테마 진입 처리가 끝났으므로, 전원을 다시 확인합니다.
	TryPrepareRace();
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
