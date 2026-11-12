#include "MWUCMContinuousMusic.h"

#include "Components/AudioComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundWave.h"

DEFINE_LOG_CATEGORY_STATIC(LogMWContinuousMusic, Log, All);

namespace
{
constexpr float MusicFadeSeconds = 0.35f;
}

void UMWUCMContinuousMusic::RequestTrack(UWorld* World, TSoftObjectPtr<USoundWave> Track, float Volume)
{
    if (!IsValid(World) || World->bIsTearingDown || World->GetGameInstance() != GetGameInstance() ||
        World->GetNetMode() == NM_DedicatedServer || Track.IsNull())
    {
        return;
    }

    const FSoftObjectPath Path = Track.ToSoftObjectPath();
    const bool bNewWorld = RequestWorld.Get() != World;
    RequestWorld = World;
    RequestedVolume = Volume;
    // 같은 곡의 반복 요청은 재생 위치를 초기화하지 않는다. 로비 진입과 메뉴 복귀도 여기에 해당한다.
    if (Path == RequestedTrack && (PendingLoad || (IsValid(PlayingMusic) && PlayingMusic->IsPlaying())))
    {
        if (bNewWorld && IsValid(PlayingMusic))
        {
            UE_LOG(LogMWContinuousMusic, Display, TEXT("Continue track=%s component=%s playing=%d world=%s"),
                *Path.ToString(), *PlayingMusic->GetPathName(), PlayingMusic->IsPlaying(), *World->GetName());
        }
        return;
    }

    if (PendingLoad)
    {
        PendingLoad->CancelHandle();
    }
    PendingLoad.Reset();
    RequestedTrack = Path;
    PendingLoad = UAssetManager::GetStreamableManager().RequestAsyncLoad(Path,
        FStreamableDelegate::CreateUObject(this, &ThisClass::StartLoadedTrack, Path));
}

void UMWUCMContinuousMusic::StartLoadedTrack(FSoftObjectPath Track)
{
    // 이전 비동기 요청이 늦게 끝나도 새로 요청한 곡을 덮어쓰지 않게 한다.
    if (Track != RequestedTrack)
    {
        return;
    }
    PendingLoad.Reset();
    UWorld* World = GetGameInstance()->GetWorld();
    USoundWave* Sound = Cast<USoundWave>(Track.ResolveObject());
    if (!IsValid(World) || World->bIsTearingDown || !Sound)
    {
        // 월드 준비 전이거나 로드가 실패하면 다음 요청에서 재시도한다. 그동안 기존 곡은 유지한다.
        RequestedTrack.Reset();
        UE_LOG(LogMWContinuousMusic, Warning, TEXT("Unable to start requested track %s"), *Track.ToString());
        return;
    }

    // 끝의 두 인수는 레벨 이동 중 재생 유지와 재생 종료 후 자동 소멸이다. 월드 종료 때 음악을 끊지 않는다.
    UAudioComponent* Next = UGameplayStatics::CreateSound2D(World, Sound, 1.f, 1.f, 0.f, nullptr, true, true);
    if (!Next)
    {
        RequestedTrack.Reset();
        return;
    }
    // 다음 곡 생성이 성공한 뒤에만 이전 곡을 줄여 로딩 중 무음 구간을 만들지 않는다.
    if (IsValid(PlayingMusic))
    {
        PlayingMusic->FadeOut(MusicFadeSeconds, 0.f);
    }
    PlayingMusic = Next;
    PlayingMusic->FadeIn(MusicFadeSeconds, RequestedVolume);
    UE_LOG(LogMWContinuousMusic, Display, TEXT("Start track=%s component=%s world=%s"),
        *Track.ToString(), *PlayingMusic->GetPathName(), *World->GetName());
}

void UMWUCMContinuousMusic::Deinitialize()
{
    // 레벨 종료가 아닌 게임 인스턴스 종료에서 로드 요청과 마지막 음악을 정리한다.
    if (PendingLoad)
    {
        PendingLoad->CancelHandle();
    }
    PendingLoad.Reset();
    if (IsValid(PlayingMusic))
    {
        PlayingMusic->Stop();
    }
    PlayingMusic = nullptr;
    RequestWorld.Reset();
    RequestedTrack.Reset();
    Super::Deinitialize();
}
