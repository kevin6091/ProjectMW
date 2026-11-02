


#include "Player/MWLobbyPlayerController.h"
#include "System/GameMode/MWLobbyGameMode.h"
#include "Engine/World.h"

void AMWLobbyPlayerController::RequestSetReady(bool bReady)
{
	if (!IsLocalController()) return;
	LobbyActionError.Reset();
	OnLobbyActionErrorChanged.Broadcast();
	ServerSetReady(bReady);
}

void AMWLobbyPlayerController::RequestStartGame()
{
	if (!IsLocalController()) return;
	LobbyActionError.Reset();
	OnLobbyActionErrorChanged.Broadcast();
	ServerStartGame();
}

void AMWLobbyPlayerController::ServerSetReady_Implementation(bool bReady)
{
	// 소유 Controller의 RPC이므로 요청자가 누구인지는 this로 결정하며 클라이언트가 지정하지 않습니다.
	auto* GM = GetWorld()->GetAuthGameMode<AMWLobbyGameMode>();
	FString Error;
	if (!GM || !GM->RequestSetReady(this, bReady, Error))
		ClientLobbyActionFailed(Error.IsEmpty() ? TEXT("로비를 확인할 수 없습니다.") : Error);
}

void AMWLobbyPlayerController::ServerStartGame_Implementation()
{
	auto* GM = GetWorld()->GetAuthGameMode<AMWLobbyGameMode>();
	FString Error;
	if (!GM || !GM->RequestStartGame(this, Error))
		ClientLobbyActionFailed(Error.IsEmpty() ? TEXT("경기를 시작할 수 없습니다.") : Error);
}

void AMWLobbyPlayerController::ClientLobbyActionFailed_Implementation(const FString& Error)
{
	LobbyActionError = Error;
	OnLobbyActionErrorChanged.Broadcast();
}

