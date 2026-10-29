#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "System/MultiplayerSubSystem/MWMultiplayerTypes.h"
#include "MWRoomListEntryWidget.generated.h"

class UButton;
class UTextBlock;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMWRoomSelected, const FString&, SessionId, int32, ListRevision);

UCLASS(Abstract)
class MW_API UMWRoomListEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetRoom(const FMWRoomInfo& Info, int32 Revision);
	void SetSelected(bool bSelected);
	UPROPERTY(BlueprintAssignable, Category = "MW|Rooms UI") FMWRoomSelected OnRoomSelected;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_Row;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_RoomName;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_Capacity;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_Theme;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_Availability;

private:
	FString SessionId;
	int32 ListRevision = 0;
	UFUNCTION() void HandleSelected();
};
