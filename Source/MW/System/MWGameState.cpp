#include "System/MWGameState.h"

#include "LogChannel.h"
#include "Player/MWPlayerController.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "System/MWPlayerState.h"

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
	DOREPLIFETIME(AMWGameState, RoomPhase);
	DOREPLIFETIME(AMWGameState, CountdownEndServerTime);
}

void AMWGameState::ApplyRoomSettingsFromServer(const FMWRoomSettings& Settings)
{
	if (!HasAuthority())
	{
		return;
	}

	RoomSettings = Settings;
	ForceNetUpdate();
	NotifyLobbyChanged(); // Listen Host의 표시도 갱신합니다.
}

void AMWGameState::NotifyLobbyChanged()
{
	OnLobbyChanged.Broadcast();
}

void AMWGameState::NotifyRoomStateChanged()
{
	UE_LOG(LogMWServer, Log, TEXT("[MWRoomState] World=%s Authority=%d Phase=%s CountdownEnd=%.2f"),
	   *GetNameSafe(GetWorld()),
	   HasAuthority() ? 1 : 0,
	   *UEnum::GetValueAsString(RoomPhase),
	   CountdownEndServerTime);

	NotifyLobbyChanged();
}

void AMWGameState::OnRep_RoomSettings()
{
    NotifyLobbyChanged();
}

void AMWGameState::OnRep_RoomState()
{
	NotifyRoomStateChanged();
}

bool AMWGameState::ValidateReadyPlayers(FString& OutError) const
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

bool AMWGameState::CanStartGame() const
{
	FString Error;
	return RoomPhase == EMWRoomPhase::Lobby && ValidateReadyPlayers(Error);
}

void AMWGameState::SetRoomPhaseFromServer(EMWRoomPhase Phase)
{
	if (!HasAuthority() || RoomPhase == Phase) return;
	RoomPhase = Phase;
	ForceNetUpdate();
	NotifyLobbyChanged();
}
