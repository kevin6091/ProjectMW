#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MWLobbyGameMode.generated.h"

class AMWLobbyPlayerController;

// L_Title에서 방 입장, Ready, 시작 승인을 처리하는 서버 규칙입니다.
UCLASS()
class MW_API AMWLobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
public:
	AMWLobbyGameMode();
	virtual void InitGameState() override;
	virtual void StartPlay() override;
	virtual void PostLogin(APlayerController* Player) override;
	virtual void PreLogin(const FString& Options, const FString& Address,
		const FUniqueNetIdRepl& UniqueId, FString& Error) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* Player) override;

	void TryInitializeRoom();
	void SetLobbyAdmission(bool Open) { bAcceptingPlayers = Open; }
	bool RequestSetReady(AMWLobbyPlayerController* Controller, bool Ready, FString& Error);
	bool RequestStartGame(AMWLobbyPlayerController* Controller, FString& Error);
	bool ValidateStartRequest(const AMWLobbyPlayerController* Controller, bool Starting, FString& Error) const;

private:
	// 이 값들은 서버만 사용합니다. 클라이언트가 표시할 결과는 GameState/PlayerState에 둡니다.
	bool bAcceptingPlayers = false;
	bool bRoomSettingsApplied = false;
	int32 NextLobbyMemberId = 1;
	void HandleStartPrepared(bool Success, const FString& Error);
	void FailStart(const FString& Error);
};
