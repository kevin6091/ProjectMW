#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "MWPlayerState.generated.h"

UCLASS()
class MW_API AMWPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	virtual void CopyProperties(APlayerState* PlayerState) override;
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

	void SetLobbyFlagsFromServer(bool bInHost, bool bInReady);
	void SetLobbyMemberIdFromServer(int32 Id);

private:
	UPROPERTY(ReplicatedUsing = OnRep_LobbyFlags)
	bool bRoomHost = false;

	UPROPERTY(ReplicatedUsing = OnRep_LobbyFlags)
	bool bReady = false;
	UPROPERTY(ReplicatedUsing = OnRep_LobbyFlags)
	int32 LobbyMemberId = INDEX_NONE;

	UFUNCTION()
	void OnRep_LobbyFlags();

	void NotifyLobby();
};
