#include "System/MWGameInstance.h"
#include "MWConnectionSubsystem.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/World.h"
#include "GameMapsSettings.h"
#include "Misc/PackageName.h"
#include "System/MWAssetManager.h"
#include "System/GameMode/MWLobbyGameMode.h"
#include "System/GameState/MWGameplayGameState.h"
#include "TimerManager.h"

namespace
{
    FName MapPackage(const UWorld* W)
    { return W ? FName(*UWorld::RemovePIEPrefix(W->GetOutermost()->GetName())) : NAME_None; }
}

void UMWGameInstance::Init()
{
    Super::Init();
    auto* Service = GetSubsystem<UMWConnectionSubsystem>();
    Connection = Service;
    if (Service)
    {
        SnapshotHandle = Service->OnSnapshotChanged().AddUObject(this, &ThisClass::HandleSnapshot);
        OperationHandle = Service->OnOperationCompleted().AddUObject(this, &ThisClass::HandleOperation);
        WorldHandle = Service->OnLocalWorldReady().AddUObject(this, &ThisClass::HandleWorld);
    }
}

void UMWGameInstance::Shutdown()
{
    StartCompletion.Unbind();
    if (auto* Service = Connection.Get())
    {
        Service->OnSnapshotChanged().Remove(SnapshotHandle);
        Service->OnOperationCompleted().Remove(OperationHandle);
        Service->OnLocalWorldReady().Remove(WorldHandle);
    }
    Connection.Reset();
    Super::Shutdown();
}

bool UMWGameInstance::IsLoggedIn() const { return Connection.IsValid() && Connection->GetSnapshot().bLoggedIn; }
bool UMWGameInstance::IsLoginRequired() const { return !Connection.IsValid() || !Connection->GetSnapshot().bLocalIP; }
bool UMWGameInstance::IsLoginInProgress() const { return Connection.IsValid() && Connection->GetSnapshot().ActiveAction == EMWConnectionAction::Login; }
bool UMWGameInstance::CanHostRoom() const { return Connection.IsValid() && Connection->CanExecute(EMWConnectionAction::HostRoom); }
bool UMWGameInstance::CanBrowseRooms() const { return Connection.IsValid() && Connection->CanExecute(EMWConnectionAction::RefreshRooms); }

EMWSessionState UMWGameInstance::GetSessionState() const
{
    if (!Connection.IsValid()) return EMWSessionState::None;
    const auto Snapshot = Connection->GetSnapshot();
    switch (Snapshot.State)
    {
    case EMWConnectionState::Creating: return EMWSessionState::Creating;
    case EMWConnectionState::Searching: return EMWSessionState::Searching;
    case EMWConnectionState::Joining: return EMWSessionState::Joining;
    case EMWConnectionState::Hosting: return EMWSessionState::Hosting;
    case EMWConnectionState::Joined: return EMWSessionState::Joined;
    case EMWConnectionState::Closing: return EMWSessionState::Leaving;
    case EMWConnectionState::Unavailable: return EMWSessionState::CleanupRequired;
    default: break;
    }
    // Engine-created LocalIP PIE connections do not need an online session record.
    if (Snapshot.bLocalIP && GetWorld())
    {
        if (GetWorld()->GetNetMode() == NM_ListenServer) return EMWSessionState::Hosting;
        if (GetWorld()->GetNetMode() == NM_Client) return EMWSessionState::Joined;
    }
    return EMWSessionState::None;
}

bool UMWGameInstance::IsSessionBusy() const
{
    const auto State = GetSessionState();
    return State == EMWSessionState::Creating || State == EMWSessionState::Searching || State == EMWSessionState::Joining || State == EMWSessionState::Leaving;
}

FString UMWGameInstance::GetLastSessionError() const
{
    if (!LastError.IsEmpty()) return LastError;
    return Connection.IsValid() ? Connection->GetSnapshot().ErrorMessage.ToString() : TEXT("연결 서비스를 확인하세요.");
}

void UMWGameInstance::ReportError(const FString& Error)
{
    LastError = Error;
    OnSessionError.Broadcast(Error);
}

void UMWGameInstance::RequestLogin()
{
    LastError.Reset();
    FText Error;
    if (!Connection.IsValid() || !Connection->RequestLogin(Error))
    {
        ReportError(Error.IsEmpty() ? TEXT("로그인을 시작할 수 없습니다.") : Error.ToString());
        OnLoginFinished.Broadcast(false, LastError);
    }
}

FName UMWGameInstance::ResolveMapTheme(FGameplayTag Theme, FString& Error) const
{
    Error.Reset();
    const auto Root = FGameplayTag::RequestGameplayTag(FName(TEXT("Asset.MapTheme")), false);
    if (!Root.IsValid() || !Theme.IsValid() || !Theme.MatchesTag(Root) || Theme == Root)
    { Error = TEXT("맵 테마 ID가 올바르지 않습니다."); return NAME_None; }
    const auto& Assets = UMWAssetManager::Get();
    const FSoftObjectPath Path = Assets.FindAssetPath(Theme);
    FAssetData Data;
    if (!Path.IsValid() || !Assets.GetAssetDataForPath(Path, Data) || !Data.IsInstanceOf<UWorld>())
    { Error = TEXT("카탈로그의 맵 테마가 유효한 레벨을 가리키지 않습니다."); return NAME_None; }
    const FString Package = Path.GetLongPackageName();
    if (!FPackageName::DoesPackageExist(Package))
    { Error = TEXT("맵 테마의 레벨 파일이 없습니다."); return NAME_None; }
    return FName(*Package);
}

bool UMWGameInstance::ValidateRoomSettings(const FMWRoomSettings& Input, FMWRoomSettings& Validated, FString& Error) const
{
    Validated = Input;
    Validated.RoomName.TrimStartAndEndInline();
    Error.Reset();
    if (Validated.RoomName.IsEmpty() || Validated.RoomName.Len() > 32 || Validated.RoomName.Contains(TEXT("\n")) || Validated.RoomName.Contains(TEXT("\r")))
    { Error = TEXT("방 이름은 줄바꿈 없이 1~32자로 입력하세요."); return false; }
    if (Validated.MaxPlayers < 2 || Validated.MaxPlayers > 4)
    { Error = TEXT("방 정원은 호스트를 포함해 2~4명입니다."); return false; }
    return !ResolveMapTheme(Validated.MapThemeId, Error).IsNone();
}

TArray<FName> UMWGameInstance::GetAvailableMapThemeIds() const
{
    TArray<FName> Result;
    const auto Root = FGameplayTag::RequestGameplayTag(FName(TEXT("Asset.MapTheme")), false);
    for (const auto Id : UMWAssetManager::Get().GetAssetIdsUnderRoot(Root))
    {
        FString Error;
        if (!ResolveMapTheme(Id, Error).IsNone()) Result.Add(Id.GetTagName());
    }
    return Result;
}

void UMWGameInstance::RequestCreateRoom(const FMWRoomSettings& Settings)
{
    FMWRoomSettings Approved;
    FString Error;
    if (!ValidateRoomSettings(Settings, Approved, Error))
    { ReportError(Error); OnCreateRoomFinished.Broadcast(false, Error); return; }
    const auto Previous = CurrentRoomSettings;
    CurrentRoomSettings = Approved;
    StartingPlayers.Reset();
    LastError.Reset();
    FMWConnectionRoomOptions Options;
    Options.RoomName = Approved.RoomName;
    Options.MaxPlayers = Approved.MaxPlayers;
    Options.Properties.Add(TEXT("MapTheme"), Approved.MapThemeId.ToString());
    FText Failure;
    if (!Connection.IsValid() || !Connection->RequestHostRoom(Options, Failure))
    {
        CurrentRoomSettings = Previous;
        ReportError(Failure.IsEmpty() ? TEXT("방 생성을 시작할 수 없습니다.") : Failure.ToString());
        OnCreateRoomFinished.Broadcast(false, LastError);
    }
}

void UMWGameInstance::RequestRefreshRooms()
{
    LastError.Reset();
    FText Error;
    if (!Connection.IsValid() || !Connection->RequestRefreshRooms(Error))
    { ReportError(Error.ToString()); OnRoomsUpdated.Broadcast(false, LastError); return; }
    ++RoomListRevision;
}

TArray<FMWRoomInfo> UMWGameInstance::GetAvailableRooms() const
{
    TArray<FMWRoomInfo> Result;
    if (!Connection.IsValid()) return Result;
    for (const auto& Found : Connection->GetSnapshot().Rooms)
    {
        FMWRoomInfo Room;
        Room.SessionId = Found.RoomHandle;
        Room.RoomName = Found.RoomName;
        Room.CurrentPlayers = Found.CurrentPlayers;
        Room.MaxPlayers = Found.MaxPlayers;
        Room.MapThemeId = FName(*Found.Properties.FindRef(TEXT("MapTheme")));
        Room.bCanJoin = Found.bJoinable;
        Room.UnavailableReason = Found.UnavailableReason;
        FString Error;
        const auto Tag = FGameplayTag::RequestGameplayTag(Room.MapThemeId, false);
        if (ResolveMapTheme(Tag, Error).IsNone())
        { Room.bCanJoin = false; Room.UnavailableReason = TEXT("이 게임에서 지원하지 않는 테마입니다."); }
        Result.Add(Room);
    }
    return Result;
}

void UMWGameInstance::RequestJoinRoom(const FString& Handle, int32 Revision)
{
    if (Revision != RoomListRevision) { ReportError(TEXT("방 목록을 새로고침하고 다시 선택하세요.")); return; }
    const auto Rooms = GetAvailableRooms();
    if (!Rooms.ContainsByPredicate([&](const auto& R) { return R.SessionId == Handle && R.bCanJoin; }))
    { ReportError(TEXT("참가 가능한 방을 선택하세요.")); return; }
    LastError.Reset();
    FText Error;
    if (!Connection.IsValid() || !Connection->RequestJoinRoom(Handle, Error))
    { ReportError(Error.ToString()); OnJoinRoomFinished.Broadcast(false, LastError); }
}

void UMWGameInstance::RequestJoinAddress(const FString& Address)
{
    LastError.Reset();
    FText Error;
    if (!Connection.IsValid() || !Connection->RequestJoinAddress(Address, Error))
    { ReportError(Error.ToString()); OnJoinRoomFinished.Broadcast(false, LastError); }
}

void UMWGameInstance::RequestLeaveRoom()
{
    StartCompletion.Unbind();
    if (auto* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMWLobbyGameMode>() : nullptr) GM->SetLobbyAdmission(false);
    FText Error;
    if (!Connection.IsValid() || !Connection->RequestLeaveRoom(Error)) ReportError(Error.ToString());
}

void UMWGameInstance::RetrySessionCleanup() { RequestLeaveRoom(); }

bool UMWGameInstance::IsLobbyWorld(const UWorld* W) const
{
    return W && W->GetGameInstance() == this && MapPackage(W) == FName(*UGameMapsSettings::GetGameDefaultMap());
}

bool UMWGameInstance::IsGameplayWorld(const UWorld* W) const
{
    if (!W || W->GetGameInstance() != this || W->GetNetMode() == NM_Standalone) return false;
    FMWRoomSettings Settings;
    if (const auto* GS = W->GetGameState<AMWGameplayGameState>()) Settings = GS->GetRoomSettings();
    if (!Settings.MapThemeId.IsValid()) Settings = CurrentRoomSettings;
    FString Error;
    return Settings.MapThemeId.IsValid() && MapPackage(W) == ResolveMapTheme(Settings.MapThemeId, Error);
}

bool UMWGameInstance::GetRoomSettingsForWorld(const UWorld* W, FMWRoomSettings& Settings) const
{
    if (!W || W->GetGameInstance() != this || !W->IsGameWorld() || W->GetNetMode() == NM_Standalone) return false;
    Settings = CurrentRoomSettings;
    if (!Settings.MapThemeId.IsValid() && !IsLoginRequired())
    {
        Settings.RoomName = TEXT("Local PIE");
        Settings.MaxPlayers = 3;
        const auto Themes = GetAvailableMapThemeIds();
        if (!Themes.IsEmpty()) Settings.MapThemeId = FGameplayTag::RequestGameplayTag(Themes[0], false);
    }
    FString Error;
    if (!ValidateRoomSettings(Settings, Settings, Error)) return false;
    return IsLobbyWorld(W) || MapPackage(W) == ResolveMapTheme(Settings.MapThemeId, Error);
}

bool UMWGameInstance::PrepareForGameStart(const TArray<int32>& Players, FMWStartPrepared Completion, FString& Error)
{
    if (StartCompletion.IsBound() || !Connection.IsValid()) { Error = TEXT("게임 시작 연결 처리가 진행 중입니다."); return false; }
    StartingPlayers = Players;
    StartCompletion = MoveTemp(Completion);
    if (!IsLoginRequired() && GetWorld() && GetWorld()->GetNetMode() == NM_ListenServer)
    {
        GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this] { CompleteStart(true, FString()); }));
        return true;
    }
    FText Failure;
    if (!Connection->RequestSetRoomJoinable(false, Failure))
    { StartCompletion.Unbind(); Error = Failure.ToString(); return false; }
    return true;
}

void UMWGameInstance::CompleteStart(bool Success, const FString& Error)
{
    auto Completion = MoveTemp(StartCompletion);
    StartCompletion.Unbind();
    Completion.ExecuteIfBound(Success, Error);
}

void UMWGameInstance::HandleSnapshot()
{
    if (!Connection.IsValid()) return;
    const auto View = Connection->GetSnapshot();
    const auto Theme = FGameplayTag::RequestGameplayTag(FName(*View.Room.Properties.FindRef(TEXT("MapTheme"))), false);
    if (Theme.IsValid())
    {
        CurrentRoomSettings.RoomName = View.Room.RoomName;
        CurrentRoomSettings.MaxPlayers = View.Room.MaxPlayers;
        CurrentRoomSettings.MapThemeId = Theme;
    }
    const auto State = GetSessionState();
    if (PreviousState != State)
    { const auto Old = PreviousState; PreviousState = State; OnSessionStateChanged.Broadcast(Old, State); }
    const FString Error = View.ErrorMessage.ToString();
    if (!Error.IsEmpty() && Error != LastProviderError) ReportError(Error);
    LastProviderError = Error;
}

void UMWGameInstance::HandleOperation(EMWConnectionAction Action, const FMWConnectionResult& Result)
{
    const FString Error = Result.Message.ToString();
    if (!Result.bSuccess && !Error.IsEmpty()) ReportError(Error);
    switch (Action)
    {
    case EMWConnectionAction::Login: OnLoginFinished.Broadcast(Result.bSuccess, Error); break;
    case EMWConnectionAction::HostRoom: OnCreateRoomFinished.Broadcast(Result.bSuccess, Error); break;
    case EMWConnectionAction::RefreshRooms: OnRoomsUpdated.Broadcast(Result.bSuccess, Error); break;
    case EMWConnectionAction::JoinRoom:
    case EMWConnectionAction::JoinAddress: OnJoinRoomFinished.Broadcast(Result.bSuccess, Error); break;
    case EMWConnectionAction::SetRoomJoinable: CompleteStart(Result.bSuccess, Error); break;
    case EMWConnectionAction::LeaveRoom:
        StartingPlayers.Reset();
        CurrentRoomSettings = FMWRoomSettings();
        if (Result.bSuccess) { LastError.Reset(); LastProviderError.Reset(); }
        break;
    default: break;
    }
}

void UMWGameInstance::HandleWorld(UWorld* W)
{
    if (!W || W->GetGameInstance() != this) return;
    // 게임 규칙 초기화는 각 GameMode가 담당합니다. 연결 알림은 상태 갱신만 전달합니다.
    HandleSnapshot();
}
