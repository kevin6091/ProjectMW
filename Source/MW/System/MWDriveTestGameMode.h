

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MWDriveTestGameMode.generated.h"

/**
 * 
 */
UCLASS()
class MW_API AMWDriveTestGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
public:
	AMWDriveTestGameMode();
	virtual void StartPlay() override;
	static bool IsItemPickupTestMap(const UWorld& World);
	static void ConfigureItemPickupGround(UWorld& World);
};
