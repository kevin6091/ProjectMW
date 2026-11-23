

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
class AMWVehiclePawn;
class AMWPlayerState;

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
	// 서버 트리거와 기존 복귀 경로가 호출한다. 클라이언트 점수 요청 RPC는 없다.
	bool TryFinishPlayerRace(AMWVehiclePawn& Vehicle, AActor& FinishTrack, int32 RoundEpoch);
	bool TryPassPlayerCheckpoint(AMWVehiclePawn& Vehicle, AActor& Track, int32 RoundEpoch);
	bool HandlePlayerRaceFailure(AMWVehiclePawn& Vehicle, int32 RoundEpoch);
	void CheckPlayerRaceFailure(AMWVehiclePawn& Vehicle);
private:
	friend class FMWGameplayRoundFlowTest;
	void TryInitializeGameplay();
	void TryStartGameplay();
	
	bool CollectStartingControllers(TArray<AMWGameplayPlayerController*>& OutControllers, bool bRaceOnly = false) const;
	bool IsMatchMember(int32 MemberId) const;
	AMWPlayerState* FindActiveRacePlayer(AMWVehiclePawn& Vehicle, int32 RoundEpoch) const;
	bool AreAllRacersDone() const;
	int32 CountQualifiedPlayers() const;
	TArray<FMWPlayerRoundState> BuildResults(bool bFinal);
	void TryEndRound();
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
	int32 NextFinishOrder = 1;
	int32 NextEliminationOrder = 1;
	TSet<int32> MatchMembers;
	bool bMatchInitialized = false;

	UPROPERTY(EditDefaultsOnly, Category = "MW|Race", meta = (ClampMin = "1"))
	int32 TargetScore = 4000;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Race", meta = (ClampMin = "0.1"))
	float RoundResultsSeconds = 3.f;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Race", meta = (ClampMin = "100", Units = "cm"))
	float FallRecoveryDepth = 1000.f;

	UPROPERTY(EditDefaultsOnly, Category = "MW|Race", meta = (ClampMin = "1.0"))
	float RacePreparationTimeoutSeconds = 30.f;

	UPROPERTY(EditDefaultsOnly, Category = "MW|Race", meta = (ClampMin = "0.1"))
	float CountdownSeconds = 3.f;

	// 종료 캡 연결점 기준: 진행 깊이/도로 반폭/높이의 절반(cm). 기존 결승 폭·높이를 따른다.
	UPROPERTY(EditDefaultsOnly, Category = "MW|Race", meta = (ClampMin = "1.0", Units = "cm"))
	FVector FinishTriggerHalfExtent = FVector(50.0, 1350.0, 1250.0);
	
	bool bRoomSettingsApplied = false;
};
