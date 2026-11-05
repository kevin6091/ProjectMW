


#include "System/MWDriveTestGameMode.h"

#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "Player/MWGameplayPlayerController.h"

AMWDriveTestGameMode::AMWDriveTestGameMode()
{
	PlayerControllerClass = AMWGameplayPlayerController::StaticClass();

	DefaultPawnClass = AMWVehiclePawn::StaticClass();

	bStartPlayersAsSpectators = false;
}
