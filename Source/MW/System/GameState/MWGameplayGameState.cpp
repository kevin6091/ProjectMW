


#include "System/GameState/MWGameplayGameState.h"
#include "Gameplay/Construction/MWTrackConstructionComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "LogChannel.h"

AMWGameplayGameState::AMWGameplayGameState()
{
	TrackConstructionComponent = CreateDefaultSubobject<UMWTrackConstructionComponent>(TEXT("TrackConstructionComponent"));
}

void AMWGameplayGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMWGameplayGameState, RoomSettings);
	DOREPLIFETIME(AMWGameplayGameState, RoomPhase);
	DOREPLIFETIME(AMWGameplayGameState, CountdownEndServerTime);
}

void AMWGameplayGameState::ApplyRoomSettingsFromServer(const FMWRoomSettings& Settings)
{
	if (!HasAuthority()) return;
	RoomSettings = Settings;
	ForceNetUpdate();
	// 서버 대입은 원격 클라이언트의 RepNotify와 별도로 호스트에게도 알려야 합니다.
	NotifyRoomStateChanged();
}

void AMWGameplayGameState::SetRoomPhaseFromServer(EMWRoomPhase Phase)
{
	if (!HasAuthority() || RoomPhase == Phase) return;
	RoomPhase = Phase;
	ForceNetUpdate();
	NotifyRoomStateChanged();
}

void AMWGameplayGameState::OnRep_RoomState()
{
	NotifyRoomStateChanged();
}

void AMWGameplayGameState::NotifyRoomStateChanged()
{
	UE_LOG(LogMWServer, Log, TEXT("[MWRoomState] World=%s Authority=%d Phase=%s CountdownEnd=%.2f"),
		*GetNameSafe(GetWorld()), HasAuthority() ? 1 : 0,
		*UEnum::GetValueAsString(RoomPhase), CountdownEndServerTime);
	OnGameplayStateChanged.Broadcast();
}

