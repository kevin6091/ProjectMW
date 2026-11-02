#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "GameplayTagContainer.h"
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
    // 서버 진행 코드용 진입점. 선택 내부 규칙은 GameState의 컴포넌트가 처리한다.
    bool BeginTrackSelection(FText& OutError);
    void EndTrackSelection();

private:
    // 이번 예제의 공통 후보 세 종류. BP 기본값에서 카탈로그에 등록된 태그로 교체할 수 있다.
    UPROPERTY(EditDefaultsOnly, Category="MW|Construction")
    TArray<FGameplayTag> TrackSelectionDefinitionIds;

    bool bAcceptingPlayers = false;
    bool bRoomSettingsApplied = false;
	int32 NextLobbyMemberId = 1;
	IOnlineSessionPtr RegistrationApi;
	FDelegateHandle PlayerRegistrationHandle;
	void HandlePlayersRegistered(FName SessionName, const TArray<FUniqueNetIdRef>& Players, bool bWasSuccessful);

    bool ApplyApprovedRoomSettings();
};
