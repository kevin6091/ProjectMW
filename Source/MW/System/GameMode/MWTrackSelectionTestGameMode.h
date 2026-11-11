#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameplayTagContainer.h"
#include "Gameplay/Construction/MWTrackPlacementTypes.h"
#include "MWTrackSelectionTestGameMode.generated.h"

UCLASS()
class AMWTrackSelectionTestGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
public:
	AMWTrackSelectionTestGameMode();
	
	virtual void StartPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	
private:
	void TryStartSelection();

	void TryFinishSelection();

	UPROPERTY(EditDefaultsOnly, Category = "Track Selection Test")
	TArray<FGameplayTag> TestTrackDefinitionIds;

	UPROPERTY(EditDefaultsOnly, Category = "Track Selection Test")
	FMWTrackCourseEnds CourseEnds;

	// 단독 실행은 1명으로 시작한다. Listen/Dedicated 테스트에서 기다릴 인원이다.
	UPROPERTY(EditDefaultsOnly, Category = "Track Selection Test", meta = (ClampMin = "1"))
	int32 RequiredNetworkPlayers = 2;

	int32 NextTestMemberId = 1;
	bool bSelectionStarted = false;
};
