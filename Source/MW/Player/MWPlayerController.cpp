#include "MWPlayerController.h"
#include "System/MWGameState.h"
#include "System/MultiplayerSubSystem/MWMultiplayerSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

void AMWPlayerController::BeginPlay()
{
    Super::BeginPlay();
    if (!IsLocalController())
    {
        return;
    }

    UMWMultiplayerSubsystem* Sessions = GetGameInstance()->GetSubsystem<UMWMultiplayerSubsystem>();
    BoundSubsystem = Sessions;
    
    if (Sessions)
    {
        Sessions->OnSessionStateChanged.AddUniqueDynamic(this, &AMWPlayerController::HandleSessionStateChanged);
        Sessions->OnSessionError.AddUniqueDynamic(this, &AMWPlayerController::HandleSessionError);
    }

    // 테스트용 자식 BP는 로비 생성을 생략하되 세션 이벤트·상태 연결은 동일하게 유지한다.
    bLobbyPresentationEnabled = bCreateLobbyScreen;
    if (bCreateLobbyScreen) CreateLobbyScreen();
    RefreshLobbyScreen();
}

void AMWPlayerController::RefreshLobbyScreen()
{
    if (!IsLocalController() || !HasActorBegunPlay())
    {
        return;
    }

    AMWGameState* GS = GetWorld()->GetGameState<AMWGameState>();
    if (BoundGameState.Get() != GS)
    {
        if (AMWGameState* Old = BoundGameState.Get())
        {
            Old->OnLobbyChanged.RemoveDynamic(
                this, &AMWPlayerController::HandleLobbyChanged);
        }

        BoundGameState = GS;
        if (GS)
        {
            GS->OnLobbyChanged.AddUniqueDynamic(
                this, &AMWPlayerController::HandleLobbyChanged);
        }
    }

    if (UMWMultiplayerSubsystem* Sessions = BoundSubsystem.Get())
    {
        Sessions->NotifyClientLobbyReady(this);
    }
    // 기존 로비 준비 통지는 유지한다. 참가자 목록 변화는 선택 상태 구독에도 반영한다.
    RebindConstructionSources();
    RefreshConstructionPresentation();
    if (bLobbyPresentationEnabled) OnLobbyScreenRefresh();
}

void AMWPlayerController::HandleLobbyChanged()
{
    RefreshLobbyScreen();
}

void AMWPlayerController::HandleSessionError(const FString& ErrorMessage)
{
    RefreshLobbyScreen();
}

void AMWPlayerController::HandleSessionStateChanged(
    EMWSessionState PreviousState, EMWSessionState NewState)
{
    RefreshLobbyScreen();
}

void AMWPlayerController::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();
    RefreshLobbyScreen();
}

void AMWPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 정리 중 들어오는 이벤트로 화면이 다시 만들어지지 않도록 종료 표식부터 설정한다.
    bConstructionShuttingDown = true;
    StopConstructionPresentation();
    UnbindConstructionSources();
    if (AMWGameState* GS = BoundGameState.Get())
    {
        GS->OnLobbyChanged.RemoveDynamic(
            this, &AMWPlayerController::HandleLobbyChanged);
    }
    if (UMWMultiplayerSubsystem* Sessions = BoundSubsystem.Get())
    {
        Sessions->OnSessionStateChanged.RemoveDynamic(this, &AMWPlayerController::HandleSessionStateChanged);
        Sessions->OnSessionError.RemoveDynamic(this, &AMWPlayerController::HandleSessionError);
    }

    BoundGameState.Reset();
    BoundSubsystem.Reset();
    Super::EndPlay(EndPlayReason);
}
