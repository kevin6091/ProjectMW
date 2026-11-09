

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameplayTagContainer.h"
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

	// 순서대로 선택판에 표시하고, 첫 번째 정의는 출발 슬롯 아래의 기준 도로로 사용한다.
	UPROPERTY(EditDefaultsOnly, Category = "MW|Construction")
	TArray<FGameplayTag> ConstructionTrackDefinitionIds;

	TWeakObjectPtr<AMWPlayerStartManager> StartManager;
	TWeakObjectPtr<AMWTrackSelectionBoard> SelectionBoard;
	TSet<int32> CourseReadyMembers;
	FTimerHandle CourseDeliveryTimeout;

	bool bRoomSettingsApplied = false;
};
