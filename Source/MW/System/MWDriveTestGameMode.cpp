


#include "System/MWDriveTestGameMode.h"

#include "Gameplay/Vehicle/MWVehicleCharacter.h"
#include "Player/MWDriveTestPlayerController.h"

AMWDriveTestGameMode::AMWDriveTestGameMode()
{
	PlayerControllerClass = AMWDriveTestPlayerController::StaticClass();

	DefaultPawnClass = AMWVehicleCharacter::StaticClass();

	bStartPlayersAsSpectators = false;
}