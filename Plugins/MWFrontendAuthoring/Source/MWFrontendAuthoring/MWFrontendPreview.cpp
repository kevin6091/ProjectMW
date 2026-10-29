#include "MWFrontendPreview.h"

#include "Components/TextBlock.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"

TArray<FMWFrontendTheme> UMWFrontendPreviewConnection::GetThemes_Implementation() const
{
	FMWFrontendTheme Theme;
	Theme.Id = TEXT("UI_TEST_THEME");
	Theme.DisplayName = FText::FromString(TEXT("UI 테스트 전용 테마"));
	TArray<FMWFrontendTheme> Themes = {Theme};
	// Multiple explicit fixture options exercise scrolling without inventing production maps.
	for (int32 Index = 2; Index <= 8; ++Index)
	{
		FMWFrontendTheme Extra;
		Extra.Id = FName(*FString::Printf(TEXT("UI_TEST_THEME_%02d"), Index));
		Extra.DisplayName = FText::FromString(FString::Printf(TEXT("UI 테스트 %02d · 스크롤 선택 확인"), Index));
		Themes.Add(Extra);
	}
	return Themes;
}

TArray<FMWRoomSearchResult> UMWFrontendPreviewConnection::GetRooms_Implementation() const
{
	if (!bSearchFinished) { return {}; }
	FMWRoomSearchResult Open;
	Open.SessionId = TEXT("UI_TEST_ONLY_OPEN"); Open.RoomName = FText::FromString(TEXT("[UI 테스트] 같은 이름의 방"));
	Open.ThemeName = FText::FromString(TEXT("테스트 테마")); Open.PlayerCount = 1; Open.MaxPlayers = 4; Open.bJoinable = true;
	FMWRoomSearchResult Full = Open;
	Full.SessionId = TEXT("UI_TEST_ONLY_FULL"); Full.PlayerCount = 4; Full.bJoinable = false;
	Full.UnavailableReason = FText::FromString(TEXT("만석 · UI 테스트"));
	TArray<FMWRoomSearchResult> Rooms={Open,Full};
	for (int32 Index=2;Index<PreviewRoomCount;++Index)
	{
		FMWRoomSearchResult Extra=Open;
		Extra.SessionId=FString::Printf(TEXT("UI_TEST_ONLY_SCROLL_%02d"),Index);
		Extra.RoomName=FText::FromString(FString::Printf(TEXT("[UI 테스트] 스크롤 행 %02d"),Index));
		Extra.bJoinable=false;
		Extra.UnavailableReason=NSLOCTEXT("MWFrontendTest","ScrollOnly","스크롤 검증 전용");
		Rooms.Add(Extra);
	}
	return Rooms;
}

bool UMWFrontendPreviewConnection::SetPreviewRoomCount(int32 Count)
{
	if (Count<2 || Count>64) { return false; }
	PreviewRoomCount=Count;
	OnChanged.Broadcast();
	return true;
}

bool UMWFrontendPreviewConnection::SearchRooms_Implementation()
{
	if (Operation != EMWFrontendOperation::Idle) { return false; }
	++SearchCount; bSearchFinished = false; Operation = EMWFrontendOperation::Searching;
	OnChanged.Broadcast();
	if (bCompleteSearchImmediately) { CompleteSearch(); }
	return true;
}

void UMWFrontendPreviewConnection::CompleteSearch()
{
	if (Operation != EMWFrontendOperation::Searching) { return; }
	bSearchFinished = true; Operation = EMWFrontendOperation::Idle; OnChanged.Broadcast();
}

void UMWFrontendPreviewConnection::PrepareLobby(APlayerController* Controller)
{
	OwnerController = Controller;
	Lobby = {};
	if (!Controller || !Controller->PlayerState || !Controller->GetWorld()->GetGameState()) { return; }
	Lobby.GameState = Controller->GetWorld()->GetGameState();
	Lobby.LocalPlayerState = Controller->PlayerState;
	Lobby.SessionId = TEXT("UI_TEST_ONLY_LOBBY");
	Lobby.RoomName = FText::FromString(TEXT("[UI 테스트] 공통 로비"));
	Lobby.ThemeName = FText::FromString(TEXT("UI 테스트 전용 테마"));
	Lobby.MaxPlayers = 3; Lobby.bLobbyPhase = true; Lobby.bConnectionReady = false;
	FMWLobbyParticipantView Host;
	Host.PlayerState = Controller->PlayerState; Host.bHost = true;
	Lobby.Participants.Add(Host);
}

bool UMWFrontendPreviewConnection::CreateRoom_Implementation(const FMWCreateRoomRequest& Request)
{
	FText Error;
	if (Operation != EMWFrontendOperation::Idle || !Request.Validate(GetThemes(), Error)) { return false; }
	++CreateCount;
	Lobby.RoomName = FText::FromString(Request.RoomName); Lobby.MaxPlayers = Request.MaxPlayers;
	for (const FMWFrontendTheme& Theme : GetThemes())
	{
		if (Theme.Id == Request.MapThemeId) { Lobby.ThemeName = Theme.DisplayName; break; }
	}
	Lobby.bConnectionReady = true; Operation = EMWFrontendOperation::Idle; OnChanged.Broadcast();
	return true;
}

bool UMWFrontendPreviewConnection::JoinRoom_Implementation(const FString& SessionId)
{
	if (Operation != EMWFrontendOperation::Idle || SessionId != TEXT("UI_TEST_ONLY_OPEN")) { return false; }
	Lobby.bConnectionReady = true; OnChanged.Broadcast(); return true;
}

bool UMWFrontendPreviewConnection::LeaveRoom_Implementation()
{
	Lobby.bConnectionReady = false; OnChanged.Broadcast(); return true;
}

bool UMWFrontendPreviewConnection::SetReady_Implementation(bool bReady)
{
	if (!Lobby.bConnectionReady || Lobby.Participants.IsEmpty()) { return false; }
	Lobby.Participants[0].bReady = bReady;
	Lobby.bCanStart = bReady && Lobby.Participants.Num() >= 2;
	OnChanged.Broadcast(); return true;
}

bool UMWFrontendPreviewConnection::StartMatch_Implementation()
{
	if (!Lobby.bCanStart) { return false; }
	Lobby.bLobbyPhase = false; OnChanged.Broadcast(); return true;
}

bool UMWFrontendPreviewConnection::CancelOperation_Implementation()
{
	Operation = EMWFrontendOperation::Idle; bSearchFinished = false; OnChanged.Broadcast(); return true;
}

bool UMWFrontendPreviewConnection::SetPreviewParticipantCount(int32 Count)
{
	APlayerController* Controller = OwnerController.Get();
	AGameStateBase* State = Lobby.GameState;
	if (!Controller || !IsValid(State) || Count < 1 || Count > Lobby.MaxPlayers) { return false; }
	while (Lobby.Participants.Num() > Count)
	{
		APlayerState* Extra = Lobby.Participants.Last().PlayerState;
		State->RemovePlayerState(Extra); Extra->Destroy(); Lobby.Participants.Pop();
	}
	while (Lobby.Participants.Num() < Count)
	{
		APlayerState* Extra = Controller->GetWorld()->SpawnActor<APlayerState>();
		if (!Extra) { return false; }
		Extra->SetPlayerName(FString::Printf(TEXT("UI 테스트 참가자 %d"), Lobby.Participants.Num()+1));
		State->AddPlayerState(Extra);
		FMWLobbyParticipantView Participant; Participant.PlayerState=Extra; Participant.bReady=true;
		Lobby.Participants.Add(Participant);
	}
	Lobby.bCanStart = Count >= 2 && Lobby.Participants[0].bReady;
	OnChanged.Broadcast();
	return true;
}
