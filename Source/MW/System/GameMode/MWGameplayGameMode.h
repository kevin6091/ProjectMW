

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
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

	void ConfirmTrackCourseReady(AMWGameplayPlayerController* Player, int32 SessionId, int32 CourseRevision);
	
private:
	void TryInitializeGameplay();
	void TryAdvanceGameplay();
	void StartTrackPlacement();
	void WaitForCourseDelivery();
	void PrepareRace(const TArray<AMWGameplayPlayerController*>& Players);
	void FailGameplay(const FString& Error);

	// 건설 단계 사용 여부는 시작·종료 BP의 설정과 별개다.
	UPROPERTY(EditDefaultsOnly, Category = "MW|Construction")
	bool bEnableTrackConstruction = true;

	TWeakObjectPtr<AMWPlayerStartManager> StartManager;
	TWeakObjectPtr<AMWTrackSelectionBoard> SelectionBoard;
	TSet<int32> CourseReadyMembers;
	FTimerHandle CourseDeliveryTimeout;

	bool bRoomSettingsApplied = false;
};
