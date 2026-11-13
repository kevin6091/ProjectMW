#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Common/MWMenuTypes.h"
#include "MWTitleRootWidget.generated.h"

class UButton;
class UWidgetSwitcher;
class UWidgetComponent;
class UMWCreateRoomWidget;
class UMWGameInstance;
class UMWRoomBrowserWidget;

UCLASS(Abstract)
class MW_API UMWTitleRootWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "MW|Title UI")
	void RefreshEntryScreen();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual bool CanPresentEntryMenu(bool bLoggedIn) { return bLoggedIn; }
	virtual void OnEntryPresentationUpdated(bool bLoggedIn, bool bMenuRequested) {}
	virtual UWidget* ResolveEntryPresentation(UWidget* Screen, bool bMenuRequested) { return Screen; }
	void OpenCreateRoomForm();
	void OpenRoomBrowser();

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidgetSwitcher> WidgetSwitcher_2;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UUserWidget> LoginScreen;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UUserWidget> TitleScreen;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> WorldMenuPlaceholder;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Title UI")
	TSubclassOf<AActor> WorldMenuActorClass;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Title UI")
	TSubclassOf<UMWCreateRoomWidget> CreateRoomWidgetClass;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Title UI")
	TSubclassOf<UMWRoomBrowserWidget> RoomBrowserWidgetClass;

private:
	UPROPERTY(Transient) TObjectPtr<UMWTitleRootWidget> PresentationRoot;
	UPROPERTY(Transient) TObjectPtr<UMWCreateRoomWidget> CreateRoomScreen;
	UPROPERTY(Transient) TObjectPtr<UMWRoomBrowserWidget> RoomBrowserScreen;
	UPROPERTY(Transient) TObjectPtr<UButton> CreateButton;
	UPROPERTY(Transient) TObjectPtr<UButton> JoinButton;
	TWeakObjectPtr<UMWGameInstance> Subsystem;
	TWeakObjectPtr<UWidgetComponent> WorldMenuSurface;
	void InitializeWorldMenu();
	enum class EEntryScreen : uint8 { Menu, CreateRoom, RoomList };
	EEntryScreen SelectedScreen = EEntryScreen::Menu;
	bool bRefreshRoomBrowserOnIdle = false;
	FString EntryMessage;

	UFUNCTION() void HandleCreateRequested();
	UFUNCTION() void HandleJoinRequested();
	UFUNCTION() void HandleBackRequested();
	UFUNCTION() void HandleLoginFinished(bool bSuccess, const FString& ErrorMessage);
	UFUNCTION() void HandleSessionStateChanged(EMWSessionState PreviousState, EMWSessionState NewState);
	UFUNCTION() void HandleSessionError(const FString& ErrorMessage);
};
