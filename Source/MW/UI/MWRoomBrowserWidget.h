#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "System/MultiplayerSubSystem/MWMultiplayerTypes.h"
#include "MWRoomBrowserWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;
class UMWMultiplayerSubsystem;
class UMWRoomListEntryWidget;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMWRoomBrowserBackRequested);

UCLASS(Abstract)
class MW_API UMWRoomBrowserWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "MW|Rooms UI") void OpenRoomList();
	UFUNCTION(BlueprintCallable, Category = "MW|Rooms UI") void RefreshView();
	UPROPERTY(BlueprintAssignable, Category = "MW|Rooms UI") FMWRoomBrowserBackRequested OnBackRequested;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Rooms UI") TSubclassOf<UMWRoomListEntryWidget> RoomEntryWidgetClass;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UVerticalBox> Box_Rooms;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_Status;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_Error;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_SelectedRoom;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_Refresh;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_Back;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_Join;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_RetryCleanup;

private:
	TWeakObjectPtr<UMWMultiplayerSubsystem> Subsystem;
	UPROPERTY(Transient) TArray<TObjectPtr<UMWRoomListEntryWidget>> Entries;
	TArray<FMWRoomInfo> Rows;
	FString SelectedSessionId;
	int32 SelectedRevision = 0;
	void RebuildRows();
	UFUNCTION() void HandleRefresh();
	UFUNCTION() void HandleBack();
	UFUNCTION() void HandleJoin();
	UFUNCTION() void HandleRetry();
	UFUNCTION() void HandleRoomSelected(const FString& SessionId, int32 Revision);
	UFUNCTION() void HandleRoomsUpdated(bool bSuccess, const FString& ErrorMessage);
	UFUNCTION() void HandleStateChanged(EMWSessionState PreviousState, EMWSessionState NewState);
	UFUNCTION() void HandleError(const FString& ErrorMessage);
};
