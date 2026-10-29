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
	// The Designer uses the Frontend sign brush; selection still belongs to the MW browser.
	Button_Row->SetBackgroundColor(FLinearColor::FromSRGBColor(
		bSelected ? FColor(0, 152, 248) : FColor(255, 178, 33)));
}

void UMWRoomListEntryWidget::HandleSelected()
{
	OnRoomSelected.Broadcast(SessionId, ListRevision);
}
