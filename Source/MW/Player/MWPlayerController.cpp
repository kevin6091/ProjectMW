#include "MWPlayerController.h"
#include "System/MWGameState.h"
#include "System/MWGameMode.h"
#include "GameMapsSettings.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

void AMWPlayerController::BeginPlay()
{
    Super::BeginPlay();
    if (!IsLocalController())
    {
        return;
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
            Old->OnLobbyChanged.RemoveDynamic(this, &AMWPlayerController::HandleLobbyChanged);
        }

        BoundGameState = GS;
        if (GS)
        {
            GS->OnLobbyChanged.AddUniqueDynamic(this, &AMWPlayerController::HandleLobbyChanged);
        }
    }

    const FName CurrentMap(*UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName()));
    if (GetWorld()->GetNetMode() != NM_Standalone && CurrentMap != FName(*UGameMapsSettings::GetGameDefaultMap()))
    {
        SetInputMode(FInputModeGameOnly());
        bShowMouseCursor = false;
    }
    OnLobbyScreenRefresh();
}

void AMWPlayerController::HandleLobbyChanged()
{
    LobbyActionError.Reset();
    RefreshLobbyScreen();
}

void AMWPlayerController::PostSeamlessTravel()
{
    Super::PostSeamlessTravel();
    RefreshLobbyScreen();
}

void AMWPlayerController::RequestSetReady(bool bReady)
{
    if (!IsLocalController()) return;
    LobbyActionError.Reset();
    ServerSetReady(bReady);
}

void AMWPlayerController::RequestStartGame()
{
    if (!IsLocalController()) return;
    LobbyActionError.Reset();
    ServerStartGame();
}

void AMWPlayerController::ServerSetReady_Implementation(bool bReady)
{
    AMWGameMode* GM = GetWorld()->GetAuthGameMode<AMWGameMode>();
    FString Error;
    if (!GM || !GM->RequestSetReady(this, bReady, Error))
        ClientLobbyActionFailed(Error.IsEmpty() ? TEXT("로비를 확인할 수 없습니다.") : Error);
}

void AMWPlayerController::ServerStartGame_Implementation()
{
    AMWGameMode* GM = GetWorld()->GetAuthGameMode<AMWGameMode>();
    FString Error;
    if (!GM || !GM->RequestStartGame(this, Error))
        ClientLobbyActionFailed(Error.IsEmpty() ? TEXT("경기를 시작할 수 없습니다.") : Error);
}

void AMWPlayerController::ClientLobbyActionFailed_Implementation(const FString& Error)
{
    LobbyActionError = Error;
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
        GS->OnLobbyChanged.RemoveDynamic(this, &AMWPlayerController::HandleLobbyChanged);
    }
    BoundGameState.Reset();
    Super::EndPlay(EndPlayReason);
}
