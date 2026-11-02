#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "MultiplayerSubSystem/MWMultiplayerSubsystem.h"
#include "MWGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMWLobbyChanged);
class UMWTrackConstructionComponent;

UCLASS()
class MW_API AMWGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AMWGameState();
	UFUNCTION(BlueprintPure, Category="MW|Construction")
	UMWTrackConstructionComponent* GetTrackConstructionComponent() const { return TrackConstruction; }

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
	// 공용 선택 기능을 별도 컴포넌트에 위임한다. 로비 정보와 선택 데이터의 책임을 분리한다.
	UPROPERTY(VisibleAnywhere, Category="MW|Construction")
	TObjectPtr<UMWTrackConstructionComponent> TrackConstruction;

	UPROPERTY(ReplicatedUsing = OnRep_RoomSettings)
	FMWRoomSettings RoomSettings;
	UPROPERTY(ReplicatedUsing = OnRep_RoomSettings)
	FString RoomSessionId;

	UFUNCTION()
	void OnRep_RoomSettings();
};
