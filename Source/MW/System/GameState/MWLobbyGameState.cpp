#include "System/GameState/MWLobbyGameState.h"

#include "LogChannel.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "System/MWPlayerState.h"

void AMWLobbyGameState::BeginPlay()
{
    Super::BeginPlay();
    // Controller나 Widget을 찾지 않고 상태 변경만 알립니다. HUD는 초기 조회 후 변경을 구독합니다.
    NotifyLobbyChanged();
}

void AMWLobbyGameState::AddPlayerState(APlayerState* PlayerState)
{
	Super::AddPlayerState(PlayerState);
	NotifyLobbyChanged();
}

void AMWLobbyGameState::RemovePlayerState(APlayerState* PlayerState)
{
	Super::RemovePlayerState(PlayerState);
	NotifyLobbyChanged();
}

void AMWLobbyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMWLobbyGameState, RoomSettings);
	DOREPLIFETIME(AMWLobbyGameState, RoomPhase);
}

void AMWLobbyGameState::ApplyRoomSettingsFromServer(const FMWRoomSettings& Settings)
{
	if (!HasAuthority())
	{
		return;
	}

	RoomSettings = Settings;
	ForceNetUpdate();
	NotifyLobbyChanged(); // Listen Host의 표시도 갱신합니다.
}

void AMWLobbyGameState::NotifyLobbyChanged()
{
	OnLobbyChanged.Broadcast();
}

void AMWLobbyGameState::OnRep_RoomSettings()
{
    NotifyLobbyChanged();
}

bool AMWLobbyGameState::ValidateReadyPlayers(FString& OutError) const
{
	OutError.Reset();
	if (PlayerArray.Num() < 2 || PlayerArray.Num() > RoomSettings.MaxPlayers)
	{
		OutError = TEXT("최소 2명 이상, 방 정원 이내에서 시작할 수 있습니다.");
		return false;
	}
	TSet<int32> MemberIds;
	int32 Hosts = 0;
	for (const APlayerState* Player : PlayerArray)
	{
		const AMWPlayerState* PS = Cast<AMWPlayerState>(Player);
		if (!PS || PS->IsInactive() ||
			PS->GetLobbyMemberId() < 1 || MemberIds.Contains(PS->GetLobbyMemberId()) ||
			!PS->IsReady())
		{
			OutError = TEXT("연결 확인이 끝난 현재 참가자 전원이 Ready여야 합니다.");
			return false;
		}
		MemberIds.Add(PS->GetLobbyMemberId());
		Hosts += PS->IsRoomHost() ? 1 : 0;
	}
	if (Hosts != 1)
	{
		OutError = TEXT("호스트 정보를 확인할 수 없습니다.");
		return false;
	}
	return true;
}

bool AMWLobbyGameState::CanStartGame() const
{
	FString Error;
	return RoomPhase == EMWRoomPhase::Lobby && ValidateReadyPlayers(Error);
}

void AMWLobbyGameState::SetRoomPhaseFromServer(EMWRoomPhase Phase)
{
	if (!HasAuthority() || RoomPhase == Phase) return;
	RoomPhase = Phase;
	ForceNetUpdate();
	NotifyLobbyChanged();
}
