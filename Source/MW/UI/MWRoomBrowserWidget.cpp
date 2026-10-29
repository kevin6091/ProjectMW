#include "UI/MWRoomBrowserWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"
#include "System/MultiplayerSubSystem/MWMultiplayerSubsystem.h"
#include "UI/MWRoomListEntryWidget.h"

void UMWRoomBrowserWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	UMWMultiplayerSubsystem* Sessions = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMWMultiplayerSubsystem>() : nullptr;
	Subsystem = Sessions;
	if (Sessions)
	{
		Sessions->OnRoomsUpdated.AddUniqueDynamic(this, &ThisClass::HandleRoomsUpdated);
		Sessions->OnSessionStateChanged.AddUniqueDynamic(this, &ThisClass::HandleStateChanged);
		Sessions->OnSessionError.AddUniqueDynamic(this, &ThisClass::HandleError);
	}
	Button_Refresh->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRefresh);
	Button_Back->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBack);
	Button_Join->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleJoin);
	Button_RetryCleanup->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRetry);
	RebuildRows();
	RefreshView();
}

void UMWRoomBrowserWidget::NativeDestruct()
{
	if (UMWMultiplayerSubsystem* Sessions = Subsystem.Get())
	{
		Sessions->OnRoomsUpdated.RemoveDynamic(this, &ThisClass::HandleRoomsUpdated);
		Sessions->OnSessionStateChanged.RemoveDynamic(this, &ThisClass::HandleStateChanged);
		Sessions->OnSessionError.RemoveDynamic(this, &ThisClass::HandleError);
	}
	for (UMWRoomListEntryWidget* Entry : Entries) Entry->OnRoomSelected.RemoveDynamic(this, &ThisClass::HandleRoomSelected);
	Button_Refresh->OnClicked.RemoveDynamic(this, &ThisClass::HandleRefresh);
	Button_Back->OnClicked.RemoveDynamic(this, &ThisClass::HandleBack);
	Button_Join->OnClicked.RemoveDynamic(this, &ThisClass::HandleJoin);
	Button_RetryCleanup->OnClicked.RemoveDynamic(this, &ThisClass::HandleRetry);
	Subsystem.Reset();
	Super::NativeDestruct();
}

void UMWRoomBrowserWidget::OpenRoomList()
{
	SelectedSessionId.Reset();
	SelectedRevision = 0;
	HandleRefresh();
}

void UMWRoomBrowserWidget::RebuildRows()
{
	for (UMWRoomListEntryWidget* Entry : Entries) Entry->OnRoomSelected.RemoveDynamic(this, &ThisClass::HandleRoomSelected);
	Box_Rooms->ClearChildren();
	Entries.Reset();
	Rows.Reset();
	UMWMultiplayerSubsystem* Sessions = Subsystem.Get();
	if (!Sessions || !RoomEntryWidgetClass) return;
	Rows = Sessions->GetAvailableRooms();
	for (const FMWRoomInfo& Room : Rows)
	{
		UMWRoomListEntryWidget* Entry = CreateWidget<UMWRoomListEntryWidget>(GetOwningPlayer(), RoomEntryWidgetClass);
		if (!Entry) continue;
		Box_Rooms->AddChild(Entry);
		Entry->SetRoom(Room, Sessions->GetRoomListRevision());
		Entry->OnRoomSelected.AddUniqueDynamic(this, &ThisClass::HandleRoomSelected);
		Entries.Add(Entry);
	}
}

void UMWRoomBrowserWidget::RefreshView()
{
	UMWMultiplayerSubsystem* Sessions = Subsystem.Get();
	const EMWSessionState State = Sessions ? Sessions->GetSessionState() : EMWSessionState::None;
	const bool bIdle = Sessions && Sessions->IsLoggedIn() && State == EMWSessionState::None;
	const FMWRoomInfo* Selected = Rows.FindByPredicate([this](const FMWRoomInfo& R) { return R.SessionId == SelectedSessionId; });
	const bool bCurrentSelection = Sessions && SelectedRevision == Sessions->GetRoomListRevision();
	Button_Refresh->SetIsEnabled(bIdle);
	Button_Back->SetIsEnabled(Sessions && State == EMWSessionState::None);
	Button_Join->SetIsEnabled(bIdle && bCurrentSelection && Selected && Selected->bCanJoin);
	Box_Rooms->SetIsEnabled(bIdle);
	Button_RetryCleanup->SetVisibility(State == EMWSessionState::CleanupRequired ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	Button_RetryCleanup->SetIsEnabled(Sessions && State == EMWSessionState::CleanupRequired);
	Text_Error->SetText(FText::FromString(Sessions ? Sessions->GetLastSessionError() : TEXT("온라인 시스템을 확인할 수 없습니다.")));
	FString Status;
	if (State == EMWSessionState::Searching) Status = TEXT("현재 열려 있는 방 목록을 불러오고 있습니다...");
	else if (State == EMWSessionState::Joining) Status = TEXT("선택한 방에 연결하고 로비 정보를 확인하고 있습니다...");
	else if (State == EMWSessionState::Leaving) Status = TEXT("세션을 정리하고 있습니다...");
	else if (State == EMWSessionState::CleanupRequired) Status = TEXT("세션 정리가 필요합니다. 정리 재시도 후에도 해결되지 않으면 게임을 다시 실행하세요.");
	else if (Rows.IsEmpty()) Status = TEXT("현재 표시할 방이 없습니다. 새로고침하거나 직접 방을 만들어 보세요.");
	else if (Selected && !Selected->bCanJoin) Status = Selected->UnavailableReason;
	else Status = FString::Printf(TEXT("방 %d개. 방을 선택하고 참가 확인을 눌러 주세요."), Rows.Num());
	Text_Status->SetText(FText::FromString(Status));
	Text_SelectedRoom->SetText(FText::FromString(Selected && bCurrentSelection ? TEXT("선택한 방: ") + Selected->RoomName : TEXT("선택한 방 없음")));
}

void UMWRoomBrowserWidget::HandleRefresh()
{
	if (UMWMultiplayerSubsystem* Sessions = Subsystem.Get(); Sessions && Sessions->GetSessionState() == EMWSessionState::None)
	{
		SelectedSessionId.Reset();
		SelectedRevision = 0;
		Sessions->RequestRefreshRooms();
		RebuildRows();
		RefreshView();
	}
}

void UMWRoomBrowserWidget::HandleBack()
{
	if (UMWMultiplayerSubsystem* Sessions = Subsystem.Get(); Sessions && Sessions->GetSessionState() == EMWSessionState::None) OnBackRequested.Broadcast();
}

void UMWRoomBrowserWidget::HandleJoin()
{
	if (UMWMultiplayerSubsystem* Sessions = Subsystem.Get()) Sessions->RequestJoinRoom(SelectedSessionId, SelectedRevision);
	RefreshView();
}

void UMWRoomBrowserWidget::HandleRetry()
{
	if (UMWMultiplayerSubsystem* Sessions = Subsystem.Get(); Sessions && Sessions->GetSessionState() == EMWSessionState::CleanupRequired) Sessions->RetrySessionCleanup();
	RefreshView();
}

void UMWRoomBrowserWidget::HandleRoomSelected(const FString& SessionId, int32 Revision)
{
	UMWMultiplayerSubsystem* Sessions = Subsystem.Get();
	if (!Sessions || Sessions->GetSessionState() != EMWSessionState::None || Revision != Sessions->GetRoomListRevision()) return;
	SelectedSessionId = SessionId;
	SelectedRevision = Revision;
	for (int32 I = 0; I < Entries.Num(); ++I) Entries[I]->SetSelected(Rows.IsValidIndex(I) && Rows[I].SessionId == SessionId);
	RefreshView();
}

void UMWRoomBrowserWidget::HandleRoomsUpdated(bool bSuccess, const FString& ErrorMessage)
{
	SelectedSessionId.Reset();
	SelectedRevision = 0;
	RebuildRows();
	RefreshView();
}

void UMWRoomBrowserWidget::HandleStateChanged(EMWSessionState PreviousState, EMWSessionState NewState) { RefreshView(); }
void UMWRoomBrowserWidget::HandleError(const FString& ErrorMessage) { RefreshView(); }
