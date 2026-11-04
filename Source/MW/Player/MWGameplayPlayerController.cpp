


#include "Player/MWGameplayPlayerController.h"

void AMWGameplayPlayerController::BeginPlay()
{
	Super::BeginPlay();
	ApplyGameplayInputMode();
}

void AMWGameplayPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	ApplyGameplayInputMode();
}

void AMWGameplayPlayerController::PostSeamlessTravel()
{
	Super::PostSeamlessTravel();
	ApplyGameplayInputMode();
}

void AMWGameplayPlayerController::ApplyGameplayInputMode()
{
	if (!IsLocalController() || !HasActorBegunPlay()) return;
	// 기존 Controller의 테마 진입 처리를 옮겼습니다. 원격 참가자의 입력 모드는 변경하지 않습니다.
	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;
}

