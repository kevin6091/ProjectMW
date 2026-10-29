#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "System/MultiplayerSubSystem/MWMultiplayerTypes.h"
#include "MWCreateRoomWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;
class UMWMultiplayerSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMWCreateRoomBackRequested);

// WBP_CreateRoom owns the layout; the existing subsystem owns session operations.
UCLASS(Abstract)
class MW_API UMWCreateRoomWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "MW|Room UI")
	void RefreshForm();

	UPROPERTY(BlueprintAssignable, Category = "MW|Room UI")
	FMWCreateRoomBackRequested OnBackRequested;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UEditableTextBox> Input_RoomName;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UComboBoxString> Select_MaxPlayers;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UComboBoxString> Select_MapTheme;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_CreateRoom;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_Back;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_RetryCleanup;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_Status;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_Error;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "MW|Room UI")
	TArray<FName> AvailableMapThemeIds;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "MW|Room UI")
	FName SelectedMapThemeId;

private:
	TWeakObjectPtr<UMWMultiplayerSubsystem> Subsystem;
	FString LocalError;

	UFUNCTION() void HandleCreateClicked();
	UFUNCTION() void HandleBackClicked();
	UFUNCTION() void HandleRetryClicked();
	UFUNCTION() void HandleThemeSelected(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION() void HandleLoginFinished(bool bSuccess, const FString& ErrorMessage);
	UFUNCTION() void HandleSessionStateChanged(EMWSessionState PreviousState, EMWSessionState NewState);
	UFUNCTION() void HandleSessionError(const FString& ErrorMessage);
	UFUNCTION() void HandleCreateFinished(bool bSuccess, const FString& ErrorMessage);
};
