#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "System/MultiplayerSubSystem/MWMultiplayerTypes.h"
#include "MWTitleRootWidget.generated.h"

class UButton;
class UWidgetSwitcher;
class UMWCreateRoomWidget;
class UMWMultiplayerSubsystem;
class UMWRoomBrowserWidget;
class UWidgetComponent;
class AMWVehicleSelectionPreviewActor;

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
	virtual bool CanPresentEntryMenu(bool bLoggedIn) { return true; }
	virtual void OnEntryPresentationUpdated(bool bLoggedIn, bool bMenuRequested) {}
	virtual UWidget* ResolveEntryPresentation(UWidget* Screen, bool bMenuRequested) { return Screen; }

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidgetSwitcher> WidgetSwitcher_2;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UUserWidget> LoginScreen;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UUserWidget> TitleScreen;
	// Optional world presentation. The Blueprint selects a level actor with one WidgetComponent.
	UPROPERTY(EditDefaultsOnly, Category = "MW|Title UI|Presentation")
	TSubclassOf<AActor> WorldMenuActorClass;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> WorldMenuPlaceholder;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_VehiclePreview;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Title UI|Development")
	TSubclassOf<AMWVehicleSelectionPreviewActor> VehiclePreviewActorClass;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Title UI")
	TSubclassOf<UMWCreateRoomWidget> CreateRoomWidgetClass;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Title UI")
	TSubclassOf<UMWRoomBrowserWidget> RoomBrowserWidgetClass;

private:
	UPROPERTY(Transient) TObjectPtr<UMWCreateRoomWidget> CreateRoomScreen;
	UPROPERTY(Transient) TObjectPtr<UMWRoomBrowserWidget> RoomBrowserScreen;
	UPROPERTY(Transient) TObjectPtr<UButton> CreateButton;
	UPROPERTY(Transient) TObjectPtr<UButton> JoinButton;
	TWeakObjectPtr<UMWMultiplayerSubsystem> Subsystem;
	enum class EEntryScreen : uint8 { Menu, CreateRoom, RoomList, VehiclePreview };
	EEntryScreen SelectedScreen = EEntryScreen::Menu;
	FString EntryMessage;
	TWeakObjectPtr<UWidgetComponent> WorldMenuComponent;
	TWeakObjectPtr<AActor> WorldMenuOwner;
	TWeakObjectPtr<AMWVehicleSelectionPreviewActor> VehiclePreview;
	void InitializeWorldMenu();
	void ReleaseWorldMenu();
	void UpdateWorldMenuPresentation(bool bShowMenu, bool bUseWorldInput);
	void ReleaseVehiclePreview();
	UFUNCTION() void HandleVehiclePreviewRequested();
	UFUNCTION() void HandleVehiclePreviewClosed();
	UFUNCTION() void HandleWorldMenuDestroyed(AActor* DestroyedActor);

	UFUNCTION() void HandleCreateRequested();
	UFUNCTION() void HandleJoinRequested();
	UFUNCTION() void HandleBackRequested();
	UFUNCTION() void HandleLoginFinished(bool bSuccess, const FString& ErrorMessage);
	UFUNCTION() void HandleSessionStateChanged(EMWSessionState PreviousState, EMWSessionState NewState);
	UFUNCTION() void HandleSessionError(const FString& ErrorMessage);
};
