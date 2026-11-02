#include "System/GameMode/MWLobbyGameMode.h"
#include "System/MWGameInstance.h"
#include "System/GameState/MWLobbyGameState.h"
#include "System/MWPlayerState.h"
#include "Player/MWLobbyPlayerController.h"
#include "Engine/World.h"
#include "GameFramework/GameSession.h"
#include "HAL/IConsoleManager.h"
#include "LogChannel.h"
#include "UI/MWLobbyHUD.h"

AMWLobbyGameMode::AMWLobbyGameMode()
{
    GameStateClass = AMWLobbyGameState::StaticClass();
    PlayerStateClass = AMWPlayerState::StaticClass();
    PlayerControllerClass = AMWLobbyPlayerController::StaticClass();
    // 화면 생성은 HUD가 담당하고 Controller는 자기 서버 요청만 전달합니다.
    HUDClass = AMWLobbyHUD::StaticClass();
    DefaultPawnClass = nullptr;
    bUseSeamlessTravel = true;
}

void AMWLobbyGameMode::InitGameState()
{
    Super::InitGameState();
    TryInitializeRoom();
}

void AMWLobbyGameMode::StartPlay()
{
    Super::StartPlay();
    TryInitializeRoom();
}

// 서버 GameInstance가 보관한 방 설정을 로비의 복제 상태에 한 번 적용합니다.
void AMWLobbyGameMode::TryInitializeRoom()
{
    if (bRoomSettingsApplied || !GetWorld() || !GetWorld()->IsGameWorld()) return;
    auto* App = GetGameInstance<UMWGameInstance>();
    auto* GS = GetGameState<AMWLobbyGameState>();
    FMWRoomSettings Settings;
    if (!App || !GS || !GameSession || !App->GetRoomSettingsForWorld(GetWorld(), Settings)) return;
    // 테마 월드의 초기화는 GameplayGameMode로 이전했습니다.
    if (!App->IsLobbyWorld(GetWorld())) return;
    GS->ApplyRoomSettingsFromServer(Settings);
    GS->SetRoomPhaseFromServer(EMWRoomPhase::Lobby);
    GameSession->MaxPlayers = Settings.MaxPlayers;
    GameSession->MaxSpectators = 0;
    bRoomSettingsApplied = true;
    bAcceptingPlayers = true;
}

// 최초 로비 입장 때만 방 내부 ID와 호스트/Ready를 설정합니다. 테마 이동 때 재발급하지 않습니다.
void AMWLobbyGameMode::PostLogin(APlayerController* Player)
{
    Super::PostLogin(Player);
    TryInitializeRoom();
    const bool Host = GetNetMode() == NM_ListenServer && Player->IsLocalController();
    if (GetNetMode() != NM_Standalone && !Host && (!bAcceptingPlayers || !bRoomSettingsApplied ||
        !GetGameState<AMWLobbyGameState>() || GetNumPlayers() > GetGameState<AMWLobbyGameState>()->GetRoomSettings().MaxPlayers))
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

void AMWLobbyGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& Id, FString& Error)
{
    Super::PreLogin(Options, Address, Id, Error);
    if (!Error.IsEmpty()) return;
    if (!bRoomSettingsApplied || !bAcceptingPlayers)
    { Error = TEXT("방을 준비 중이거나 입장이 닫혔습니다."); return; }
    // 계정 인증과 세션 등록은 연결 모듈의 책임입니다. 여기서는 게임의 입장 규칙만 검사합니다.
}

void AMWLobbyGameMode::HandleStartingNewPlayer_Implementation(APlayerController*)
{
    // 기존처럼 자동 Pawn 생성을 막습니다. 차량 생성 기능은 이번 이전 범위에 없습니다.
}

// 클라이언트의 요청을 서버에서 검증한 뒤 해당 플레이어의 Ready만 변경합니다.
bool AMWLobbyGameMode::RequestSetReady(AMWLobbyPlayerController* PC, bool Ready, FString& Error)
{
    const auto* GS = GetGameState<AMWLobbyGameState>();
    auto* PS = PC ? PC->GetPlayerState<AMWPlayerState>() : nullptr;
    if (GetNetMode() != NM_ListenServer || !bRoomSettingsApplied || !bAcceptingPlayers || !GS ||
        GS->GetRoomPhase() != EMWRoomPhase::Lobby || !PC || PC->GetWorld() != GetWorld() ||
        !PS || PS->IsInactive() || PS->GetLobbyMemberId() < 1 || !GS->PlayerArray.Contains(PS))
    { Error = TEXT("현재 로비 참가자만 자신의 준비 상태를 변경할 수 있습니다."); return false; }
    PS->SetLobbyFlagsFromServer(PS->IsRoomHost(), Ready);
    return true;
}

bool AMWLobbyGameMode::ValidateStartRequest(const AMWLobbyPlayerController* PC, bool Starting, FString& Error) const
{
    const auto* GS = GetGameState<AMWLobbyGameState>();
    const auto* PS = PC ? PC->GetPlayerState<AMWPlayerState>() : nullptr;
    if (GetNetMode() != NM_ListenServer || !bRoomSettingsApplied || !GS ||
        GS->GetRoomPhase() != (Starting ? EMWRoomPhase::Starting : EMWRoomPhase::Lobby) ||
        !PC || PC->GetWorld() != GetWorld() || !PC->IsLocalController() || !PS || !PS->IsRoomHost() ||
        !GS->PlayerArray.Contains(PS))
    { Error = TEXT("로비에서 호스트만 경기를 시작할 수 있습니다."); return false; }
    return GS->ValidateReadyPlayers(Error);
}

// 시작 대상 ID를 고정하고 입장을 닫은 뒤 기존 연결 API의 준비 완료를 기다립니다.
bool AMWLobbyGameMode::RequestStartGame(AMWLobbyPlayerController* Host, FString& Error)
{
    if (!ValidateStartRequest(Host, false, Error)) return false;
    auto* App = GetGameInstance<UMWGameInstance>();
    if (!App) { Error = TEXT("프로젝트 GameInstance 설정을 확인하세요."); return false; }
    TArray<int32> Players;
    for (const APlayerState* Player : GetGameState<AMWLobbyGameState>()->PlayerArray)
        if (const auto* PS = Cast<AMWPlayerState>(Player)) Players.Add(PS->GetLobbyMemberId());
    Players.Sort();
    bAcceptingPlayers = false;
    GetGameState<AMWLobbyGameState>()->SetRoomPhaseFromServer(EMWRoomPhase::Starting);
    if (!App->PrepareForGameStart(Players, FMWStartPrepared::CreateUObject(this, &ThisClass::HandleStartPrepared), Error))
    { FailStart(Error); return false; }
    return true;
}

// 비동기 준비 중 참가자가 바뀌지 않았는지 다시 검사하고 승인된 테마로 이동합니다.
void AMWLobbyGameMode::HandleStartPrepared(bool Success, const FString& Message)
{
    auto* GS = GetGameState<AMWLobbyGameState>();
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
    
    // 이번 작업은 기존 기능의 이전입니다. 아직 없는 차량 선택을 추가하지 않고 기존 즉시 이동을 유지합니다.
    GS->SetRoomPhaseFromServer(EMWRoomPhase::Traveling);
    if (!GetWorld()->ServerTravel(Destination.ToString(), true)) FailStart(TEXT("테마 이동을 시작하지 못했습니다."));
}

// 시작 실패 시 반쯤 열린 방을 유지하지 않고 기존 종료 경로로 정리합니다.
void AMWLobbyGameMode::FailStart(const FString& Error)
{
    bAcceptingPlayers = false;
    if (auto* GS = GetGameState<AMWLobbyGameState>()) GS->SetRoomPhaseFromServer(EMWRoomPhase::Failed);
    UE_LOG(LogMWServer, Warning, TEXT("Start failed: %s"), *Error);
    if (auto* App = GetGameInstance<UMWGameInstance>()) App->RequestLeaveRoom();
}
