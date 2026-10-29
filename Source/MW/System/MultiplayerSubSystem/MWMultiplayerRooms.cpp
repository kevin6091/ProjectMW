#include "System/MultiplayerSubSystem/MWMultiplayerSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSubsystemEOSTypesPublic.h"
#include "Player/MWPlayerController.h"
#include "System/MWGameState.h"
#include "System/MWPlayerState.h"

namespace
{
	const FName RoomNameKey(TEXT("MW_ROOM_NAME"));
	const FName ThemeKey(TEXT("MW_MAP_THEME"));
	const FName StatusKey(TEXT("MW_ROOM_STATUS"));
	const FName ProtocolKey(TEXT("MW_PROTOCOL_ID"));
	const FString Protocol(TEXT("MW_DEV_V1"));
	const FString Bucket(TEXT("MW_DEV_V1"));
}

void UMWMultiplayerSubsystem::InvalidateRoomList()
{
	AvailableRooms.Reset();
	RoomListRevision = RoomListRevision == MAX_int32 ? 1 : RoomListRevision + 1;
}

void UMWMultiplayerSubsystem::ClearRoomListDelegate()
{
	if (SessionApi.IsValid() && RoomListHandle.IsValid())
	{
		SessionApi->ClearOnFindSessionsCompleteDelegate_Handle(RoomListHandle);
	}
	RoomListHandle.Reset();
}

void UMWMultiplayerSubsystem::ClearJoinDelegate()
{
	if (SessionApi.IsValid() && JoinSessionHandle.IsValid())
	{
		SessionApi->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionHandle);
	}
	JoinSessionHandle.Reset();
}

FString UMWMultiplayerSubsystem::GetActiveRoomSessionId() const
{
	const FNamedOnlineSession* Named = SessionApi.IsValid() ? SessionApi->GetNamedSession(NAME_GameSession) : nullptr;
	return Named && Named->SessionInfo.IsValid() ? Named->SessionInfo->GetSessionId().ToString() : FString();
}

bool UMWMultiplayerSubsystem::DescribeRoom(const FOnlineSessionSearchResult& Result, FMWRoomInfo& OutRoom) const
{
	if (!Result.IsValid() || !Result.Session.SessionInfo.IsValid()) return false;
	const FOnlineSessionSettings& Settings = Result.Session.SessionSettings;
	FString RoomProtocol, Status, Theme;
	if (Settings.bUseLobbiesIfAvailable ||
		!Settings.Get(ProtocolKey, RoomProtocol) || RoomProtocol != Protocol ||
		!Settings.Get(StatusKey, Status) || Status != TEXT("Lobby") ||
		!Settings.Get(RoomNameKey, OutRoom.RoomName) || OutRoom.RoomName.IsEmpty() ||
		!Settings.Get(ThemeKey, Theme)) return false;
	OutRoom.SessionId = Result.GetSessionIdStr();
	if (OutRoom.SessionId.IsEmpty()) return false;
	OutRoom.MapThemeId = FName(*Theme);
	OutRoom.MaxPlayers = Settings.NumPublicConnections;
	if (Settings.NumPrivateConnections != 0 || OutRoom.MaxPlayers < MinRoomPlayers || OutRoom.MaxPlayers > MaxRoomPlayers) return false;
	// This project has public slots only. UE 5.8 EOS AddSearchResult currently writes
	// EOS's public availability into NumOpenPrivateConnections; total free slots works in both layouts.
	const int32 OpenSlots = FMath::Clamp(Result.Session.NumOpenPublicConnections + Result.Session.NumOpenPrivateConnections, 0, OutRoom.MaxPlayers);
	OutRoom.CurrentPlayers = OutRoom.MaxPlayers - OpenSlots;
	const FGameplayTag ThemeTag = FGameplayTag::RequestGameplayTag(OutRoom.MapThemeId, false);
	FString Error;
	if (!ThemeTag.IsValid() || ResolveMapPackageName(ThemeTag, Error).IsNone())
	{
		OutRoom.UnavailableReason = TEXT("현재 게임에서 지원하지 않는 맵 테마입니다.");
	}
	else if (OpenSlots <= 0)
	{
		OutRoom.UnavailableReason = TEXT("정원이 가득 찼습니다.");
	}
	else
	{
		const IOnlineIdentityPtr Identity = ResolveIdentity(Error);
		const FUniqueNetIdPtr LocalId = Identity.IsValid() ? Identity->GetUniquePlayerId(LoginLocalUserNum) : nullptr;
		if (LocalId.IsValid() && Result.Session.OwningUserId.IsValid() && *LocalId == *Result.Session.OwningUserId)
		{
			OutRoom.UnavailableReason = TEXT("같은 계정이 만든 방입니다. 다른 계정으로 참가해 주세요.");
		}
	}
	OutRoom.bCanJoin = OutRoom.UnavailableReason.IsEmpty();
	return true;
}

void UMWMultiplayerSubsystem::RequestRefreshRooms()
{
	if (bDeinitializing || SessionState != EMWSessionState::None) return;
	if (!IsLoggedIn() || bLoginInProgress)
	{
		EmitSessionError(TEXT("방 목록을 보려면 먼저 로그인해 주세요."));
		OnRoomsUpdated.Broadcast(false, LastSessionError);
		return;
	}
	FString Error;
	IOnlineSessionPtr Api = ResolveSession(Error);
	if (!Api.IsValid())
	{
		EmitSessionError(Error);
		OnRoomsUpdated.Broadcast(false, Error);
		return;
	}
	if (Api->GetNamedSession(NAME_GameSession))
	{
		EmitSessionError(TEXT("참가 중인 세션을 먼저 종료해 주세요."));
		OnRoomsUpdated.Broadcast(false, LastSessionError);
		return;
	}
	SessionApi = Api;
	InvalidateRoomList();
	RoomSearch = MakeShared<FOnlineSessionSearch>();
	RoomSearch->bIsLanQuery = false;
	RoomSearch->MaxSearchResults = 100;
	RoomSearch->QuerySettings.Set(SEARCH_LOBBIES, false, EOnlineComparisonOp::Equals);
	RoomSearch->QuerySettings.Set(OSSEOS_BUCKET_ID_ATTRIBUTE_KEY, Bucket, EOnlineComparisonOp::Equals);
	RoomSearch->QuerySettings.Set(ProtocolKey, Protocol, EOnlineComparisonOp::Equals);
	RoomSearch->QuerySettings.Set(StatusKey, FString(TEXT("Lobby")), EOnlineComparisonOp::Equals);
	LastSessionError.Reset();
	CleanupCause = EMWCleanupCause::None;
	const uint64 RequestId = ++SessionRequestId;
	RoomListHandle = Api->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::HandleRoomsComplete, RequestId));
	SetSessionState(EMWSessionState::Searching);
	ArmSessionTimeout();
	if (!Api->FindSessions(LoginLocalUserNum, RoomSearch.ToSharedRef()) && IsCurrentSessionRequest(RequestId) && RoomListHandle.IsValid())
	{
		ClearSessionTimeout();
		ClearRoomListDelegate();
		RoomSearch.Reset();
		SetSessionState(EMWSessionState::None);
		EmitSessionError(TEXT("방 목록 요청을 시작하지 못했습니다."));
		OnRoomsUpdated.Broadcast(false, LastSessionError);
	}
}

void UMWMultiplayerSubsystem::HandleRoomsComplete(bool bWasSuccessful, uint64 RequestId)
{
	if (!IsCurrentSessionRequest(RequestId) || !RoomListHandle.IsValid()) return;
	ClearSessionTimeout();
	ClearRoomListDelegate();
	if (CleanupCause != EMWCleanupCause::None)
	{
		RoomSearch.Reset();
		StartCleanup();
		return;
	}
	if (bWasSuccessful && RoomSearch)
	{
		TSet<FString> Seen;
		for (const FOnlineSessionSearchResult& Result : RoomSearch->SearchResults)
		{
			FMWRoomInfo Room;
			if (DescribeRoom(Result, Room) && !Seen.Contains(Room.SessionId))
			{
				Seen.Add(Room.SessionId);
				AvailableRooms.Add(MoveTemp(Room));
			}
		}
		AvailableRooms.Sort([](const FMWRoomInfo& A, const FMWRoomInfo& B)
		{
			return A.RoomName == B.RoomName ? A.SessionId < B.SessionId : A.RoomName < B.RoomName;
		});
	}
	else
	{
		RoomSearch.Reset();
		EmitSessionError(TEXT("방 목록을 불러오지 못했습니다. 다시 시도해 주세요."));
	}
	SetSessionState(EMWSessionState::None);
	OnRoomsUpdated.Broadcast(bWasSuccessful, bWasSuccessful ? FString() : LastSessionError);
}

void UMWMultiplayerSubsystem::RequestJoinRoom(const FString& SessionId, int32 ListRevision)
{
	if (bDeinitializing || SessionState != EMWSessionState::None) return;
	if (!IsLoggedIn() || bLoginInProgress)
	{
		EmitSessionError(TEXT("먼저 로그인해 주세요."));
		OnJoinRoomFinished.Broadcast(false, LastSessionError);
		return;
	}
	const FMWRoomInfo* Row = AvailableRooms.FindByPredicate([&SessionId](const FMWRoomInfo& R) { return R.SessionId == SessionId; });
	if (ListRevision != RoomListRevision || !Row || !RoomSearch)
	{
		EmitSessionError(TEXT("방 목록이 변경되었습니다. 새로고침 후 다시 선택해 주세요."));
		OnJoinRoomFinished.Broadcast(false, LastSessionError);
		return;
	}
	const FOnlineSessionSearchResult* Found = RoomSearch->SearchResults.FindByPredicate(
		[&SessionId](const FOnlineSessionSearchResult& R) { return R.GetSessionIdStr() == SessionId; });
	FMWRoomInfo Checked;
	if (!Found || !DescribeRoom(*Found, Checked) || !Checked.bCanJoin)
	{
		EmitSessionError(Checked.UnavailableReason.IsEmpty() ? TEXT("선택한 방에 참가할 수 없습니다. 목록을 새로고침해 주세요.") : Checked.UnavailableReason);
		OnJoinRoomFinished.Broadcast(false, LastSessionError);
		return;
	}
	FString Error;
	const FName Title = ResolveTitleMapPackageName(Error);
	const FGameplayTag Theme = FGameplayTag::RequestGameplayTag(Checked.MapThemeId, false);
	const FName Map = ResolveMapPackageName(Theme, Error);
	IOnlineSessionPtr Api = ResolveSession(Error);
	if (Title.IsNone() || Map.IsNone() || !Api.IsValid())
	{
		EmitSessionError(Error.IsEmpty() ? TEXT("참가할 방의 설정을 확인할 수 없습니다.") : Error);
		OnJoinRoomFinished.Broadcast(false, LastSessionError);
		return;
	}
	if (Api->GetNamedSession(NAME_GameSession))
	{
		EmitSessionError(TEXT("이전 세션을 먼저 종료해 주세요."));
		OnJoinRoomFinished.Broadcast(false, LastSessionError);
		return;
	}
	const FOnlineSessionSearchResult Selected = *Found;
	SessionApi = Api;
	ReturnMapPackageName = Title;
	PendingJoinSessionId = SessionId;
	PendingJoinThemeId = Theme;
	PendingJoinMapPackage = Map;
	bJoinConnecting = false;
	bClientTravelPending = false;
	bJoinResultPending = true;
	CleanupCause = EMWCleanupCause::None;
	bReturningToTitle = false;
	bCleanupUncertain = false;
	LastSessionError.Reset();
	const uint64 RequestId = ++SessionRequestId;
	JoinSessionHandle = Api->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleJoinSessionComplete, RequestId));
	InvalidateRoomList();
	SetSessionState(EMWSessionState::Joining);
	ArmSessionTimeout();
	if (!Api->JoinSession(LoginLocalUserNum, NAME_GameSession, Selected) && IsCurrentSessionRequest(RequestId) && JoinSessionHandle.IsValid())
	{
		ClearJoinDelegate();
		FailJoin(TEXT("방 참가 요청을 시작하지 못했습니다."));
	}
}

void UMWMultiplayerSubsystem::HandleJoinSessionComplete(FName SessionName,
	EOnJoinSessionCompleteResult::Type Result, uint64 RequestId)
{
	if (!IsCurrentSessionRequest(RequestId) || SessionName != NAME_GameSession || !JoinSessionHandle.IsValid()) return;
	ClearSessionTimeout();
	ClearJoinDelegate();
	if (CleanupCause != EMWCleanupCause::None)
	{
		StartCleanup();
		return;
	}
	if (Result != EOnJoinSessionCompleteResult::Success)
	{
		FailJoin(Result == EOnJoinSessionCompleteResult::SessionIsFull
			? TEXT("방의 정원이 가득 찼습니다.") : TEXT("방에 참가하지 못했습니다. 방이 종료되었을 수 있습니다."));
		return;
	}
	FString Address;
	APlayerController* PC = GetGameInstance() ? UGameplayStatics::GetPlayerController(GetGameInstance(), LoginLocalUserNum) : nullptr;
	if (!PC || !SessionApi.IsValid() || !SessionApi->GetResolvedConnectString(NAME_GameSession, Address) || Address.IsEmpty())
	{
		FailJoin(TEXT("선택한 방의 연결 주소를 확인하지 못했습니다."));
		return;
	}
	bJoinConnecting = true;
	bClientTravelPending = true;
	ArmSessionTimeout();
	PC->ClientTravel(Address, TRAVEL_Absolute);
}

void UMWMultiplayerSubsystem::NotifyClientLobbyReady(AMWPlayerController* Controller)
{
	if (bDeinitializing || SessionState != EMWSessionState::Joining ||
		CleanupCause != EMWCleanupCause::None || !bJoinConnecting || bClientTravelPending ||
		!Controller || !Controller->IsLocalController()) return;
	UWorld* World = Controller->GetWorld();
	if (!IsOwnWorld(World) || World->GetNetMode() != NM_Client || !World->GetNetDriver()) return;
	AMWGameState* GS = World->GetGameState<AMWGameState>();
	AMWPlayerState* PS = Controller->GetPlayerState<AMWPlayerState>();
	if (!GS || !PS || GS->GetRoomSessionId().IsEmpty() || !PS->GetUniqueId().IsValid() ||
		PS->GetLobbyMemberId() < 1 || !PS->IsSessionRegistrationConfirmed() || !GS->PlayerArray.Contains(PS)) return;
	const FMWRoomSettings Settings = GS->GetRoomSettings();
	if (Settings.RoomName.IsEmpty() || !Settings.MapThemeId.IsValid()) return;
	if (GS->GetRoomSessionId() != PendingJoinSessionId || Settings.MapThemeId != PendingJoinThemeId ||
		FName(*UWorld::RemovePIEPrefix(World->GetOutermost()->GetName())) != PendingJoinMapPackage)
	{
		FailJoin(TEXT("접속한 서버의 방 정보가 선택한 방과 다릅니다."));
		return;
	}
	FString Error;
	const IOnlineIdentityPtr Identity = ResolveIdentity(Error);
	const FUniqueNetIdPtr LocalId = Identity.IsValid() ? Identity->GetUniquePlayerId(LoginLocalUserNum) : nullptr;
	if (!LocalId.IsValid() || *LocalId != *PS->GetUniqueId().GetUniqueNetId()) return;
	ClearSessionTimeout();
	bJoinConnecting = false;
	bJoinResultPending = false;
	HostWorld = World;
	SetSessionState(EMWSessionState::Joined);
	OnJoinRoomFinished.Broadcast(true, FString());
}

void UMWMultiplayerSubsystem::FailJoin(const FString& Error)
{
	const bool bNotify = bJoinResultPending;
	bJoinResultPending = false;
	CleanupCause = EMWCleanupCause::JoinFailure;
	LastSessionError = Error;
	if (HasPendingSessionWork()) SetSessionState(EMWSessionState::CleanupRequired);
	else StartCleanup();
	EmitSessionError(Error);
	if (bNotify) OnJoinRoomFinished.Broadcast(false, Error);
}
