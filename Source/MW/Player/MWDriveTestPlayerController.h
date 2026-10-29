

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MWDriveTestPlayerController.generated.h"

/**
 * 
 */
UCLASS()
class MW_API AMWDriveTestPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	AMWDriveTestPlayerController();

	virtual void ReceivedPlayer() override;

protected:
	virtual void BeginPlay() override;

private:
	void ApplyDriveTestInputMode();
};
