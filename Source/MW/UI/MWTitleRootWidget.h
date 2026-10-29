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

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidgetSwitcher> WidgetSwitcher_2;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UUserWidget> LoginScreen;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UUserWidget> TitleScreen;
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
	enum class EEntryScreen : uint8 { Menu, CreateRoom, RoomList };
	EEntryScreen SelectedScreen = EEntryScreen::Menu;
	FString EntryMessage;

	UFUNCTION() void HandleCreateRequested();
	UFUNCTION() void HandleJoinRequested();
	UFUNCTION() void HandleBackRequested();
	UFUNCTION() void HandleLoginFinished(bool bSuccess, const FString& ErrorMessage);
	UFUNCTION() void HandleSessionStateChanged(EMWSessionState PreviousState, EMWSessionState NewState);
	UFUNCTION() void HandleSessionError(const FString& ErrorMessage);
};
