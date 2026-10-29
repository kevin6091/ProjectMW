#include "Frontend/MWRoomRowWidget.h"

#include "Components/Border.h"
#include "Components/TextBlock.h"

void UMWRoomRowWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);
	const UMWRoomListItem* Item = Cast<UMWRoomListItem>(ListItemObject);
	if (!Item)
	{
		RoomLabel->SetText(FText::GetEmpty());
		return;
	}
	const FMWRoomSearchResult& Room = Item->Room;
	RoomLabel->SetText(FText::Format(NSLOCTEXT("MWFrontend", "RoomRow", "{0}    |    {1}/{2}명    |    {3}    {4}"),
		Room.RoomName, FText::AsNumber(Room.PlayerCount), FText::AsNumber(Room.MaxPlayers), Room.ThemeName,
		Room.IsJoinable() ? FText::GetEmpty() : Room.UnavailableReason));
	SetToolTipText(Room.UnavailableReason);
	SetRenderOpacity(Room.IsJoinable() ? 1.f : .78f);
	NativeOnItemSelectionChanged(IsListItemSelected());
}

void UMWRoomRowWidget::NativeOnItemSelectionChanged(bool bIsSelected)
{
	IUserObjectListEntry::NativeOnItemSelectionChanged(bIsSelected);
	SelectionBorder->SetBrushColor(FLinearColor::FromSRGBColor(bIsSelected ? FColor(0, 152, 248) : FColor(255, 178, 33)));
}

void UMWRoomRowWidget::NativeOnEntryReleased()
{
	IUserObjectListEntry::NativeOnEntryReleased();
	RoomLabel->SetText(FText::GetEmpty());
	SetToolTipText(FText::GetEmpty());
	SetRenderOpacity(1.f);
}
