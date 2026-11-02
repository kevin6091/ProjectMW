#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MWPlayerController.generated.h"

class AMWGameState;

UCLASS()
class MW_API AMWPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnRep_PlayerState() override;

	UFUNCTION(BlueprintCallable, Category = "MW|Lobby")
	void RefreshLobbyScreen();
	UFUNCTION(BlueprintCallable, Category = "MW|Lobby")
	void RequestSetReady(bool bReady);
	UFUNCTION(BlueprintCallable, Category = "MW|Lobby")
	void RequestStartGame();
	FString GetLobbyActionError() const { return LobbyActionError; }
	virtual void PostSeamlessTravel() override;

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "MW|Lobby")
	void CreateLobbyScreen();

	UFUNCTION(BlueprintImplementableEvent, Category = "MW|Lobby")
	void OnLobbyScreenRefresh();

private:
	FString LobbyActionError;
	UFUNCTION(Server, Reliable) void ServerSetReady(bool bReady);
	UFUNCTION(Server, Reliable) void ServerStartGame();
	UFUNCTION(Client, Reliable) void ClientLobbyActionFailed(const FString& Error);
	TWeakObjectPtr<AMWGameState> BoundGameState;

	UFUNCTION()
	void HandleLobbyChanged();
};
