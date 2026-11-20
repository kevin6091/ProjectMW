

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameplayTagContainer.h"
#include "System/MWRoomTypes.h"
#include "Gameplay/Construction/MWTrackPlacementTypes.h"
#include "MWGameplayGameMode.generated.h"

class AMWGameplayPlayerController;
class AMWPlayerStartManager;
class AMWTrackSelectionBoard;

// 테마 레벨의 서버 초기화입니다. 로비 규칙을 상속하지 않습니다.
UCLASS()
class MW_API AMWGameplayGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
public:
	AMWGameplayGameMode();
	virtual void InitGameState() override;
	virtual void StartPlay() override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& Error) override;
	virtual void Logout(AController* Exiting) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* Player) override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void ReportRaceReady(AMWGameplayPlayerController* Player, int32 RoundEpoch, int32 CourseSessionId, int32 CourseRevision);
private:
	friend struct FMWRaceStartTestScene;
	void TryInitializeGameplay();
	void TryStartGameplay();
	
	bool CollectStartingControllers(TArray<AMWGameplayPlayerController*>& OutControllers) const;
	bool IsRaceCourseCurrent() const;
	bool AreAllRaceReady() const;
	
	void EnterPhase(EMWRoomPhase NewPhase, const FString& FailureReason = FString());
	
	void StartTrackPlacement();
	void WaitForRacePhaseDeadline();
	void PrepareRace(const TArray<AMWGameplayPlayerController*>& Players);
	void FailGameplay(const FString& Error);

	void HandleAllSelectionsConfirmed(int32 SessionId);
	void HandlePlacementFinished(int32 SessionId, int32 CourseRevision);
	void HandleConstructionFailed(int32 SessionId, const FString& Error);
	
	// 순서대로 선택판에 표시한다. 빈 배열이면 건설 단계를 건너뛴다.
	UPROPERTY(EditDefaultsOnly, Category = "MW|Construction")
	TArray<FGameplayTag> ConstructionTrackDefinitionIds;

	UPROPERTY(EditDefaultsOnly, Category = "MW|Construction")
	FMWTrackCourseEnds CourseEnds;

	TWeakObjectPtr<AMWPlayerStartManager> StartManager;
	TWeakObjectPtr<AMWTrackSelectionBoard> SelectionBoard;
	TSet<int32> RaceReadyMembers;
	FTimerHandle RacePhaseTimer;

	UPROPERTY(EditDefaultsOnly, Category = "MW|Race", meta = (ClampMin = "1.0"))
	float RacePreparationTimeoutSeconds = 30.f;

	UPROPERTY(EditDefaultsOnly, Category = "MW|Race", meta = (ClampMin = "0.1"))
	float CountdownSeconds = 3.f;
	
	bool bRoomSettingsApplied = false;
};
