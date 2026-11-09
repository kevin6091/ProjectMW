#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameplayTagContainer.h"
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
	virtual void Logout(AController* Exiting) override;
	virtual void PreLogin(const FString& Options, const FString& Address,
		const FUniqueNetIdRepl& UniqueId, FString& Error) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* Player) override;

	void TryInitializeRoom();
	void SetLobbyAdmission(bool Open) { bAcceptingPlayers = Open; }
	bool RequestSetReady(AMWLobbyPlayerController* Controller, bool Ready, FString& Error);
	bool RequestStartGame(AMWLobbyPlayerController* Controller, FString& Error);
	bool ValidateStartRequest(const AMWLobbyPlayerController* Controller, bool Starting, FString& Error) const;
	
	bool RequestConfirmVehicle(AMWLobbyPlayerController* Controller, FGameplayTag VehicleAssetID, FString& Error);

	// 별도 완료 플래그 없이 PlayerState의 ID들로 판단합니다.
	// GameMode는 서버에만 있으므로 서버 코드에서 조회합니다.
	bool IsAllVehiclesConfirmed() const;
	void HandleVehicleDepartureReady(AMWLobbyPlayerController* Controller);
	
private:
	// 이 값들은 서버만 사용합니다. 클라이언트가 표시할 결과는 GameState/PlayerState에 둡니다.
	bool bAcceptingPlayers = false;
	bool bRoomSettingsApplied = false;
	int32 NextLobbyMemberId = 1;
	FName PendingThemeDestination;
	TSet<int32> PlayersAwaitingDeparture;
	FTimerHandle DepartureTimeout;
	
	void HandleStartPrepared(bool Success, const FString& Error);
	void FailStart(const FString& Error);
	
	// 준비된 모든 차량을 검사.
	bool ValidateVehicleAssetLabel(FGameplayTag AssetLabelVehicles, FString& Error) const;
	// 이번 버전에서 허용한 ID와 Definition의 등록·로드를 검사합니다.
	bool ValidateVehicleAssetID(FGameplayTag VehicleAssetID, FString& Error) const;
	void BeginVehicleSelection();
	void TryCompleteVehicleSelection();
	void HandleVehicleDepartureTimeout();
	void SetVehicleCursorStreams(bool bOpen);
};
