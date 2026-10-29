#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "System/MultiplayerSubSystem/MWMultiplayerTypes.h"
#include "MWPlayerController.generated.h"

class AMWGameState;
class UMWMultiplayerSubsystem;

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

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "MW|Lobby")
	void CreateLobbyScreen();

	UFUNCTION(BlueprintImplementableEvent, Category = "MW|Lobby")
	void OnLobbyScreenRefresh();

private:
	TWeakObjectPtr<AMWGameState> BoundGameState;
	TWeakObjectPtr<UMWMultiplayerSubsystem> BoundSubsystem;

	UFUNCTION()
	void HandleLobbyChanged();

	UFUNCTION()
	void HandleSessionError(const FString& ErrorMessage);

	UFUNCTION()
	void HandleSessionStateChanged(
		EMWSessionState PreviousState, EMWSessionState NewState);
};