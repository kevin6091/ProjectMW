#include "UI/Lobby/MWRoomLobbyWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "System/GameState/MWLobbyGameState.h"
#include "System/MWPlayerState.h"
#include "Player/MWLobbyPlayerController.h"
#include "System/MWGameInstance.h"

void UMWRoomLobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	Subsystem = GetGameInstance() ? Cast<UMWGameInstance>(GetGameInstance()) : nullptr;
	if (UMWGameInstance* Sessions = Subsystem.Get())
	{
		Sessions->OnSessionStateChanged.AddUniqueDynamic(this, &ThisClass::HandleSessionStateChanged);
		Sessions->OnSessionError.AddUniqueDynamic(this, &ThisClass::HandleSessionError);
	}
	// A world-space Slate button can lose its capture before mouse-up in a game window.
	// PreciseClick still requires a press and release on the button.
	for (UButton* Button : {Button_Leave.Get(), Button_RetryCleanup.Get(), Button_Ready.Get(), Button_Start.Get()})
		Button->SetClickMethod(EButtonClickMethod::PreciseClick);
	Button_Leave->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleLeaveClicked);
	Button_RetryCleanup->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRetryClicked);
	Button_Ready->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleReadyClicked);
	Button_Start->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleStartClicked);
	Button_Ready->SetToolTipText(FText::FromString(TEXT("자신의 준비 상태를 변경합니다.")));
	Button_Start->SetToolTipText(FText::FromString(TEXT("최소 2명 · 전원 준비 완료 후 호스트가 시작합니다.")));
	Refresh();
}

void UMWRoomLobbyWidget::NativeDestruct()
{
	if (UMWGameInstance* Sessions = Subsystem.Get())
	{
		Sessions->OnSessionStateChanged.RemoveDynamic(this, &ThisClass::HandleSessionStateChanged);
		Sessions->OnSessionError.RemoveDynamic(this, &ThisClass::HandleSessionError);
	}
	Button_Leave->OnClicked.RemoveDynamic(this, &ThisClass::HandleLeaveClicked);
	Button_RetryCleanup->OnClicked.RemoveDynamic(this, &ThisClass::HandleRetryClicked);
	Button_Ready->OnClicked.RemoveDynamic(this, &ThisClass::HandleReadyClicked);
	Button_Start->OnClicked.RemoveDynamic(this, &ThisClass::HandleStartClicked);
	Subsystem.Reset();
	Super::NativeDestruct();
}

void UMWRoomLobbyWidget::Refresh()
{
	UMWGameInstance* Sessions = Subsystem.Get();
	const EMWSessionState State = Sessions ? Sessions->GetSessionState() : EMWSessionState::None;
	const bool bInRoom = State == EMWSessionState::Hosting || State == EMWSessionState::Joined;
	const AMWLobbyGameState* RoomGS = GetWorld() ? GetWorld()->GetGameState<AMWLobbyGameState>() : nullptr;
	const AMWPlayerState* OwnPlayer = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<AMWPlayerState>() : nullptr;
	const bool bLobbyPhase = RoomGS && RoomGS->GetRoomPhase() == EMWRoomPhase::Lobby;
	const bool bNetworkLobby = Sessions && Sessions->IsLobbyWorld(GetWorld()) && GetWorld()->GetNetMode() != NM_Standalone;

	SetVisibility(bNetworkLobby ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	Button_Leave->SetIsEnabled(bInRoom && bLobbyPhase);
	Button_Ready->SetIsEnabled(bInRoom && bLobbyPhase && OwnPlayer);
	Button_Start->SetIsEnabled(bInRoom && OwnPlayer && OwnPlayer->IsRoomHost() && RoomGS && RoomGS->CanStartGame());

	if (UTextBlock* Label = Cast<UTextBlock>(GetWidgetFromName(TEXT("Button_Ready_Label"))))
		Label->SetText(FText::FromString(OwnPlayer && OwnPlayer->IsReady() ? TEXT("준비 취소") : TEXT("준비 완료")));
	if (UTextBlock* Info = Cast<UTextBlock>(GetWidgetFromName(TEXT("LobbyInfo"))))
		Info->SetText(FText::FromString(TEXT("최소 2명 · 호스트 포함 전원 Ready\n\n호스트가 게임 시작을 누르면 선택한 맵 테마로 함께 이동합니다.")));

	Button_RetryCleanup->SetVisibility(State == EMWSessionState::CleanupRequired
		? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	Button_RetryCleanup->SetIsEnabled(Sessions && State == EMWSessionState::CleanupRequired);
	Box_RoomInfo->SetVisibility(ESlateVisibility::Collapsed);
	const AMWLobbyPlayerController* LocalPC = Cast<AMWLobbyPlayerController>(GetOwningPlayer());
	const FString ActionError = LocalPC ? LocalPC->GetLobbyActionError() : FString();
	Text_Error->SetText(FText::FromString(!ActionError.IsEmpty() ? ActionError : (Sessions ? Sessions->GetLastSessionError() : FString())));

	FString Status = TEXT("로비 정보를 준비 중입니다...");
	if (!Sessions) Status = TEXT("멀티플레이 시스템을 확인할 수 없습니다.");
	else if (State == EMWSessionState::Leaving) Status = TEXT("방을 종료하고 세션을 정리하고 있습니다...");
	else if (State == EMWSessionState::CleanupRequired) Status = TEXT("세션 정리가 필요합니다. 재시도 후에도 해결되지 않으면 게임을 다시 실행하세요.");
	else if (bInRoom)
	{
		const APlayerController* PC = GetOwningPlayer();
		const AMWPlayerState* OwnPS = PC ? PC->GetPlayerState<AMWPlayerState>() : nullptr;
		const AMWLobbyGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMWLobbyGameState>() : nullptr;
		if (OwnPS && GS)
		{
			const FMWRoomSettings Settings = GS->GetRoomSettings();
			if (!Settings.RoomName.IsEmpty() && Settings.MapThemeId.IsValid())
			{
				Box_RoomInfo->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
				Text_RoomName->SetText(FText::FromString(Settings.RoomName));
				Text_Capacity->SetText(FText::FromString(FString::Printf(TEXT("참가 인원  %d / %d명"), GS->PlayerArray.Num(), Settings.MaxPlayers)));
				Text_MapTheme->SetText(FText::FromString(Settings.MapThemeId.ToString()));
				TArray<FString> Rows;
				for (const APlayerState* Player : GS->PlayerArray)
				{
					const AMWPlayerState* PS = Cast<AMWPlayerState>(Player);
					if (!PS) continue;
					const FString Name = PS->GetPlayerName().IsEmpty() ? TEXT("플레이어") : PS->GetPlayerName();
					const FString MemberId = PS->GetLobbyMemberId() > 0 ? FString::Printf(TEXT("ID %02d"), PS->GetLobbyMemberId()) : TEXT("ID 확인 중");
					Rows.Add(FString::Printf(TEXT("%s  ·  %s    %s    %s"), *MemberId, *Name,
						PS->IsRoomHost() ? TEXT("호스트") : TEXT("참가자"),
						(PS->IsReady() ? TEXT("준비 완료") : TEXT("준비 대기"))));
				}
				Text_Players->SetText(FText::FromString(FString::Join(Rows, TEXT("\n"))));
				// Designer-only participant boards. All values still come from MW replicated state.
				TArray<const AMWPlayerState*> DisplayPlayers;
				for (const APlayerState* Player : GS->PlayerArray)
				{
					if (const AMWPlayerState* PS = Cast<AMWPlayerState>(Player)) DisplayPlayers.Add(PS);
				}
				DisplayPlayers.Sort([](const AMWPlayerState& A, const AMWPlayerState& B)
				{
					return A.GetLobbyMemberId() < B.GetLobbyMemberId();
				});
				for (int32 Index = 0; Index < 4; ++Index)
				{
					const AMWPlayerState* PS = DisplayPlayers.IsValidIndex(Index) ? DisplayPlayers[Index] : nullptr;
					const bool bSlotAvailable = Index < Settings.MaxPlayers;
					if (UTextBlock* Label = Cast<UTextBlock>(GetWidgetFromName(FName(*FString::Printf(TEXT("ParticipantSlot%d"), Index)))))
					{
						const FString Name = PS && !PS->GetPlayerName().IsEmpty() ? PS->GetPlayerName() : TEXT("플레이어");
						Label->SetText(FText::FromString(PS ? FString::Printf(TEXT("ID %02d  ·  %s"), PS->GetLobbyMemberId(), *Name)
							: (bSlotAvailable ? TEXT("참가 대기") : TEXT("정원 외 슬롯"))));
					}
					if (UTextBlock* Label = Cast<UTextBlock>(GetWidgetFromName(FName(*FString::Printf(TEXT("ParticipantState%d"), Index)))))
					{
						Label->SetText(FText::FromString(PS ? FString::Printf(TEXT("%s\n%s"),
							PS->IsRoomHost() ? TEXT("호스트") : TEXT("참가자"),
							 (PS->IsReady() ? TEXT("준비 완료") : TEXT("준비 대기"))) : TEXT("")));
					}
					if (UWidget* Icon = GetWidgetFromName(FName(*FString::Printf(TEXT("EmptySlotIcon%d"), Index))))
					{
						Icon->SetVisibility(PS || !bSlotAvailable ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
					}
					if (UWidget* Board = GetWidgetFromName(FName(*FString::Printf(TEXT("ParticipantBoard%d"), Index))))
					{
						Board->SetRenderOpacity(bSlotAvailable ? 1.f : .4f);
					}
				}
				Text_LeaveLabel->SetText(FText::FromString(OwnPS->IsRoomHost() ? TEXT("방 종료") : TEXT("나가기")));

				switch (RoomGS->GetRoomPhase())
				{
				case EMWRoomPhase::Lobby:
					Status = TEXT(
						"현재 참가자 전원이 준비를 완료하면 호스트가 게임을 시작할 수 있습니다.");
					break;

				case EMWRoomPhase::Starting:
					Status = TEXT("입장을 닫고 경기 시작을 준비하고 있습니다...");
					break;

				case EMWRoomPhase::VehicleSelection:
					Status = TEXT("참가자들의 차량 선택을 기다리고 있습니다...");
					break;

				case EMWRoomPhase::Traveling:
					Status = TEXT("선택한 테마 레벨로 이동하고 있습니다...");
					break;

				case EMWRoomPhase::Failed:
					Status = TEXT("진행을 중단하고 방을 정리하고 있습니다...");
					break;

				default:
					Status = TEXT("경기를 준비하고 있습니다...");
					break;
				}
			}
		}
	}
	Text_Status->SetText(FText::FromString(Status));
}

void UMWRoomLobbyWidget::HandleLeaveClicked()
{
	if (UMWGameInstance* Sessions = Subsystem.Get(); Sessions &&
		(Sessions->GetSessionState() == EMWSessionState::Hosting || Sessions->GetSessionState() == EMWSessionState::Joined))
	{
		Sessions->RequestLeaveRoom();
		Refresh();
	}
}

void UMWRoomLobbyWidget::HandleSessionStateChanged(EMWSessionState PreviousState, EMWSessionState NewState)
{
	// The widget is constructed while Host/Join is still pending. Its game state may
	// already be complete before the connection service emits Hosting/Joined.
	Refresh();
}

void UMWRoomLobbyWidget::HandleSessionError(const FString& ErrorMessage)
{
	Refresh();
}

void UMWRoomLobbyWidget::HandleRetryClicked()
{
	if (UMWGameInstance* Sessions = Subsystem.Get();
		Sessions && Sessions->GetSessionState() == EMWSessionState::CleanupRequired)
	{
		Sessions->RetrySessionCleanup();
		Refresh();
	}
}

void UMWRoomLobbyWidget::HandleReadyClicked()
{
	AMWLobbyPlayerController* PC = Cast<AMWLobbyPlayerController>(GetOwningPlayer());
	const AMWPlayerState* PS = PC ? PC->GetPlayerState<AMWPlayerState>() : nullptr;
	if (PC && PS && Button_Ready->GetIsEnabled()) PC->RequestSetReady(!PS->IsReady());
}

void UMWRoomLobbyWidget::HandleStartClicked()
{
	if (AMWLobbyPlayerController* PC = Cast<AMWLobbyPlayerController>(GetOwningPlayer()); PC && Button_Start->GetIsEnabled())
		PC->RequestStartGame();
}
