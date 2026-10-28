#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Frontend/MWFrontendConnection.h"
#include "MWFrontendWidget.generated.h"

class UMWRoadButtonWidget;
class UComboBoxString;
class UEditableTextBox;
class UListView;
class UMWMultiplayerSubsystem;
class UMWRoomListItem;
class UPanelWidget;
class UTextBlock;
class UWidgetAnimation;
class UWidgetSwitcher;

UENUM()
enum class EMWFrontendPage : uint8 { Login, Choice, Create, Browser, Lobby };

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMWFrontendPresentationChanged, bool, bFrontendVisible);

/** Local presentation, form validation and intent forwarding. No session or server authority. */
UCLASS(Abstract)
class MW_API UMWFrontendWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="MW|Frontend") void SetConnection(UMWFrontendConnection* InConnection);
	UPROPERTY(BlueprintAssignable, Category="MW|Frontend") FMWFrontendPresentationChanged OnPresentationChanged;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual bool IsAuthenticated() const;

	UPROPERTY(EditDefaultsOnly, Category="MW|Frontend") TSubclassOf<UUserWidget> LoginWidgetClass;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidgetSwitcher> Pages;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UPanelWidget> LoginHost;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UMWRoadButtonWidget> OpenCreateButton;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UMWRoadButtonWidget> OpenBrowserButton;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UMWRoadButtonWidget> CreateButton;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UMWRoadButtonWidget> RefreshButton;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UMWRoadButtonWidget> JoinButton;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UMWRoadButtonWidget> CreateBackButton;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UMWRoadButtonWidget> BrowserBackButton;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UMWRoadButtonWidget> ReadyButton;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UMWRoadButtonWidget> StartButton;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UMWRoadButtonWidget> LeaveButton;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UEditableTextBox> RoomNameInput;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UComboBoxString> CapacityInput;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UComboBoxString> ThemeInput;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UListView> RoomList;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> StatusLabel;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> RoomCountLabel;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> LobbyTitle;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> ParticipantLabel;
	UPROPERTY(Transient, meta=(BindWidgetAnimOptional)) TObjectPtr<UWidgetAnimation> PageTransition;

private:
	UFUNCTION() void HandleLoginFinished(bool bSuccess, const FString& ErrorMessage);
	UFUNCTION() void HandleConnectionChanged();
	UFUNCTION() void HandleConnectionError(const FText& Message);
	UFUNCTION() void OpenCreate();
	UFUNCTION() void OpenBrowser();
	UFUNCTION() void SubmitCreate();
	UFUNCTION() void RefreshRooms();
	UFUNCTION() void JoinSelectedRoom();
	UFUNCTION() void GoBack();
	UFUNCTION() void ToggleReady();
	UFUNCTION() void StartMatch();
	UFUNCTION() void LeaveRoom();
	UFUNCTION() void HandleRoomSelection(UObject* Item);

	void ShowPage(EMWFrontendPage Page);
	void RefreshAvailability();
	void RefreshThemes();
	void RefreshRoomList();
	void RefreshLobby(const FMWLobbyView& Lobby);
	void SetStatus(const FText& Message);
	void ReportRejectedRequest();
	bool CanRequest() const;
	bool HasPreparedLobby(FMWLobbyView& OutLobby) const;
	void UnbindConnection();
	void SetFrontendVisible(bool bVisible);

	UPROPERTY(Transient) TObjectPtr<UMWMultiplayerSubsystem> Multiplayer;
	UPROPERTY(Transient) TObjectPtr<UMWFrontendConnection> Connection;
	UPROPERTY(Transient) TObjectPtr<UUserWidget> LoginWidget;
	UPROPERTY(Transient) TObjectPtr<UMWRoomListItem> SelectedRoom;
	UPROPERTY(Transient) TArray<FMWFrontendTheme> Themes;
	EMWFrontendPage CurrentPage = EMWFrontendPage::Login;
	bool bScreenActive = false;
};
