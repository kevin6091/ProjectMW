#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameplayTagContainer.h"
#include "MWTrackConstructionTestGameMode.generated.h"

class UMWAssetData;

// 선택 기능 제작용. 실제 EOS 등록 성공이나 정식 경기 시작을 대체하지 않는다.
UCLASS()
class MW_API AMWTrackConstructionTestGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AMWTrackConstructionTestGameMode();
    virtual void InitGameState() override;
    virtual void StartPlay() override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
    virtual void PreLogin(const FString& Options, const FString& Address,
        const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
    void RestartSelection();
protected:
    UPROPERTY(EditDefaultsOnly, Category="Construction Test")
    TObjectPtr<UMWAssetData> TestCatalog;
    UPROPERTY(EditDefaultsOnly, Category="Construction Test")
    TArray<FGameplayTag> TestDefinitionIds;
private:
    void ScheduleSelection();
    FTimerHandle SelectionStartTimer;
    int32 NextTestPlayerId = 1;
    bool bSelectionScheduled = false;
};
