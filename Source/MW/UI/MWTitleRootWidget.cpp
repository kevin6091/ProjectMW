#include "UI/MWTitleRootWidget.h"

#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Components/WidgetComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "System/MultiplayerSubSystem/MWMultiplayerSubsystem.h"
#include "UI/MWCreateRoomWidget.h"
#include "UI/MWRoomBrowserWidget.h"
#include "UI/MWVehicleSelectionPreviewActor.h"

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
	if (Button_VehiclePreview) Button_VehiclePreview->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleVehiclePreviewRequested);
	InitializeWorldMenu();
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
	if (IsValid(CreateRoomScreen))
	{
		CreateRoomScreen->OnBackRequested.RemoveDynamic(this, &ThisClass::HandleBackRequested);
	}
	if (IsValid(RoomBrowserScreen))
	{
		RoomBrowserScreen->OnBackRequested.RemoveDynamic(this, &ThisClass::HandleBackRequested);
	}
	if (IsValid(CreateButton))
	{
		CreateButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleCreateRequested);
	}
	if (IsValid(JoinButton))
	{
		JoinButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleJoinRequested);
	}
	CreateButton = nullptr;
	JoinButton = nullptr;
	if (IsValid(Button_VehiclePreview)) Button_VehiclePreview->OnClicked.RemoveDynamic(this, &ThisClass::HandleVehiclePreviewRequested);
	ReleaseVehiclePreview();
	ReleaseWorldMenu();
	Subsystem.Reset();
	Super::NativeDestruct();
}

void UMWTitleRootWidget::RefreshEntryScreen()
{
	if (!IsValid(WidgetSwitcher_2) || !IsValid(LoginScreen) || !IsValid(TitleScreen))
	{
		return;
	}
	if (TitleScreen->GetParent() != WidgetSwitcher_2 &&
		(!WorldMenuComponent.IsValid() || WorldMenuComponent->GetWidget() != TitleScreen))
	{
		ReleaseWorldMenu();
	}
	UMWMultiplayerSubsystem* Sessions = Subsystem.Get();
	const bool bLoggedIn = Sessions && Sessions->IsLoggedIn();
	const EMWSessionState State = Sessions ? Sessions->GetSessionState() : EMWSessionState::None;
	if (SelectedScreen == EEntryScreen::VehiclePreview &&
		(!bLoggedIn || State != EMWSessionState::None || !VehiclePreview.IsValid())) ReleaseVehiclePreview();
	const bool bNeedsCleanup = State == EMWSessionState::CleanupRequired;
	if (bNeedsCleanup && SelectedScreen == EEntryScreen::Menu) SelectedScreen = EEntryScreen::CreateRoom;
	UWidget* Screen = TitleScreen;
	if (!bLoggedIn && !bNeedsCleanup) Screen = LoginScreen;
	else if (SelectedScreen == EEntryScreen::CreateRoom && CreateRoomScreen) Screen = CreateRoomScreen;
	else if (SelectedScreen == EEntryScreen::RoomList && RoomBrowserScreen) Screen = RoomBrowserScreen;
	const bool bMenuRequested = bLoggedIn && Screen == TitleScreen;
	const bool bPresentationReady = CanPresentEntryMenu(bLoggedIn);
	const bool bWaitingForPresentation = bMenuRequested && !bPresentationReady;
	if (bWaitingForPresentation && WorldMenuPlaceholder) Screen = WorldMenuPlaceholder;
	const bool bVehiclePreview = SelectedScreen == EEntryScreen::VehiclePreview && VehiclePreview.IsValid();
	if (bVehiclePreview) Screen = WorldMenuPlaceholder;
	const bool bShowWorldMenu = WorldMenuComponent.IsValid() && Screen == TitleScreen;
	if (bShowWorldMenu)
	{
		Screen = WorldMenuPlaceholder;
	}
	Screen = ResolveEntryPresentation(Screen, bMenuRequested);
	const bool bChanged = WidgetSwitcher_2->GetActiveWidget() != Screen;
	WidgetSwitcher_2->SetActiveWidget(Screen);
	UpdateWorldMenuPresentation(bShowWorldMenu, bShowWorldMenu || bVehiclePreview || bWaitingForPresentation);
	if (bChanged && !bShowWorldMenu && !bVehiclePreview && !bWaitingForPresentation) Screen->SetKeyboardFocus();
	if (Button_VehiclePreview)
	{
		const bool bAllowPreview = GetWorld() && GetWorld()->WorldType == EWorldType::PIE &&
			VehiclePreviewActorClass && WorldMenuPlaceholder && bLoggedIn && State == EMWSessionState::None &&
			SelectedScreen == EEntryScreen::Menu && bPresentationReady;
		Button_VehiclePreview->SetVisibility(bAllowPreview ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (CreateButton) CreateButton->SetIsEnabled(bLoggedIn && State == EMWSessionState::None && CreateRoomScreen);
	if (JoinButton) JoinButton->SetIsEnabled(bLoggedIn && State == EMWSessionState::None && RoomBrowserScreen);
	if (UTextBlock* Status = Cast<UTextBlock>(TitleScreen->GetWidgetFromName(TEXT("Text_EntryStatus"))))
	{
		const FString Error = Sessions ? Sessions->GetLastSessionError() : FString();
		Status->SetText(FText::FromString(!EntryMessage.IsEmpty() ? EntryMessage : Error));
		if (UBorder* Feedback = Cast<UBorder>(Status->GetParent()))
		{
			Feedback->SetVisibility(Status->GetText().IsEmpty()
				? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
		}
	}
	if (CreateRoomScreen) CreateRoomScreen->RefreshForm();
	if (RoomBrowserScreen) RoomBrowserScreen->RefreshView();
	OnEntryPresentationUpdated(bLoggedIn, bMenuRequested);
}

void UMWTitleRootWidget::InitializeWorldMenu()
{
	if (!WorldMenuActorClass || !IsValid(WorldMenuPlaceholder) || !IsValid(TitleScreen) || !GetWorld())
	{
		return;
	}

	UWidgetComponent* MenuComponent = nullptr;
	for (TActorIterator<AActor> It(GetWorld(), WorldMenuActorClass); It; ++It)
	{
		TArray<UWidgetComponent*> Components;
		It->GetComponents(Components);
		if (MenuComponent || Components.Num() != 1)
		{
			return; // Ambiguous level configuration: preserve the existing screen menu.
		}
		MenuComponent = Components[0];
	}
	if (!IsValid(MenuComponent) || MenuComponent->GetWidgetSpace() != EWidgetSpace::World ||
		!MenuComponent->GetReceiveHardwareInput() || MenuComponent->GetWidget())
	{
		return;
	}

	TitleScreen->RemoveFromParent();
	MenuComponent->SetOwnerPlayer(GetOwningLocalPlayer());
	MenuComponent->SetWidget(TitleScreen);
	WorldMenuComponent = MenuComponent;
	WorldMenuOwner = MenuComponent->GetOwner();
	WorldMenuOwner->OnDestroyed.AddUniqueDynamic(this, &ThisClass::HandleWorldMenuDestroyed);
}

void UMWTitleRootWidget::UpdateWorldMenuPresentation(bool bShowMenu, bool bUseWorldInput)
{
	if (UWidgetComponent* Component = WorldMenuComponent.Get())
	{
		Component->SetVisibility(bShowMenu);
		Component->SetCollisionEnabled(bShowMenu ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		// A hidden world menu must not consume mouse traces behind the screen form.
		TitleScreen->SetVisibility(bShowMenu ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (WorldMenuActorClass)
	{
		if (APlayerController* Controller = GetOwningPlayer())
		{
			if (bUseWorldInput)
			{
				FInputModeGameAndUI Mode;
				Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
				Mode.SetHideCursorDuringCapture(false);
				Controller->SetInputMode(Mode);
			}
			else
			{
				FInputModeUIOnly Mode;
				Mode.SetWidgetToFocus(WidgetSwitcher_2->GetActiveWidget()->TakeWidget());
				Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
				Controller->SetInputMode(Mode);
			}
		}
	}
}

void UMWTitleRootWidget::HandleVehiclePreviewRequested()
{
	UMWMultiplayerSubsystem* Sessions = Subsystem.Get();
	if (!GetWorld() || GetWorld()->WorldType != EWorldType::PIE || !VehiclePreviewActorClass ||
		!WorldMenuPlaceholder || !Sessions || !Sessions->IsLoggedIn() ||
		Sessions->GetSessionState() != EMWSessionState::None || SelectedScreen != EEntryScreen::Menu) return;
	AMWVehicleSelectionPreviewActor* Preview = nullptr;
	for (TActorIterator<AMWVehicleSelectionPreviewActor> It(GetWorld(), VehiclePreviewActorClass); It; ++It)
	{
		if (Preview) return;
		Preview = *It;
	}
	if (!Preview || !Preview->OpenPreview()) return;
	VehiclePreview = Preview;
	Preview->OnPreviewClosed.AddUniqueDynamic(this, &ThisClass::HandleVehiclePreviewClosed);
	SelectedScreen = EEntryScreen::VehiclePreview;
	RefreshEntryScreen();
}

void UMWTitleRootWidget::ReleaseVehiclePreview()
{
	if (AMWVehicleSelectionPreviewActor* Preview = VehiclePreview.Get())
	{
		Preview->OnPreviewClosed.RemoveDynamic(this, &ThisClass::HandleVehiclePreviewClosed);
		Preview->ClosePreview();
	}
	VehiclePreview.Reset();
	if (SelectedScreen == EEntryScreen::VehiclePreview) SelectedScreen = EEntryScreen::Menu;
}

void UMWTitleRootWidget::HandleVehiclePreviewClosed()
{
	ReleaseVehiclePreview();
	RefreshEntryScreen();
}

void UMWTitleRootWidget::ReleaseWorldMenu()
{
	if (UWidgetComponent* Component = WorldMenuComponent.Get())
	{
		Component->SetVisibility(false);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetWidget(nullptr);
	}
	if (AActor* Owner = WorldMenuOwner.Get())
	{
		Owner->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleWorldMenuDestroyed);
	}
	if (!HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed) && IsValid(WidgetSwitcher_2) &&
		IsValid(TitleScreen) && TitleScreen->GetParent() != WidgetSwitcher_2)
	{
		TitleScreen->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		WidgetSwitcher_2->AddChild(TitleScreen);
	}
	WorldMenuComponent.Reset();
	WorldMenuOwner.Reset();
}

void UMWTitleRootWidget::HandleWorldMenuDestroyed(AActor* DestroyedActor)
{
	if (DestroyedActor)
	{
		ReleaseWorldMenu();
		RefreshEntryScreen();
	}
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
