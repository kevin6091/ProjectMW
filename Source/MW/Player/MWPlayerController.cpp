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

    CreateLobbyScreen();
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
    OnLobbyScreenRefresh();
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
