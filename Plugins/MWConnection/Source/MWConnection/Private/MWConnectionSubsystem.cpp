#include "MWConnectionSubsystem.h"
#include "MWConnectionRoomComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameMapsSettings.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogMWConnection, Log, All);

namespace
{
    const FName NameKey(TEXT("MWC_ROOM_NAME"));
    const FName ProtocolKey(TEXT("MWC_PROTOCOL"));
    const FName StatusKey(TEXT("MWC_STATUS"));
    // Provider's wire key, private to this module; no EOS headers or link dependency.
    const FName BucketKey(TEXT("OSSEOS_BUCKET_ID_ATTRIBUTE_KEY"));
    const FString PropertyPrefix(TEXT("MWC_GAME_"));
    enum class EApi : uint8 { None, Login, Create, Find, Join, Update, Destroy };

    FName PackageName(const UWorld* World)
    {
        return World ? FName(*UWorld::RemovePIEPrefix(World->GetOutermost()->GetName())) : NAME_None;
    }

    FMWConnectionResult Outcome(bool Success, EMWConnectionError Code = EMWConnectionError::None, const FString& Message = {})
    {
        FMWConnectionResult Result;
        Result.bSuccess = Success;
        Result.ErrorCode = Code;
        Result.Message = FText::FromString(Message);
        return Result;
    }

    FString PlayerOnlineKey(const APlayerState* Player)
    {
        const auto Id = Player ? Player->GetUniqueId().GetUniqueNetId() : nullptr;
        return Id.IsValid() ? Id->ToString() : FString();
    }
}

/** All provider pointers, callback identities, recovery and world handling are hidden here. */
class FMWConnectionImpl
{
public:
    explicit FMWConnectionImpl(UMWConnectionSubsystem& InOwner) : Owner(InOwner) {}
    UMWConnectionSubsystem& Owner;
    FMWConnectionSnapshot View;
    IOnlineIdentityPtr Identity;
    IOnlineSessionPtr Sessions;
    TSharedPtr<FOnlineSessionSearch> Search;
    TMap<FString, FOnlineSessionSearchResult> SearchResults;
    TSet<FString> RegisteredUsers;
    FString HostUser;
    FString SessionId;
    FString ExpectedSessionId;
    FString Protocol = TEXT("MW_DEV_V3");
    FName LobbyMap;
    FName ReturnMap;
    float TimeoutSeconds = 60.f;
    bool bStopped = false;
    bool bClosing = false;
    bool bBackendPending = false;
    bool bReturnReady = false;
    bool bHostRegistered = false;
    bool bPublishing = false;
    bool bWantedJoinable = true;
    uint64 ApiSerial = 0; // Backend implementation detail; never part of gameplay/API.
    uint64 WorkSerial = 0;
    EApi ApiKind = EApi::None;
    FDelegateHandle ApiHandle, RegisterHandle, MapHandle, ActorsHandle, TravelHandle, NetworkHandle, PreLoginHandle, LoginEventHandle, LogoutEventHandle;
    TMap<TWeakObjectPtr<UWorld>, FDelegateHandle> BeginPlayHandles;
    FTimerHandle Deadline, Readiness;
    TWeakObjectPtr<UWorld> LastEmittedWorld;
    EMWConnectionAction PendingFailureAction = EMWConnectionAction::None;
    FMWConnectionResult PendingFailure;

    UGameInstance* GI() const { return Owner.GetGameInstance(); }
    UWorld* World() const { return GI() ? GI()->GetWorld() : nullptr; }
    bool OwnWorld(const UWorld* W) const { return W && W->IsGameWorld() && W->GetGameInstance() == GI(); }
    TWeakObjectPtr<UMWConnectionSubsystem> Weak() const { return &Owner; }

    void Later(TFunction<void(FMWConnectionImpl&)> Work)
    {
        const auto WeakOwner = Weak();
        const uint64 Serial = WorkSerial;
        GI()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakOwner, Serial, Work = MoveTemp(Work)]() mutable
        {
            if (auto* Service = WeakOwner.Get(); Service && Service->Impl && !Service->Impl->bStopped && Service->Impl->WorkSerial == Serial)
                Work(*Service->Impl);
        }));
    }

    void Initialize()
    {
        LobbyMap = FName(*UGameMapsSettings::GetGameDefaultMap());
        ReturnMap = LobbyMap;
        FString Path;
        if (GConfig->GetString(TEXT("MWConnection"), TEXT("LobbyMap"), Path, GGameIni) && !Path.IsEmpty()) LobbyMap = FName(*Path);
        if (GConfig->GetString(TEXT("MWConnection"), TEXT("ReturnMap"), Path, GGameIni) && !Path.IsEmpty()) ReturnMap = FName(*Path);
        GConfig->GetString(TEXT("MWConnection"), TEXT("Protocol"), Protocol, GGameIni);
        GConfig->GetFloat(TEXT("MWConnection"), TEXT("TimeoutSeconds"), TimeoutSeconds, GGameIni);
        TimeoutSeconds = FMath::Clamp(TimeoutSeconds, 5.f, 180.f);
        FString Profile;
        GConfig->GetString(TEXT("MWConnection"), TEXT("Profile"), Profile, GGameIni);
        View.bLocalIP = Profile.Equals(TEXT("LocalIP"), ESearchCase::IgnoreCase) || FParse::Param(FCommandLine::Get(), TEXT("MWConnectionLocal"));
        if (View.bLocalIP && GEngine)
        {
            // Profile is process-wide, before any game listen driver is created. No ini rewrite.
            for (FNetDriverDefinition& Driver : GEngine->NetDriverDefinitions)
                if (Driver.DefName == NAME_GameNetDriver)
                {
                    Driver.DriverClassName = FName(TEXT("/Script/OnlineSubsystemUtils.IpNetDriver"));
                    Driver.DriverClassNameFallback = Driver.DriverClassName;
                }
        }
        MapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(&Owner, &UMWConnectionSubsystem::HandleMapLoaded);
        ActorsHandle = FWorldDelegates::OnWorldInitializedActors.AddUObject(&Owner, &UMWConnectionSubsystem::HandleActorsInitialized);
        LoginEventHandle = FGameModeEvents::OnGameModePostLoginEvent().AddUObject(&Owner, &UMWConnectionSubsystem::HandlePlayerEntered);
        PreLoginHandle = FGameModeEvents::OnGameModePreLoginEvent().AddUObject(&Owner, &UMWConnectionSubsystem::HandlePlayerEntering);
        LogoutEventHandle = FGameModeEvents::OnGameModeLogoutEvent().AddUObject(&Owner, &UMWConnectionSubsystem::HandlePlayerExited);
        if (GEngine)
        {
            TravelHandle = GEngine->OnTravelFailure().AddUObject(&Owner, &UMWConnectionSubsystem::HandleTravelFailure);
            NetworkHandle = GEngine->OnNetworkFailure().AddUObject(&Owner, &UMWConnectionSubsystem::HandleNetworkFailure);
        }
        UE_LOG(LogMWConnection, Display, TEXT("Profile=%s Protocol=%s"), View.bLocalIP ? TEXT("LocalIP") : TEXT("OnlineEOS"), *Protocol);
    }

    void ClearApi()
    {
        if (ApiHandle.IsValid())
        {
            if (ApiKind == EApi::Login && Identity) Identity->ClearOnLoginCompleteDelegate_Handle(0, ApiHandle);
            if (Sessions)
                switch (ApiKind)
                {
                case EApi::Create: Sessions->ClearOnCreateSessionCompleteDelegate_Handle(ApiHandle); break;
                case EApi::Find: Sessions->ClearOnFindSessionsCompleteDelegate_Handle(ApiHandle); break;
                case EApi::Join: Sessions->ClearOnJoinSessionCompleteDelegate_Handle(ApiHandle); break;
                case EApi::Update: Sessions->ClearOnUpdateSessionCompleteDelegate_Handle(ApiHandle); break;
                case EApi::Destroy: Sessions->ClearOnDestroySessionCompleteDelegate_Handle(ApiHandle); break;
                default: break;
                }
        }
        ApiHandle.Reset();
        ApiKind = EApi::None;
        bBackendPending = false;
    }

    void Shutdown()
    {
        bStopped = true;
        ++ApiSerial;
        ClearApi();
        if (Sessions && RegisterHandle.IsValid()) Sessions->ClearOnRegisterPlayersCompleteDelegate_Handle(RegisterHandle);
        if (GI())
        {
            GI()->GetTimerManager().ClearTimer(Deadline);
            GI()->GetTimerManager().ClearTimer(Readiness);
        }
        FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(MapHandle);
        FWorldDelegates::OnWorldInitializedActors.Remove(ActorsHandle);
        for (const auto& Item : BeginPlayHandles)
            if (auto* W = Item.Key.Get()) W->OnWorldBeginPlay.Remove(Item.Value);
        BeginPlayHandles.Reset();
        FGameModeEvents::OnGameModePostLoginEvent().Remove(LoginEventHandle);
        FGameModeEvents::OnGameModePreLoginEvent().Remove(PreLoginHandle);
        FGameModeEvents::OnGameModeLogoutEvent().Remove(LogoutEventHandle);
        if (GEngine)
        {
            GEngine->OnTravelFailure().Remove(TravelHandle);
            GEngine->OnNetworkFailure().Remove(NetworkHandle);
        }
    }

    bool ResolveOnline(FText& Error)
    {
        IOnlineSubsystem* Provider = World() ? Online::GetSubsystem(World()) : nullptr;
        if (!Provider || Provider->GetSubsystemName() != FName(TEXT("EOS")))
        {
            Error = FText::FromString(TEXT("EOS 서비스를 사용할 수 없습니다. 로컬 시험은 -MWConnectionLocal 프로필로 실행하세요."));
            return false;
        }
        Identity = Provider->GetIdentityInterface();
        Sessions = Provider->GetSessionInterface();
        if (!Identity || !Sessions)
        {
            Error = FText::FromString(TEXT("온라인 서비스 인터페이스가 준비되지 않았습니다."));
            return false;
        }
        const auto Id = Identity->GetUniquePlayerId(0);
        View.bLoggedIn = Identity->GetLoginStatus(0) == ELoginStatus::LoggedIn && Id.IsValid() && Id->IsValid();
        return true;
    }

    bool RequireAction(EMWConnectionAction Action, FText& Error) const
    {
        if (Can(Action)) return true;
        Error = FText::FromString(TEXT("다른 연결 작업을 완료하거나 현재 연결 상태를 확인하세요."));
        return false;
    }

    bool Can(EMWConnectionAction Action) const
    {
        if (bStopped) return false;
        if (Action == EMWConnectionAction::LeaveRoom) return true;
        if (View.bBusy || bClosing || View.State == EMWConnectionState::Unavailable) return false;
        if (Action == EMWConnectionAction::SetRoomJoinable) return View.bInRoom && View.bIsHost;
        if (View.bInRoom || (World() && World()->GetNetMode() != NM_Standalone)) return false;
        switch (Action)
        {
        case EMWConnectionAction::Login: return !View.bLocalIP;
        case EMWConnectionAction::HostRoom: return View.bLocalIP || View.bLoggedIn;
        case EMWConnectionAction::RefreshRooms:
        case EMWConnectionAction::JoinRoom: return !View.bLocalIP && View.bLoggedIn;
        case EMWConnectionAction::JoinAddress: return View.bLocalIP;
        default: return false;
        }
    }

    bool Accept(EMWConnectionAction Action, EMWConnectionState State, FText& Error)
    {
        Error = FText::GetEmpty();
        if (!Can(Action))
        {
            Error = FText::FromString(View.bBusy ? TEXT("다른 연결 작업을 완료한 뒤 요청하세요.") : TEXT("현재 연결 상태에서는 이 요청을 실행할 수 없습니다."));
            return false;
        }
        View.ActiveAction = Action;
        ++WorkSerial;
        View.bBusy = true;
        View.State = State;
        View.ErrorMessage = FText::GetEmpty();
        ArmDeadline();
        Owner.EmitSnapshot();
        return true;
    }

    void ArmDeadline()
    {
        const auto WeakOwner = Weak();
        GI()->GetTimerManager().SetTimer(Deadline, FTimerDelegate::CreateLambda([WeakOwner]
        {
            if (auto* S = WeakOwner.Get(); S && S->Impl) S->Impl->TimedOut();
        }), TimeoutSeconds, false);
    }

    void Finish(const FMWConnectionResult& Result)
    {
        const EMWConnectionAction Action = View.ActiveAction;
        View.ActiveAction = EMWConnectionAction::None;
        View.bBusy = false;
        if (!Result.bSuccess) View.ErrorMessage = Result.Message;
        GI()->GetTimerManager().ClearTimer(Deadline);
        Owner.EmitSnapshot();
        if (Action != EMWConnectionAction::None) Owner.QueueResult(Action, Result);
    }

    void PollReadiness()
    {
        const auto WeakOwner = Weak();
        GI()->GetTimerManager().SetTimer(Readiness, FTimerDelegate::CreateLambda([WeakOwner]
        {
            if (auto* S = WeakOwner.Get(); S && S->Impl) S->Impl->CheckWorldReady();
        }), .05f, true);
    }

    bool Login(FText& Error)
    {
        if (View.bLocalIP) { Error = FText::FromString(TEXT("LocalIP 프로필은 온라인 로그인을 사용하지 않습니다.")); return false; }
        if (!RequireAction(EMWConnectionAction::Login, Error)) return false;
        if (!ResolveOnline(Error) || !Accept(EMWConnectionAction::Login, EMWConnectionState::Idle, Error)) return false;
        Later([](FMWConnectionImpl& I)
        {
            if (I.bClosing) return;
            if (I.View.bLoggedIn) { I.Finish(Outcome(true)); return; }
            const auto WeakOwner = I.Weak();
            const uint64 Serial = ++I.ApiSerial;
            I.ApiKind = EApi::Login;
            I.bBackendPending = true;
            I.ApiHandle = I.Identity->AddOnLoginCompleteDelegate_Handle(0, FOnLoginCompleteDelegate::CreateLambda(
                [WeakOwner, Serial](int32 User, bool Success, const FUniqueNetId& Id, const FString& Message)
                {
                    if (auto* S = WeakOwner.Get(); S && S->Impl && S->Impl->ApiSerial == Serial && User == 0)
                    {
                        auto& P = *S->Impl;
                        P.ClearApi();
                        P.View.bLoggedIn = Success && Id.IsValid();
                        if (P.bClosing) { P.TryCleanup(); return; }
                        P.View.State = EMWConnectionState::Idle;
                        P.Finish(Outcome(P.View.bLoggedIn, P.View.bLoggedIn ? EMWConnectionError::None : EMWConnectionError::NotLoggedIn,
                            P.View.bLoggedIn ? FString() : (Message.IsEmpty() ? TEXT("로그인에 실패했습니다.") : Message)));
                    }
                }));
            FOnlineAccountCredentials Credentials;
            Credentials.Type = TEXT("accountportal");
            if (!I.Identity->Login(0, Credentials) && I.bBackendPending && I.ApiSerial == Serial)
            {
                I.ClearApi();
                I.Finish(Outcome(false, EMWConnectionError::NotLoggedIn, TEXT("로그인 요청을 시작하지 못했습니다.")));
            }
        });
        return true;
    }

    bool Host(const FMWConnectionRoomOptions& Options, FText& Error)
    {
        FMWConnectionRoomOptions Clean = Options;
        Clean.RoomName.TrimStartAndEndInline();
        if (Clean.RoomName.IsEmpty() || Clean.RoomName.Len() > 64 || Clean.MaxPlayers < 1 || Clean.MaxPlayers > 64 || Clean.Properties.Num() > 16)
        { Error = FText::FromString(TEXT("방 이름(1~64자), 정원(1~64명), 메타데이터(최대 16개)를 확인하세요.")); return false; }
        for (const auto& Property : Clean.Properties)
            if (Property.Key.IsEmpty() || Property.Key.Len() > 32 || Property.Value.Len() > 256)
            { Error = FText::FromString(TEXT("메타데이터 키·값 길이를 확인하세요.")); return false; }
        if (!RequireAction(EMWConnectionAction::HostRoom, Error)) return false;
        if (!View.bLocalIP && !ResolveOnline(Error)) return false;
        if (!Accept(EMWConnectionAction::HostRoom, EMWConnectionState::Creating, Error)) return false;
        View.Room = Clean;
        View.bIsHost = true;
        bWantedJoinable = true;
        RegisteredUsers.Reset();
        bHostRegistered = View.bLocalIP;
        SessionId.Reset();
        ExpectedSessionId.Reset();
        Later([](FMWConnectionImpl& I)
        {
            if (I.bClosing) return;
            if (I.View.bLocalIP) { I.OpenLobby(); return; }
            if (I.Sessions->GetNamedSession(NAME_GameSession))
            { I.FailRoom(EMWConnectionError::RoomUnavailable, TEXT("기존 세션을 먼저 종료해야 합니다.")); return; }
            const auto Id = I.Identity->GetUniquePlayerId(0);
            if (!Id.IsValid()) { I.FailRoom(EMWConnectionError::NotLoggedIn, TEXT("로그인 ID를 확인할 수 없습니다.")); return; }
            I.HostUser = Id->ToString();
            I.BindRegistration();
            FOnlineSessionSettings Settings;
            Settings.NumPublicConnections = I.View.Room.MaxPlayers;
            Settings.NumPrivateConnections = 0;
            Settings.bIsLANMatch = false;
            Settings.bShouldAdvertise = false;
            Settings.bAllowJoinInProgress = true;
            Settings.bUseLobbiesIfAvailable = false;
            Settings.bUsesPresence = false;
            Settings.bAllowInvites = false;
            Settings.Set(NameKey, I.View.Room.RoomName, EOnlineDataAdvertisementType::ViaOnlineService);
            Settings.Set(ProtocolKey, I.Protocol, EOnlineDataAdvertisementType::ViaOnlineService);
            Settings.Set(BucketKey, I.Protocol, EOnlineDataAdvertisementType::ViaOnlineService);
            Settings.Set(StatusKey, FString(TEXT("Preparing")), EOnlineDataAdvertisementType::ViaOnlineService);
            for (const auto& Item : I.View.Room.Properties)
                Settings.Set(FName(*(PropertyPrefix + Item.Key)), Item.Value, EOnlineDataAdvertisementType::ViaOnlineService);
            const auto WeakOwner = I.Weak();
            const uint64 Serial = ++I.ApiSerial;
            I.ApiKind = EApi::Create;
            I.bBackendPending = true;
            I.ApiHandle = I.Sessions->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateLambda(
                [WeakOwner, Serial](FName Name, bool Success)
                {
                    if (auto* S = WeakOwner.Get(); S && S->Impl && S->Impl->ApiSerial == Serial && Name == NAME_GameSession)
                    {
                        auto& P = *S->Impl;
                        P.ClearApi();
                        if (P.bClosing) { P.TryCleanup(); return; }
                        if (!Success) { P.FailRoom(EMWConnectionError::ConnectionFailed, TEXT("온라인 방 생성에 실패했습니다.")); return; }
                        const auto* Session = P.Sessions->GetNamedSession(NAME_GameSession);
                        if (!Session || !Session->SessionInfo.IsValid()) { P.FailRoom(EMWConnectionError::ConnectionFailed, TEXT("생성한 세션 정보를 확인할 수 없습니다.")); return; }
                        P.SessionId = Session->SessionInfo->GetSessionId().ToString();
                        P.OpenLobby();
                    }
                }));
            if (!I.Sessions->CreateSession(0, NAME_GameSession, Settings) && I.bBackendPending && I.ApiSerial == Serial)
            { I.ClearApi(); I.FailRoom(EMWConnectionError::ConnectionFailed, TEXT("방 생성 요청을 시작하지 못했습니다.")); }
        });
        return true;
    }

    void OpenLobby()
    {
        PollReadiness();
        UGameplayStatics::OpenLevel(&Owner, LobbyMap, true, TEXT("listen"));
    }

    void BindRegistration()
    {
        if (RegisterHandle.IsValid()) Sessions->ClearOnRegisterPlayersCompleteDelegate_Handle(RegisterHandle);
        const auto WeakOwner = Weak();
        RegisterHandle = Sessions->AddOnRegisterPlayersCompleteDelegate_Handle(FOnRegisterPlayersCompleteDelegate::CreateLambda(
            [WeakOwner](FName Name, const TArray<FUniqueNetIdRef>& Users, bool Success)
            {
                if (auto* S = WeakOwner.Get(); S && S->Impl && Name == NAME_GameSession)
                    S->Impl->Registered(Users, Success);
            }));
    }

    void Registered(const TArray<FUniqueNetIdRef>& Users, bool Success)
    {
        if (bStopped || bClosing || !View.bIsHost) return;
        for (const auto& User : Users)
        {
            const FString Key = User->ToString();
            if (Success) RegisteredUsers.Add(Key);
            if (Key == HostUser)
            {
                if (!Success) { FailRoom(EMWConnectionError::ConnectionFailed, TEXT("호스트의 온라인 참가자 등록이 거절됐습니다.")); return; }
                bHostRegistered = true;
            }
            else if (!Success && World())
            {
                // Infrastructure admission failure. Uses UE base classes only.
                for (auto It = World()->GetPlayerControllerIterator(); It; ++It)
                {
                    auto* PC = It->Get();
                    if (PC && PC->PlayerState && PlayerOnlineKey(PC->PlayerState) == Key)
                        if (auto* GM = World()->GetAuthGameMode(); GM && GM->GameSession)
                            GM->GameSession->KickPlayer(PC, FText::FromString(TEXT("온라인 참가자 등록에 실패했습니다.")));
                }
            }
        }
        UpdateRoomComponent();
        CheckWorldReady();
    }

    void UpdateRoomComponent()
    {
        UWorld* W = World();
        if (!OwnWorld(W) || W->GetNetMode() != NM_ListenServer || !View.bIsHost || View.Room.RoomName.IsEmpty()) return;
        auto* GS = W->GetGameState<AGameStateBase>();
        if (!GS) return;
        if (auto* GM = W->GetAuthGameMode(); GM && GM->GameSession)
            GM->GameSession->MaxPlayers = View.Room.MaxPlayers;
        auto* Info = GS->FindComponentByClass<UMWConnectionRoomComponent>();
        if (!Info)
        {
            Info = NewObject<UMWConnectionRoomComponent>(GS, TEXT("ConnectionRoomInfo"));
            GS->AddInstanceComponent(Info);
            Info->RegisterComponent();
        }
        Info->SetRoom(SessionId, View.Room, RegisteredUsers.Array());
    }

    void MapLoaded(UWorld* W)
    {
        if (!OwnWorld(W)) return;
        if (bClosing)
        {
            bReturnReady = W->GetNetMode() == NM_Standalone && PackageName(W) == ReturnMap;
            if (LastEmittedWorld.Get() != W) { LastEmittedWorld = W; Owner.EmitWorld(W); }
            TryCleanup();
            return;
        }
        UpdateRoomComponent();
        if (LastEmittedWorld.Get() != W) { LastEmittedWorld = W; Owner.EmitWorld(W); }
        CheckWorldReady();
    }

    void CheckWorldReady()
    {
        if (bStopped || bClosing || !OwnWorld(World()) || !World()->HasBegunPlay()) return;
        if (View.ActiveAction == EMWConnectionAction::HostRoom)
        {
            if (World()->GetNetMode() != NM_ListenServer || PackageName(World()) != LobbyMap || !World()->GetNetDriver() || !bHostRegistered || bBackendPending) return;
            UpdateRoomComponent();
            GI()->GetTimerManager().ClearTimer(Readiness);
            if (View.bLocalIP)
            {
                View.State = EMWConnectionState::Hosting;
                View.bInRoom = true;
                Finish(Outcome(true));
            }
            else { bPublishing = true; UpdateJoinability(true); }
        }
        else if (View.ActiveAction == EMWConnectionAction::JoinRoom || View.ActiveAction == EMWConnectionAction::JoinAddress)
        {
            if (bBackendPending || World()->GetNetMode() != NM_Client || PackageName(World()) != LobbyMap || !World()->GetNetDriver()) return;
            auto* PC = UGameplayStatics::GetPlayerController(GI(), 0);
            auto* GS = World()->GetGameState<AGameStateBase>();
            auto* Info = GS ? GS->FindComponentByClass<UMWConnectionRoomComponent>() : nullptr;
            if (!PC || !PC->IsLocalController() || !PC->PlayerState || !Info || Info->GetRoom().RoomName.IsEmpty()) return;
            if (!View.bLocalIP)
            {
                if (Info->GetSessionId() != ExpectedSessionId) return;
                if (!Info->GetRegisteredUsers().Contains(PlayerOnlineKey(PC->PlayerState))) return;
            }
            View.Room = Info->GetRoom();
            View.State = EMWConnectionState::Joined;
            View.bInRoom = true;
            View.bIsHost = false;
            GI()->GetTimerManager().ClearTimer(Readiness);
            Finish(Outcome(true));
        }
    }

    bool SetJoinable(bool Joinable, FText& Error)
    {
        if (!Accept(EMWConnectionAction::SetRoomJoinable, EMWConnectionState::Hosting, Error)) return false;
        bWantedJoinable = Joinable; // Apply local admission immediately, before provider advertisement completes.
        bPublishing = false;
        Later([Joinable](FMWConnectionImpl& I) { if (!I.bClosing) I.UpdateJoinability(Joinable); });
        return true;
    }

    void UpdateJoinability(bool Joinable)
    {
        bWantedJoinable = Joinable;
        if (View.bLocalIP) { Finish(Outcome(true)); return; }
        const auto* Named = Sessions ? Sessions->GetNamedSession(NAME_GameSession) : nullptr;
        if (!Named) { FailUpdate(TEXT("현재 온라인 방이 없습니다.")); return; }
        FOnlineSessionSettings Settings = Named->SessionSettings;
        Settings.bShouldAdvertise = Joinable;
        Settings.bAllowJoinInProgress = Joinable;
        Settings.Set(StatusKey, FString(Joinable ? TEXT("Lobby") : TEXT("Closed")), EOnlineDataAdvertisementType::ViaOnlineService);
        const auto WeakOwner = Weak();
        const uint64 Serial = ++ApiSerial;
        ApiKind = EApi::Update;
        bBackendPending = true;
        ApiHandle = Sessions->AddOnUpdateSessionCompleteDelegate_Handle(FOnUpdateSessionCompleteDelegate::CreateLambda(
            [WeakOwner, Serial](FName Name, bool Success)
            {
                if (auto* S = WeakOwner.Get(); S && S->Impl && S->Impl->ApiSerial == Serial && Name == NAME_GameSession)
                {
                    auto& I = *S->Impl;
                    I.ClearApi();
                    if (I.bClosing) { I.TryCleanup(); return; }
                    if (!Success) { I.FailUpdate(TEXT("온라인 방 공개 상태를 변경하지 못했습니다.")); return; }
                    I.View.State = EMWConnectionState::Hosting;
                    I.View.bInRoom = true;
                    I.Finish(Outcome(true));
                    I.bPublishing = false;
                }
            }));
        if (!Sessions->UpdateSession(NAME_GameSession, Settings, true) && bBackendPending && ApiSerial == Serial)
        { ClearApi(); FailUpdate(TEXT("방 공개 상태 요청을 시작하지 못했습니다.")); }
    }

    void FailUpdate(const FString& Message)
    {
        if (bPublishing) { FailRoom(EMWConnectionError::ConnectionFailed, Message); return; }
        Finish(Outcome(false, EMWConnectionError::ConnectionFailed, Message));
    }

    bool Refresh(FText& Error)
    {
        if (View.bLocalIP) { Error = FText::FromString(TEXT("LocalIP에서는 방 목록 대신 RequestJoinAddress를 사용하세요.")); return false; }
        if (!RequireAction(EMWConnectionAction::RefreshRooms, Error)) return false;
        if (!ResolveOnline(Error) || !Accept(EMWConnectionAction::RefreshRooms, EMWConnectionState::Searching, Error)) return false;
        SearchResults.Reset();
        View.Rooms.Reset();
        Later([](FMWConnectionImpl& I)
        {
            if (I.bClosing) return;
            I.Search = MakeShared<FOnlineSessionSearch>();
            I.Search->bIsLanQuery = false;
            I.Search->MaxSearchResults = 100;
            I.Search->QuerySettings.Set(BucketKey, I.Protocol, EOnlineComparisonOp::Equals);
            const auto WeakOwner = I.Weak();
            const uint64 Serial = ++I.ApiSerial;
            I.ApiKind = EApi::Find;
            I.bBackendPending = true;
            I.ApiHandle = I.Sessions->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateLambda(
                [WeakOwner, Serial](bool Success)
                {
                    if (auto* S = WeakOwner.Get(); S && S->Impl && S->Impl->ApiSerial == Serial)
                    {
                        auto& P = *S->Impl;
                        P.ClearApi();
                        if (P.bClosing) { P.TryCleanup(); return; }
                        P.View.Rooms.Reset();
                        P.SearchResults.Reset();
                        if (Success && P.Search)
                            for (const auto& Found : P.Search->SearchResults) P.Describe(Found);
                        P.Search.Reset();
                        P.View.State = EMWConnectionState::Idle;
                        P.Finish(Outcome(Success, Success ? EMWConnectionError::None : EMWConnectionError::ConnectionFailed,
                            Success ? FString() : TEXT("방 목록 조회에 실패했습니다.")));
                    }
                }));
            if (!I.Sessions->FindSessions(0, I.Search.ToSharedRef()) && I.bBackendPending && I.ApiSerial == Serial)
            { I.ClearApi(); I.View.State = EMWConnectionState::Idle; I.Finish(Outcome(false, EMWConnectionError::ConnectionFailed, TEXT("방 목록 요청을 시작하지 못했습니다."))); }
        });
        return true;
    }

    void Describe(const FOnlineSessionSearchResult& Found)
    {
        if (!Found.IsValid()) return;
        FString FoundProtocol, Status;
        const auto& Settings = Found.Session.SessionSettings;
        if (!Settings.Get(ProtocolKey, FoundProtocol) || FoundProtocol != Protocol || !Settings.Get(StatusKey, Status) || Status != TEXT("Lobby")) return;
        FMWConnectionRoomInfo Room;
        Room.RoomHandle = Found.GetSessionIdStr();
        if (Room.RoomHandle.IsEmpty() || !Settings.Get(NameKey, Room.RoomName)) return;
        Room.MaxPlayers = Settings.NumPublicConnections;
        const int32 Open = FMath::Clamp(Found.Session.NumOpenPublicConnections + Found.Session.NumOpenPrivateConnections, 0, Room.MaxPlayers);
        Room.CurrentPlayers = Room.MaxPlayers - Open;
        for (const auto& Entry : Settings.Settings)
            if (Entry.Key.ToString().StartsWith(PropertyPrefix))
            {
                FString Value;
                if (Entry.Value.Data.GetType() == EOnlineKeyValuePairDataType::String)
                {
                    Entry.Value.Data.GetValue(Value);
                    Room.Properties.Add(Entry.Key.ToString().RightChop(PropertyPrefix.Len()), Value);
                }
            }
        if (Open <= 0) Room.UnavailableReason = TEXT("정원이 가득 찼습니다.");
        const auto LocalId = Identity ? Identity->GetUniquePlayerId(0) : nullptr;
        if (LocalId && Found.Session.OwningUserId && *LocalId == *Found.Session.OwningUserId)
            Room.UnavailableReason = TEXT("같은 계정이 만든 방입니다. 다른 계정으로 참가하세요.");
        Room.bJoinable = Room.UnavailableReason.IsEmpty();
        SearchResults.Add(Room.RoomHandle, Found);
        View.Rooms.Add(Room);
    }

    bool Join(const FString& Handle, FText& Error)
    {
        const auto* Found = SearchResults.Find(Handle);
        const auto* Room = View.Rooms.FindByPredicate([&](const auto& R) { return R.RoomHandle == Handle; });
        if (!Found || !Room || !Room->bJoinable)
        { Error = FText::FromString(TEXT("현재 목록에서 참가 가능한 방을 선택하세요.")); return false; }
        const FOnlineSessionSearchResult Target = *Found;
        const FMWConnectionRoomInfo Chosen = *Room;
        if (!RequireAction(EMWConnectionAction::JoinRoom, Error) || !ResolveOnline(Error) || !Accept(EMWConnectionAction::JoinRoom, EMWConnectionState::Joining, Error)) return false;
        ExpectedSessionId = Handle;
        View.Room.RoomName = Chosen.RoomName;
        View.Room.MaxPlayers = Chosen.MaxPlayers;
        View.Room.Properties = Chosen.Properties;
        View.bIsHost = false;
        Later([Target](FMWConnectionImpl& I)
        {
            if (I.bClosing) return;
            const auto WeakOwner = I.Weak();
            const uint64 Serial = ++I.ApiSerial;
            I.ApiKind = EApi::Join;
            I.bBackendPending = true;
            I.ApiHandle = I.Sessions->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateLambda(
                [WeakOwner, Serial](FName Name, EOnJoinSessionCompleteResult::Type Result)
                {
                    if (auto* S = WeakOwner.Get(); S && S->Impl && S->Impl->ApiSerial == Serial && Name == NAME_GameSession)
                    {
                        auto& P = *S->Impl;
                        P.ClearApi();
                        if (P.bClosing) { P.TryCleanup(); return; }
                        if (Result != EOnJoinSessionCompleteResult::Success)
                        { P.FailRoom(EMWConnectionError::RoomUnavailable, TEXT("방이 닫혔거나 정원이 가득 찼습니다.")); return; }
                        FString Address;
                        if (!P.Sessions->GetResolvedConnectString(NAME_GameSession, Address) || Address.IsEmpty())
                        { P.FailRoom(EMWConnectionError::ConnectionFailed, TEXT("연결 주소를 확인할 수 없습니다.")); return; }
                        P.Connect(Address);
                    }
                }));
            if (!I.Sessions->JoinSession(0, NAME_GameSession, Target) && I.bBackendPending && I.ApiSerial == Serial)
            { I.ClearApi(); I.FailRoom(EMWConnectionError::ConnectionFailed, TEXT("참가 요청을 시작하지 못했습니다.")); }
        });
        return true;
    }

    bool JoinAddress(const FString& Address, FText& Error)
    {
        if (Address.IsEmpty() || Address.Len() > 256 || Address.Contains(TEXT("?")) || Address.Contains(TEXT("/")) ||
            Address.Contains(TEXT("\n")) || Address.Contains(TEXT("\r")) || Address.Contains(TEXT(" ")))
        { Error = FText::FromString(TEXT("host:port 형식의 로컬 연결 주소를 입력하세요.")); return false; }
        if (!Accept(EMWConnectionAction::JoinAddress, EMWConnectionState::Joining, Error)) return false;
        ExpectedSessionId.Reset();
        View.bIsHost = false;
        Later([Address](FMWConnectionImpl& I) { if (!I.bClosing) I.Connect(Address); });
        return true;
    }

    void Connect(const FString& Address)
    {
        auto* PC = UGameplayStatics::GetPlayerController(GI(), 0);
        if (!PC) { FailRoom(EMWConnectionError::ConnectionFailed, TEXT("로컬 PlayerController가 없습니다.")); return; }
        PollReadiness();
        PC->ClientTravel(Address, TRAVEL_Absolute);
    }

    void FailRoom(EMWConnectionError Code, const FString& Message)
    {
        if (bClosing) return;
        PendingFailureAction = View.ActiveAction;
        PendingFailure = Outcome(false, Code, Message);
        View.ActiveAction = EMWConnectionAction::None;
        View.ErrorMessage = PendingFailure.Message;
        BeginCleanup();
    }

    bool Leave(FText& Error)
    {
        Error = FText::GetEmpty();
        if (bStopped) { Error = FText::FromString(TEXT("연결 서비스가 종료 중입니다.")); return false; }
        if (View.ActiveAction == EMWConnectionAction::LeaveRoom)
        { Error = FText::FromString(TEXT("이미 종료 중입니다.")); return false; }
        if (View.ActiveAction != EMWConnectionAction::None)
        {
            Owner.QueueResult(View.ActiveAction, Outcome(false, EMWConnectionError::Cancelled, TEXT("방 종료 요청으로 연결 작업을 종료합니다.")));
            View.ActiveAction = EMWConnectionAction::None;
        }
        if (PendingFailureAction != EMWConnectionAction::None)
        { Owner.QueueResult(PendingFailureAction, PendingFailure); PendingFailureAction = EMWConnectionAction::None; }
        View.ActiveAction = EMWConnectionAction::LeaveRoom;
        BeginCleanup();
        return true;
    }

    void BeginCleanup()
    {
        ++WorkSerial;
        bClosing = true;
        View.State = EMWConnectionState::Closing;
        View.bBusy = true;
        View.bInRoom = false;
        GI()->GetTimerManager().ClearTimer(Readiness);
        ArmDeadline();
        bReturnReady = World() && World()->GetNetMode() == NM_Standalone && PackageName(World()) == ReturnMap;
        Owner.EmitSnapshot();
        Later([](FMWConnectionImpl& I)
        {
            if (!I.bReturnReady) UGameplayStatics::OpenLevel(&I.Owner, I.ReturnMap, true);
            I.TryCleanup();
        });
    }

    void TryCleanup()
    {
        if (!bClosing || bStopped || bBackendPending) return;
        if (Sessions && Sessions->GetNamedSession(NAME_GameSession))
        {
            const auto WeakOwner = Weak();
            const uint64 Serial = ++ApiSerial;
            ApiKind = EApi::Destroy;
            bBackendPending = true;
            ApiHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateLambda(
                [WeakOwner, Serial](FName Name, bool Success)
                {
                    if (auto* S = WeakOwner.Get(); S && S->Impl && S->Impl->ApiSerial == Serial && Name == NAME_GameSession)
                    {
                        auto& I = *S->Impl;
                        I.ClearApi();
                        if (!Success || I.Sessions->GetNamedSession(NAME_GameSession))
                        { I.CleanupUncertain(TEXT("온라인 세션 정리를 확인하지 못했습니다. 게임을 다시 실행하세요.")); return; }
                        I.TryCleanup();
                    }
                }));
            if (!Sessions->DestroySession(NAME_GameSession) && bBackendPending && ApiSerial == Serial)
            {
                ClearApi();
                if (Sessions->GetNamedSession(NAME_GameSession)) CleanupUncertain(TEXT("세션 종료 요청에 실패했습니다. 게임을 다시 실행하세요."));
                else TryCleanup();
            }
            return;
        }
        if (!bReturnReady) return;
        GI()->GetTimerManager().ClearTimer(Deadline);
        if (Sessions && RegisterHandle.IsValid()) Sessions->ClearOnRegisterPlayersCompleteDelegate_Handle(RegisterHandle);
        RegisterHandle.Reset();
        SessionId.Reset();
        ExpectedSessionId.Reset();
        HostUser.Reset();
        RegisteredUsers.Reset();
        View.bIsHost = false;
        View.bInRoom = false;
        View.Room = FMWConnectionRoomOptions();
        View.State = EMWConnectionState::Idle;
        bClosing = false;
        bPublishing = false;
        if (PendingFailureAction != EMWConnectionAction::None)
        {
            Owner.QueueResult(PendingFailureAction, PendingFailure);
            PendingFailureAction = EMWConnectionAction::None;
        }
        if (View.ActiveAction == EMWConnectionAction::LeaveRoom) Finish(Outcome(true));
        else { View.bBusy = false; Owner.EmitSnapshot(); }
    }

    void CleanupUncertain(const FString& Message)
    {
        GI()->GetTimerManager().ClearTimer(Deadline);
        View.State = EMWConnectionState::Unavailable;
        View.ErrorMessage = FText::FromString(Message);
        if (PendingFailureAction != EMWConnectionAction::None)
        { Owner.QueueResult(PendingFailureAction, PendingFailure); PendingFailureAction = EMWConnectionAction::None; }
        Finish(Outcome(false, EMWConnectionError::RestartRequired, Message));
        // In-flight callbacks remain bound. No new host/join until cleanup is confirmed.
    }

    void TimedOut()
    {
        if (bClosing) { CleanupUncertain(TEXT("연결 정리의 제한시간을 초과했습니다. 게임 재시작이 필요합니다.")); return; }
        if (bBackendPending)
        {
            const auto Action = View.ActiveAction;
            View.ActiveAction = EMWConnectionAction::None;
            Owner.QueueResult(Action, Outcome(false, EMWConnectionError::Timeout, TEXT("연결 작업의 제한시간을 초과했습니다.")));
            View.ErrorMessage = FText::FromString(TEXT("늦은 온라인 응답을 정리 중입니다."));
            BeginCleanup();
        }
        else FailRoom(EMWConnectionError::Timeout, TEXT("연결 준비의 제한시간을 초과했습니다."));
    }

    void NetworkFailure(UWorld* W, UNetDriver* Driver, const FString& Message)
    {
        if (!OwnWorld(W) || (Driver && Driver->NetDriverName != NAME_GameNetDriver) || bClosing) return;
        if (View.bBusy || View.bInRoom || W->GetNetMode() == NM_Client)
            FailRoom(EMWConnectionError::ConnectionFailed, Message.IsEmpty() ? TEXT("서버 연결이 종료됐습니다.") : Message);
    }
};

UMWConnectionSubsystem::UMWConnectionSubsystem() = default;
UMWConnectionSubsystem::~UMWConnectionSubsystem() = default;
void FMWConnectionImplDeleter::operator()(FMWConnectionImpl* Pointer) const { delete Pointer; }

void UMWConnectionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Impl = TUniquePtr<FMWConnectionImpl, FMWConnectionImplDeleter>(new FMWConnectionImpl(*this));
    Impl->Initialize();
}

void UMWConnectionSubsystem::Deinitialize()
{
    if (Impl) Impl->Shutdown();
    Impl.Reset();
    SnapshotChanged.Clear();
    OperationCompleted.Clear();
    LocalWorldReady.Clear();
    Super::Deinitialize();
}

bool UMWConnectionSubsystem::RequestLogin(FText& E) { return Impl && Impl->Login(E); }
bool UMWConnectionSubsystem::RequestHostRoom(const FMWConnectionRoomOptions& O, FText& E) { return Impl && Impl->Host(O, E); }
bool UMWConnectionSubsystem::RequestRefreshRooms(FText& E) { return Impl && Impl->Refresh(E); }
bool UMWConnectionSubsystem::RequestJoinRoom(const FString& H, FText& E) { return Impl && Impl->Join(H, E); }
bool UMWConnectionSubsystem::RequestJoinAddress(const FString& A, FText& E) { return Impl && Impl->JoinAddress(A, E); }
bool UMWConnectionSubsystem::RequestSetRoomJoinable(bool J, FText& E) { return Impl && Impl->SetJoinable(J, E); }
bool UMWConnectionSubsystem::RequestLeaveRoom(FText& E) { return Impl && Impl->Leave(E); }
bool UMWConnectionSubsystem::CanExecute(EMWConnectionAction A) const { return Impl && Impl->Can(A); }
FMWConnectionSnapshot UMWConnectionSubsystem::GetSnapshot() const { return Impl ? Impl->View : FMWConnectionSnapshot(); }
void UMWConnectionSubsystem::EmitSnapshot() { SnapshotChanged.Broadcast(); }
void UMWConnectionSubsystem::EmitWorld(UWorld* W) { LocalWorldReady.Broadcast(W); }
void UMWConnectionSubsystem::QueueResult(EMWConnectionAction A, const FMWConnectionResult& R)
{
    if (!Impl || A == EMWConnectionAction::None) return;
    const TWeakObjectPtr<UMWConnectionSubsystem> Weak(this);
    GetGameInstance()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([Weak, A, R]
    {
        if (auto* S = Weak.Get(); S && S->Impl && !S->Impl->bStopped) S->OperationCompleted.Broadcast(A, R);
    }));
}
void UMWConnectionSubsystem::HandleMapLoaded(UWorld* W) { if (Impl) Impl->MapLoaded(W); }
void UMWConnectionSubsystem::HandleActorsInitialized(const FActorsInitializedParams& Params)
{
    UWorld* W = Params.World;
    if (!Impl || !Impl->OwnWorld(W)) return;
    // Seamless travel does not require LoadMap/PostLoadMap. Observe the actual target world too.
    if (W->HasBegunPlay()) { Impl->MapLoaded(W); return; }
    if (Impl->BeginPlayHandles.Contains(W)) return;
    const TWeakObjectPtr<UMWConnectionSubsystem> Weak(this);
    const TWeakObjectPtr<UWorld> WeakWorld(W);
    Impl->BeginPlayHandles.Add(W, W->OnWorldBeginPlay.AddLambda([Weak, WeakWorld]
    {
        if (auto* S = Weak.Get(); S && S->Impl)
            if (auto* World = WeakWorld.Get()) S->Impl->MapLoaded(World);
    }));
}
void UMWConnectionSubsystem::HandleTravelFailure(UWorld* W, ETravelFailure::Type, const FString& E)
{
    if (!Impl || !Impl->OwnWorld(W)) return;
    if (Impl->bClosing) Impl->CleanupUncertain(E);
    else Impl->NetworkFailure(W, nullptr, E);
}
void UMWConnectionSubsystem::HandleNetworkFailure(UWorld* W, UNetDriver* D, ENetworkFailure::Type Type, const FString& E)
{
    // A remote participant leaving is not a failure of the whole listen server.
    if (W && W->GetNetMode() == NM_ListenServer &&
        (Type == ENetworkFailure::ConnectionLost || Type == ENetworkFailure::ConnectionTimeout || Type == ENetworkFailure::FailureReceived)) return;
    if (Impl) Impl->NetworkFailure(W, D, E);
}
void UMWConnectionSubsystem::HandlePlayerEntering(AGameModeBase* M, const FUniqueNetIdRepl& Player, FString& Error)
{
    if (!Impl || !M || !Impl->OwnWorld(M->GetWorld()) || !Impl->View.bIsHost || !Error.IsEmpty()) return;
    if (Impl->bClosing || !Impl->bWantedJoinable || !Impl->View.bInRoom)
        Error = TEXT("방을 준비 중이거나 입장이 닫혔습니다.");
    else if (!Impl->View.bLocalIP && !Player.IsValid())
        Error = TEXT("온라인 인증 정보를 확인할 수 없습니다.");
    else if (!Impl->View.bLocalIP)
        for (auto It = M->GetWorld()->GetPlayerControllerIterator(); It; ++It)
            if (const auto* PC = It->Get(); PC && PC->PlayerState && PC->PlayerState->GetUniqueId() == Player)
            { Error = TEXT("같은 계정이 이미 참가 중입니다."); break; }
}
void UMWConnectionSubsystem::HandlePlayerEntered(AGameModeBase* M, APlayerController*)
{ if (Impl && M && Impl->OwnWorld(M->GetWorld())) Impl->UpdateRoomComponent(); }
void UMWConnectionSubsystem::HandlePlayerExited(AGameModeBase* M, AController* P)
{
    if (!Impl || !M || !P || !Impl->OwnWorld(M->GetWorld())) return;
    if (P->PlayerState) Impl->RegisteredUsers.Remove(PlayerOnlineKey(P->PlayerState));
    Impl->UpdateRoomComponent();
}
void UMWConnectionSubsystem::HandleRoomInfoChanged() { if (Impl) Impl->CheckWorldReady(); }
