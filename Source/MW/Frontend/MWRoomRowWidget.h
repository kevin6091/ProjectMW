#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Frontend/MWFrontendConnection.h"
#include "MWRoomRowWidget.generated.h"

class UTextBlock;
class UBorder;

UCLASS()
class MW_API UMWRoomListItem : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY() FMWRoomSearchResult Room;
};

UCLASS(Abstract)
class MW_API UMWRoomRowWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()
protected:
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	virtual void NativeOnItemSelectionChanged(bool bIsSelected) override;
	virtual void NativeOnEntryReleased() override;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> RoomLabel;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UBorder> SelectionBorder;
};
