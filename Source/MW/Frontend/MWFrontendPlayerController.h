#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameModeBase.h"
#include "MWFrontendPlayerController.generated.h"

class UMWFrontendWidget;

/** Only the new menu map opts into this controller. Team gameplay controllers remain untouched. */
UCLASS()
class MW_API AMWFrontendPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	AMWFrontendPlayerController();
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	UPROPERTY(EditDefaultsOnly, Category="MW|Frontend") TSubclassOf<UMWFrontendWidget> FrontendClass;
private:
	UFUNCTION() void HandlePresentationChanged(bool bFrontendVisible);
	UPROPERTY(Transient) TObjectPtr<UMWFrontendWidget> Frontend;
};

UCLASS()
class MW_API AMWFrontendGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AMWFrontendGameMode();
};
