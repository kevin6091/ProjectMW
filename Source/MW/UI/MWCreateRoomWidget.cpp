#include "UI/MWCreateRoomWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "System/MultiplayerSubSystem/MWMultiplayerSubsystem.h"

void UMWCreateRoomWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	UMWMultiplayerSubsystem* Sessions = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UMWMultiplayerSubsystem>() : nullptr;
	Subsystem = Sessions;
	if (Sessions)
	{
		Sessions->OnLoginFinished.AddUniqueDynamic(this, &ThisClass::HandleLoginFinished);
		Sessions->OnSessionStateChanged.AddUniqueDynamic(this, &ThisClass::HandleSessionStateChanged);
		Sessions->OnSessionError.AddUniqueDynamic(this, &ThisClass::HandleSessionError);
		Sessions->OnCreateRoomFinished.AddUniqueDynamic(this, &ThisClass::HandleCreateFinished);
		AvailableMapThemeIds = Sessions->GetAvailableMapThemeIds();
	}

	// Preserve a valid selection when the same widget is added again.
	Select_MapTheme->OnSelectionChanged.RemoveDynamic(this, &ThisClass::HandleThemeSelected);
	Select_MapTheme->ClearOptions();
	for (FName Id : AvailableMapThemeIds)
	{
		Select_MapTheme->AddOption(Id.ToString());
	}
	if (!AvailableMapThemeIds.Contains(SelectedMapThemeId))
	{
		SelectedMapThemeId = AvailableMapThemeIds.IsEmpty() ? NAME_None : AvailableMapThemeIds[0];
	}
	if (!SelectedMapThemeId.IsNone())
	{
		Select_MapTheme->SetSelectedOption(SelectedMapThemeId.ToString());
	}
	Select_MapTheme->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleThemeSelected);

	const FString PreviousCapacity = Select_MaxPlayers->GetSelectedOption();
	// Rebuild runtime options so the displayed selection belongs to this instance.
	Select_MaxPlayers->ClearOptions();
	Select_MaxPlayers->AddOption(TEXT("2"));
	Select_MaxPlayers->AddOption(TEXT("3"));
	Select_MaxPlayers->AddOption(TEXT("4"));
	Select_MaxPlayers->SetSelectedOption(
		PreviousCapacity == TEXT("2") || PreviousCapacity == TEXT("4") ? PreviousCapacity : TEXT("3"));

	Button_CreateRoom->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCreateClicked);
	Button_Back->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBackClicked);
	Button_RetryCleanup->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRetryClicked);
	RefreshForm();
}

void UMWCreateRoomWidget::NativeDestruct()
{
	if (UMWMultiplayerSubsystem* Sessions = Subsystem.Get())
	{
		Sessions->OnLoginFinished.RemoveDynamic(this, &ThisClass::HandleLoginFinished);
		Sessions->OnSessionStateChanged.RemoveDynamic(this, &ThisClass::HandleSessionStateChanged);
		Sessions->OnSessionError.RemoveDynamic(this, &ThisClass::HandleSessionError);
		Sessions->OnCreateRoomFinished.RemoveDynamic(this, &ThisClass::HandleCreateFinished);
	}
	Button_CreateRoom->OnClicked.RemoveDynamic(this, &ThisClass::HandleCreateClicked);
	Button_Back->OnClicked.RemoveDynamic(this, &ThisClass::HandleBackClicked);
	Button_RetryCleanup->OnClicked.RemoveDynamic(this, &ThisClass::HandleRetryClicked);
	Select_MapTheme->OnSelectionChanged.RemoveDynamic(this, &ThisClass::HandleThemeSelected);
	Subsystem.Reset();
	Super::NativeDestruct();
}

void UMWCreateRoomWidget::RefreshForm()
{
	UMWMultiplayerSubsystem* Sessions = Subsystem.Get();
	const EMWSessionState State = Sessions ? Sessions->GetSessionState() : EMWSessionState::None;
	const bool bIdle = Sessions && State == EMWSessionState::None;
	const bool bCanCreate = bIdle && Sessions->IsLoggedIn() && !Sessions->IsLoginInProgress();
	Input_RoomName->SetIsEnabled(bCanCreate);
	Select_MaxPlayers->SetIsEnabled(bCanCreate);
	Select_MapTheme->SetIsEnabled(bCanCreate && !AvailableMapThemeIds.IsEmpty());
	Button_CreateRoom->SetIsEnabled(bCanCreate && AvailableMapThemeIds.Contains(SelectedMapThemeId));
	Button_Back->SetIsEnabled(bIdle);
	Button_RetryCleanup->SetVisibility(State == EMWSessionState::CleanupRequired
		? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	Button_RetryCleanup->SetIsEnabled(Sessions && State == EMWSessionState::CleanupRequired);

	FString Status;
	if (!Sessions) Status = TEXT("멀티플레이 시스템을 확인할 수 없습니다.");
	else if (State == EMWSessionState::CleanupRequired) Status = TEXT("이전 세션 정리가 필요합니다. 정리 재시도 후에도 해결되지 않으면 게임을 다시 실행하세요.");
	else if (State == EMWSessionState::Creating) Status = TEXT("방을 생성하고 서버와 맵을 준비하고 있습니다...");
	else if (State == EMWSessionState::Leaving) Status = TEXT("세션을 정리하고 타이틀로 돌아가고 있습니다...");
	else if (State != EMWSessionState::None) Status = TEXT("진행 중인 세션 작업을 완료해 주세요.");
	else if (!Sessions->IsLoggedIn()) Status = TEXT("먼저 로그인해 주세요.");
	else if (AvailableMapThemeIds.IsEmpty()) Status = TEXT("사용 가능한 맵 테마가 없습니다. 게임 콘텐츠 설정을 확인해 주세요.");
	else Status = TEXT("방 이름, 최대 인원, 맵 테마를 선택해 주세요. 최대 인원에는 호스트가 포함됩니다.");
	Text_Status->SetText(FText::FromString(Status));
	const FString Error = !LocalError.IsEmpty() ? LocalError : (Sessions ? Sessions->GetLastSessionError() : FString());
	Text_Error->SetText(FText::FromString(Error));
}

void UMWCreateRoomWidget::HandleCreateClicked()
{
	UMWMultiplayerSubsystem* Sessions = Subsystem.Get();
	if (!Sessions || Sessions->GetSessionState() != EMWSessionState::None ||
		!Sessions->IsLoggedIn() || Sessions->IsLoginInProgress())
	{
		RefreshForm();
		return;
	}

	FMWRoomSettings Settings;
	Settings.RoomName = Input_RoomName->GetText().ToString();
	Settings.MaxPlayers = FCString::Atoi(*Select_MaxPlayers->GetSelectedOption());
	Settings.MapThemeId = FGameplayTag::RequestGameplayTag(SelectedMapThemeId, false);
	if (!AvailableMapThemeIds.Contains(SelectedMapThemeId) || !Settings.MapThemeId.IsValid())
	{
		LocalError = TEXT("카탈로그에 등록된 맵 테마를 선택해 주세요.");
		RefreshForm();
		return;
	}

	LocalError.Reset();
	// The subsystem validates all inputs and resolves the level via AssetManager.
	Sessions->RequestCreateRoom(Settings);
	RefreshForm();
}

void UMWCreateRoomWidget::HandleBackClicked()
{
	if (UMWMultiplayerSubsystem* Sessions = Subsystem.Get();
		Sessions && Sessions->GetSessionState() == EMWSessionState::None)
	{
		OnBackRequested.Broadcast();
	}
}

void UMWCreateRoomWidget::HandleRetryClicked()
{
	if (UMWMultiplayerSubsystem* Sessions = Subsystem.Get();
		Sessions && Sessions->GetSessionState() == EMWSessionState::CleanupRequired)
	{
		LocalError.Reset();
		Sessions->RetrySessionCleanup();
		RefreshForm();
	}
}

void UMWCreateRoomWidget::HandleThemeSelected(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	const FName Id(*SelectedItem);
	SelectedMapThemeId = AvailableMapThemeIds.Contains(Id) ? Id : NAME_None;
	RefreshForm();
}

void UMWCreateRoomWidget::HandleLoginFinished(bool bSuccess, const FString& ErrorMessage)
{
	RefreshForm();
}

void UMWCreateRoomWidget::HandleSessionStateChanged(EMWSessionState PreviousState, EMWSessionState NewState)
{
	if (NewState == EMWSessionState::Creating) LocalError.Reset();
	RefreshForm();
}

void UMWCreateRoomWidget::HandleSessionError(const FString& ErrorMessage)
{
	LocalError = ErrorMessage;
	RefreshForm();
}

void UMWCreateRoomWidget::HandleCreateFinished(bool bSuccess, const FString& ErrorMessage)
{
	if (!bSuccess) LocalError = ErrorMessage;
	// A completion bool never overrides the subsystem's actual session state.
	RefreshForm();
}
