#include "System/MWPlayerState.h"
#include "System/MWGameState.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

void AMWPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMWPlayerState, bRoomHost);
	DOREPLIFETIME(AMWPlayerState, bReady);
	DOREPLIFETIME(AMWPlayerState, LobbyMemberId);
	DOREPLIFETIME(AMWPlayerState, bSessionRegistrationConfirmed);
}

void AMWPlayerState::SetLobbyMemberIdFromServer(int32 Id)
{
	if (!HasAuthority() || Id < 1 || LobbyMemberId != INDEX_NONE) return;
	LobbyMemberId = Id;
	ForceNetUpdate();
	NotifyLobby();
}

void AMWPlayerState::SetSessionRegistrationConfirmedFromServer(bool bConfirmed)
{
	if (!HasAuthority()) return;
	bSessionRegistrationConfirmed = bConfirmed;
	ForceNetUpdate();
	NotifyLobby();
}

void AMWPlayerState::SetLobbyFlagsFromServer(bool bInHost, bool bInReady)
{
	if (!HasAuthority())
	{
		return;
	}
	bRoomHost = bInHost;
	bReady = bInReady;
	ForceNetUpdate();
	NotifyLobby();
}

void AMWPlayerState::OnRep_LobbyFlags()
{
	NotifyLobby();
}

void AMWPlayerState::OnRep_PlayerName()
{
	Super::OnRep_PlayerName();
	NotifyLobby();
}

void AMWPlayerState::OnRep_UniqueId()
{
	Super::OnRep_UniqueId();
	NotifyLobby();
}

void AMWPlayerState::NotifyLobby()
{
	if (AMWGameState* GS = GetWorld()->GetGameState<AMWGameState>())
	{
		GS->NotifyLobbyChanged();
	}
}
