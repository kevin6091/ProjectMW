#include "MWUCMWorldMusic.h"

#include "Components/AudioComponent.h"
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
constexpr float MusicFadeSeconds = 0.35f;
constexpr float PlayerReadinessInterval = 0.15f;
}

bool UMWUCMWorldMusic::DoesSupportWorldType(EWorldType::Type Type) const
{
    return Type == EWorldType::Game || Type == EWorldType::PIE;
}

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

void UMWUCMWorldMusic::OnWorldEndPlay(UWorld& World)
{
    World.GameStateSetEvent.RemoveAll(this);
    if (auto* GameInstance = World.GetGameInstance<UMWGameInstance>())
    {
        GameInstance->OnSessionStateChanged.RemoveDynamic(this, &ThisClass::HandleSessionChanged);
    }
    BindGameState(nullptr);
    World.GetTimerManager().ClearTimer(PlayerReadinessTimer);
    StopMusic(false);
    MenuEffectsLoad.Reset();
    Super::OnWorldEndPlay(World);
}

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

void UMWUCMWorldMusic::HandleGameStateSet(AGameStateBase* State)
{
    BindGameState(State);
    RefreshMusic();
}

void UMWUCMWorldMusic::HandleSessionChanged(EMWSessionState, EMWSessionState)
{
    RefreshMusic();
}

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
        Effects.RemoveAll([](const FSoftObjectPath& Path) { return Path.IsNull(); });
        if (!Effects.IsEmpty())
        {
            MenuEffectsLoad = UAssetManager::GetStreamableManager().RequestAsyncLoad(Effects);
        }
    }

    RefreshMusic();
}

void UMWUCMWorldMusic::PlayMenuHighlight() const
{
    PlayMenuEffect(MenuHighlight.Get());
}

void UMWUCMWorldMusic::PlayMenuConfirm() const
{
    PlayMenuEffect(MenuConfirm.Get());
}

void UMWUCMWorldMusic::PlayMenuEffect(USoundWave* Sound) const
{
    if (bMainMenuVisible && Sound)
    {
        UGameplayStatics::PlaySound2D(this, Sound, MenuEffectVolume);
        UE_LOG(LogMWUCMMusic, Verbose, TEXT("Main menu effect %s"), *Sound->GetName());
    }
}

TSoftObjectPtr<USoundWave> UMWUCMWorldMusic::GetDesiredTrack() const
{
    if (bMainMenuVisible)
    {
        return MenuTrack;
    }

    if (const auto* Lobby = Cast<AMWLobbyGameState>(BoundState.Get()))
    {
        const auto* GameInstance = GetWorld()->GetGameInstance<UMWGameInstance>();
        const auto Session = GameInstance ? GameInstance->GetSessionState() : EMWSessionState::None;
        const auto Phase = Lobby->GetRoomPhase();
        if ((Session == EMWSessionState::Hosting || Session == EMWSessionState::Joined) &&
            (Phase == EMWRoomPhase::Lobby || Phase == EMWRoomPhase::Starting))
        {
            return MenuTrack;
        }
    }

    if (const auto* Gameplay = Cast<AMWGameplayGameState>(BoundState.Get()))
    {
        const auto Phase = Gameplay->GetRoomPhase();
        if (Phase == EMWRoomPhase::PreparingRace || Phase == EMWRoomPhase::Countdown ||
            Phase == EMWRoomPhase::Racing)
        {
            const FName ThemeId = Gameplay->GetRoomSettings().MapThemeId.GetTagName();
            if (const auto* Theme = ThemeTracks.FindByPredicate([ThemeId](const FMWUCMThemeMusic& Entry)
                { return Entry.MapThemeId == ThemeId; }))
            {
                return Theme->Track;
            }
        }
    }

    return {};
}

void UMWUCMWorldMusic::RefreshMusic()
{
    UWorld* World = GetWorld();
    if (World->bIsTearingDown)
    {
        return;
    }

    const FSoftObjectPath Track = GetDesiredTrack().ToSoftObjectPath();
    auto& Timers = World->GetTimerManager();
    if (Track == RequestedTrack)
    {
        Timers.ClearTimer(PlayerReadinessTimer);
        return;
    }

    StopMusic(true);
    const auto* GameInstance = World->GetGameInstance();
    const auto* LocalPlayer = GameInstance ? GameInstance->GetFirstLocalPlayerController(World) : nullptr;
    const bool bNeedsPawn = BoundState.IsValid() && BoundState->IsA<AMWGameplayGameState>();
    if (!Track.IsNull() && (!LocalPlayer || (bNeedsPawn && !LocalPlayer->GetPawn())))
    {
        // GameState replication may arrive before the local controller or possession.
        if (!Timers.IsTimerActive(PlayerReadinessTimer))
        {
            Timers.SetTimer(PlayerReadinessTimer, this, &ThisClass::RefreshMusic, PlayerReadinessInterval, true);
        }
        return;
    }

    Timers.ClearTimer(PlayerReadinessTimer);
    if (Track.IsNull())
    {
        return;
    }

    RequestedTrack = Track;
    PendingLoad = UAssetManager::GetStreamableManager().RequestAsyncLoad(
        Track, FStreamableDelegate::CreateUObject(this, &ThisClass::StartLoadedTrack, Track));
}

void UMWUCMWorldMusic::StartLoadedTrack(FSoftObjectPath Track)
{
    if (Track != RequestedTrack || GetWorld()->bIsTearingDown)
    {
        return;
    }

    if (auto* Sound = Cast<USoundWave>(Track.ResolveObject()))
    {
        PlayingMusic = UGameplayStatics::CreateSound2D(this, Sound);
        if (PlayingMusic)
        {
            PlayingMusic->FadeIn(MusicFadeSeconds, MusicVolume);
            UE_LOG(LogMWUCMMusic, Display, TEXT("Start %s in %s"), *Track.ToString(), *GetWorld()->GetName());
        }
    }
    else
    {
        UE_LOG(LogMWUCMMusic, Warning, TEXT("Local music asset unavailable: %s"), *Track.ToString());
    }
    PendingLoad.Reset();
}

void UMWUCMWorldMusic::StopMusic(bool bFade)
{
    if (PendingLoad)
    {
        PendingLoad->CancelHandle();
        PendingLoad.Reset();
    }

    if (IsValid(PlayingMusic))
    {
        UE_LOG(LogMWUCMMusic, Display, TEXT("Stop %s in %s"), *RequestedTrack.ToString(), *GetWorld()->GetName());
        if (bFade)
        {
            PlayingMusic->FadeOut(MusicFadeSeconds, 0.f);
        }
        else
        {
            PlayingMusic->Stop();
        }
    }
    PlayingMusic = nullptr;
    RequestedTrack.Reset();
}
