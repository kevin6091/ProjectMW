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

	// 단독 실행은 1명으로 시작한다. Listen/Dedicated 테스트에서 기다릴 인원이다.
	UPROPERTY(EditDefaultsOnly, Category = "Track Selection Test", meta = (ClampMin = "1"))
	int32 RequiredNetworkPlayers = 2;

	int32 NextTestMemberId = 1;
	int32 SelectionParticipantCount = 0;
	bool bSelectionStarted = false;
};
