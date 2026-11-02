#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MWGameMode.generated.h"

UCLASS()
class MW_API AMWGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AMWGameMode();
    virtual void InitGameState() override;
    virtual void StartPlay() override;
    virtual void PostLogin(APlayerController* Player) override;
    virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& Error) override;
    virtual void HandleStartingNewPlayer_Implementation(APlayerController* Player) override;
    void TryInitializeRoom();
    void SetLobbyAdmission(bool Open) { bAcceptingPlayers = Open; }
    bool RequestSetReady(class AMWPlayerController* Controller, bool Ready, FString& Error);
    bool RequestStartGame(class AMWPlayerController* Controller, FString& Error);
    bool ValidateStartRequest(const class AMWPlayerController* Controller, bool Starting, FString& Error) const;
private:
    bool bAcceptingPlayers = false;
    bool bRoomSettingsApplied = false;
    int32 NextLobbyMemberId = 1;
    void HandleStartPrepared(bool Success, const FString& Error);
    void FailStart(const FString& Error);
};
