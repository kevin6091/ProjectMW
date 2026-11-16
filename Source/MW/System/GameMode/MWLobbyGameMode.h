#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameplayTagContainer.h"
#include "MWLobbyGameMode.generated.h"

class AMWLobbyPlayerController;
class AMWPlayerState;

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

	// 기존 참가자 이탈 처리와 함께 출발 대기가 남지 않도록 현재 시작 상태를 확인한다.
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

	// 예상 ID와 현재 선택이 일치한 서버 승인만 기존 차량 ID 해제로 연결한다.
	bool RequestCancelVehicle(AMWLobbyPlayerController* Controller, FGameplayTag ExpectedVehicleID, FString& Error);

	// 별도 완료 플래그 없이 PlayerState의 ID들로 판단합니다.
	// GameMode는 서버에만 있으므로 서버 코드에서 조회합니다.
	bool IsAllVehiclesConfirmed() const;

	// 소유자·단계·중복을 검사해 대기자를 제거하고 전원이 끝났을 때만 서버 이동을 시작한다.
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

	// 확정·취소가 같은 단계·월드·소유권·고정 시작 참가자 검사를 공유한다.
	AMWPlayerState* ValidateVehicleSelectionRequester(AMWLobbyPlayerController* Controller, FString& Error) const;
	
	// 준비된 모든 차량을 검사.
	bool ValidateVehicleAssetLabel(FGameplayTag AssetLabelVehicles, FString& Error) const;
	// 이번 버전에서 허용한 ID와 Definition의 등록·로드를 검사합니다.
	bool ValidateVehicleAssetID(FGameplayTag VehicleAssetID, FString& Error) const;
	void BeginVehicleSelection();
	void TryCompleteVehicleSelection();

	// 완료 누락을 시작 실패로 처리한다. 준비 검사를 건너뛰고 강제로 이동하지 않는다.
	void HandleVehicleDepartureTimeout();
	void SetVehicleCursorStreams(bool bOpen);
};
