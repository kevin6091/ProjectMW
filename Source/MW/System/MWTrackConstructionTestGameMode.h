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
    // 새 접속 인원을 포함해 테스트 선택을 다시 만든다. 이전 확정 결과도 초기화된다.
    void RestartSelection();
protected:
    // 기존 입력·차량·맵 항목까지 보존한 제작 맵 전용 카탈로그를 BP에서 지정한다.
    UPROPERTY(EditDefaultsOnly, Category="Construction Test")
    TObjectPtr<UMWAssetData> TestCatalog;
    UPROPERTY(EditDefaultsOnly, Category="Construction Test")
    TArray<FGameplayTag> TestDefinitionIds;
private:
    // 같은 프레임의 시작 요청을 합쳐 다음 Tick에 한 번만 재시작한다.
    void ScheduleSelection();
    FTimerHandle SelectionStartTimer;
    int32 NextTestPlayerId = 1;
    bool bSelectionScheduled = false;
};
