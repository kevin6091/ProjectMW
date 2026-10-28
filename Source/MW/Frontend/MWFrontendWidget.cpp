#include "Frontend/MWFrontendWidget.h"

#include "Frontend/MWRoomRowWidget.h"
#include "System/MWMultiplayerSubsystem.h"
#include "Frontend/MWRoadButtonWidget.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/ListView.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "LogChannel.h"

#define LOCTEXT_NAMESPACE "MWFrontend"

void UMWFrontendWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	bScreenActive = true;
	Multiplayer = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMWMultiplayerSubsystem>() : nullptr;
	if (Multiplayer)
	{
		Multiplayer->OnLoginFinished.AddUniqueDynamic(this, &ThisClass::HandleLoginFinished);
	}
	if (!LoginWidget && LoginWidgetClass && GetOwningPlayer())
	{
		LoginWidget = CreateWidget<UUserWidget>(GetOwningPlayer(), LoginWidgetClass);
		if (LoginWidget) { LoginHost->AddChild(LoginWidget); }
	}
	OpenCreateButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OpenCreate);
	OpenBrowserButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OpenBrowser);
	CreateButton->OnClicked.AddUniqueDynamic(this, &ThisClass::SubmitCreate);
	RefreshButton->OnClicked.AddUniqueDynamic(this, &ThisClass::RefreshRooms);
	JoinButton->OnClicked.AddUniqueDynamic(this, &ThisClass::JoinSelectedRoom);
	CreateBackButton->OnClicked.AddUniqueDynamic(this, &ThisClass::GoBack);
	BrowserBackButton->OnClicked.AddUniqueDynamic(this, &ThisClass::GoBack);
	ReadyButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ToggleReady);
	StartButton->OnClicked.AddUniqueDynamic(this, &ThisClass::StartMatch);
	LeaveButton->OnClicked.AddUniqueDynamic(this, &ThisClass::LeaveRoom);
	RoomList->OnItemSelectionChanged().RemoveAll(this);
	RoomList->OnItemSelectionChanged().AddUObject(this, &ThisClass::HandleRoomSelection);
	CapacityInput->ClearOptions();
	for (int32 Capacity = 2; Capacity <= 4; ++Capacity) { CapacityInput->AddOption(FString::FromInt(Capacity)); }
	// UE 5.8 ClearOptions can retain SelectedOption while clearing its Slate content.
	// Select a distinct entry before the default so the displayed value is regenerated.
	CapacityInput->SetSelectedOption(TEXT("2"));
	CapacityInput->SetSelectedOption(TEXT("3"));
	if (Connection)
	{
		Connection->OnChanged.AddUniqueDynamic(this, &ThisClass::HandleConnectionChanged);
		Connection->OnError.AddUniqueDynamic(this, &ThisClass::HandleConnectionError);
	}
	RefreshThemes();
	ShowPage(IsAuthenticated() ? EMWFrontendPage::Choice : EMWFrontendPage::Login);
	HandleConnectionChanged();
	if (!Multiplayer || !LoginWidget)
	{
		SetStatus(LOCTEXT("MissingLogin", "로그인 화면 또는 로그인 시스템을 불러오지 못했습니다."));
		UE_LOG(LogDefault, Error, TEXT("Frontend login dependency missing. Subsystem=%s Widget=%s"), *GetNameSafe(Multiplayer), *GetNameSafe(LoginWidget));
	}
}

void UMWFrontendWidget::NativeDestruct()
{
	bScreenActive = false;
	if (Multiplayer) { Multiplayer->OnLoginFinished.RemoveDynamic(this, &ThisClass::HandleLoginFinished); }
	UnbindConnection();
	RoomList->OnItemSelectionChanged().RemoveAll(this);
	OpenCreateButton->OnClicked.RemoveDynamic(this, &ThisClass::OpenCreate);
	OpenBrowserButton->OnClicked.RemoveDynamic(this, &ThisClass::OpenBrowser);
	CreateButton->OnClicked.RemoveDynamic(this, &ThisClass::SubmitCreate);
	RefreshButton->OnClicked.RemoveDynamic(this, &ThisClass::RefreshRooms);
	JoinButton->OnClicked.RemoveDynamic(this, &ThisClass::JoinSelectedRoom);
	CreateBackButton->OnClicked.RemoveDynamic(this, &ThisClass::GoBack);
	BrowserBackButton->OnClicked.RemoveDynamic(this, &ThisClass::GoBack);
	ReadyButton->OnClicked.RemoveDynamic(this, &ThisClass::ToggleReady);
	StartButton->OnClicked.RemoveDynamic(this, &ThisClass::StartMatch);
	LeaveButton->OnClicked.RemoveDynamic(this, &ThisClass::LeaveRoom);
	SelectedRoom = nullptr;
	Multiplayer = nullptr;
	Super::NativeDestruct();
}

void UMWFrontendWidget::UnbindConnection()
{
	if (Connection)
	{
		Connection->OnChanged.RemoveDynamic(this, &ThisClass::HandleConnectionChanged);
		Connection->OnError.RemoveDynamic(this, &ThisClass::HandleConnectionError);
	}
}

void UMWFrontendWidget::SetConnection(UMWFrontendConnection* InConnection)
{
	if (Connection == InConnection) { return; }
	UnbindConnection();
	Connection = InConnection;
	if (!bScreenActive) { return; }
	if (Connection)
	{
		Connection->OnChanged.AddUniqueDynamic(this, &ThisClass::HandleConnectionChanged);
		Connection->OnError.AddUniqueDynamic(this, &ThisClass::HandleConnectionError);
	}
	RefreshThemes();
	HandleConnectionChanged();
}

bool UMWFrontendWidget::IsAuthenticated() const { return Multiplayer && Multiplayer->IsLoggedIn(); }
bool UMWFrontendWidget::CanRequest() const { return bScreenActive && IsAuthenticated() && Connection && Connection->GetOperation() == EMWFrontendOperation::Idle; }

void UMWFrontendWidget::HandleLoginFinished(bool bSuccess, const FString& ErrorMessage)
{
	if (!bScreenActive) { return; }
	if (bSuccess && IsAuthenticated())
	{
		ShowPage(EMWFrontendPage::Choice);
		HandleConnectionChanged();
	}
	else { SetStatus(FText::FromString(ErrorMessage)); }
}

void UMWFrontendWidget::ShowPage(EMWFrontendPage Page)
{
	SetFrontendVisible(true);
	CurrentPage = Page;
	Pages->SetActiveWidgetIndex(static_cast<int32>(Page));
	SetStatus(FText::GetEmpty());
	RefreshAvailability();
	if (PageTransition) { PlayAnimation(PageTransition); }
	if (GetOwningPlayer())
	{
		if (Page == EMWFrontendPage::Create) { RoomNameInput->SetUserFocus(GetOwningPlayer()); }
		else if (Page == EMWFrontendPage::Choice) { OpenCreateButton->FocusButton(GetOwningPlayer()); }
		else if (Page == EMWFrontendPage::Browser) { RefreshButton->FocusButton(GetOwningPlayer()); }
		else if (Page == EMWFrontendPage::Lobby) { ReadyButton->FocusButton(GetOwningPlayer()); }
	}
}

void UMWFrontendWidget::SetFrontendVisible(bool bVisible)
{
	const ESlateVisibility Desired = bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	if (GetVisibility() != Desired)
	{
		SetVisibility(Desired);
		OnPresentationChanged.Broadcast(bVisible);
	}
}

void UMWFrontendWidget::SetStatus(const FText& Message) { StatusLabel->SetText(Message); }

void UMWFrontendWidget::RefreshAvailability()
{
	const bool bIdle = !Connection || Connection->GetOperation() == EMWFrontendOperation::Idle;
	const bool bCanRequest = CanRequest();
	OpenCreateButton->SetIsEnabled(bIdle && IsAuthenticated());
	OpenBrowserButton->SetIsEnabled(bIdle && IsAuthenticated());
	RoomNameInput->SetIsEnabled(bIdle);
	CapacityInput->SetIsEnabled(bIdle);
	ThemeInput->SetIsEnabled(bIdle && !Themes.IsEmpty());
	CreateButton->SetIsEnabled(bCanRequest && !Themes.IsEmpty());
	RefreshButton->SetIsEnabled(bCanRequest);
	JoinButton->SetIsEnabled(bCanRequest && SelectedRoom && SelectedRoom->Room.IsJoinable());
	RoomList->SetIsEnabled(bIdle);
	FMWLobbyView Lobby;
	const bool bLobbyReady = HasPreparedLobby(Lobby);
	const FMWLobbyParticipantView* Local = bLobbyReady ? Lobby.FindLocalParticipant() : nullptr;
	ReadyButton->SetIsEnabled(bCanRequest && Local);
	LeaveButton->SetIsEnabled(bCanRequest && Local);
	StartButton->SetVisibility(Local && Local->bHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	StartButton->SetIsEnabled(bCanRequest && Local && Local->bHost && Lobby.bCanStart);
	// Back remains reachable; a service may reject cancellation while a connection is committing.
	CreateBackButton->SetIsEnabled(true);
	BrowserBackButton->SetIsEnabled(true);
}

void UMWFrontendWidget::RefreshThemes()
{
	const FString Previous = ThemeInput->GetSelectedOption();
	Themes = Connection ? Connection->GetThemes() : TArray<FMWFrontendTheme>();
	ThemeInput->ClearOptions();
	TSet<FName> Seen;
	Themes.RemoveAll([&Seen](const FMWFrontendTheme& Theme) { if (Theme.Id.IsNone() || Seen.Contains(Theme.Id)) { return true; } Seen.Add(Theme.Id); return false; });
	for (const FMWFrontendTheme& Theme : Themes) { ThemeInput->AddOption(Theme.DisplayName.ToString()); }
	if (ThemeInput->FindOptionIndex(Previous) != INDEX_NONE) { ThemeInput->SetSelectedOption(Previous); }
	else if (!Themes.IsEmpty()) { ThemeInput->SetSelectedIndex(0); }
}

void UMWFrontendWidget::OpenCreate()
{
	if (!bScreenActive || !IsAuthenticated()) { return; }
	RefreshThemes();
	ShowPage(EMWFrontendPage::Create);
	if (!Connection || Themes.IsEmpty()) { SetStatus(LOCTEXT("ThemesMissing", "방 생성 연결을 준비 중입니다. 사용 가능한 맵 테마가 아직 없습니다.")); }
}

void UMWFrontendWidget::OpenBrowser()
{
	if (!bScreenActive || !IsAuthenticated()) { return; }
	ShowPage(EMWFrontendPage::Browser);
	RefreshRoomList();
	RefreshRooms();
}

void UMWFrontendWidget::SubmitCreate()
{
	if (CurrentPage != EMWFrontendPage::Create || !CanRequest()) { return; }
	FMWCreateRoomRequest Request;
	Request.RoomName = RoomNameInput->GetText().ToString().TrimStartAndEnd();
	Request.MaxPlayers = FCString::Atoi(*CapacityInput->GetSelectedOption());
	const int32 ThemeIndex = ThemeInput->GetSelectedIndex();
	Request.MapThemeId = Themes.IsValidIndex(ThemeIndex) ? Themes[ThemeIndex].Id : NAME_None;
	FText Error;
	if (!Request.Validate(Themes, Error)) { SetStatus(Error); return; }
	SetStatus(LOCTEXT("Creating", "방과 서버를 준비하고 있습니다…"));
	if (!Connection->CreateRoom(Request)) { ReportRejectedRequest(); }
	RefreshAvailability();
}

void UMWFrontendWidget::RefreshRooms()
{
	if (CurrentPage != EMWFrontendPage::Browser) { return; }
	if (!Connection) { SetStatus(LOCTEXT("SearchMissing", "방 검색 연결을 준비 중입니다. 아직 검색을 실행할 수 없습니다.")); return; }
	if (!CanRequest()) { return; }
	SelectedRoom = nullptr;
	RoomList->ClearSelection();
	RoomList->ClearListItems();
	SetStatus(LOCTEXT("Searching", "참가 가능한 방을 찾고 있습니다…"));
	if (!Connection->SearchRooms()) { ReportRejectedRequest(); }
	RefreshAvailability();
}

void UMWFrontendWidget::RefreshRoomList()
{
	SelectedRoom = nullptr;
	RoomList->ClearSelection();
	RoomList->ClearListItems();
	TSet<FString> Seen;
	if (Connection)
	{
		for (const FMWRoomSearchResult& Room : Connection->GetRooms())
		{
			if (Room.SessionId.IsEmpty() || Seen.Contains(Room.SessionId)) { continue; }
			Seen.Add(Room.SessionId);
			UMWRoomListItem* Item = NewObject<UMWRoomListItem>(this);
			Item->Room = Room;
			RoomList->AddItem(Item);
		}
	}
	RoomCountLabel->SetText(FText::Format(LOCTEXT("RoomsCount", "검색 결과 {0}개"), FText::AsNumber(RoomList->GetNumItems())));
	if (Connection && Connection->GetOperation() == EMWFrontendOperation::Idle && RoomList->GetNumItems() == 0)
	{
		SetStatus(LOCTEXT("NoRooms", "참가 가능한 방이 없습니다. 새로고침으로 다시 찾아보세요."));
	}
}

void UMWFrontendWidget::HandleRoomSelection(UObject* Item)
{
	SelectedRoom = Cast<UMWRoomListItem>(Item);
	if (SelectedRoom && !SelectedRoom->Room.IsJoinable()) { SetStatus(SelectedRoom->Room.UnavailableReason); }
	RefreshAvailability();
}

void UMWFrontendWidget::JoinSelectedRoom()
{
	if (CurrentPage != EMWFrontendPage::Browser || !CanRequest() || !SelectedRoom || !SelectedRoom->Room.IsJoinable()) { return; }
	const FString SessionId = SelectedRoom->Room.SessionId;
	SetStatus(LOCTEXT("Joining", "서버에 연결하고 로비 정보를 기다리고 있습니다…"));
	if (!Connection->JoinRoom(SessionId)) { ReportRejectedRequest(); }
	RefreshAvailability();
}

bool UMWFrontendWidget::HasPreparedLobby(FMWLobbyView& OutLobby) const
{
	if (!Connection || !GetOwningPlayer()) { return false; }
	OutLobby = Connection->GetLobby();
	return OutLobby.IsReadyFor(GetWorld(), GetOwningPlayer()->PlayerState);
}

void UMWFrontendWidget::RefreshLobby(const FMWLobbyView& Lobby)
{
	LobbyTitle->SetText(FText::Format(LOCTEXT("LobbyTitle", "{0}  |  {1}  |  {2}/{3}명"), Lobby.RoomName, Lobby.ThemeName,
		FText::AsNumber(Lobby.Participants.Num()), FText::AsNumber(Lobby.MaxPlayers)));
	FString Lines;
	for (const FMWLobbyParticipantView& Participant : Lobby.Participants)
	{
		Lines += FString::Printf(TEXT("%s%s    %s\n\n"), *Participant.PlayerState->GetPlayerName(),
			Participant.bHost ? TEXT("  [호스트]") : TEXT(""), Participant.bReady ? TEXT("준비 완료") : TEXT("준비 중"));
	}
	ParticipantLabel->SetText(FText::FromString(Lines));
	const FMWLobbyParticipantView* Local = Lobby.FindLocalParticipant();
	ReadyButton->SetLabel(Local && Local->bReady ? LOCTEXT("Unready", "준비 취소") : LOCTEXT("Ready", "준비 완료"));
}

void UMWFrontendWidget::HandleConnectionChanged()
{
	if (!bScreenActive) { return; }
	if (!IsAuthenticated()) { if (CurrentPage != EMWFrontendPage::Login) { ShowPage(EMWFrontendPage::Login); } return; }
	FMWLobbyView Lobby;
	if (HasPreparedLobby(Lobby))
	{
		SetFrontendVisible(true);
		if (CurrentPage != EMWFrontendPage::Lobby) { ShowPage(EMWFrontendPage::Lobby); }
		RefreshLobby(Lobby);
	}
	else if (CurrentPage == EMWFrontendPage::Lobby)
	{
		// Invalidated connection is distinct from the team's next gameplay phase.
		const FMWLobbyView Current = Connection ? Connection->GetLobby() : FMWLobbyView();
		if (Current.IsConnectedTo(GetWorld(), GetOwningPlayer() ? GetOwningPlayer()->PlayerState.Get() : nullptr) && !Current.bLobbyPhase) { SetFrontendVisible(false); }
		else { ShowPage(EMWFrontendPage::Choice); }
	}
	else if (CurrentPage == EMWFrontendPage::Browser && Connection && Connection->GetOperation() == EMWFrontendOperation::Idle)
	{
		SetStatus(FText::GetEmpty());
		RefreshRoomList();
	}
	RefreshAvailability();
}

void UMWFrontendWidget::HandleConnectionError(const FText& Message)
{
	if (!bScreenActive) { return; }
	SetStatus(Message);
	UE_LOG(LogDefault, Warning, TEXT("Frontend service error: %s"), *Message.ToString());
	RefreshAvailability();
}

void UMWFrontendWidget::ReportRejectedRequest()
{
	SetStatus(LOCTEXT("Rejected", "요청을 시작하지 못했습니다. 연결 상태를 확인하고 다시 시도하세요."));
	UE_LOG(LogDefault, Warning, TEXT("Frontend request was not accepted. Page=%d"), static_cast<int32>(CurrentPage));
}

void UMWFrontendWidget::GoBack()
{
	if (CurrentPage != EMWFrontendPage::Create && CurrentPage != EMWFrontendPage::Browser) { return; }
	if (Connection && Connection->GetOperation() != EMWFrontendOperation::Idle && !Connection->CancelOperation())
	{
		SetStatus(LOCTEXT("CannotCancel", "현재 작업을 정리하고 있습니다. 완료 후 다시 시도하세요."));
		return;
	}
	SelectedRoom = nullptr;
	ShowPage(EMWFrontendPage::Choice);
}

void UMWFrontendWidget::ToggleReady()
{
	FMWLobbyView Lobby;
	if (CurrentPage != EMWFrontendPage::Lobby || !CanRequest() || !HasPreparedLobby(Lobby)) { return; }
	const FMWLobbyParticipantView* Local = Lobby.FindLocalParticipant();
	if (!Local || !Connection->SetReady(!Local->bReady)) { ReportRejectedRequest(); }
	RefreshAvailability();
}

void UMWFrontendWidget::StartMatch()
{
	FMWLobbyView Lobby;
	if (CurrentPage != EMWFrontendPage::Lobby || !CanRequest() || !HasPreparedLobby(Lobby)) { return; }
	const FMWLobbyParticipantView* Local = Lobby.FindLocalParticipant();
	if (!Local || !Local->bHost || !Lobby.bCanStart) { return; }
	if (!Connection->StartMatch()) { ReportRejectedRequest(); }
	RefreshAvailability();
}

void UMWFrontendWidget::LeaveRoom()
{
	FMWLobbyView Lobby;
	if (CurrentPage != EMWFrontendPage::Lobby || !CanRequest() || !HasPreparedLobby(Lobby)) { return; }
	if (!Connection->LeaveRoom()) { ReportRejectedRequest(); }
	RefreshAvailability();
}

#undef LOCTEXT_NAMESPACE
