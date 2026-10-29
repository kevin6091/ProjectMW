#include "UI/MWRoomLobbyWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "System/MWGameState.h"
#include "System/MWPlayerState.h"
#include "System/MultiplayerSubSystem/MWMultiplayerSubsystem.h"

void UMWRoomLobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	Subsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMWMultiplayerSubsystem>() : nullptr;
	Button_Leave->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleLeaveClicked);
	Button_RetryCleanup->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRetryClicked);
	Refresh();
}

void UMWRoomLobbyWidget::NativeDestruct()
{
	Button_Leave->OnClicked.RemoveDynamic(this, &ThisClass::HandleLeaveClicked);
	Button_RetryCleanup->OnClicked.RemoveDynamic(this, &ThisClass::HandleRetryClicked);
	Subsystem.Reset();
	Super::NativeDestruct();
}

void UMWRoomLobbyWidget::Refresh()
{
	UMWMultiplayerSubsystem* Sessions = Subsystem.Get();
	const EMWSessionState State = Sessions ? Sessions->GetSessionState() : EMWSessionState::None;
	const bool bInRoom = State == EMWSessionState::Hosting || State == EMWSessionState::Joined;
	Button_Leave->SetIsEnabled(bInRoom);
	Button_Ready->SetIsEnabled(false);
	Button_Start->SetIsEnabled(false);
	Button_RetryCleanup->SetVisibility(State == EMWSessionState::CleanupRequired
		? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	Button_RetryCleanup->SetIsEnabled(Sessions && State == EMWSessionState::CleanupRequired);
	Box_RoomInfo->SetVisibility(ESlateVisibility::Collapsed);
	Text_Error->SetText(FText::FromString(Sessions ? Sessions->GetLastSessionError() : FString()));

	FString Status = TEXT("로비 정보를 준비 중입니다...");
	if (!Sessions) Status = TEXT("멀티플레이 시스템을 확인할 수 없습니다.");
	else if (State == EMWSessionState::Leaving) Status = TEXT("방을 종료하고 세션을 정리하고 있습니다...");
	else if (State == EMWSessionState::CleanupRequired) Status = TEXT("세션 정리가 필요합니다. 재시도 후에도 해결되지 않으면 게임을 다시 실행하세요.");
	else if (bInRoom)
	{
		const APlayerController* PC = GetOwningPlayer();
		const AMWPlayerState* OwnPS = PC ? PC->GetPlayerState<AMWPlayerState>() : nullptr;
		const AMWGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMWGameState>() : nullptr;
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
						!PS->IsSessionRegistrationConfirmed() ? TEXT("연결 확인 중") :
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
							!PS->IsSessionRegistrationConfirmed() ? TEXT("연결 확인 중") : (PS->IsReady() ? TEXT("준비 완료") : TEXT("준비 대기"))) : TEXT("")));
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
				Status = TEXT("로비에 입장했습니다. 경기 준비 기능은 준비 중입니다.");
			}
		}
	}
	Text_Status->SetText(FText::FromString(Status));
}

void UMWRoomLobbyWidget::HandleLeaveClicked()
{
	if (UMWMultiplayerSubsystem* Sessions = Subsystem.Get(); Sessions &&
		(Sessions->GetSessionState() == EMWSessionState::Hosting || Sessions->GetSessionState() == EMWSessionState::Joined))
	{
		Sessions->RequestLeaveRoom();
		Refresh();
	}
}

void UMWRoomLobbyWidget::HandleRetryClicked()
{
	if (UMWMultiplayerSubsystem* Sessions = Subsystem.Get();
		Sessions && Sessions->GetSessionState() == EMWSessionState::CleanupRequired)
	{
		Sessions->RetrySessionCleanup();
		Refresh();
	}
}
