#include "System/MWPlayerState.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

void AMWPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);
	// Controller와 PlayerState 객체가 교체돼도 기존 참가자 데이터는 새 PlayerState에 인계합니다.
	if (AMWPlayerState* Target = Cast<AMWPlayerState>(PlayerState))
	{
		Target->bRoomHost = bRoomHost;
		Target->bReady = bReady;
		Target->LobbyMemberId = LobbyMemberId;
		Target->VehicleAssetID = VehicleAssetID;
	}
}

void AMWPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMWPlayerState, bRoomHost);
	DOREPLIFETIME(AMWPlayerState, bReady);
	DOREPLIFETIME(AMWPlayerState, LobbyMemberId);	
}

void AMWPlayerState::SetLobbyMemberIdFromServer(int32 Id)
{
	if (!HasAuthority() || Id < 1 || LobbyMemberId != INDEX_NONE) return;
	LobbyMemberId = Id;
	ForceNetUpdate();
	NotifyPlayerStateChanged();
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
	NotifyPlayerStateChanged();
}

void AMWPlayerState::OnRep_LobbyFlags()
{
	NotifyPlayerStateChanged();
}

void AMWPlayerState::OnRep_PlayerName()
{
	Super::OnRep_PlayerName();
	NotifyPlayerStateChanged();
}

void AMWPlayerState::OnRep_UniqueId()
{
	Super::OnRep_UniqueId();
	NotifyPlayerStateChanged();
}

// 로비나 주행 화면을 직접 찾지 않습니다. 필요한 로컬 표시 객체가 이 알림을 구독합니다.
void AMWPlayerState::NotifyPlayerStateChanged()
{
    OnPlayerStateChanged.Broadcast();
}
