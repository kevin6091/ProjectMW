#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "System/MWRoomTypes.h"
#include "MWGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMWLobbyChanged);

UCLASS()
class MW_API AMWGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void AddPlayerState(APlayerState* PlayerState) override;
	virtual void RemovePlayerState(APlayerState* PlayerState) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "MW|Lobby")
	FMWRoomSettings GetRoomSettings() const { return RoomSettings; }

	UFUNCTION(BlueprintPure, Category = "MW|Lobby")

	EMWRoomPhase GetRoomPhase() const { return RoomPhase; }
	UFUNCTION(BlueprintPure, Category = "MW|Lobby")

	bool CanStartGame() const;
	bool ValidateReadyPlayers(FString& OutError) const;
	void SetRoomPhaseFromServer(EMWRoomPhase Phase);

	UPROPERTY(BlueprintAssignable, Category = "MW|Lobby")
	FMWLobbyChanged OnLobbyChanged;

	void ApplyRoomSettingsFromServer(const FMWRoomSettings& Settings);
	void NotifyLobbyChanged();

private:
	UPROPERTY(ReplicatedUsing = OnRep_RoomSettings)
	EMWRoomPhase RoomPhase = EMWRoomPhase::Lobby;

	UPROPERTY(ReplicatedUsing = OnRep_RoomSettings)
	FMWRoomSettings RoomSettings;

// 남은 시간이 아니라 서버 기준 종료 시각입니다.
	// 카운트다운이 아니면 0을 사용합니다.
	UPROPERTY(ReplicatedUsing = OnRep_RoomState)
	double CountdownEndServerTime = 0.0;

	UFUNCTION()
	void OnRep_RoomSettings();

	UFUNCTION()
	void OnRep_RoomState();

	void NotifyRoomStateChanged();
};
