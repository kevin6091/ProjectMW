#include "UI/Title/MWTitleRootWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Components/WidgetComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "System/MWGameInstance.h"
#include "UI/Lobby/MWCreateRoomWidget.h"
#include "UI/Lobby/MWRoomBrowserWidget.h"

void UMWTitleRootWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	// The title level already owns this root's lifetime. Use the supplied WorldDev
	// widget tree inside that root so the level script still creates one screen.
	if (GetWorld() && GetWorld()->GetNetMode() == NM_Standalone &&
		GetClass()->GetPathName() == TEXT("/Game/UI/Lobby/WBP_TitleRoot.WBP_TitleRoot_C"))
	{
		const auto PresentationClass = LoadClass<UMWTitleRootWidget>(nullptr,
			TEXT("/Game/UI/Lobby/WBP_TitleRoot_WorldDev.WBP_TitleRoot_WorldDev_C"));
		if (PresentationClass && GetOwningPlayer())
		{
			PresentationRoot = CreateWidget<UMWTitleRootWidget>(GetOwningPlayer(), PresentationClass);
			if (PresentationRoot)
			{
				WidgetSwitcher_2->AddChild(PresentationRoot);
				WidgetSwitcher_2->SetActiveWidget(PresentationRoot);
				return;
			}
		}
	}
	UMWGameInstance* Sessions = GetGameInstance()
		? Cast<UMWGameInstance>(GetGameInstance()) : nullptr;
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
	if (auto* PreviewButton = GetWidgetFromName(TEXT("Button_VehiclePreview")))
		PreviewButton->SetVisibility(ESlateVisibility::Collapsed);
	InitializeWorldMenu();
	RefreshEntryScreen();
}

void UMWTitleRootWidget::InitializeWorldMenu()
{
	auto* PC = GetOwningPlayer();
	if (!PC || !WorldMenuActorClass || !WorldMenuPlaceholder || !GetWorld() ||
		GetWorld()->GetNetMode() != NM_Standalone) return;
	for (TActorIterator<AActor> It(GetWorld(), WorldMenuActorClass); It; ++It)
	{
		auto* Surface = It->FindComponentByClass<UWidgetComponent>();
		if (!Surface || Surface->GetWidgetSpace() != EWidgetSpace::World ||
			!Surface->GetReceiveHardwareInput()) continue;
		WorldMenuSurface = Surface;
		TitleScreen->RemoveFromParent();
		Surface->SetOwnerPlayer(PC->GetLocalPlayer());
		Surface->SetWidget(TitleScreen);
		return;
	}
}

void UMWTitleRootWidget::NativeDestruct()
{
	if (PresentationRoot)
	{
		PresentationRoot->RemoveFromParent();
		PresentationRoot = nullptr;
	}
	if (auto* Surface = WorldMenuSurface.Get())
	{
		Surface->SetWidget(nullptr);
		Surface->SetVisibility(false);
		Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	WorldMenuSurface.Reset();
	if (UMWGameInstance* Sessions = Subsystem.Get())
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
	if (PresentationRoot)
	{
		PresentationRoot->RefreshEntryScreen();
		return;
	}
	UMWGameInstance* Sessions = Subsystem.Get();
	const bool bLoggedIn = Sessions && (!Sessions->IsLoginRequired() || Sessions->IsLoggedIn());
	const EMWSessionState State = Sessions ? Sessions->GetSessionState() : EMWSessionState::None;
	// The same title level is also the networked room. Its menu must not cover the lobby.
	SetVisibility(GetWorld() && GetWorld()->GetNetMode() == NM_Standalone
		? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	const bool bNeedsCleanup = State == EMWSessionState::CleanupRequired;
	if (bNeedsCleanup && SelectedScreen == EEntryScreen::Menu) SelectedScreen = EEntryScreen::CreateRoom;
	UUserWidget* Screen = TitleScreen;
	if (!bLoggedIn && !bNeedsCleanup) Screen = LoginScreen;
	else if (SelectedScreen == EEntryScreen::CreateRoom && CreateRoomScreen) Screen = CreateRoomScreen;
	else if (SelectedScreen == EEntryScreen::RoomList && RoomBrowserScreen) Screen = RoomBrowserScreen;
	const bool bMenuRequested = Screen == TitleScreen && bLoggedIn && !bNeedsCleanup;
	const bool bShowWorldMenu = bMenuRequested && CanPresentEntryMenu(bLoggedIn) && WorldMenuSurface.IsValid();
	OnEntryPresentationUpdated(bLoggedIn, bMenuRequested);
	UWidget* Presentation = ResolveEntryPresentation(Screen, bMenuRequested);
	if (auto* Surface = WorldMenuSurface.Get())
	{
		Surface->SetVisibility(bShowWorldMenu);
		Surface->SetCollisionEnabled(bShowWorldMenu ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		if (Presentation == TitleScreen) Presentation = WorldMenuPlaceholder;
	}
	const bool bChanged = WidgetSwitcher_2->GetActiveWidget() != Presentation;
	WidgetSwitcher_2->SetActiveWidget(Presentation);
	if (bChanged && !bShowWorldMenu) Screen->SetKeyboardFocus();
	if (WorldMenuSurface.IsValid() && GetOwningPlayer())
	{
		if (bShowWorldMenu)
		{
			FInputModeGameAndUI Mode;
			Mode.SetHideCursorDuringCapture(false);
			GetOwningPlayer()->SetInputMode(Mode);
		}
		else
		{
			FInputModeUIOnly Mode;
			Mode.SetWidgetToFocus(Screen->TakeWidget());
			GetOwningPlayer()->SetInputMode(Mode);
		}
	}
	if (CreateButton) CreateButton->SetIsEnabled(bLoggedIn && State == EMWSessionState::None && CreateRoomScreen);
	if (JoinButton) JoinButton->SetIsEnabled(Sessions && Sessions->CanBrowseRooms() && State == EMWSessionState::None && RoomBrowserScreen);
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
	if (UMWGameInstance* Sessions = Subsystem.Get(); Sessions &&
		(!Sessions->IsLoginRequired() || Sessions->IsLoggedIn()) && Sessions->GetSessionState() == EMWSessionState::None)
	{
		OpenCreateRoomForm();
	}
}

void UMWTitleRootWidget::OpenCreateRoomForm()
{
	SelectedScreen = EEntryScreen::CreateRoom;
	EntryMessage.Reset();
	RefreshEntryScreen();
}

void UMWTitleRootWidget::HandleBackRequested()
{
	SelectedScreen = EEntryScreen::Menu;
	RefreshEntryScreen();
}

void UMWTitleRootWidget::HandleJoinRequested()
{
	if (UMWGameInstance* Sessions = Subsystem.Get(); Sessions && (!Sessions->IsLoginRequired() || Sessions->IsLoggedIn()) &&
		Sessions->GetSessionState() == EMWSessionState::None && RoomBrowserScreen)
	{
		OpenRoomBrowser();
	}
}

void UMWTitleRootWidget::OpenRoomBrowser()
{
	auto* Sessions = Subsystem.Get();
	if (!Sessions || !RoomBrowserScreen) return;

	EntryMessage.Reset();
	SelectedScreen = EEntryScreen::RoomList;
	bRefreshRoomBrowserOnIdle = Sessions->GetSessionState() == EMWSessionState::Leaving;
	RefreshEntryScreen();
	if (Sessions->CanBrowseRooms()) RoomBrowserScreen->OpenRoomList();
}

void UMWTitleRootWidget::HandleLoginFinished(bool bSuccess, const FString& ErrorMessage)
{
	RefreshEntryScreen();
}

void UMWTitleRootWidget::HandleSessionStateChanged(EMWSessionState PreviousState, EMWSessionState NewState)
{
	RefreshEntryScreen();
	if (bRefreshRoomBrowserOnIdle && NewState == EMWSessionState::None)
	{
		bRefreshRoomBrowserOnIdle = false;
		if (auto* Sessions = Subsystem.Get(); Sessions && Sessions->CanBrowseRooms() &&
			SelectedScreen == EEntryScreen::RoomList && RoomBrowserScreen)
			RoomBrowserScreen->OpenRoomList();
	}
}

void UMWTitleRootWidget::HandleSessionError(const FString& ErrorMessage)
{
	EntryMessage.Reset();
	RefreshEntryScreen();
}
