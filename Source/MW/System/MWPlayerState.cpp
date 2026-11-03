#include "System/MWPlayerState.h"
#include "Gameplay/Construction/MWPlayerConstructionComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

AMWPlayerState::AMWPlayerState()
{
	PlayerConstructionComponent = CreateDefaultSubobject<UMWPlayerConstructionComponent>(TEXT("PlayerConstructionComponent"));
}

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
	DOREPLIFETIME(AMWPlayerState, VehicleAssetID);
}

void AMWPlayerState::SetLobbyMemberIdFromServer(int32 Id)
{
	if (!HasAuthority() || Id < 1 || LobbyMemberId != INDEX_NONE) return;
	LobbyMemberId = Id;
	ForceNetUpdate();
	NotifyPlayerStateChanged();
}

bool AMWPlayerState::ConfirmVehicleAssetIDFromServer(FGameplayTag InVehicleAssetID)
{
	// 클라이언트 직접 변경과 빈 ID 확정을 막습니다.
	if (!HasAuthority() || !InVehicleAssetID.IsValid())
	{
		return false;
	}

	// 동일 요청은 성공으로 처리하고, 확정 후 다른 차량으로의 변경은 거절합니다.
	if (VehicleAssetID.IsValid())
	{
		return VehicleAssetID == InVehicleAssetID;
	}

	VehicleAssetID = InVehicleAssetID;
	ForceNetUpdate();

	// 호스트도 원격 클라이언트와 같은 변경 알림을 받습니다.
	NotifyPlayerStateChanged();
	return true;
}

void AMWPlayerState::OnRep_VehicleAssetID()
{
	// 원격 클라이언트가 복제된 결과를 받았을 때 알립니다.
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
