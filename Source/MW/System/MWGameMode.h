#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "MWGameMode.generated.h"

UCLASS()
class MW_API AMWGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AMWGameMode();

    virtual void InitGameState() override;
    virtual void StartPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;

    virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

    void TryPrepareLobby();
    void SetLobbyAdmission(bool bOpen) { bAcceptingPlayers = bOpen; }

private:
    bool bAcceptingPlayers = false;
    bool bRoomSettingsApplied = false;
	int32 NextLobbyMemberId = 1;
	IOnlineSessionPtr RegistrationApi;
	FDelegateHandle PlayerRegistrationHandle;
	void HandlePlayersRegistered(FName SessionName, const TArray<FUniqueNetIdRef>& Players, bool bWasSuccessful);

    bool ApplyApprovedRoomSettings();
};
