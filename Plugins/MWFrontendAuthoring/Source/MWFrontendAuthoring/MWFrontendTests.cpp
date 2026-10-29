#include "MWFrontendPreview.h"
#include "MWFrontendAuthoringLibrary.h"

#include "Frontend/MWRoadButtonWidget.h"
#include "Frontend/MWRoomRowWidget.h"
#include "Components/ComboBoxString.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/EditableTextBox.h"
#include "Components/ListView.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMWFrontendValidationTest, "MW.Frontend.InputAndLobbyValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMWFrontendValidationTest::RunTest(const FString& Parameters)
{
	FMWFrontendTheme Theme;
	Theme.Id = TEXT("Approved");
	FMWCreateRoomRequest Request;
	Request.MapThemeId = Theme.Id;
	FText Error;
	TestFalse(TEXT("Empty name rejected"), Request.Validate({Theme}, Error));
	Request.RoomName = TEXT("   ");
	TestFalse(TEXT("Whitespace name rejected"), Request.Validate({Theme}, Error));
	Request.RoomName = FString::ChrN(33, TCHAR('x'));
	TestFalse(TEXT("Long name rejected"), Request.Validate({Theme}, Error));
	Request.RoomName = TEXT("One\nTwo");
	TestFalse(TEXT("Control characters rejected"), Request.Validate({Theme}, Error));
	Request.RoomName = TEXT("같은 방 이름");
	Request.MaxPlayers = 1;
	TestFalse(TEXT("Host included minimum 2"), Request.Validate({Theme}, Error));
	Request.MaxPlayers = 5;
	TestFalse(TEXT("Maximum 4"), Request.Validate({Theme}, Error));
	Request.MaxPlayers = 3;
	TestTrue(TEXT("Valid Korean name"), Request.Validate({Theme}, Error));
	Request.MapThemeId = TEXT("Unknown");
	TestFalse(TEXT("Unapproved theme rejected"), Request.Validate({Theme}, Error));
	FMWRoomSearchResult Room;
	Room.SessionId = TEXT("UniqueSession"); Room.MaxPlayers = 3; Room.PlayerCount = 1; Room.bJoinable = true;
	TestTrue(TEXT("Open room allowed"), Room.IsJoinable());
	Room.PlayerCount = 3;
	TestFalse(TEXT("Full room rejected"), Room.IsJoinable());
	Room.PlayerCount = 1; Room.SessionId.Reset();
	TestFalse(TEXT("Missing identity rejected"), Room.IsJoinable());
	FMWLobbyView Lobby;
	Lobby.bConnectionReady = true; Lobby.bLobbyPhase = true; Lobby.MaxPlayers = 3;
	TestFalse(TEXT("Service success cannot admit without a real world and PlayerState"), Lobby.IsReadyFor(nullptr, nullptr));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMWFrontendWidgetFlowTest, "MW.Frontend.WidgetRequestsAndLifetime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMWFrontendWidgetFlowTest::RunTest(const FString& Parameters)
{
	// Independent transient game context; never replace the user's editor world or shared level.
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->AddToRoot();
	Instance->InitializeStandalone();
	UWorld* World = Instance->GetWorld();
	// InitializeStandalone already creates and initializes its world.
	// No GameViewport exists in this isolated context; CreateLocalPlayer requires one.
	ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	// Actors in this unbegun world have not run PostInitializeComponents registration.
	World->AddController(Controller);
	Controller->SetPlayer(LocalPlayer);
	Controller->PlayerState = World->SpawnActor<APlayerState>();
	AGameStateBase* GameState = World->SpawnActor<AGameStateBase>();
	GameState->AddPlayerState(Controller->PlayerState); World->SetGameState(GameState);
	UMWFrontendWidget* Widget=Cast<UMWFrontendWidget>(UMWFrontendAuthoringLibrary::CreateTransientTestWidget(Controller));
	if (!TestNotNull(TEXT("Unsaved Editor fixture uses the current production widget tree"), Widget))
	{
		Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); Instance->RemoveFromRoot(); return false;
	}
	Widget->AddToRoot();
	UMWFrontendPreviewConnection* Connection = NewObject<UMWFrontendPreviewConnection>(Widget);
	Connection->PrepareLobby(Controller); Widget->SetConnection(Connection);
	TSharedPtr<SWidget> SlateWidget = Widget->TakeWidget();
	TestEqual(TEXT("Widget resolves the registered owning Controller"), Widget->GetOwningPlayer(), Controller);
	auto Click = [Widget](const TCHAR* Name) { CastChecked<UMWRoadButtonWidget>(Widget->GetWidgetFromName(Name))->OnClicked.Broadcast(); };
	UWidgetSwitcher* Pages = CastChecked<UWidgetSwitcher>(Widget->GetWidgetFromName(TEXT("Pages")));
	TestEqual(TEXT("Authenticated fixture starts at match choice"), Pages->GetActiveWidgetIndex(), 1);
	Click(TEXT("OpenCreateButton"));
	TestEqual(TEXT("Create form shown"), Pages->GetActiveWidgetIndex(), 2);
	Click(TEXT("CreateButton"));
	TestEqual(TEXT("Invalid name never dispatches"), Connection->CreateCount, 0);
	CastChecked<UEditableTextBox>(Widget->GetWidgetFromName(TEXT("RoomNameInput")))->SetText(FText::FromString(TEXT("  테스트 방  ")));
	UComboBoxString* Themes = CastChecked<UComboBoxString>(Widget->GetWidgetFromName(TEXT("ThemeInput")));
	Themes->SetSelectedIndex(Themes->GetOptionCount() - 1);
	const FString SelectedTheme = Themes->GetSelectedOption();
	Click(TEXT("CreateButton"));
	TestEqual(TEXT("Selected dropdown theme reaches the confirmed lobby"), Connection->Lobby.ThemeName.ToString(), SelectedTheme);
	TestEqual(TEXT("Valid create dispatched once"), Connection->CreateCount, 1);
	TestEqual(TEXT("Prepared world enters same lobby"), Pages->GetActiveWidgetIndex(), 4);
	TestEqual(TEXT("Trimmed name sent"), Connection->Lobby.RoomName.ToString(), FString(TEXT("테스트 방")));
	TestEqual(TEXT("Displayed lobby title uses the confirmed room name"), CastChecked<UTextBlock>(Widget->GetWidgetFromName(TEXT("LobbyTitle")))->GetText().ToString(), FString(TEXT("테스트 방")));
	UWidget* FourthSlot=Widget->GetWidgetFromName(TEXT("ParticipantBoard3"));
	if (TestNotNull(TEXT("Current production tree contains the fourth participant slot"),FourthSlot))
	{
		TestEqual(TEXT("Fourth slot is hidden for a three-player capacity"),FourthSlot->GetVisibility(),ESlateVisibility::Collapsed);
	}
	TestNotNull(TEXT("Road button retains named original arrow"), CastChecked<UMWRoadButtonWidget>(Widget->GetWidgetFromName(TEXT("ReadyButton")))->GetWidgetFromName(TEXT("Arrow")));
	Click(TEXT("ReadyButton"));
	TestTrue(TEXT("Ready reads service-confirmed value"), Connection->Lobby.Participants[0].bReady);
	TestFalse(TEXT("One-player fixture cannot start"), Widget->GetWidgetFromName(TEXT("StartButton"))->GetIsEnabled());
	TestTrue(TEXT("Editor-only fixture can prepare a second valid PlayerState"), Connection->SetPreviewParticipantCount(2));
	TestTrue(TEXT("Confirmed two-player start state reaches button availability"), Widget->GetWidgetFromName(TEXT("StartButton"))->GetIsEnabled());
	TestTrue(TEXT("Editor-only fixture can return to one participant"), Connection->SetPreviewParticipantCount(1));
	Click(TEXT("LeaveButton"));
	TestEqual(TEXT("Confirmed leave returns to choice"), Pages->GetActiveWidgetIndex(), 1);
	Click(TEXT("OpenBrowserButton"));
	TestEqual(TEXT("First search dispatched"), Connection->SearchCount, 1);
	Click(TEXT("RefreshButton"));
	TestEqual(TEXT("Repeated click during pending search ignored"), Connection->SearchCount, 1);
	Click(TEXT("BrowserBackButton"));
	TestEqual(TEXT("Cancel returns to choice"), Pages->GetActiveWidgetIndex(), 1);
	Connection->CompleteSearch();
	TestEqual(TEXT("Late completion cannot reopen browser"), Pages->GetActiveWidgetIndex(), 1);
	Click(TEXT("OpenBrowserButton")); Connection->CompleteSearch();
	UListView* List = CastChecked<UListView>(Widget->GetWidgetFromName(TEXT("RoomList")));
	TestEqual(TEXT("Both same-name rooms retained by SessionId"), List->GetNumItems(), 2);
	List->SetSelectedItem(List->GetItemAt(1));
	TestFalse(TEXT("Full row disables join"), Widget->GetWidgetFromName(TEXT("JoinButton"))->GetIsEnabled());
	List->SetSelectedItem(List->GetItemAt(0));
	TestTrue(TEXT("Open row enables join"), Widget->GetWidgetFromName(TEXT("JoinButton"))->GetIsEnabled());
	TestTrue(TEXT("Selecting an open room clears the previous full-room error"), CastChecked<UTextBlock>(Widget->GetWidgetFromName(TEXT("StatusLabel")))->GetText().IsEmpty());
	Click(TEXT("JoinButton"));
	TestEqual(TEXT("Selected session enters common lobby"), Pages->GetActiveWidgetIndex(), 4);
	Connection->Lobby.bConnectionReady = false; Connection->OnChanged.Broadcast();
	TestEqual(TEXT("Connection loss leaves lobby"), Pages->GetActiveWidgetIndex(), 1);
	SlateWidget.Reset(); Widget->ReleaseSlateResources(true);
	TestFalse(TEXT("Screen release detaches connection delegates"), Connection->OnChanged.IsBound());
	Connection->OnChanged.Broadcast();
	Widget->RemoveFromRoot();
	Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); Instance->RemoveFromRoot();
	return true;
}

#endif
