// 현재 월드의 메뉴·로비·경기 상태를 읽어 필요한 음악과 효과음만 선택한다.

#include "MWUCMWorldMusic.h"
#include "MWUCMContinuousMusic.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundWave.h"
#include "System/GameState/MWGameplayGameState.h"
#include "System/GameState/MWLobbyGameState.h"
#include "System/MWGameInstance.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogMWUCMMusic, Log, All);

namespace
{
    constexpr float PlayerReadinessInterval = 0.15f;
}

// 일반 게임과 PIE 월드만 대상으로 해 에디터 미리보기에서 런타임 기능이 켜지지 않게 한다.
bool UMWUCMWorldMusic::DoesSupportWorldType(EWorldType::Type Type) const
{
    return Type == EWorldType::Game || Type == EWorldType::PIE;
}

// 현재 연결 상태와 GameState 알림을 구독해 로컬 음악 선택을 시작한다.
void UMWUCMWorldMusic::OnWorldBeginPlay(UWorld& World)
{
    Super::OnWorldBeginPlay(World);
    if (World.GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    World.GameStateSetEvent.AddUObject(this, &ThisClass::HandleGameStateSet);
    if (auto* GameInstance = World.GetGameInstance<UMWGameInstance>())
    {
        GameInstance->OnSessionStateChanged.AddUniqueDynamic(this, &ThisClass::HandleSessionChanged);
    }
    HandleGameStateSet(World.GetGameState());
}

// 이 월드의 준비 타이머와 상태 구독만 정리하고 지속 음악은 게임 인스턴스에 남긴다.
void UMWUCMWorldMusic::OnWorldEndPlay(UWorld& World)
{
    World.GameStateSetEvent.RemoveAll(this);
    if (auto* GameInstance = World.GetGameInstance<UMWGameInstance>())
    {
        GameInstance->OnSessionStateChanged.RemoveDynamic(this, &ThisClass::HandleSessionChanged);
    }
    BindGameState(nullptr);
    World.GetTimerManager().ClearTimer(PlayerReadinessTimer);
    // 월드에 묶인 구독과 효과음 준비만 해제한다. 음악 재생 객체는 게임 인스턴스가 관리한다.
    MenuEffectsLoad.Reset();
    Super::OnWorldEndPlay(World);
}

// 이전 상태 구독을 끊은 뒤 실제 현재 로비·경기 GameState에 연결한다.
void UMWUCMWorldMusic::BindGameState(AGameStateBase* State)
{
    if (auto* Lobby = Cast<AMWLobbyGameState>(BoundState.Get()))
    {
        Lobby->OnLobbyChanged.RemoveDynamic(this, &ThisClass::RefreshMusic);
    }
    if (auto* Gameplay = Cast<AMWGameplayGameState>(BoundState.Get()))
    {
        Gameplay->OnGameplayStateChanged.RemoveDynamic(this, &ThisClass::RefreshMusic);
    }

    BoundState = State;
    if (auto* Lobby = Cast<AMWLobbyGameState>(State))
    {
        Lobby->OnLobbyChanged.AddUniqueDynamic(this, &ThisClass::RefreshMusic);
    }
    if (auto* Gameplay = Cast<AMWGameplayGameState>(State))
    {
        Gameplay->OnGameplayStateChanged.AddUniqueDynamic(this, &ThisClass::RefreshMusic);
    }
}

// 늦게 준비된 GameState도 구독해 초기 음악 선택을 놓치지 않는다.
void UMWUCMWorldMusic::HandleGameStateSet(AGameStateBase* State)
{
    BindGameState(State);
    RefreshMusic();
}

// 기존 연결 상태 알림을 받아 현재 화면에 맞는 음악만 다시 평가한다.
void UMWUCMWorldMusic::HandleSessionChanged(EMWSessionState, EMWSessionState)
{
    RefreshMusic();
}

// 메뉴가 실제 보이는지 기록해 선택 효과음이 다른 화면에서 나지 않게 한다.
void UMWUCMWorldMusic::SetMainMenuVisible(bool bVisible)
{
    if (bMainMenuVisible == bVisible)
    {
        return;
    }

    bMainMenuVisible = bVisible;
    if (bVisible && !MenuEffectsLoad)
    {
        TArray<FSoftObjectPath> Effects = {MenuHighlight.ToSoftObjectPath(), MenuConfirm.ToSoftObjectPath()};
        Effects.RemoveAll(
            [](const FSoftObjectPath& Path)
            {
                return Path.IsNull();
            });
        if (!Effects.IsEmpty())
        {
            MenuEffectsLoad = UAssetManager::GetStreamableManager().RequestAsyncLoad(Effects);
        }
    }

    RefreshMusic();
}

// 생성·검색 창에서도 같은 프런트엔드 음악이 이어지도록 메뉴 가시성과 분리한다.
void UMWUCMWorldMusic::SetFrontendMusicActive(bool bActive)
{
    if (bFrontendMusicActive != bActive)
    {
        bFrontendMusicActive = bActive;
        RefreshMusic();
    }
}

// 현재 메인 메뉴의 선택 이동 효과음을 공통 표시 조건으로 재생한다.
void UMWUCMWorldMusic::PlayMenuHighlight() const
{
    PlayMenuEffect(MenuHighlight.Get());
}

// 현재 메인 메뉴의 선택 확정 효과음을 공통 표시 조건으로 재생한다.
void UMWUCMWorldMusic::PlayMenuConfirm() const
{
    PlayMenuEffect(MenuConfirm.Get());
}

// 메인 메뉴가 실제 표시되는 로컬 화면에서만 설정된 효과음을 재생한다.
void UMWUCMWorldMusic::PlayMenuEffect(USoundWave* Sound) const
{
    if (bMainMenuVisible && Sound)
    {
        UGameplayStatics::PlaySound2D(this, Sound, MenuEffectVolume);
        UE_LOG(LogMWUCMMusic, Verbose, TEXT("Main menu effect %s"), *Sound->GetName());
    }
}

// 기존 경기 단계와 테마로 필요한 곡을 고른다. 게임 진행 상태를 직접 변경하지 않는다.
TSoftObjectPtr<USoundWave> UMWUCMWorldMusic::GetDesiredTrack() const
{
    if (bFrontendMusicActive)
    {
        return MenuTrack;
    }

    if (const auto* Lobby = Cast<AMWLobbyGameState>(BoundState.Get()))
    {
        const auto* GameInstance = GetWorld()->GetGameInstance<UMWGameInstance>();
        const auto Session = GameInstance ? GameInstance->GetSessionState() : EMWSessionState::None;
        const auto Phase = Lobby->GetRoomPhase();
        if ((Session == EMWSessionState::Hosting || Session == EMWSessionState::Joined) &&
            (Phase == EMWRoomPhase::Lobby || Phase == EMWRoomPhase::Starting ||
             Phase == EMWRoomPhase::VehicleSelection))
        {
            return MenuTrack;
        }
    }

    if (const auto* Gameplay = Cast<AMWGameplayGameState>(BoundState.Get()))
    {
        const auto Phase = Gameplay->GetRoomPhase();
        if (Phase == EMWRoomPhase::PreparingRace || Phase == EMWRoomPhase::Countdown || Phase == EMWRoomPhase::Racing)
        {
            const FName ThemeId = Gameplay->GetRoomSettings().MapThemeId.GetTagName();
            const auto* Theme = ThemeTracks.FindByPredicate(
                [ThemeId](const FMWUCMThemeMusic& Entry)
                {
                    return Entry.MapThemeId == ThemeId;
                });
            if (Theme)
            {
                return Theme->Track;
            }
        }
    }

    return {};
}

// 로컬 표시 준비와 곡 설정을 확인한 뒤 게임 인스턴스의 지속 음악 서비스에 요청한다.
void UMWUCMWorldMusic::RefreshMusic()
{
    UWorld* World = GetWorld();
    if (World->bIsTearingDown)
    {
        return;
    }

    const FSoftObjectPath Track = GetDesiredTrack().ToSoftObjectPath();
    auto& Timers = World->GetTimerManager();
    const auto* GameInstance = World->GetGameInstance();
    const auto* LocalPlayer = GameInstance ? GameInstance->GetFirstLocalPlayerController(World) : nullptr;
    const bool bNeedsPawn = BoundState.IsValid() && BoundState->IsA<AMWGameplayGameState>();
    if (!Track.IsNull() && (!LocalPlayer || (bNeedsPawn && !LocalPlayer->GetPawn())))
    {
        // 경기 상태가 먼저 복제될 수 있으므로 로컬 컨트롤러와 차량 소유가 준비된 뒤 곡을 요청한다.
        if (!Timers.IsTimerActive(PlayerReadinessTimer))
        {
            Timers.SetTimer(PlayerReadinessTimer, this, &ThisClass::RefreshMusic, PlayerReadinessInterval, true);
        }
        return;
    }

    Timers.ClearTimer(PlayerReadinessTimer);
    if (Track.IsNull())
    {
        // 이동 중 아직 다음 곡을 결정할 수 없다는 뜻이다. 기존 음악의 중지 요청으로 취급하지 않는다.
        return;
    }

    if (auto* Playback = GetWorld()->GetGameInstance()->GetSubsystem<UMWUCMContinuousMusic>())
    {
        Playback->RequestTrack(World, TSoftObjectPtr<USoundWave>(Track), MusicVolume);
    }
}
