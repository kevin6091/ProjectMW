#include "Frontend/MWFrontendConnection.h"

#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "MWFrontendConnection"

bool FMWCreateRoomRequest::Validate(const TArray<FMWFrontendTheme>& Themes, FText& OutError) const
{
	const FString TrimmedName = RoomName.TrimStartAndEnd();
	if (TrimmedName.IsEmpty() || TrimmedName.Len() > 32)
	{
		OutError = LOCTEXT("RoomNameLength", "방 이름을 1~32자로 입력하세요.");
		return false;
	}
	for (const TCHAR Character : TrimmedName)
	{
		if (FChar::IsControl(Character))
		{
			OutError = LOCTEXT("RoomNameControl", "방 이름에는 줄바꿈이나 제어 문자를 사용할 수 없습니다.");
			return false;
		}
	}
	if (MaxPlayers < 2 || MaxPlayers > 4)
	{
		OutError = LOCTEXT("Capacity", "최대 인원은 호스트를 포함해 2~4명입니다.");
		return false;
	}
	if (MapThemeId.IsNone() || !Themes.ContainsByPredicate([this](const FMWFrontendTheme& Theme) { return Theme.Id == MapThemeId; }))
	{
		OutError = LOCTEXT("Theme", "사용 가능한 맵 테마를 선택하세요.");
		return false;
	}
	OutError = FText::GetEmpty();
	return true;
}

bool FMWRoomSearchResult::IsJoinable() const
{
	return bJoinable && !SessionId.IsEmpty() && MaxPlayers >= 2 && MaxPlayers <= 4 && PlayerCount >= 1 && PlayerCount < MaxPlayers;
}

const FMWLobbyParticipantView* FMWLobbyView::FindLocalParticipant() const
{
	return Participants.FindByPredicate([this](const FMWLobbyParticipantView& Participant) { return Participant.PlayerState == LocalPlayerState; });
}

bool FMWLobbyView::IsReadyFor(const UWorld* World, const APlayerState* OwningPlayerState) const
{
	return bLobbyPhase && IsConnectedTo(World, OwningPlayerState);
}

bool FMWLobbyView::IsConnectedTo(const UWorld* World, const APlayerState* OwningPlayerState) const
{
	if (!World || !IsValid(GameState) || GameState->GetWorld() != World || World->GetGameState() != GameState ||
		!IsValid(LocalPlayerState) || LocalPlayerState != OwningPlayerState || LocalPlayerState->GetWorld() != World ||
		!bConnectionReady || SessionId.IsEmpty() || MaxPlayers < 2 || MaxPlayers > 4 ||
		Participants.IsEmpty() || Participants.Num() > MaxPlayers || !FindLocalParticipant())
	{
		return false;
	}
	TSet<const APlayerState*> SeenPlayers;
	int32 HostCount = 0;
	for (const FMWLobbyParticipantView& Participant : Participants)
	{
		if (!IsValid(Participant.PlayerState) || !GameState->PlayerArray.Contains(Participant.PlayerState) || SeenPlayers.Contains(Participant.PlayerState))
		{
			return false;
		}
		SeenPlayers.Add(Participant.PlayerState);
		HostCount += Participant.bHost ? 1 : 0;
	}
	return HostCount == 1;
}

EMWFrontendOperation UMWFrontendConnection::GetOperation_Implementation() const { return EMWFrontendOperation::Idle; }
TArray<FMWFrontendTheme> UMWFrontendConnection::GetThemes_Implementation() const { return {}; }
TArray<FMWRoomSearchResult> UMWFrontendConnection::GetRooms_Implementation() const { return {}; }
FMWLobbyView UMWFrontendConnection::GetLobby_Implementation() const { return {}; }
bool UMWFrontendConnection::CreateRoom_Implementation(const FMWCreateRoomRequest& Request) { return false; }
bool UMWFrontendConnection::SearchRooms_Implementation() { return false; }
bool UMWFrontendConnection::JoinRoom_Implementation(const FString& SessionId) { return false; }
bool UMWFrontendConnection::LeaveRoom_Implementation() { return false; }
bool UMWFrontendConnection::SetReady_Implementation(bool bReady) { return false; }
bool UMWFrontendConnection::StartMatch_Implementation() { return false; }
bool UMWFrontendConnection::CancelOperation_Implementation() { return false; }

#undef LOCTEXT_NAMESPACE
