#include "System/MWTrackConstructionTestGameMode.h"
#include "System/MWGameState.h"
#include "System/MWPlayerState.h"
#include "System/MWAssetManager.h"
#include "Data/MWAssetData.h"
#include "Player/MWPlayerController.h"
#include "Gameplay/Construction/MWTrackConstructionComponent.h"
#include "MWGameplayTags.h"
#include "Engine/World.h"
#include "TimerManager.h"

AMWTrackConstructionTestGameMode::AMWTrackConstructionTestGameMode()
{
    GameStateClass = AMWGameState::StaticClass();
    PlayerStateClass = AMWPlayerState::StaticClass();
    PlayerControllerClass = AMWPlayerController::StaticClass();
    DefaultPawnClass = nullptr;
    TestDefinitionIds = { MWGameplayTags::Asset_Track_Straight,
        MWGameplayTags::Asset_Track_Curve, MWGameplayTags::Asset_Track_Ramp };
}

void AMWTrackConstructionTestGameMode::InitGameState()
{
    Super::InitGameState();
#if !UE_BUILD_SHIPPING
    if (TestCatalog)
    {
        TArray<FText> Errors;
        if (!UMWAssetManager::Get().InitializeCatalog(TestCatalog, Errors))
            for (const FText& Error : Errors) UE_LOG(LogTemp, Error, TEXT("%s"), *Error.ToString());
    }
#endif
}

void AMWTrackConstructionTestGameMode::StartPlay()
{
    Super::StartPlay();
    ScheduleSelection();
}

void AMWTrackConstructionTestGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
#if !UE_BUILD_SHIPPING
    if (AMWPlayerState* PS = NewPlayer->GetPlayerState<AMWPlayerState>())
    {
        const int32 Id = NextTestPlayerId++;
        PS->SetPlayerName(FString::Printf(TEXT("플레이어 %d"), Id));
        PS->SetLobbyMemberIdFromServer(Id);
        PS->SetLobbyFlagsFromServer(NewPlayer->IsLocalController(), false);
        // 전용 제작 맵에서만 합성 참가자로 취급한다. 실제 세션 등록은 하지 않는다.
        PS->SetSessionRegistrationConfirmedFromServer(true);
    }
    ScheduleSelection();
#endif
}

void AMWTrackConstructionTestGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    // 선택 전용 맵은 차량과 SpectatorPawn을 생성하지 않는다.
}

void AMWTrackConstructionTestGameMode::PreLogin(const FString& Options, const FString& Address,
    const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
#if UE_BUILD_SHIPPING
    ErrorMessage = TEXT("제작 전용 맵입니다.");
#endif
}

void AMWTrackConstructionTestGameMode::ScheduleSelection()
{
#if !UE_BUILD_SHIPPING
    if (!HasAuthority() || bSelectionScheduled) return;
    bSelectionScheduled = true;
    SelectionStartTimer = GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::RestartSelection);
#endif
}

void AMWTrackConstructionTestGameMode::RestartSelection()
{
    bSelectionScheduled = false;
#if !UE_BUILD_SHIPPING
    AMWGameState* GS = GetWorld()->GetGameState<AMWGameState>();
    if (!HasAuthority() || !GS || !GS->GetTrackConstructionComponent()) return;
    UMWTrackConstructionComponent* Construction = GS->GetTrackConstructionComponent();
    Construction->EndSelection();
    FText Error;
    if (!Construction->BeginSelection(TestDefinitionIds, Error))
        UE_LOG(LogTemp, Error, TEXT("%s"), *Error.ToString());
#endif
}

void AMWTrackConstructionTestGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorldTimerManager().ClearTimer(SelectionStartTimer);
    Super::EndPlay(Reason);
}
