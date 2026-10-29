#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "MultiplayerSubSystem/MWMultiplayerSubsystem.h"
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
	FString GetRoomSessionId() const { return RoomSessionId; }

	UPROPERTY(BlueprintAssignable, Category = "MW|Lobby")
	FMWLobbyChanged OnLobbyChanged;

	void ApplyRoomSettingsFromServer(const FMWRoomSettings& Settings, const FString& SessionId);
	void NotifyLobbyChanged();

private:
	UPROPERTY(ReplicatedUsing = OnRep_RoomSettings)
	FMWRoomSettings RoomSettings;
	UPROPERTY(ReplicatedUsing = OnRep_RoomSettings)
	FString RoomSessionId;

	UFUNCTION()
	void OnRep_RoomSettings();
};
