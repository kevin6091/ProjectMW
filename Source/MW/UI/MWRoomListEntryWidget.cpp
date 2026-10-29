#include "UI/MWRoomListEntryWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

void UMWRoomListEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Button_Row->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSelected);
}

void UMWRoomListEntryWidget::NativeDestruct()
{
	Button_Row->OnClicked.RemoveDynamic(this, &ThisClass::HandleSelected);
	Super::NativeDestruct();
}

void UMWRoomListEntryWidget::SetRoom(const FMWRoomInfo& Info, int32 Revision)
{
	SessionId = Info.SessionId;
	ListRevision = Revision;
	Text_RoomName->SetText(FText::FromString(Info.RoomName));
	Text_Capacity->SetText(FText::FromString(FString::Printf(TEXT("%d / %d명"), Info.CurrentPlayers, Info.MaxPlayers)));
	Text_Theme->SetText(FText::FromName(Info.MapThemeId));
	Text_Availability->SetText(FText::FromString(Info.bCanJoin ? TEXT("참가 가능") : Info.UnavailableReason));
	Button_Row->SetToolTipText(FText::FromString(Info.UnavailableReason));
	SetSelected(false);
}

void UMWRoomListEntryWidget::SetSelected(bool bSelected)
{
	Button_Row->SetBackgroundColor(bSelected ? FLinearColor(0.12f, 0.45f, 0.67f, 1)
		: FLinearColor(0.12f, 0.18f, 0.25f, 1));
}

void UMWRoomListEntryWidget::HandleSelected()
{
	OnRoomSelected.Broadcast(SessionId, ListRevision);
}
