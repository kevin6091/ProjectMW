


#include "Player/MWDriveTestPlayerController.h"

AMWDriveTestPlayerController::AMWDriveTestPlayerController()
{
	bAutoManageActiveCameraTarget = true;

	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
}

void AMWDriveTestPlayerController::BeginPlay()
{
	Super::BeginPlay();

	ApplyDriveTestInputMode();
}

void AMWDriveTestPlayerController::ReceivedPlayer()
{
	Super::ReceivedPlayer();

	ApplyDriveTestInputMode();
}

void AMWDriveTestPlayerController::ApplyDriveTestInputMode()
{
	if (!IsLocalController() || !GetLocalPlayer())
	{
		return;
	}

	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);

	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
}