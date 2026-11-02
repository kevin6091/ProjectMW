#include "System/MWGameMode.h"
#include "System/MWGameInstance.h"
#include "System/MWGameState.h"
#include "System/MWPlayerState.h"
#include "Player/MWPlayerController.h"
#include "Engine/World.h"
#include "GameFramework/GameSession.h"
#include "HAL/IConsoleManager.h"
#include "LogChannel.h"

AMWGameMode::AMWGameMode()
{
    GameStateClass = AMWGameState::StaticClass();
    PlayerStateClass = AMWPlayerState::StaticClass();
    PlayerControllerClass = AMWPlayerController::StaticClass();
    DefaultPawnClass = nullptr;
    bUseSeamlessTravel = true;
}

void AMWGameMode::InitGameState()
{
    Super::InitGameState();
    TryInitializeRoom();
}

void AMWGameMode::StartPlay()
{
    Super::StartPlay();
    TryInitializeRoom();
}

void AMWGameMode::TryInitializeRoom()
{
    if (bRoomSettingsApplied || !GetWorld() || !GetWorld()->IsGameWorld()) return;
    auto* App = GetGameInstance<UMWGameInstance>();
    auto* GS = GetGameState<AMWGameState>();
    FMWRoomSettings Settings;
    if (!App || !GS || !GameSession || !App->GetRoomSettingsForWorld(GetWorld(), Settings)) return;
    GS->ApplyRoomSettingsFromServer(Settings);
    const bool Lobby = App->IsLobbyWorld(GetWorld());
    GS->SetRoomPhaseFromServer(Lobby ? EMWRoomPhase::Lobby : EMWRoomPhase::PreparingConstruction);
    GameSession->MaxPlayers = Settings.MaxPlayers;
    GameSession->MaxSpectators = 0;
    bRoomSettingsApplied = true;
    bAcceptingPlayers = Lobby;
}

void AMWGameMode::PostLogin(APlayerController* Player)
{
    Super::PostLogin(Player);
    TryInitializeRoom();
    const bool Host = GetNetMode() == NM_ListenServer && Player->IsLocalController();
    if (GetNetMode() != NM_Standalone && !Host && (!bAcceptingPlayers || !bRoomSettingsApplied ||
        !GetGameState<AMWGameState>() || GetNumPlayers() > GetGameState<AMWGameState>()->GetRoomSettings().MaxPlayers))
    {
        if (GameSession) GameSession->KickPlayer(Player, FText::FromString(TEXT("방이 닫혔거나 정원이 찼습니다.")));
        return;
    }
    if (auto* PS = Player->GetPlayerState<AMWPlayerState>())
    {
        PS->SetLobbyMemberIdFromServer(NextLobbyMemberId++);
        PS->SetLobbyFlagsFromServer(Host, false);
    }
}

void AMWGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& Id, FString& Error)
{
    Super::PreLogin(Options, Address, Id, Error);
    if (!Error.IsEmpty()) return;
    if (!bRoomSettingsApplied || !bAcceptingPlayers)
    { Error = TEXT("방을 준비 중이거나 입장이 닫혔습니다."); return; }
    // Account identity/admission belongs to the connection module. Local players need no account.
}

void AMWGameMode::HandleStartingNewPlayer_Implementation(APlayerController*)
{
    // Vehicle spawning remains a separate gameplay feature. Never auto-spawn lobby pawns.
}

bool AMWGameMode::RequestSetReady(AMWPlayerController* PC, bool Ready, FString& Error)
{
    const auto* GS = GetGameState<AMWGameState>();
    auto* PS = PC ? PC->GetPlayerState<AMWPlayerState>() : nullptr;
    if (GetNetMode() != NM_ListenServer || !bRoomSettingsApplied || !bAcceptingPlayers || !GS ||
        GS->GetRoomPhase() != EMWRoomPhase::Lobby || !PC || PC->GetWorld() != GetWorld() ||
        !PS || PS->IsInactive() || PS->GetLobbyMemberId() < 1 || !GS->PlayerArray.Contains(PS))
    { Error = TEXT("현재 로비 참가자만 자신의 준비 상태를 변경할 수 있습니다."); return false; }
    PS->SetLobbyFlagsFromServer(PS->IsRoomHost(), Ready);
    return true;
}

bool AMWGameMode::ValidateStartRequest(const AMWPlayerController* PC, bool Starting, FString& Error) const
{
    const auto* GS = GetGameState<AMWGameState>();
    const auto* PS = PC ? PC->GetPlayerState<AMWPlayerState>() : nullptr;
    if (GetNetMode() != NM_ListenServer || !bRoomSettingsApplied || !GS ||
        GS->GetRoomPhase() != (Starting ? EMWRoomPhase::Starting : EMWRoomPhase::Lobby) ||
        !PC || PC->GetWorld() != GetWorld() || !PC->IsLocalController() || !PS || !PS->IsRoomHost() ||
        !GS->PlayerArray.Contains(PS))
    { Error = TEXT("로비에서 호스트만 경기를 시작할 수 있습니다."); return false; }
    return GS->ValidateReadyPlayers(Error);
}

bool AMWGameMode::RequestStartGame(AMWPlayerController* Host, FString& Error)
{
    if (!ValidateStartRequest(Host, false, Error)) return false;
    auto* App = GetGameInstance<UMWGameInstance>();
    if (!App) { Error = TEXT("프로젝트 GameInstance 설정을 확인하세요."); return false; }
    TArray<int32> Players;
    for (const APlayerState* Player : GetGameState<AMWGameState>()->PlayerArray)
        if (const auto* PS = Cast<AMWPlayerState>(Player)) Players.Add(PS->GetLobbyMemberId());
    Players.Sort();
    bAcceptingPlayers = false;
    GetGameState<AMWGameState>()->SetRoomPhaseFromServer(EMWRoomPhase::Starting);
    if (!App->PrepareForGameStart(Players, FMWStartPrepared::CreateUObject(this, &ThisClass::HandleStartPrepared), Error))
    { FailStart(Error); return false; }
    return true;
}

void AMWGameMode::HandleStartPrepared(bool Success, const FString& Message)
{
    auto* GS = GetGameState<AMWGameState>();
    auto* App = GetGameInstance<UMWGameInstance>();
    if (!GS || !App || GS->GetRoomPhase() != EMWRoomPhase::Starting) return;
    FString Error = Message;
    TArray<int32> Players;
    for (const APlayerState* Player : GS->PlayerArray)
        if (const auto* PS = Cast<AMWPlayerState>(Player)) Players.Add(PS->GetLobbyMemberId());
    Players.Sort();
    if (!Success || Players != App->GetStartingPlayers() || !GS->ValidateReadyPlayers(Error))
    { FailStart(Error.IsEmpty() ? TEXT("시작 중 참가자 또는 준비 상태가 바뀌었습니다.") : Error); return; }
    const FName Destination = App->ResolveMapTheme(GS->GetRoomSettings().MapThemeId, Error);
    if (Destination.IsNone()) { FailStart(Error); return; }
#if WITH_EDITOR
    if (GetWorld()->WorldType == EWorldType::PIE)
    {
        const auto* Setting = IConsoleManager::Get().FindConsoleVariable(TEXT("net.AllowPIESeamlessTravel"));
        if (!Setting || Setting->GetInt() == 0) { FailStart(TEXT("PIE Seamless Travel 설정이 필요합니다.")); return; }
    }
#endif
    // Preserves the existing implemented flow. Vehicle selection is inserted here later.
    GS->SetRoomPhaseFromServer(EMWRoomPhase::Traveling);
    if (!GetWorld()->ServerTravel(Destination.ToString(), true)) FailStart(TEXT("테마 이동을 시작하지 못했습니다."));
}

void AMWGameMode::FailStart(const FString& Error)
{
    bAcceptingPlayers = false;
    if (auto* GS = GetGameState<AMWGameState>()) GS->SetRoomPhaseFromServer(EMWRoomPhase::Failed);
    UE_LOG(LogMWServer, Warning, TEXT("Start failed: %s"), *Error);
    if (auto* App = GetGameInstance<UMWGameInstance>()) App->RequestLeaveRoom();
}
