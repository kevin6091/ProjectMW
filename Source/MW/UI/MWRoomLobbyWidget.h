#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MWRoomLobbyWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;
class UMWMultiplayerSubsystem;

UCLASS(Abstract)
class MW_API UMWRoomLobbyWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "MW|Lobby UI")
	void Refresh();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UVerticalBox> Box_RoomInfo;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_RoomName;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_Capacity;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_MapTheme;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_Players;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_Status;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_Error;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_LeaveLabel;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_Leave;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_RetryCleanup;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_Ready;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_Start;

private:
	TWeakObjectPtr<UMWMultiplayerSubsystem> Subsystem;
	UFUNCTION() void HandleLeaveClicked();
	UFUNCTION() void HandleRetryClicked();
};
