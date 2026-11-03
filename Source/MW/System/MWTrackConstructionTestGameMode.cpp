#include "System/MWTrackConstructionTestGameMode.h"
#include "System/MWGameState.h"
#include "System/MWPlayerState.h"
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
    // 테스트 편의를 위해 새 참가자가 들어올 때 전체 선택을 다시 시작한다.
    // 정식 게임의 시작 시점 참가자 고정 규칙은 공용 컴포넌트에 그대로 둔다.
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
    // 합성 등록을 사용하는 제작 맵이 정식 접속 경로로 사용되지 않게 한다.
    ErrorMessage = TEXT("제작 전용 맵입니다.");
#endif
}

void AMWTrackConstructionTestGameMode::ScheduleSelection()
{
#if !UE_BUILD_SHIPPING
    if (!HasAuthority() || bSelectionScheduled) return;
    // 로그인 콜백 안에서 즉시 시작하지 않고 다음 Tick으로 미뤄 중복 예약을 합친다.
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
    // 열려 있는 선택을 먼저 닫아 BeginSelection의 중복 시작 방지 조건을 만족시킨다.
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
