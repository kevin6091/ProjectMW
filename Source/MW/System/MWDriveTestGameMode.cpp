


#include "System/MWDriveTestGameMode.h"

#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "Player/MWDriveTestPlayerController.h"

AMWDriveTestGameMode::AMWDriveTestGameMode()
{
	PlayerControllerClass = AMWDriveTestPlayerController::StaticClass();

	DefaultPawnClass = AMWVehiclePawn::StaticClass();

	bStartPlayersAsSpectators = false;
}