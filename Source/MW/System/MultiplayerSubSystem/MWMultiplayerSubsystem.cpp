#include "MWMultiplayerSubsystem.h"

#include "AssetRegistry/AssetData.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameMapsSettings.h"
#include "GameplayTagContainer.h"
#include "Kismet/GameplayStatics.h"
#include "LogChannel.h"
#include "Misc/Char.h"
#include "Misc/PackageName.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemEOSTypesPublic.h"
#include "OnlineSubsystemUtils.h"
#include "Player/MWPlayerController.h"
#include "System/MWAssetManager.h"
#include "System/MWGameMode.h"
#include "System/MWGameState.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace
{
    const FName MWRoomNameKey(TEXT("MW_ROOM_NAME"));
    const FName MWMapThemeKey(TEXT("MW_MAP_THEME"));
    const FName MWRoomStatusKey(TEXT("MW_ROOM_STATUS"));
    const FName MWProtocolKey(TEXT("MW_PROTOCOL_ID"));

    const FString MWProtocol(TEXT("MW_DEV_V1"));
    const FString MWBucket(TEXT("MW_DEV_V1"));

    FName MWGetWorldMapPackage(const UWorld* World)
    {
        if (!World)
        {
            return NAME_None;
        }
        return FName(*UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()));
    }
}

void UMWMultiplayerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    bLoginInProgress = false;
    bDeinitializing = false;

    PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UMWMultiplayerSubsystem::HandlePostLoadMap);

    if (GEngine)
    {
        TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &UMWMultiplayerSubsystem::HandleTravelFailure);

        NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UMWMultiplayerSubsystem::HandleNetworkFailure);
    }
}

void UMWMultiplayerSubsystem::Deinitialize()
{
    bDeinitializing = true;
    ++LoginRequestId;
    ++SessionRequestId;

    ClearSessionTimeout();
    ClearLoginDelegate();
    ClearSessionApiDelegates();
    ClearHostRegistrationDelegate();
    ClearRoomListDelegate();
    ClearJoinDelegate();
    RoomSearch.Reset();
    InvalidateRoomList();

    FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
    PostLoadMapHandle.Reset();

    if (GEngine)
    {
        GEngine->OnTravelFailure().Remove(TravelFailureHandle);
        GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
    }
    TravelFailureHandle.Reset();
    NetworkFailureHandle.Reset();

    SessionApi.Reset();
    HostUserId.Reset();
    HostWorld.Reset();
    bLoginInProgress = false;

    OnLoginFinished.Clear();
    OnSessionStateChanged.Clear();
    OnCreateRoomFinished.Clear();
    OnSessionError.Clear();
    OnRoomsUpdated.Clear();
    OnJoinRoomFinished.Clear();

    Super::Deinitialize();
}

void UMWMultiplayerSubsystem::RequestLogin()
{
    if (bDeinitializing)
    {
        return;
    }

    if (bLoginInProgress)
    {
        UE_LOG(
            LogMWEOS,
            Verbose,
            TEXT("Login request ignored: another login is in progress."));

        return;
    }

    FString ResolveError;
    IOnlineIdentityPtr Identity = ResolveIdentity(ResolveError);

    if (!Identity.IsValid())
    {
        FinishLogin(false, ResolveError);
        return;
    }

    // 이미 로그인돼 있으면 온라인 로그인 요청을 다시 보내지 않음.
    if (Identity->GetLoginStatus(LoginLocalUserNum) == ELoginStatus::LoggedIn)
    {
        const auto UserId = Identity->GetUniquePlayerId(LoginLocalUserNum);

        if (UserId.IsValid() && UserId->IsValid())
        {
            FinishLogin(true, FString());
        }
        else
        {
            FinishLogin(false, TEXT("Login status is LoggedIn, but the user ID is invalid."));
        }

        return;
    }

    ClearLoginDelegate();

    LoginIdentity = Identity;

    const uint64 CurrentRequestId = ++LoginRequestId;

    // Login()이 콜백을 즉시 호출할 가능성에 대비해 먼저 설정합니다.
    bLoginInProgress = true;

    LoginCompleteHandle = LoginIdentity->AddOnLoginCompleteDelegate_Handle(
            LoginLocalUserNum,
            FOnLoginCompleteDelegate::CreateUObject(this, &UMWMultiplayerSubsystem::HandleLoginComplete, CurrentRequestId));

    FOnlineAccountCredentials Credentials;
    Credentials.Type = TEXT("accountportal");

    UE_LOG(LogMWEOS, Display, TEXT("Starting EOS login. LocalUserNum=%d"),
           LoginLocalUserNum);

    // 호출 도중 완료 콜백이 실행돼도 인터페이스 참조가 유지되게 .
    const IOnlineIdentityPtr IdentityForRequest = LoginIdentity;

    const bool bStarted = IdentityForRequest->Login(LoginLocalUserNum, Credentials);

    // Login() 내부에서 이미 콜백이 실행됐을 수 있음.
    // 같은 요청이 아직 진행 중일 때만 즉시 실패를 처리.
    if (!bStarted && bLoginInProgress && LoginRequestId == CurrentRequestId)
    {
        FinishLogin(false, TEXT("EOS rejected the login request before it could start."));
    }
}

bool UMWMultiplayerSubsystem::IsLoggedIn() const
{
    FString Error;
    IOnlineIdentityPtr Identity = ResolveIdentity(Error);

    if (!Identity.IsValid())
    {
        return false;
    }

    if (Identity->GetLoginStatus(LoginLocalUserNum) != ELoginStatus::LoggedIn)
    {
        return false;
    }

    const auto UserId = Identity->GetUniquePlayerId(LoginLocalUserNum);

    return UserId.IsValid() && UserId->IsValid();
}

IOnlineIdentityPtr UMWMultiplayerSubsystem::ResolveIdentity(FString& OutError) const
{
    OutError.Reset();

    if (bDeinitializing)
    {
        OutError = TEXT("Multiplayer subsystem is deinitialized.");
        return nullptr;
    }

    UGameInstance* GameInstance = GetGameInstance();
    UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;

    if (!World)
    {
        OutError = TEXT("None World");
        return nullptr;
    }

    IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(World);

    if (!OnlineSubsystem)
    {
        OutError = TEXT("None OnlineSubsystem");
        return nullptr;
    }

    // NULL 등 다른 서비스로 실행되는 상태를 구분.
    if (OnlineSubsystem->GetSubsystemName() != TEXT("EOS"))
    {
        OutError = FString::Printf(TEXT("Expected EOS, but the active subsystem is '%s'."),
                                   *OnlineSubsystem->GetSubsystemName().ToString());

        return nullptr;
    }

    IOnlineIdentityPtr Identity = OnlineSubsystem->GetIdentityInterface();

    if (!Identity.IsValid())
    {
        OutError = TEXT("EOS identity interface is not valid.");
        return nullptr;
    }

    return Identity;
}

void UMWMultiplayerSubsystem::HandleLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId,
                                                  const FString& Error, uint64 RequestId)
{
    if (bDeinitializing || !bLoginInProgress || RequestId != LoginRequestId || LocalUserNum != LoginLocalUserNum)
    {
        return;
    }

    const bool bValidLogin =
        bWasSuccessful && UserId.IsValid() &&
        LoginIdentity.IsValid() &&
        LoginIdentity->GetLoginStatus(LoginLocalUserNum) == ELoginStatus::LoggedIn;

    FString ResultError = Error;

    if (bValidLogin)
    {
        UE_LOG(LogMWEOS, Display, TEXT("EOS login succeeded. LocalUserNum=%d UserId=%s"), LocalUserNum, *UserId.ToString());
    }
    else if (ResultError.IsEmpty())
    {
        ResultError = TEXT("EOS login completed without a valid authenticated user.");
    }

    FinishLogin(bValidLogin, ResultError);
}

void UMWMultiplayerSubsystem::ClearLoginDelegate()
{
    if (LoginIdentity.IsValid() && LoginCompleteHandle.IsValid())
    {
        LoginIdentity->ClearOnLoginCompleteDelegate_Handle(LoginLocalUserNum, LoginCompleteHandle);
    }

    LoginCompleteHandle.Reset();
    LoginIdentity.Reset();
}

void UMWMultiplayerSubsystem::FinishLogin(bool bSuccess, const FString& ErrorMessage)
{
    const FString FinalError = bSuccess ? FString() : ErrorMessage;

    // 상태를 정리한 후 UI를 호출해야,
    // UI에서 재시도하더라도 이전 요청과 섞이지 않습니다.
    ++LoginRequestId;

    ClearLoginDelegate();

    bLoginInProgress = false;

    if (bSuccess)
    {
        UE_LOG(LogMWEOS, Display, TEXT("Login finished successfully."));
    }
    else
    {
        UE_LOG(LogMWEOS, Warning, TEXT("Login failed: %s"), *FinalError);
    }

    OnLoginFinished.Broadcast(bSuccess, FinalError);
}

bool UMWMultiplayerSubsystem::ValidateRoomSettings(const FMWRoomSettings& InputSettings,
    FMWRoomSettings& OutValidatedSettings, FString& OutErrorMessage) const
{
    // 원본을 직접 수정하지 않고 복사본을 정리합니다.
    FMWRoomSettings Candidate = InputSettings;

    // 이전 호출의 출력값이 남지 않도록 초기화합니다.
    OutValidatedSettings = FMWRoomSettings{};
    OutErrorMessage.Reset();

    // 1. 이름 앞뒤의 공백류를 제거합니다.
    Candidate.RoomName.TrimStartAndEndInline();

    // 2. 공백을 제거한 결과가 비어 있는지 검사합니다.
    if (Candidate.RoomName.IsEmpty())
    {
        OutErrorMessage = TEXT("방 이름을 입력해 주세요.");
        return false;
    }

    // 3. 정리된 이름의 길이를 검사합니다.
    if (Candidate.RoomName.Len() > MaxRoomNameLength)
    {
        OutErrorMessage = FString::Printf(TEXT("방 이름은 최대 %d자까지 입력할 수 있습니다."), MaxRoomNameLength);

        return false;
    }

    // 4. 이름 내부의 줄바꿈, 탭 등 제어문자를 거절합니다.
    for (const TCHAR Character : Candidate.RoomName)
    {
        if (FChar::IsControl(Character))
        {
            OutErrorMessage = TEXT("방 이름에는 줄바꿈이나 탭을 사용할 수 없습니다.");

            return false;
        }
    }

    // 5. 호스트를 포함한 최대 인원을 검사합니다.
    if (Candidate.MaxPlayers < MinRoomPlayers || Candidate.MaxPlayers > MaxRoomPlayers)
    {
        OutErrorMessage = FString::Printf(TEXT("최대 인원은 호스트를 포함해 %d~%d명이어야 합니다."), MinRoomPlayers, MaxRoomPlayers);

        return false;
    }

    // 6. 테마를 선택했는지 검사합니다.
    if (!Candidate.MapThemeId.IsValid())
    {
        OutErrorMessage = TEXT("맵 테마를 선택해 주세요.");
        return false;
    }

    // 7. 프로젝트에서 허용한 테마인지 검사합니다.
    const FName MapPackageName = ResolveMapPackageName(Candidate.MapThemeId, OutErrorMessage);

    if (MapPackageName.IsNone())
    {
        return false;
    }

    // 모든 검사를 통과했을 때만 승인된 결과를 반환합니다.
    OutValidatedSettings = Candidate;

    return true;
}

FName UMWMultiplayerSubsystem::ResolveMapPackageName(FGameplayTag MapThemeId, FString& OutError) const
{
    OutError.Reset();

    // FName 입력을 등록된 태그로 해석하고, 다른 종류의 에셋 ID는 거절합니다.
    const FGameplayTag ThemeTag = FGameplayTag::RequestGameplayTag(MapThemeId.GetTagName(), false);
    const FGameplayTag ThemeRoot = FGameplayTag::RequestGameplayTag(FName(TEXT("Asset.MapTheme")), false);
    if (!ThemeTag.IsValid() || !ThemeRoot.IsValid() || ThemeTag == ThemeRoot || !ThemeTag.MatchesTag(ThemeRoot))
    {
        OutError = TEXT("등록된 맵 테마를 선택해 주세요.");
        return NAME_None;
    }

    const UMWAssetManager& Assets = UMWAssetManager::Get();
    if (!Assets.HasCatalog())
    {
        OutError = TEXT("맵 목록이 준비되지 않았습니다. StartupAssetData 설정을 확인해 주세요.");
        return NAME_None;
    }

    const FSoftObjectPath LevelPath = Assets.FindAssetPath(ThemeTag);
    if (!LevelPath.IsValid() || !LevelPath.IsAsset())
    {
        OutError = TEXT("선택한 맵 테마에 레벨이 등록되지 않았습니다.");
        return NAME_None;
    }

    // 레벨을 메모리에 보관하지 않고 Asset Registry 메타데이터로 존재와 타입을 확인합니다.
    FAssetData LevelAsset;
    if (!Assets.GetAssetDataForPath(LevelPath, LevelAsset))
    {
        OutError = TEXT("등록된 레벨을 찾을 수 없습니다. 카탈로그와 패키지 구성을 확인해 주세요.");
        return NAME_None;
    }
    if (!LevelAsset.IsInstanceOf<UWorld>())
    {
        OutError = TEXT("맵 테마는 레벨 에셋에 연결해야 합니다.");
        return NAME_None;
    }

    return LevelAsset.PackageName;
}

TArray<FName> UMWMultiplayerSubsystem::GetAvailableMapThemeIds() const
{
    TArray<FName> Result;
    const FGameplayTag Root = FGameplayTag::RequestGameplayTag(
        FName(TEXT("Asset.MapTheme")), false);

    const UMWAssetManager& Assets = UMWAssetManager::Get();
    for (const FGameplayTag Id : Assets.GetAssetIdsUnderRoot(Root))
    {
        FString Error;
        if (!ResolveMapPackageName(Id, Error).IsNone())
        {
            Result.Add(Id.GetTagName());
        }
    }
    return Result;
}

bool UMWMultiplayerSubsystem::IsSessionBusy() const
{
    switch (SessionState)
    {
    case EMWSessionState::Creating:
    case EMWSessionState::Searching:
    case EMWSessionState::Joining:
    case EMWSessionState::Leaving:
        return true;

    case EMWSessionState::None:
    case EMWSessionState::Hosting:
    case EMWSessionState::Joined:
    case EMWSessionState::CleanupRequired:
    default:
        return false;
    }
}

void UMWMultiplayerSubsystem::SetSessionState(EMWSessionState NewState)
{
    if (SessionState == NewState)
    {
        return;
    }

    const EMWSessionState PreviousState = SessionState;

    SessionState = NewState;

    OnSessionStateChanged.Broadcast(PreviousState, SessionState);
}

bool UMWMultiplayerSubsystem::IsCurrentSessionRequest(uint64 RequestId) const
{
    return !bDeinitializing && RequestId == SessionRequestId;
}

IOnlineSessionPtr UMWMultiplayerSubsystem::ResolveSession(FString& OutError) const
{
    // 이전 호출의 오류 문장이 남지 않도록 초기화합니다.
    OutError.Reset();

    // 1. 종료 중에는 새 세션 작업을 시작하지 않습니다.
    if (bDeinitializing)
    {
        OutError = TEXT("Multiplayer subsystem is shutting down.");

        return nullptr;
    }

    // 2. 이 Subsystem이 속한 GameInstance를 확인합니다.
    UGameInstance* GameInstance = GetGameInstance();

    if (!GameInstance)
    {
        OutError = TEXT("GameInstance is not available.");
        return nullptr;
    }

    // 3. 현재 GameInstance의 World를 확인합니다.
    UWorld* World = GameInstance->GetWorld();

    if (!World)
    {
        OutError = TEXT("World is not available.");
        return nullptr;
    }

    // 4. 현재 World에 대응하는 온라인 서비스를 구합니다.
    IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(World);

    if (!OnlineSubsystem)
    {
        OutError = TEXT("No online subsystem is available for the current world.");

        return nullptr;
    }

    // 5. 현재 프로젝트에서 사용할 서비스인지 확인합니다.
    static const FName ExpectedSubsystemName(TEXT("EOS"));

    if (OnlineSubsystem->GetSubsystemName() != ExpectedSubsystemName)
    {
        OutError = FString::Printf(TEXT("Expected EOS, but the active subsystem is '%s'."),
            *OnlineSubsystem->GetSubsystemName().ToString());

        return nullptr;
    }

    // 6. 세션 기능을 제공하는 인터페이스를 구합니다.
    const IOnlineSessionPtr Session = OnlineSubsystem->GetSessionInterface();

    if (!Session.IsValid())
    {
        OutError = TEXT("EOS session interface is not available.");

        return nullptr;
    }

    // 성공하면 OutError는 빈 문자열로 유지됩니다.
    return Session;
}

FName UMWMultiplayerSubsystem::ResolveTitleMapPackageName(FString& OutError) const
{
    OutError.Reset();

    // GetGameDefaultMap returns a package name, while AssetManager expects an object path.
    const FString TitlePackage = UGameMapsSettings::GetGameDefaultMap(EDefaultMapRequestType::Client);
    if (!FPackageName::IsValidLongPackageName(TitlePackage))
    {
        OutError = TEXT("프로젝트 기본 타이틀 맵을 설정해 주세요.");
        return NAME_None;
    }
    const FSoftObjectPath Path(FString::Printf(TEXT("%s.%s"), *TitlePackage, *FPackageName::GetLongPackageAssetName(TitlePackage)));
    FAssetData Asset;

    if (!Path.IsValid() || !UMWAssetManager::Get().GetAssetDataForPath(Path, Asset) || !Asset.IsInstanceOf<UWorld>())
    {
        OutError = TEXT("프로젝트 기본 맵을 실제 타이틀 레벨로 설정해 주세요.");
        return NAME_None;
    }
    return Asset.PackageName;
}

bool UMWMultiplayerSubsystem::IsOwnWorld(const UWorld* World) const
{
    return World && World->GetGameInstance() == GetGameInstance();
}

bool UMWMultiplayerSubsystem::HasPendingSessionWork() const
{
    return CreateSessionHandle.IsValid() ||
        UpdateSessionHandle.IsValid() ||
        DestroySessionHandle.IsValid() ||
        RoomListHandle.IsValid() || JoinSessionHandle.IsValid() ||
        bClientTravelPending || bWorldLoadPending || bHostRegistrationExpected;
}

bool UMWMultiplayerSubsystem::GetApprovedRoomSettingsForWorld(const UWorld* World, FMWRoomSettings& OutSettings) const
{
    OutSettings = FMWRoomSettings{};

    if (bDeinitializing || !IsOwnWorld(World) ||
        CleanupCause != EMWCleanupCause::None ||
        (SessionState != EMWSessionState::Creating &&
         SessionState != EMWSessionState::Hosting) ||
        PendingMapPackageName.IsNone() ||
        MWGetWorldMapPackage(World) != PendingMapPackageName)
    {
        return false;
    }

    OutSettings = PendingRoomSettings;
    return true;
}

void UMWMultiplayerSubsystem::RequestCreateRoom(
    const FMWRoomSettings& Settings)
{
    if (bDeinitializing)
    {
        return;
    }

    // 중복 요청은 진행 중인 첫 작업을 건드리지 않습니다.
    if (SessionState != EMWSessionState::None)
    {
        UE_LOG(LogMWEOS, Verbose, TEXT("Create ignored: session state is not None."));
        return;
    }

    if (bLoginInProgress || !IsLoggedIn())
    {
        EmitSessionError(TEXT("먼저 로그인해 주세요."));
        return;
    }

    FMWRoomSettings Validated;
    FString Error;
    if (!ValidateRoomSettings(Settings, Validated, Error))
    {
        EmitSessionError(Error);
        return;
    }

    const FName MapPackage =
        ResolveMapPackageName(Validated.MapThemeId, Error);
    if (MapPackage.IsNone())
    {
        EmitSessionError(Error);
        return;
    }

    const FName TitlePackage = ResolveTitleMapPackageName(Error);
    if (TitlePackage.IsNone() || TitlePackage == MapPackage)
    {
        EmitSessionError(TitlePackage.IsNone() ? Error :
            TEXT("타이틀 맵과 경기 테마 맵을 분리해 주세요."));
        return;
    }

    IOnlineSessionPtr Sessions = ResolveSession(Error);
    if (!Sessions.IsValid())
    {
        EmitSessionError(Error);
        return;
    }
    if (Sessions->GetNamedSession(NAME_GameSession) != nullptr)
    {
        // 관리 상태와 실제 세션이 불일치합니다. 모르는 작업을 덮어쓰지 않습니다.
        SessionApi = Sessions;
        ReturnMapPackageName = TitlePackage;
        CleanupCause = EMWCleanupCause::Leave;
        bCleanupUncertain = true;
        ++SessionRequestId;
        SetSessionState(EMWSessionState::CleanupRequired);
        EmitSessionError(TEXT(
            "관리 상태와 다른 세션이 남아 있습니다. 상태 확인 또는 게임 재시작이 필요합니다."));
        return;
    }
    const IOnlineIdentityPtr Identity = ResolveIdentity(Error);
    const FUniqueNetIdPtr CreatorId = Identity.IsValid()
        ? Identity->GetUniquePlayerId(LoginLocalUserNum) : nullptr;
    if (!CreatorId.IsValid() || !CreatorId->IsValid())
    {
        EmitSessionError(TEXT("방 생성자의 온라인 ID를 확인할 수 없습니다."));
        return;
    }

#if !WITH_EOS_SDK
    EmitSessionError(TEXT("이 빌드에는 EOS SDK가 포함되어 있지 않습니다."));
    return;
#else
    FOnlineSessionSettings OnlineSettings;
    OnlineSettings.NumPublicConnections = Validated.MaxPlayers;
    OnlineSettings.NumPrivateConnections = 0;
    OnlineSettings.bIsLANMatch = false;
    OnlineSettings.bIsDedicated = false;
    OnlineSettings.bUseLobbiesIfAvailable = false;
    OnlineSettings.bUsesPresence = false;
    OnlineSettings.bAllowJoinInProgress = true;
    OnlineSettings.bAllowInvites = false;
    OnlineSettings.bAllowJoinViaPresence = false;
    OnlineSettings.bAllowJoinViaPresenceFriendsOnly = false;
    OnlineSettings.bShouldAdvertise = false;

    OnlineSettings.Set(MWRoomNameKey, Validated.RoomName,
        EOnlineDataAdvertisementType::ViaOnlineService);
    OnlineSettings.Set(MWMapThemeKey, Validated.MapThemeId.ToString(),
        EOnlineDataAdvertisementType::ViaOnlineService);
    OnlineSettings.Set(MWRoomStatusKey, FString(TEXT("Preparing")),
        EOnlineDataAdvertisementType::ViaOnlineService);
    OnlineSettings.Set(MWProtocolKey, MWProtocol,
        EOnlineDataAdvertisementType::ViaOnlineService);
    OnlineSettings.Set(OSSEOS_BUCKET_ID_ATTRIBUTE_KEY, MWBucket,
        EOnlineDataAdvertisementType::ViaOnlineService);

    SessionApi = Sessions;
    HostUserId = CreatorId;
    PendingRoomSettings = Validated;
    PendingMapPackageName = MapPackage;
    ReturnMapPackageName = TitlePackage;
    HostWorld.Reset();

    LastSessionError.Reset();
    CleanupCause = EMWCleanupCause::None;
    bReturningToTitle = false;
    bCleanupUncertain = false;
    bCreateResultPending = true;
    bWorldLoadPending = false;
    bJoinConnecting = false;
    bClientTravelPending = false;
    bHostRegistrationExpected = false;
    bHostRegistrationConfirmed = false;

    const uint64 RequestId = ++SessionRequestId;
    CreateRoomStage = EMWCreateRoomStage::CreatingSession;

    CreateSessionHandle =
        SessionApi->AddOnCreateSessionCompleteDelegate_Handle(
            FOnCreateSessionCompleteDelegate::CreateUObject(
                this, &UMWMultiplayerSubsystem::HandleCreateSessionComplete,
                RequestId));

    SetSessionState(EMWSessionState::Creating);

    // 상태 이벤트 이후에도 같은 작업인지 확인합니다.
    if (!IsCurrentSessionRequest(RequestId) ||
        CleanupCause != EMWCleanupCause::None)
    {
        return;
    }

    ArmSessionTimeout();
    const IOnlineSessionPtr ApiForCall = SessionApi;
    const bool bStarted = ApiForCall->CreateSession(
        LoginLocalUserNum, NAME_GameSession, OnlineSettings);

    if (!bStarted && IsCurrentSessionRequest(RequestId) &&
        CreateRoomStage == EMWCreateRoomStage::CreatingSession)
    {
        FailCreation(TEXT("세션 생성 요청이 시작되지 않았습니다."));
    }
#endif
}

void UMWMultiplayerSubsystem::HandleCreateSessionComplete(
    FName SessionName, bool bWasSuccessful, uint64 RequestId)
{
    if (!IsCurrentSessionRequest(RequestId) ||
        SessionName != NAME_GameSession ||
        CreateRoomStage != EMWCreateRoomStage::CreatingSession)
    {
        return;
    }

    ClearSessionTimeout();
    ClearSessionApiDelegates();

    if (CleanupCause != EMWCleanupCause::None)
    {
        StartCleanup(); // 시간초과 후 늦게 온 성공도 여기서 정리합니다.
        return;
    }

    if (!bWasSuccessful)
    {
        FailCreation(TEXT("EOS 방 생성에 실패했습니다."));
        return;
    }

    const FNamedOnlineSession* Named =
        SessionApi->GetNamedSession(NAME_GameSession);
    if (!Named || !Named->SessionInfo.IsValid() ||
        !Named->SessionInfo->GetSessionId().IsValid())
    {
        FailCreation(TEXT("생성된 세션 정보를 찾을 수 없습니다."));
        return;
    }

    RegisterPlayersHandle =
        SessionApi->AddOnRegisterPlayersCompleteDelegate_Handle(
            FOnRegisterPlayersCompleteDelegate::CreateUObject(
                this, &UMWMultiplayerSubsystem::HandleHostPlayersRegistered,
                RequestId));

    CreateRoomStage = EMWCreateRoomStage::LoadingWorld;
    bWorldLoadPending = true;
    ArmSessionTimeout();

    UGameplayStatics::OpenLevel(
        this, PendingMapPackageName, true, TEXT("listen"));
}

void UMWMultiplayerSubsystem::ExpectHostRegistration(UWorld* World)
{
    if (bDeinitializing || !IsOwnWorld(World) ||
        CreateRoomStage != EMWCreateRoomStage::LoadingWorld ||
        World->GetNetMode() != NM_ListenServer)
    {
        return;
    }

    // 이후 기본 GameSession 경로가 등록을 수행합니다.
    // 취소/정리 중이어도 실제 시작되는 등록 콜백은 추적합니다.
    bHostRegistrationExpected = true;
}

void UMWMultiplayerSubsystem::ClearHostRegistrationDelegate()
{
    if (SessionApi.IsValid() && RegisterPlayersHandle.IsValid())
    {
        SessionApi->ClearOnRegisterPlayersCompleteDelegate_Handle(
            RegisterPlayersHandle);
    }
    RegisterPlayersHandle.Reset();
}

void UMWMultiplayerSubsystem::HandleHostPlayersRegistered(
    FName SessionName, const TArray<FUniqueNetIdRef>& Players,
    bool bWasSuccessful, uint64 RequestId)
{
    if (!IsCurrentSessionRequest(RequestId) ||
        SessionName != NAME_GameSession ||
        !bHostRegistrationExpected || !HostUserId.IsValid())
    {
        return;
    }

    const bool bIncludesHost = Players.ContainsByPredicate(
        [this](const FUniqueNetIdRef& Id)
        {
            return *Id == *HostUserId;
        });
    if (!bIncludesHost)
    {
        return;
    }

    bHostRegistrationExpected = false;
    bHostRegistrationConfirmed = bWasSuccessful;
    ClearHostRegistrationDelegate();

    if (CleanupCause != EMWCleanupCause::None)
    {
        StartCleanup();
        return;
    }
    if (!bWasSuccessful)
    {
        FailCreation(TEXT("호스트의 EOS 참가자 등록에 실패했습니다."));
        return;
    }

    if (UWorld* World = GetGameInstance()->GetWorld())
    {
        if (AMWGameMode* GM = Cast<AMWGameMode>(World->GetAuthGameMode()))
        {
            GM->TryPrepareLobby();
        }
    }
}

void UMWMultiplayerSubsystem::HandlePostLoadMap(UWorld* World)
{
    if (bDeinitializing || !IsOwnWorld(World))
    {
        return;
    }
    bWorldLoadPending = false;
    if (bJoinConnecting) bClientTravelPending = false;

    if (bReturningToTitle)
    {
        if (MWGetWorldMapPackage(World) == ReturnMapPackageName &&
            World->GetNetMode() == NM_Standalone)
        {
            FinishCleanup();
        }
        return;
    }

    if (CleanupCause != EMWCleanupCause::None)
    {
        if (!HasPendingSessionWork())
        {
            StartCleanup();
        }
        return;
    }

    if (SessionState == EMWSessionState::Joining)
    {
        NotifyClientLobbyReady(Cast<AMWPlayerController>(UGameplayStatics::GetPlayerController(World, LoginLocalUserNum)));
        return;
    }

    if (SessionState == EMWSessionState::Creating &&
        CreateRoomStage == EMWCreateRoomStage::LoadingWorld)
    {
        if (MWGetWorldMapPackage(World) != PendingMapPackageName ||
            World->GetNetMode() != NM_ListenServer)
        {
            FailCreation(TEXT("요청한 Listen 경기 월드가 준비되지 않았습니다."));
            return;
        }

        AMWGameMode* GM = Cast<AMWGameMode>(World->GetAuthGameMode());
        if (!GM)
        {
            FailCreation(TEXT("경기 맵의 GameMode를 확인해 주세요."));
            return;
        }
        GM->TryPrepareLobby();
    }
}

void UMWMultiplayerSubsystem::NotifyListenWorldReady(UWorld* World)
{
    if (bDeinitializing || !IsOwnWorld(World) ||
        SessionState != EMWSessionState::Creating ||
        CleanupCause != EMWCleanupCause::None ||
        CreateRoomStage != EMWCreateRoomStage::LoadingWorld ||
        MWGetWorldMapPackage(World) != PendingMapPackageName)
    {
        return;
    }

    if (bWorldLoadPending || !bHostRegistrationConfirmed ||
        World->GetNetMode() != NM_ListenServer ||
        !World->HasBegunPlay() || !World->GetNetDriver())
    {
        return;
    }

    AMWGameMode* GM = Cast<AMWGameMode>(World->GetAuthGameMode());
    AMWGameState* GS = World->GetGameState<AMWGameState>();
    FNamedOnlineSession* Named = SessionApi.IsValid()
        ? SessionApi->GetNamedSession(NAME_GameSession) : nullptr;

    if (!GM || !GS || !Named ||
        GS->GetRoomSettings().MapThemeId != PendingRoomSettings.MapThemeId)
    {
        FailCreation(TEXT("서버의 방 설정 초기화를 확인해 주세요."));
        return;
    }

    HostWorld = World;

    UE_LOG(LogMWEOS, Display, TEXT("Listen ready. Map=%s Driver=%s"),
        *PendingMapPackageName.ToString(),
        *World->GetNetDriver()->GetClass()->GetPathName());

    // 실제 서버가 준비된 뒤 참가 가능한 상태를 공개합니다.
    GM->SetLobbyAdmission(true);
    FOnlineSessionSettings Updated = Named->SessionSettings;
    Updated.bShouldAdvertise = true;
    Updated.Set(MWRoomStatusKey, FString(TEXT("Lobby")),
        EOnlineDataAdvertisementType::ViaOnlineService);

    CreateRoomStage = EMWCreateRoomStage::Publishing;
    const uint64 RequestId = SessionRequestId;

    UpdateSessionHandle =
        SessionApi->AddOnUpdateSessionCompleteDelegate_Handle(
            FOnUpdateSessionCompleteDelegate::CreateUObject(
                this, &UMWMultiplayerSubsystem::HandleUpdateSessionComplete,
                RequestId));

    ArmSessionTimeout();
    const IOnlineSessionPtr ApiForCall = SessionApi;
    const bool bStarted = ApiForCall->UpdateSession(
        NAME_GameSession, Updated, true);

    if (!bStarted && IsCurrentSessionRequest(RequestId) &&
        CreateRoomStage == EMWCreateRoomStage::Publishing)
    {
        FailCreation(TEXT("방 공개 요청이 시작되지 않았습니다."));
    }
}

void UMWMultiplayerSubsystem::HandleUpdateSessionComplete(
    FName SessionName, bool bWasSuccessful, uint64 RequestId)
{
    if (!IsCurrentSessionRequest(RequestId) ||
        SessionName != NAME_GameSession ||
        CreateRoomStage != EMWCreateRoomStage::Publishing)
    {
        return;
    }

    ClearSessionTimeout();
    ClearSessionApiDelegates();

    if (CleanupCause != EMWCleanupCause::None)
    {
        StartCleanup();
        return;
    }

    UWorld* World = HostWorld.Get();
    if (!bWasSuccessful || !IsOwnWorld(World) ||
        World->GetNetMode() != NM_ListenServer)
    {
        FailCreation(TEXT("방 공개를 완료하지 못했습니다."));
        return;
    }

    CreateRoomStage = EMWCreateRoomStage::None;
    bCreateResultPending = false;
    SetSessionState(EMWSessionState::Hosting);
    OnCreateRoomFinished.Broadcast(true, FString());
}

void UMWMultiplayerSubsystem::RequestLeaveRoom()
{
    if (bDeinitializing ||
        (SessionState != EMWSessionState::Hosting &&
         SessionState != EMWSessionState::Joined))
    {
        return;
    }

    ++SessionRequestId;
    CleanupCause = EMWCleanupCause::Leave;
    LastSessionError.Reset();
    CloseServerAdmission();
    SetSessionState(EMWSessionState::Leaving);
    StartCleanup();
}

void UMWMultiplayerSubsystem::CloseServerAdmission()
{
    UWorld* World = HostWorld.Get();
    if (!World && GetGameInstance())
    {
        World = GetGameInstance()->GetWorld();
    }

    if (IsOwnWorld(World))
    {
        if (AMWGameMode* GM = Cast<AMWGameMode>(World->GetAuthGameMode()))
        {
            GM->SetLobbyAdmission(false);
        }
    }
}

void UMWMultiplayerSubsystem::FailCreation(const FString& Error)
{
    const bool bNotifyCreate = bCreateResultPending;
    bCreateResultPending = false;
    CleanupCause = EMWCleanupCause::CreationFailure;
    LastSessionError = Error;
    CloseServerAdmission();

    // 진행 중 호출/맵 로드의 결과를 먼저 회수합니다.
    // 콜백을 지우고 같은 이름의 새 요청을 시작하지 않습니다.
    if (HasPendingSessionWork())
    {
        SetSessionState(EMWSessionState::CleanupRequired);
    }
    else
    {
        StartCleanup();
    }

    EmitSessionError(Error);
    if (bNotifyCreate)
    {
        OnCreateRoomFinished.Broadcast(false, Error);
    }
}

void UMWMultiplayerSubsystem::StartCleanup()
{
    if (bDeinitializing || CleanupCause == EMWCleanupCause::None)
    {
        return;
    }
    if (HasPendingSessionWork())
    {
        SetSessionState(EMWSessionState::CleanupRequired);
        return;
    }
    if (bCleanupUncertain)
    {
        SetSessionState(EMWSessionState::CleanupRequired);
        return;
    }

    ClearSessionTimeout();
    ClearHostRegistrationDelegate();
    CreateRoomStage = EMWCreateRoomStage::None;
    bJoinConnecting = false;
    CloseServerAdmission();

    if (!SessionApi.IsValid())
    {
        SetSessionState(EMWSessionState::CleanupRequired);
        EmitSessionError(TEXT("정리에 사용할 세션 인터페이스가 없습니다."));
        return;
    }

    if (!SessionApi->GetNamedSession(NAME_GameSession))
    {
        BeginReturnToTitle();
        return;
    }

    const uint64 RequestId = SessionRequestId;
    DestroySessionHandle =
        SessionApi->AddOnDestroySessionCompleteDelegate_Handle(
            FOnDestroySessionCompleteDelegate::CreateUObject(
                this, &UMWMultiplayerSubsystem::HandleDestroySessionComplete,
                RequestId));

    SetSessionState(EMWSessionState::Leaving);
    ArmSessionTimeout();

    const IOnlineSessionPtr ApiForCall = SessionApi;
    const bool bStarted = ApiForCall->DestroySession(NAME_GameSession);

    if (!bStarted && IsCurrentSessionRequest(RequestId) &&
        DestroySessionHandle.IsValid())
    {
        ClearSessionTimeout();
        SetSessionState(EMWSessionState::CleanupRequired);
        EmitSessionError(TEXT(
            "종료 요청의 시작 여부를 확인하지 못했습니다. 결과 확인이 필요합니다."));
    }
}

void UMWMultiplayerSubsystem::HandleDestroySessionComplete(
    FName SessionName, bool bWasSuccessful, uint64 RequestId)
{
    if (!IsCurrentSessionRequest(RequestId) ||
        SessionName != NAME_GameSession ||
        CleanupCause == EMWCleanupCause::None)
    {
        return;
    }

    ClearSessionTimeout();
    ClearSessionApiDelegates();

    if (!bWasSuccessful)
    {
        bCleanupUncertain = !SessionApi.IsValid() ||
            SessionApi->GetNamedSession(NAME_GameSession) == nullptr;

        SetSessionState(EMWSessionState::CleanupRequired);
        EmitSessionError(TEXT(
            "세션 정리에 실패했습니다. 새 방을 만들기 전에 정리가 필요합니다."));
        return;
    }

    if (!SessionApi.IsValid() ||
        SessionApi->GetNamedSession(NAME_GameSession) != nullptr)
    {
        SetSessionState(EMWSessionState::CleanupRequired);
        EmitSessionError(TEXT("종료 후에도 로컬 세션이 남아 있습니다."));
        return;
    }

    bCleanupUncertain = false;
    BeginReturnToTitle();
}

void UMWMultiplayerSubsystem::BeginReturnToTitle()
{
    if (ReturnMapPackageName.IsNone())
    {
        FString Error;
        ReturnMapPackageName = ResolveTitleMapPackageName(Error);
        if (ReturnMapPackageName.IsNone())
        {
            SetSessionState(EMWSessionState::CleanupRequired);
            EmitSessionError(Error);
            return;
        }
    }

    bReturningToTitle = true;
    SetSessionState(EMWSessionState::Leaving);
    ArmSessionTimeout();

    UWorld* World = GetGameInstance()->GetWorld();
    if (IsOwnWorld(World) &&
        MWGetWorldMapPackage(World) == ReturnMapPackageName &&
        World->GetNetMode() == NM_Standalone)
    {
        FinishCleanup();
        return;
    }

    // 절대 이동 + 빈 옵션: Listen 옵션을 메뉴로 상속하지 않습니다.
    UGameplayStatics::OpenLevel(
        this, ReturnMapPackageName, true, FString());
}

void UMWMultiplayerSubsystem::FinishCleanup()
{
    if (HasPendingSessionWork() || bCleanupUncertain ||
        (SessionApi.IsValid() &&
         SessionApi->GetNamedSession(NAME_GameSession) != nullptr))
    {
        SetSessionState(EMWSessionState::CleanupRequired);
        return;
    }

    ClearSessionTimeout();
    ClearSessionApiDelegates();
    ClearHostRegistrationDelegate();

    ClearRoomListDelegate();
    ClearJoinDelegate();
    RoomSearch.Reset();
    InvalidateRoomList();
    SessionApi.Reset();
    HostUserId.Reset();
    HostWorld.Reset();

    PendingRoomSettings = FMWRoomSettings{};
    PendingMapPackageName = NAME_None;
    ReturnMapPackageName = NAME_None;

    CreateRoomStage = EMWCreateRoomStage::None;
    CleanupCause = EMWCleanupCause::None;
    bReturningToTitle = false;
    bWorldLoadPending = false;
    bHostRegistrationExpected = false;
    bHostRegistrationConfirmed = false;
    bCreateResultPending = false;
    bCleanupUncertain = false;
    bJoinConnecting = false;
    bClientTravelPending = false;
    bJoinResultPending = false;
    PendingJoinSessionId.Reset();
    PendingJoinThemeId = FGameplayTag();
    PendingJoinMapPackage = NAME_None;

    ++SessionRequestId;
    SetSessionState(EMWSessionState::None);
}

void UMWMultiplayerSubsystem::RetrySessionCleanup()
{
    if (bDeinitializing ||
        SessionState != EMWSessionState::CleanupRequired)
    {
        return;
    }
    if (HasPendingSessionWork())
    {
        EmitSessionError(TEXT(
            "이전 작업의 완료를 기다리고 있습니다. 같은 작업을 겹쳐 보내지 않습니다."));
        return;
    }
    if (bCleanupUncertain)
    {
        EmitSessionError(TEXT(
            "세션 정리 상태를 확정할 수 없습니다. 상태 확인 또는 게임 재시작이 필요합니다."));
        return;
    }

    bReturningToTitle = false;
    StartCleanup();
}

void UMWMultiplayerSubsystem::ArmSessionTimeout()
{
    ClearSessionTimeout();
    GetGameInstance()->GetTimerManager().SetTimer(
        SessionTimeoutHandle,
        FTimerDelegate::CreateUObject(
            this, &UMWMultiplayerSubsystem::HandleSessionTimeout,
            SessionRequestId),
        SessionOperationTimeoutSeconds, false);
}

void UMWMultiplayerSubsystem::ClearSessionTimeout()
{
    if (UGameInstance* GI = GetGameInstance())
    {
        GI->GetTimerManager().ClearTimer(SessionTimeoutHandle);
    }
}

void UMWMultiplayerSubsystem::HandleSessionTimeout(uint64 RequestId)
{
    if (!IsCurrentSessionRequest(RequestId))
    {
        return;
    }
    ClearSessionTimeout();

    if (bReturningToTitle)
    {
        SetSessionState(EMWSessionState::CleanupRequired);
        EmitSessionError(TEXT("타이틀 이동 완료를 확인하지 못했습니다."));
        return;
    }

    if (CleanupCause == EMWCleanupCause::None && SessionState == EMWSessionState::Searching)
    {
        // Keep the delegate until the in-flight EOS operation has actually completed.
        CleanupCause = EMWCleanupCause::RoomListFailure;
        SetSessionState(EMWSessionState::CleanupRequired);
        EmitSessionError(TEXT("방 목록 응답의 제한시간을 초과했습니다. 응답 확인 또는 정리가 필요합니다."));
        OnRoomsUpdated.Broadcast(false, LastSessionError);
        return;
    }
    if (CleanupCause == EMWCleanupCause::None && SessionState == EMWSessionState::Joining)
    {
        FailJoin(TEXT("방 연결 또는 로비 정보 수신의 제한시간을 초과했습니다."));
        return;
    }
    if (CleanupCause == EMWCleanupCause::None &&
        SessionState == EMWSessionState::Creating)
    {
        FailCreation(TEXT("방 생성 또는 준비 확인의 제한시간을 초과했습니다."));
        return;
    }

    if (CleanupCause != EMWCleanupCause::None)
    {
        CloseServerAdmission();
        SetSessionState(EMWSessionState::CleanupRequired);
        EmitSessionError(TEXT(
            "정리 결과 확인의 제한시간을 초과했습니다. 재생성을 보류합니다."));
    }
}

void UMWMultiplayerSubsystem::HandleTravelFailure(
    UWorld* World, ETravelFailure::Type Failure, const FString& Error)
{
    if (bDeinitializing || !IsOwnWorld(World))
    {
        return;
    }
    if (!bReturningToTitle && (SessionState == EMWSessionState::Joining || CleanupCause == EMWCleanupCause::JoinFailure))
    {
        bClientTravelPending = false;
        if (CleanupCause != EMWCleanupCause::None) StartCleanup();
        else FailJoin(FString::Printf(TEXT("방 이동 실패: %s"), *Error));
        return;
    }
    if (SessionState == EMWSessionState::Creating &&
        CleanupCause == EMWCleanupCause::None)
    {
        FailCreation(FString::Printf(TEXT("맵 이동 실패: %s"), *Error));
    }
    else if (bReturningToTitle)
    {
        ClearSessionTimeout();
        SetSessionState(EMWSessionState::CleanupRequired);
        EmitSessionError(FString::Printf(TEXT("타이틀 이동 실패: %s"), *Error));
    }
}

void UMWMultiplayerSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Failure, const FString& Error)
{
    if (bDeinitializing)
    {
        return;
    }

    // PendingConnectionFailure is broadcast with a null World before map travel.
    // Resolve its GameInstance so another PIE client's failure cannot end our join.
    const bool bPendingJoinFailure = Driver && Driver->NetDriverName == NAME_PendingNetDriver &&
        (SessionState == EMWSessionState::Joining || CleanupCause == EMWCleanupCause::JoinFailure);
    if (bPendingJoinFailure)
    {
        const FWorldContext* Context = GEngine ? GEngine->GetWorldContextFromPendingNetGameNetDriver(Driver) : nullptr;
        if (!Context || Context->OwningGameInstance != GetGameInstance())
        {
            return;
        }
    }
    else if (!IsOwnWorld(World) || (Driver && Driver->NetDriverName != NAME_GameNetDriver))
    {
        return;
    }

    if (!bReturningToTitle && (SessionState == EMWSessionState::Joining || CleanupCause == EMWCleanupCause::JoinFailure))
    {
        bClientTravelPending = false;
        if (CleanupCause != EMWCleanupCause::None) StartCleanup();
        else FailJoin(FString::Printf(TEXT("방 연결 실패: %s"), *Error));
        return;
    }

    if (SessionState == EMWSessionState::Creating && CleanupCause == EMWCleanupCause::None)
    {
        FailCreation(FString::Printf(TEXT("연결 준비 실패: %s"), *Error));
    }
    else if (SessionState == EMWSessionState::Hosting || SessionState == EMWSessionState::Joined)
    {
        ++SessionRequestId;
        CleanupCause = EMWCleanupCause::Leave;
        CloseServerAdmission();
        EmitSessionError(FString::Printf(TEXT("네트워크 연결 실패: %s"), *Error));
        SetSessionState(EMWSessionState::Leaving);
        StartCleanup();
    }
}

void UMWMultiplayerSubsystem::ClearSessionApiDelegates()
{
    if (SessionApi.IsValid())
    {
        if (CreateSessionHandle.IsValid())
        {
            SessionApi->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionHandle);
        }
        if (UpdateSessionHandle.IsValid())
        {
            SessionApi->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateSessionHandle);
        }
        if (DestroySessionHandle.IsValid())
        {
            SessionApi->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionHandle);
        }
    }

    CreateSessionHandle.Reset();
    UpdateSessionHandle.Reset();
    DestroySessionHandle.Reset();
}

void UMWMultiplayerSubsystem::EmitSessionError(const FString& Error)
{
    LastSessionError = Error;
    UE_LOG(LogMWEOS, Warning, TEXT("%s"), *Error);
    OnSessionError.Broadcast(Error);
}
