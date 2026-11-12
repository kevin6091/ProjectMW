#include "System/GameMode/MWLobbyGameMode.h"
#include "System/MWGameInstance.h"
#include "System/GameState/MWLobbyGameState.h"
#include "System/MWPlayerState.h"
#include "Player/MWLobbyPlayerController.h"
#include "Engine/World.h"
#include "GameFramework/GameSession.h"
#include "LogChannel.h"
#include "UI/Lobby/MWLobbyHUD.h"
#include "GameFramework/Character.h"
#include "Data/MWVehicleDefinitionData.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "MWGameplayTags.h"
#include "System/MWAssetManager.h"
#include "Player/Cursor/MWPlayerCursorComponent.h"
#include "TimerManager.h"

namespace
{
    constexpr float VehicleDepartureTimeoutSeconds = 12.f;
}

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

// 기존 참가자 이탈 처리와 함께 출발 대기가 남지 않도록 현재 시작 상태를 확인한다.
void AMWLobbyGameMode::Logout(AController* Exiting)
{
    const auto* App = GetGameInstance<UMWGameInstance>();
    const auto* Player = Exiting ? Exiting->GetPlayerState<AMWPlayerState>() : nullptr;
    const bool bStartingPlayerLeft = !PendingThemeDestination.IsNone() && App && Player &&
                                     App->GetStartingPlayers().Contains(Player->GetLobbyMemberId());

    Super::Logout(Exiting);

    if (bStartingPlayerLeft)
    {
        FailStart(TEXT("출발 중 참가자가 방을 나갔습니다."));
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

    if (!GS || !App || GS->GetRoomPhase() != EMWRoomPhase::Starting)
    {
        return;
    }

    FString Error = Message;
    TArray<int32> Players;
    // 비동기 입장 잠금 처리가 끝난 시점의 참가자 목록입니다.
    for (const APlayerState* Player : GS->PlayerArray)
    {
        if (const auto* PS = Cast<AMWPlayerState>(Player))
        {
            Players.Add(PS->GetLobbyMemberId());
        }
    }

    Players.Sort();
    // 준비 성공, 참가자 유지, 전원 Ready를 다시 확인합니다.
    if (!Success || Players != App->GetStartingPlayers() || !GS->ValidateReadyPlayers(Error))
    {
        FailStart(Error.IsEmpty() ? TEXT("시작 중 참가자 또는 준비 상태가 바뀌었습니다.") : Error);
        return;
    }

    // 준비를 마쳤으므로 같은 L_Title에서 차량 선택을 시작합니다.
    BeginVehicleSelection();
}

void AMWLobbyGameMode::BeginVehicleSelection()
{
    AMWLobbyGameState* GS = GetGameState<AMWLobbyGameState>();

    // 시작 준비를 마친 로비에서만 차량 선택을 시작합니다.
    if (!GS || GS->GetRoomPhase() != EMWRoomPhase::Starting)
    {
        return;
    }

    FString Error;

    // 모든 클라이언트가 제출할 기본 차량이 실제로 준비돼 있는지 확인합니다.
    if (!ValidateVehicleAssetLabel(MWGameplayTags::AssetLabel_Vehicles, Error))
    {
        FailStart(Error);
        return;
    }

    // 클라이언트가 요청을 보내기 전에 서버가 먼저 선택 단계에 진입합니다.
    SetVehicleCursorStreams(true);
    GS->SetRoomPhaseFromServer(EMWRoomPhase::VehicleSelection);

    UE_LOG(LogMWServer, Log, TEXT("[MWVehicleSelection] 차량 선택을 시작합니다."));

    // 각 로컬 HUD가 복제된 Phase를 보고 실제 선택 화면을 표시합니다.
}

// 확정·취소가 같은 단계·월드·소유권·고정 시작 참가자 검사를 공유한다.
AMWPlayerState* AMWLobbyGameMode::ValidateVehicleSelectionRequester(AMWLobbyPlayerController* Controller,
                                                                    FString& Error) const
{
    Error.Reset();

    auto* GS = GetGameState<AMWLobbyGameState>();
    auto* App = GetGameInstance<UMWGameInstance>();

    if (GetNetMode() != NM_ListenServer ||
        !GS ||
        !App ||
        GS->GetRoomPhase() != EMWRoomPhase::VehicleSelection)
    {
        Error = TEXT("현재는 차량 선택 요청을 처리할 수 없습니다.");
        return nullptr;
    }

    auto* PS = IsValid(Controller) ? Controller->GetPlayerState<AMWPlayerState>() : nullptr;

    if (!IsValid(Controller) ||
        Controller->GetWorld() != GetWorld() ||
        !IsValid(PS) ||
        PS->IsInactive() ||
        PS->GetOwner() != Controller ||
        !GS->PlayerArray.Contains(PS) ||
        !App->GetStartingPlayers().Contains(PS->GetLobbyMemberId()))
    {
        Error = TEXT("시작 대상으로 확정된 참가자만 차량 선택을 변경할 수 있습니다.");
        return nullptr;
    }

    return PS;
}

// 공통 요청자 검사 뒤 유효한 차량 ID만 서버의 기존 확정 경로로 전달한다.
bool AMWLobbyGameMode::RequestConfirmVehicle(AMWLobbyPlayerController* Controller, FGameplayTag VehicleAssetID,
                                             FString& Error)
{
    auto* PS = ValidateVehicleSelectionRequester(Controller, Error);
    if (!PS)
        return false;

    const FGameplayTag PreviousID = PS->GetVehicleAssetID();

    if (PreviousID.IsValid())
    {
        // 같은 ID의 중복 요청은 결과 변경이나 완료 처리를 반복하지 않습니다.
        if (PreviousID == VehicleAssetID)
        {
            return true;
        }

        Error = TEXT("다른 차량을 선택하려면 현재 선택을 먼저 취소하세요.");
        return false;
    }

    if (!ValidateVehicleAssetID(VehicleAssetID, Error))
    {
        return false;
    }

    if (!PS->ConfirmVehicleAssetIDFromServer(VehicleAssetID))
    {
        Error = TEXT("PlayerState에 차량 선택 결과를 저장하지 못했습니다.");
        return false;
    }

    UE_LOG(LogMWServer, Log, TEXT("[MWVehicleSelection] ID=%d 차량 확정: %s"),
        PS->GetLobbyMemberId(), *VehicleAssetID.ToString());

    TryCompleteVehicleSelection();
    return true;
}

// 예상 ID와 현재 선택이 일치한 서버 승인만 기존 차량 ID 해제로 연결한다.
bool AMWLobbyGameMode::RequestCancelVehicle(AMWLobbyPlayerController* Controller, FGameplayTag ExpectedVehicleID,
                                            FString& Error)
{
    auto* PS = ValidateVehicleSelectionRequester(Controller, Error);
    if (!PS)
        return false;

    if (!ExpectedVehicleID.IsValid() || !PS->ClearVehicleAssetIDFromServer(ExpectedVehicleID))
    {
        Error = TEXT("취소하려던 차량 선택이 바뀌었습니다.");
        return false;
    }

    return true;
}

void AMWLobbyGameMode::TryCompleteVehicleSelection()
{
    if (!IsAllVehiclesConfirmed())
    {
        return;
    }

    UE_LOG(LogMWServer, Log, TEXT("[MWVehicleSelection] 시작 대상 전원의 차량 선택이 확정되었습니다."));

    auto* GS = GetGameState<AMWLobbyGameState>();
    const auto* App = GetGameInstance<UMWGameInstance>();

    // 방 생성 때 확정한 테마 ID로 실제 이동할 레벨을 찾습니다.
    FString Error;
    const FName Destination = App->ResolveMapTheme(GS->GetRoomSettings().MapThemeId, Error);

    if (Destination.IsNone())
    {
        FailStart(Error);
        return;
    }

    PendingThemeDestination = Destination;
    for (int32 PlayerId : App->GetStartingPlayers())
    {
        PlayersAwaitingDeparture.Add(PlayerId);
    }
    GetWorldTimerManager().SetTimer(DepartureTimeout, this, &ThisClass::HandleVehicleDepartureTimeout,
                                    VehicleDepartureTimeoutSeconds, false);

    // Traveling 알림은 호스트 HUD에서도 즉시 실행되므로 대기 상태를 먼저 준비합니다.
    UE_LOG(LogMWServer, Log, TEXT("[MWTravel] Waiting for vehicle departure. Players=%d Destination=%s"),
           PlayersAwaitingDeparture.Num(), *Destination.ToString());
    SetVehicleCursorStreams(false);
    GS->SetRoomPhaseFromServer(EMWRoomPhase::Traveling);
}

// 소유자·단계·중복을 검사해 대기자를 제거하고 전원이 끝났을 때만 서버 이동을 시작한다.
void AMWLobbyGameMode::HandleVehicleDepartureReady(AMWLobbyPlayerController* Controller)
{
    const auto* GS = GetGameState<AMWLobbyGameState>();
    const auto* Player = IsValid(Controller) ? Controller->GetPlayerState<AMWPlayerState>() : nullptr;
    if (PendingThemeDestination.IsNone() || !GS || GS->GetRoomPhase() != EMWRoomPhase::Traveling || !IsValid(Player) ||
        Controller->GetWorld() != GetWorld() || Player->IsInactive() || Player->GetOwner() != Controller ||
        !GS->PlayerArray.Contains(Player))
    {
        return;
    }

    if (PlayersAwaitingDeparture.Remove(Player->GetLobbyMemberId()) == 0)
    {
        return;
    }

    UE_LOG(LogMWServer, Log, TEXT("[MWTravel] Vehicle departure ready. Player=%d Remaining=%d"),
           Player->GetLobbyMemberId(), PlayersAwaitingDeparture.Num());
    if (!PlayersAwaitingDeparture.IsEmpty())
    {
        return;
    }

    GetWorldTimerManager().ClearTimer(DepartureTimeout);
    const FString Destination = PendingThemeDestination.ToString();
    PendingThemeDestination = NAME_None;
    UE_LOG(LogMWServer, Log, TEXT("[MWTravel] Vehicle departure complete. ServerTravel Destination=%s"), *Destination);
    if (!GetWorld()->ServerTravel(Destination, true))
    {
        FailStart(TEXT("테마 레벨 이동을 시작하지 못했습니다."));
    }
}

// 완료 누락을 시작 실패로 처리한다. 준비 검사를 건너뛰고 강제로 이동하지 않는다.
void AMWLobbyGameMode::HandleVehicleDepartureTimeout()
{
    UE_LOG(LogMWServer, Warning, TEXT("[MWTravel] Vehicle departure timed out. Remaining=%d"),
           PlayersAwaitingDeparture.Num());
    FailStart(TEXT("일부 참가자의 출발 연출이 끝나지 않았습니다."));
}

bool AMWLobbyGameMode::IsAllVehiclesConfirmed() const
{
    const auto* GS = GetGameState<AMWLobbyGameState>();
    const auto* App = GetGameInstance<UMWGameInstance>();

    if (!GS || !App || GS->GetRoomPhase() != EMWRoomPhase::VehicleSelection)
    {
        return false;
    }

    const TArray<int32>& StartingPlayers = App->GetStartingPlayers();

    // 시작 대상이 없는 상태를 전원 확정으로 판단하지 않습니다.
    if (StartingPlayers.IsEmpty())
    {
        return false;
    }

    TArray<int32> CurrentPlayers;

    for (const APlayerState* Player : GS->PlayerArray)
    {
        const auto* PS = Cast<AMWPlayerState>(Player);

        // 정상 참가자이며 차량 ID가 확정돼 있어야 합니다.
        if (!IsValid(PS) || PS->IsInactive() || !PS->GetVehicleAssetID().IsValid())
        {
            return false;
        }

        CurrentPlayers.Add(PS->GetLobbyMemberId());
    }

    // PlayerArray 순서는 참가자 동일 여부의 기준으로 사용하지 않습니다.
    CurrentPlayers.Sort();

    // 시작 때 고정한 ID 목록과 현재 ID 목록이 같아야 합니다.
    return CurrentPlayers == StartingPlayers;
}

bool AMWLobbyGameMode::ValidateVehicleAssetLabel(FGameplayTag AssetLabelVehicles, FString& Error) const
{
    Error.Reset();
    
    if(!UMWAssetManager::Get().LoadAssetsByLabelSynchronous(AssetLabelVehicles))
    {
        Error = TEXT("모든 차량을 불러오지 못했습니다.");
        return false;
    }

    return true;
}

bool AMWLobbyGameMode::ValidateVehicleAssetID(FGameplayTag VehicleAssetID, FString& Error) const
{
    Error.Reset();
    const FGameplayTag Root = FGameplayTag::RequestGameplayTag(FName(TEXT("Asset.Vehicle")), false);
    if (!Root.IsValid() || VehicleAssetID == Root || !VehicleAssetID.MatchesTag(Root))
    {
        Error = TEXT("선택할 수 없는 차량 ID입니다.");
        return false;
    }

    // AssetData에 등록된 ID를 통해 차량 Definition을 불러옵니다.
    const UMWVehicleDefinitionData* Definition = UMWAssetManager::Get().LoadAssetSynchronous<UMWVehicleDefinitionData>(VehicleAssetID);

    if (!Definition)
    {
        Error = TEXT("차량 Definition을 불러오지 못했습니다. \nAssetData의 차량 ID와 에셋 연결을 확인하세요.");
        return false;
    }

    const UClass* PawnClass = Definition->GetPawnClass().LoadSynchronous();
    TArray<FText> Errors;
    if (!PawnClass || PawnClass->HasAnyClassFlags(CLASS_Abstract) ||
        !Definition->ValidateDefinition(Errors))
    {
        Error = TEXT("이 차량의 주행 설정을 불러오지 못했습니다. 다른 차량을 선택하세요.");
        return false;
    }
    return true;
}

// 시작 실패 시 반쯤 열린 방을 유지하지 않고 기존 종료 경로로 정리합니다.
void AMWLobbyGameMode::FailStart(const FString& Error)
{
    GetWorldTimerManager().ClearTimer(DepartureTimeout);
    PendingThemeDestination = NAME_None;
    PlayersAwaitingDeparture.Reset();
    SetVehicleCursorStreams(false);
    bAcceptingPlayers = false;
    if (auto* GS = GetGameState<AMWLobbyGameState>()) GS->SetRoomPhaseFromServer(EMWRoomPhase::Failed);
    UE_LOG(LogMWServer, Warning, TEXT("Start failed: %s"), *Error);
    if (auto* App = GetGameInstance<UMWGameInstance>()) App->RequestLeaveRoom();
}

void AMWLobbyGameMode::SetVehicleCursorStreams(bool bOpen)
{
    const auto* GS = GetGameState<AMWLobbyGameState>();
    const auto* App = GetGameInstance<UMWGameInstance>();
    if (!GS) return;
    for (APlayerState* Base : GS->PlayerArray)
    {
        auto* PS = Cast<AMWPlayerState>(Base);
        if (!PS || !PS->GetCursorComponent()) continue;
        if (bOpen && App && !PS->IsInactive() && App->GetStartingPlayers().Contains(PS->GetLobbyMemberId()))
            PS->GetCursorComponent()->OpenStreamForServer(MWCursorContexts::VehicleSelection, EMWCursorSpace::NormalizedWidget);
        else
            PS->GetCursorComponent()->CloseStreamForServer();
    }
}
