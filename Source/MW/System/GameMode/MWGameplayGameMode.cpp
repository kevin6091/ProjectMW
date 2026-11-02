


#include "System/GameMode/MWGameplayGameMode.h"

#include "System/GameState/MWGameplayGameState.h"
#include "System/MWPlayerState.h"
#include "Player/MWGameplayPlayerController.h"
#include "System/MWGameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameSession.h"

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
	GS->SetRoomPhaseFromServer(EMWRoomPhase::PreparingConstruction);
	GameSession->MaxPlayers = Settings.MaxPlayers;
	GameSession->MaxSpectators = 0;
	bRoomSettingsApplied = true;
}

void AMWGameplayGameMode::PreLogin(const FString& Options, const FString& Address,
	const FUniqueNetIdRepl& UniqueId, FString& Error)
{
	Super::PreLogin(Options, Address, UniqueId, Error);
	// 기존 테마 월드처럼 새 접속은 닫아 둡니다. 기존 참가자의 Seamless Travel 도착과는 별도 경로입니다.
	if (Error.IsEmpty()) Error = TEXT("방을 준비 중이거나 입장이 닫혔습니다.");
}

void AMWGameplayGameMode::HandleStartingNewPlayer_Implementation(APlayerController*)
{
	// 로비에서 넘겨받은 PlayerState를 그대로 사용하며 ID/Ready를 재설정하지 않습니다.
	// 기존 동작처럼 Pawn 자동 생성도 하지 않습니다. 차량 생성은 아직 구현하지 않습니다.
}
