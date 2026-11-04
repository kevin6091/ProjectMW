

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "System/MWRoomTypes.h"
#include "MWLobbyGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMWLobbyStateChanged);

// 방 설정과 로비 Phase를 복제합니다. 화면 생성/갱신은 로컬 HUD가 맡습니다.
UCLASS()
class MW_API AMWLobbyGameState : public AGameStateBase
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
	void ApplyRoomSettingsFromServer(const FMWRoomSettings& Settings);
	void SetRoomPhaseFromServer(EMWRoomPhase Phase);
	void NotifyLobbyChanged();

	UPROPERTY(BlueprintAssignable, Category = "MW|Lobby")
	FMWLobbyStateChanged OnLobbyChanged;

private:
	UPROPERTY(ReplicatedUsing = OnRep_RoomSettings)
	FMWRoomSettings RoomSettings;
	UPROPERTY(ReplicatedUsing = OnRep_RoomSettings)
	EMWRoomPhase RoomPhase = EMWRoomPhase::Lobby;
	UFUNCTION()
	void OnRep_RoomSettings();
};
