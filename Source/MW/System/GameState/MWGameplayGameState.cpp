


#include "System/GameState/MWGameplayGameState.h"
#include "Gameplay/Construction/MWTrackConstructionComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "LogChannel.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "EngineUtils.h"

AMWGameplayGameState::AMWGameplayGameState()
{
	TrackConstructionComponent = CreateDefaultSubobject<UMWTrackConstructionComponent>(TEXT("TrackConstructionComponent"));
}

void AMWGameplayGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMWGameplayGameState, RoomSettings);
	DOREPLIFETIME(AMWGameplayGameState, Progress);
}

void AMWGameplayGameState::ApplyProgressFromServer(const FMWGameplayProgress& NewProgress)
{
	if (!HasAuthority())
		return;

	if (Progress == NewProgress)
		return;

	Progress = NewProgress;

	ForceNetUpdate();
	NotifyRoomStateChanged();
}

double AMWGameplayGameState::GetPhaseSecondsRemaining() const
{
	return Progress.PhaseEndServerTime > 0.0
		? FMath::Max(0.0, Progress.PhaseEndServerTime - GetServerWorldTimeSeconds()) : 0.0;
}

void AMWGameplayGameState::OnRep_Progress()
{
	NotifyRoomStateChanged();
}

void AMWGameplayGameState::ApplyRoomSettingsFromServer(const FMWRoomSettings& Settings)
{
	if (!HasAuthority()) return;
	RoomSettings = Settings;
	ForceNetUpdate();
	// 서버 대입은 원격 클라이언트의 RepNotify와 별도로 호스트에게도 알려야 합니다.
	NotifyRoomStateChanged();
}

void AMWGameplayGameState::OnRep_RoomState()
{
	NotifyRoomStateChanged();
}

void AMWGameplayGameState::NotifyRoomStateChanged()
{
	for (TActorIterator<AMWVehiclePawn> It(GetWorld()); It; ++It)
	{
		It->RefreshDrivePermission();
	}
	UE_LOG(LogMWServer, Log, TEXT("[MWGameplayState] World=%s Authority=%d Phase=%s Round=%d Epoch=%d End=%.2f Mode=%s Course=%d/%d"),
	   *GetNameSafe(GetWorld()),
	   HasAuthority() ? 1 : 0,
	   *UEnum::GetValueAsString(Progress.Phase),
	   Progress.RoundIndex,
	   Progress.RoundEpoch,
	   Progress.PhaseEndServerTime,
	   *UEnum::GetValueAsString(Progress.RaceMode), Progress.CourseSessionId, Progress.CourseRevision);

	OnGameplayStateChanged.Broadcast();
}

