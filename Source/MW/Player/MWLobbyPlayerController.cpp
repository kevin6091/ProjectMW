

#include "Player/MWLobbyPlayerController.h"
#include "System/GameMode/MWLobbyGameMode.h"
#include "MWGameplayTags.h"
#include "Engine/World.h"
#include "System/MWPlayerState.h"

void AMWLobbyPlayerController::RequestSetReady(bool bReady)
{
	if (!IsLocalController()) return;
	LobbyActionError.Reset();
	OnLobbyActionErrorChanged.Broadcast();
	ServerSetReady(bReady);
}

void AMWLobbyPlayerController::RequestStartGame()
{
	if (!IsLocalController()) return;
	LobbyActionError.Reset();
	OnLobbyActionErrorChanged.Broadcast();
	ServerStartGame();
}

void AMWLobbyPlayerController::ServerSetReady_Implementation(bool bReady)
{
	// 소유 Controller의 RPC이므로 요청자가 누구인지는 this로 결정하며 클라이언트가 지정하지 않습니다.
	auto* GM = GetWorld()->GetAuthGameMode<AMWLobbyGameMode>();
	FString Error;
	if (!GM || !GM->RequestSetReady(this, bReady, Error))
		ClientLobbyActionFailed(Error.IsEmpty() ? TEXT("로비를 확인할 수 없습니다.") : Error);
}

void AMWLobbyPlayerController::ServerStartGame_Implementation()
{
	auto* GM = GetWorld()->GetAuthGameMode<AMWLobbyGameMode>();
	FString Error;
	if (!GM || !GM->RequestStartGame(this, Error))
		ClientLobbyActionFailed(Error.IsEmpty() ? TEXT("경기를 시작할 수 없습니다.") : Error);
}

void AMWLobbyPlayerController::ClientLobbyActionFailed_Implementation(const FString& Error)
{
	LobbyActionError = Error;
	OnLobbyActionErrorChanged.Broadcast();
}

void AMWLobbyPlayerController::RequestConfirmVehicle(FGameplayTag VehicleAssetID)
{
	if (!IsLocalController())
	{
		return;
	}

	LobbyActionError.Reset();
	OnLobbyActionErrorChanged.Broadcast();

	// GameState의 Phase 복제가 아직 늦게 도착했을 수도 있으므로
	// 로컬 Phase로 제출을 막지 않습니다. 서버에서 최종 검사합니다.
	ServerConfirmVehicle(VehicleAssetID);
}

// 로컬 소유자의 현재 차량 ID를 취소 요청으로 전달한다.
void AMWLobbyPlayerController::RequestCancelVehicle(FGameplayTag ExpectedVehicleID)
{
    if (!IsLocalController())
        return;
    LobbyActionError.Reset();
    OnLobbyActionErrorChanged.Broadcast();
    ServerCancelVehicle(ExpectedVehicleID);
}

// 서버 GameMode의 기존 검증을 통과한 취소만 반영하고 거절 이유는 소유자에게 돌려준다.
void AMWLobbyPlayerController::ServerCancelVehicle_Implementation(FGameplayTag ExpectedVehicleID)
{
    auto* GM = GetWorld()->GetAuthGameMode<AMWLobbyGameMode>();
    FString Error;
    if (!GM || !GM->RequestCancelVehicle(this, ExpectedVehicleID, Error))
    {
        ClientLobbyActionFailed(Error.IsEmpty() ? TEXT("차량 선택을 취소할 수 없습니다.") : Error);
    }
}

// 로컬 출발 연출이 화면을 가린 뒤 소유 Controller로 완료를 보고한다.
void AMWLobbyPlayerController::ReportVehicleDepartureReady()
{
    if (IsLocalController())
    {
        ServerReportVehicleDepartureReady();
    }
}

// 실제 요청자 Controller로 서버의 출발 대기 상태를 갱신한다.
void AMWLobbyPlayerController::ServerReportVehicleDepartureReady_Implementation()
{
    if (auto* GM = GetWorld()->GetAuthGameMode<AMWLobbyGameMode>())
    {
        GM->HandleVehicleDepartureReady(this);
    }
}

void AMWLobbyPlayerController::ServerConfirmVehicle_Implementation(FGameplayTag VehicleAssetID)
{
	auto* GM = GetWorld()->GetAuthGameMode<AMWLobbyGameMode>();

	FString Error; 
	if (!GM || !GM->RequestConfirmVehicle(this, VehicleAssetID, Error))
	{
		ClientLobbyActionFailed(Error.IsEmpty() ? TEXT("차량 선택을 확정할 수 없습니다.") : Error);
	}
}
