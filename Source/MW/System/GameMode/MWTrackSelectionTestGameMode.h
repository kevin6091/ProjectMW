#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameplayTagContainer.h"
#include "MWTrackSelectionTestGameMode.generated.h"

UCLASS()
class AMWTrackSelectionTestGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
public:
	AMWTrackSelectionTestGameMode();
	
	virtual void StartPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	
private:
	void TryStartSelection();

	UFUNCTION()
	void TryFinishSelection();

	UPROPERTY(EditDefaultsOnly, Category = "Track Selection Test")
	TArray<FGameplayTag> TestTrackDefinitionIds;

	int32 NextTestMemberId = 1;
	bool bSelectionStarted = false;
};
