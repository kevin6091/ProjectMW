#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "MWPlayerState.generated.h"

class UMWPlayerConstructionComponent;

UCLASS()
class MW_API AMWPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AMWPlayerState();
	UFUNCTION(BlueprintPure, Category="MW|Construction")
	UMWPlayerConstructionComponent* GetPlayerConstructionComponent() const { return PlayerConstruction; }

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnRep_PlayerName() override;
	virtual void OnRep_UniqueId() override;

	UFUNCTION(BlueprintPure, Category = "MW|Lobby")
	bool IsRoomHost() const { return bRoomHost; }

	UFUNCTION(BlueprintPure, Category = "MW|Lobby")
	bool IsReady() const { return bReady; }

	UFUNCTION(BlueprintPure, Category = "MW|Lobby")
	int32 GetLobbyMemberId() const { return LobbyMemberId; }

	UFUNCTION(BlueprintPure, Category = "MW|Lobby")
	bool IsSessionRegistrationConfirmed() const { return bSessionRegistrationConfirmed; }

	void SetLobbyFlagsFromServer(bool bInHost, bool bInReady);
	void SetLobbyMemberIdFromServer(int32 Id);
	void SetSessionRegistrationConfirmedFromServer(bool bConfirmed);

private:
	// 다른 플레이어에게도 보여줄 개인 선택을 저장한다. Controller는 입력·요청만 담당한다.
	UPROPERTY(VisibleAnywhere, Category="MW|Construction")
	TObjectPtr<UMWPlayerConstructionComponent> PlayerConstruction;

	UPROPERTY(ReplicatedUsing = OnRep_LobbyFlags)
	bool bRoomHost = false;

	UPROPERTY(ReplicatedUsing = OnRep_LobbyFlags)
	bool bReady = false;
	UPROPERTY(ReplicatedUsing = OnRep_LobbyFlags)
	int32 LobbyMemberId = INDEX_NONE;
	UPROPERTY(ReplicatedUsing = OnRep_LobbyFlags)
	bool bSessionRegistrationConfirmed = false;

	UFUNCTION()
	void OnRep_LobbyFlags();

	void NotifyLobby();
};
