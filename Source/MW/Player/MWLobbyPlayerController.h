

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "MWLobbyPlayerController.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMWLobbyActionErrorChanged);

// 자기 플레이어의 요청을 서버에 전달합니다. 특정 Widget의 생성/Refresh는 하지 않습니다.
UCLASS()
class MW_API AMWLobbyPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "MW|Lobby")
	void RequestSetReady(bool bReady);
	UFUNCTION(BlueprintCallable, Category = "MW|Lobby")
	void RequestStartGame();
	FString GetLobbyActionError() const { return LobbyActionError; }

	// 요청 결과를 로컬 표시 담당자에게 알리는 이벤트입니다. 게임 상태의 원본은 아닙니다.
	UPROPERTY(BlueprintAssignable, Category = "MW|Lobby")
	FMWLobbyActionErrorChanged OnLobbyActionErrorChanged;

	// 로컬 차량 선택 UI의 요청 진입점입니다.
	UFUNCTION(BlueprintCallable, Category = "MW|Vehicle")
	void RequestConfirmVehicle(FGameplayTag VehicleAssetID);
	void ReportVehicleDepartureReady();
	
private:
	FString LobbyActionError;
	UFUNCTION(Server, Reliable)
	void ServerSetReady(bool bReady);
	UFUNCTION(Server, Reliable)
	void ServerStartGame();
	UFUNCTION(Client, Reliable)
	void ClientLobbyActionFailed(const FString& Error);
	
	UFUNCTION(Server, Reliable)
	void ServerConfirmVehicle(FGameplayTag VehicleAssetID);
	UFUNCTION(Server, Reliable)
	void ServerReportVehicleDepartureReady();
};
