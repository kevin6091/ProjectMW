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
	// 월드에 묶인 구독과 효과음 준비만 해제한다. 음악 재생 객체는 게임 인스턴스가 관리한다.
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

void UMWUCMWorldMusic::SetFrontendMusicActive(bool bActive)
{
	if (bFrontendMusicActive != bActive)
	{
		bFrontendMusicActive = bActive;
		RefreshMusic();
	}
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
		if (Phase == EMWRoomPhase::PreparingRace || Phase == EMWRoomPhase::Countdown ||
			Phase == EMWRoomPhase::Racing)
		{
			const FName ThemeId = Gameplay->GetRoomSettings().MapThemeId.GetTagName();
			const auto* Theme = ThemeTracks.FindByPredicate([ThemeId](const FMWUCMThemeMusic& Entry)
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
