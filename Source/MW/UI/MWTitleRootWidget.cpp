#include "UI/MWTitleRootWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/GameInstance.h"
#include "System/MultiplayerSubSystem/MWMultiplayerSubsystem.h"
#include "UI/MWCreateRoomWidget.h"
#include "UI/MWRoomBrowserWidget.h"

void UMWTitleRootWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	UMWMultiplayerSubsystem* Sessions = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UMWMultiplayerSubsystem>() : nullptr;
	Subsystem = Sessions;
	if (Sessions)
	{
		Sessions->OnLoginFinished.AddUniqueDynamic(this, &ThisClass::HandleLoginFinished);
		Sessions->OnSessionStateChanged.AddUniqueDynamic(this, &ThisClass::HandleSessionStateChanged);
		Sessions->OnSessionError.AddUniqueDynamic(this, &ThisClass::HandleSessionError);
	}

	if (!IsValid(CreateRoomScreen) && CreateRoomWidgetClass && GetOwningPlayer())
	{
		CreateRoomScreen = CreateWidget<UMWCreateRoomWidget>(GetOwningPlayer(), CreateRoomWidgetClass);
		WidgetSwitcher_2->AddChild(CreateRoomScreen);
	}
	if (CreateRoomScreen)
	{
		CreateRoomScreen->OnBackRequested.AddUniqueDynamic(this, &ThisClass::HandleBackRequested);
	}
	if (!IsValid(RoomBrowserScreen) && RoomBrowserWidgetClass && GetOwningPlayer())
	{
		RoomBrowserScreen = CreateWidget<UMWRoomBrowserWidget>(GetOwningPlayer(), RoomBrowserWidgetClass);
		WidgetSwitcher_2->AddChild(RoomBrowserScreen);
	}
	if (RoomBrowserScreen)
	{
		RoomBrowserScreen->OnBackRequested.AddUniqueDynamic(this, &ThisClass::HandleBackRequested);
	}
	CreateButton = Cast<UButton>(TitleScreen->GetWidgetFromName(TEXT("Button_CreateRoom")));
	JoinButton = Cast<UButton>(TitleScreen->GetWidgetFromName(TEXT("Button_JoinRoom")));
	if (CreateButton) CreateButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCreateRequested);
	if (JoinButton) JoinButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleJoinRequested);
	RefreshEntryScreen();
}

void UMWTitleRootWidget::NativeDestruct()
{
	if (UMWMultiplayerSubsystem* Sessions = Subsystem.Get())
	{
		Sessions->OnLoginFinished.RemoveDynamic(this, &ThisClass::HandleLoginFinished);
		Sessions->OnSessionStateChanged.RemoveDynamic(this, &ThisClass::HandleSessionStateChanged);
		Sessions->OnSessionError.RemoveDynamic(this, &ThisClass::HandleSessionError);
	}
	if (CreateRoomScreen) CreateRoomScreen->OnBackRequested.RemoveDynamic(this, &ThisClass::HandleBackRequested);
	if (RoomBrowserScreen) RoomBrowserScreen->OnBackRequested.RemoveDynamic(this, &ThisClass::HandleBackRequested);
	if (CreateButton) CreateButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleCreateRequested);
	if (JoinButton) JoinButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleJoinRequested);
	CreateButton = nullptr;
	JoinButton = nullptr;
	Subsystem.Reset();
	Super::NativeDestruct();
}

void UMWTitleRootWidget::RefreshEntryScreen()
{
	UMWMultiplayerSubsystem* Sessions = Subsystem.Get();
	const bool bLoggedIn = Sessions && Sessions->IsLoggedIn();
	const EMWSessionState State = Sessions ? Sessions->GetSessionState() : EMWSessionState::None;
	const bool bNeedsCleanup = State == EMWSessionState::CleanupRequired;
	if (bNeedsCleanup && SelectedScreen == EEntryScreen::Menu) SelectedScreen = EEntryScreen::CreateRoom;
	UUserWidget* Screen = TitleScreen;
	if (!bLoggedIn && !bNeedsCleanup) Screen = LoginScreen;
	else if (SelectedScreen == EEntryScreen::CreateRoom && CreateRoomScreen) Screen = CreateRoomScreen;
	else if (SelectedScreen == EEntryScreen::RoomList && RoomBrowserScreen) Screen = RoomBrowserScreen;
	const bool bChanged = WidgetSwitcher_2->GetActiveWidget() != Screen;
	WidgetSwitcher_2->SetActiveWidget(Screen);
	if (bChanged) Screen->SetKeyboardFocus();
	if (CreateButton) CreateButton->SetIsEnabled(bLoggedIn && State == EMWSessionState::None && CreateRoomScreen);
	if (JoinButton) JoinButton->SetIsEnabled(bLoggedIn && State == EMWSessionState::None && RoomBrowserScreen);
	if (UTextBlock* Status = Cast<UTextBlock>(TitleScreen->GetWidgetFromName(TEXT("Text_EntryStatus"))))
	{
		const FString Error = Sessions ? Sessions->GetLastSessionError() : FString();
		Status->SetText(FText::FromString(!EntryMessage.IsEmpty() ? EntryMessage : Error));
	}
	if (CreateRoomScreen) CreateRoomScreen->RefreshForm();
	if (RoomBrowserScreen) RoomBrowserScreen->RefreshView();
}

void UMWTitleRootWidget::HandleCreateRequested()
{
	if (UMWMultiplayerSubsystem* Sessions = Subsystem.Get(); Sessions &&
		Sessions->IsLoggedIn() && Sessions->GetSessionState() == EMWSessionState::None)
	{
		SelectedScreen = EEntryScreen::CreateRoom;
		EntryMessage.Reset();
		RefreshEntryScreen();
	}
}

void UMWTitleRootWidget::HandleBackRequested()
{
	SelectedScreen = EEntryScreen::Menu;
	RefreshEntryScreen();
}

void UMWTitleRootWidget::HandleJoinRequested()
{
	if (UMWMultiplayerSubsystem* Sessions = Subsystem.Get(); Sessions && Sessions->IsLoggedIn() &&
		Sessions->GetSessionState() == EMWSessionState::None && RoomBrowserScreen)
	{
		EntryMessage.Reset();
		SelectedScreen = EEntryScreen::RoomList;
		RefreshEntryScreen();
		RoomBrowserScreen->OpenRoomList();
	}
}

void UMWTitleRootWidget::HandleLoginFinished(bool bSuccess, const FString& ErrorMessage)
{
	RefreshEntryScreen();
}

void UMWTitleRootWidget::HandleSessionStateChanged(EMWSessionState PreviousState, EMWSessionState NewState)
{
	RefreshEntryScreen();
}

void UMWTitleRootWidget::HandleSessionError(const FString& ErrorMessage)
{
	EntryMessage.Reset();
	RefreshEntryScreen();
}
