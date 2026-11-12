#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MWUCMContinuousMusic.generated.h"

class UAudioComponent;
class USoundWave;
struct FStreamableHandle;

// 레벨 이동에도 살아 있는 음악 재생만 소유한다. 재생할 곡의 선택은 월드 음악 서브시스템이 담당한다.
UCLASS()
class UMWUCMContinuousMusic : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // 같은 곡을 다시 요청해도 현재 재생 위치를 유지한다. 다른 게임 인스턴스의 월드는 받지 않는다.
    void RequestTrack(UWorld* World, TSoftObjectPtr<USoundWave> Track, float Volume);
    virtual void Deinitialize() override;

private:
    // 월드가 교체되어도 재생 객체가 수거되지 않도록 강한 참조를 유지한다.
    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> PlayingMusic;

    FSoftObjectPath RequestedTrack;
    TSharedPtr<FStreamableHandle> PendingLoad;
    TWeakObjectPtr<UWorld> RequestWorld;
    float RequestedVolume = 1.f;

    void StartLoadedTrack(FSoftObjectPath Track);
};
