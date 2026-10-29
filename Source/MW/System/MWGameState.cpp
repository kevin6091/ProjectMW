#include "System/MWGameState.h"
#include "Player/MWPlayerController.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

void AMWGameState::BeginPlay()
{
	Super::BeginPlay();
	
	// GameState가 Controller보다 늦게 준비된 경로도 연결합니다.
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (AMWPlayerController* PC = Cast<AMWPlayerController>(It->Get()))
		{
			if (PC->IsLocalController())
			{
				PC->RefreshLobbyScreen();
			}
		}
	}
	NotifyLobbyChanged();
}

void AMWGameState::AddPlayerState(APlayerState* PlayerState)
{
	Super::AddPlayerState(PlayerState);
	NotifyLobbyChanged();
}

void AMWGameState::RemovePlayerState(APlayerState* PlayerState)
{
	Super::RemovePlayerState(PlayerState);
	NotifyLobbyChanged();
}

void AMWGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMWGameState, RoomSettings);
	DOREPLIFETIME(AMWGameState, RoomSessionId);
}

void AMWGameState::ApplyRoomSettingsFromServer(const FMWRoomSettings& Settings, const FString& SessionId)
{
	if (!HasAuthority())
	{
		return;
	}

	RoomSettings = Settings;
	RoomSessionId = SessionId;
	ForceNetUpdate();
	NotifyLobbyChanged(); // Listen Host의 표시도 갱신합니다.
}

void AMWGameState::NotifyLobbyChanged()
{
	OnLobbyChanged.Broadcast();
}

void AMWGameState::OnRep_RoomSettings()
{
    NotifyLobbyChanged();
}
