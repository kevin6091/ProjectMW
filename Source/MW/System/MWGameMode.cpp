
#include "MWGameMode.h"
#include "System/MWGameState.h"
#include "System/MWPlayerState.h"
#include "Player/MWPlayerController.h"
#include "System/MultiplayerSubSystem/MWMultiplayerSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameSession.h"
#include "OnlineSubsystemUtils.h"

AMWGameMode::AMWGameMode()
{
    GameStateClass = AMWGameState::StaticClass();
    PlayerStateClass = AMWPlayerState::StaticClass();
    PlayerControllerClass = AMWPlayerController::StaticClass();

    // 첫 로비에서는 차량을 자동 생성하지 않습니다.
    DefaultPawnClass = nullptr;
}

void AMWGameMode::InitGameState()
{
    Super::InitGameState();
    if (GetWorld() && GetWorld()->IsGameWorld())
    {
        RegistrationApi = Online::GetSessionInterface(GetWorld());
        if (RegistrationApi.IsValid())
        {
            PlayerRegistrationHandle = RegistrationApi->AddOnRegisterPlayersCompleteDelegate_Handle(
                FOnRegisterPlayersCompleteDelegate::CreateUObject(this, &ThisClass::HandlePlayersRegistered));
        }
        ApplyApprovedRoomSettings();
    }
}

bool AMWGameMode::ApplyApprovedRoomSettings()
{
    // PIE is also an editor world. Permit game worlds and reject editor previews.
    UWorld* World = GetWorld();
    if (!World || !World->IsGameWorld())
    {
        return false;
    }

    // 2. GameInstance 유효성 검사 (에디터 BP 열기 시 nullptr일 가능성 높음)
    UGameInstance* GameInstance = GetGameInstance();
    if (!GameInstance)
    {
        return false;
    }

    // 3. 서브시스템 검사
    UMWMultiplayerSubsystem* Sessions = GameInstance->GetSubsystem<UMWMultiplayerSubsystem>();
    if (!Sessions)
    {
        return false;
    }

    // 4. GameState 및 GameSession 검사
    AMWGameState* GS = World->GetGameState<AMWGameState>();
    if (!GS || !GameSession)
    {
        return false;
    }

    // 5. 방 설정 가져오기 및 적용
    FMWRoomSettings Approved;
    if (!Sessions->GetApprovedRoomSettingsForWorld(World, Approved))
    {
        return false;
    }

    const FString SessionId = Sessions->GetActiveRoomSessionId();
    if (SessionId.IsEmpty()) return false;
    GS->ApplyRoomSettingsFromServer(Approved, SessionId);
    GameSession->MaxPlayers = Approved.MaxPlayers;
    GameSession->MaxSpectators = 0;
    bRoomSettingsApplied = true;

    return true;
}
void AMWGameMode::StartPlay()
{
    Super::StartPlay();
    TryPrepareLobby();
}

FString AMWGameMode::InitNewPlayer(
    APlayerController* NewPlayerController,
    const FUniqueNetIdRepl& UniqueId,
    const FString& Options, const FString& Portal)
{
    if (GetNetMode() == NM_ListenServer && UniqueId.IsValid() &&
        NewPlayerController->IsLocalController())
    {
        GetGameInstance()->GetSubsystem<UMWMultiplayerSubsystem>()
            ->ExpectHostRegistration(GetWorld());
    }

    // 기본 GameSession/PlayerState 등록 경로를 그대로 실행합니다.
    return Super::InitNewPlayer(
        NewPlayerController, UniqueId, Options, Portal);
}

void AMWGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    AMWGameState* GS = GetWorld()->GetGameState<AMWGameState>();
    const bool bLocalHost =
        GetNetMode() == NM_ListenServer && NewPlayer->IsLocalController();

    // PreLogin 후 입장 잠금/정원이 바뀐 경로를 다시 검사합니다.
    if (!bLocalHost && (!bAcceptingPlayers || !GS ||
        GetNumPlayers() > GS->GetRoomSettings().MaxPlayers))
    {
        if (GameSession)
        {
            GameSession->KickPlayer(NewPlayer,
                FText::FromString(TEXT("방이 닫혔거나 정원이 찼습니다.")));
        }
        return;
    }

    if (AMWPlayerState* PS = NewPlayer->GetPlayerState<AMWPlayerState>())
    {
        PS->SetLobbyMemberIdFromServer(NextLobbyMemberId++);
        PS->SetLobbyFlagsFromServer(bLocalHost, false);
    }
    TryPrepareLobby();
}

void AMWGameMode::HandlePlayersRegistered(FName SessionName,
    const TArray<FUniqueNetIdRef>& Players, bool bWasSuccessful)
{
    if (SessionName != NAME_GameSession || !GetWorld()) return;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        AMWPlayerState* PS = PC ? PC->GetPlayerState<AMWPlayerState>() : nullptr;
        if (!PS || !PS->GetUniqueId().IsValid()) continue;
        const bool bIncluded = Players.ContainsByPredicate([PS](const FUniqueNetIdRef& Id)
        {
            return *PS->GetUniqueId().GetUniqueNetId() == *Id;
        });
        if (!bIncluded) continue;
        PS->SetSessionRegistrationConfirmedFromServer(bWasSuccessful);
        if (!bWasSuccessful && !PC->IsLocalController() && GameSession)
        {
            GameSession->KickPlayer(PC, FText::FromString(TEXT("온라인 세션 참가자 등록에 실패했습니다.")));
        }
    }
}

void AMWGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (RegistrationApi.IsValid() && PlayerRegistrationHandle.IsValid())
    {
        RegistrationApi->ClearOnRegisterPlayersCompleteDelegate_Handle(PlayerRegistrationHandle);
    }
    PlayerRegistrationHandle.Reset();
    RegistrationApi.Reset();
    Super::EndPlay(EndPlayReason);
}

void AMWGameMode::PreLogin(const FString& Options,
    const FString& Address, const FUniqueNetIdRepl& UniqueId,
    FString& ErrorMessage)
{
    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
    if (!ErrorMessage.IsEmpty())
    {
        return;
    }

    if (!bRoomSettingsApplied || !bAcceptingPlayers)
    {
        ErrorMessage = TEXT("방을 준비 중이거나 입장이 닫혔습니다.");
        return;
    }
    if (!UniqueId.IsValid())
    {
        ErrorMessage = TEXT("온라인 사용자 정보를 확인할 수 없습니다.");
        return;
    }
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        const APlayerController* PC = It->Get();
        const APlayerState* PS = PC ? PC->PlayerState : nullptr;
        if (PS && PS->GetUniqueId() == UniqueId)
        {
            ErrorMessage = TEXT("같은 계정이 이미 이 방에 참가 중입니다.");
            return;
        }
    }
}

void AMWGameMode::HandleStartingNewPlayer_Implementation(
    APlayerController* NewPlayer)
{
    // 로비에서는 Super의 기본 Pawn 자동 스폰 경로를 실행하지 않습니다.
    // 차량은 이후 승인 VehicleDefinition을 사용하는 한 경로에서 생성합니다.
}

void AMWGameMode::TryPrepareLobby()
{
    if (GetNetMode() != NM_ListenServer || !GetWorld()->HasBegunPlay())
    {
        return;
    }
    UMWMultiplayerSubsystem* Sessions =
        GetGameInstance()->GetSubsystem<UMWMultiplayerSubsystem>();
    if (!Sessions || Sessions->GetSessionState() != EMWSessionState::Creating)
    {
        return;
    }

    if (!bRoomSettingsApplied && !ApplyApprovedRoomSettings())
    {
        return;
    }

    for (FConstPlayerControllerIterator It =
        GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (PC && PC->IsLocalController())
        {
            AMWPlayerState* PS = PC->GetPlayerState<AMWPlayerState>();
            if (PS && PS->GetUniqueId().IsValid())
            {
                PS->SetLobbyFlagsFromServer(true, false);
                Sessions->NotifyListenWorldReady(GetWorld());
                return;
            }
        }
    }
}
