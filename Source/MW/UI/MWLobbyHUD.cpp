#include "UI/MWLobbyHUD.h"

#include "Player/MWLobbyPlayerController.h"
#include "System/GameState/MWLobbyGameState.h"
#include "System/MWGameInstance.h"
#include "System/MWPlayerState.h"
#include "UI/MWRoomLobbyWidget.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AMWLobbyHUD::AMWLobbyHUD()
{
	// 기존 Frontend 디자인이 적용된 Widget을 재사용하며 새 화면을 만들지 않습니다.
	static ConstructorHelpers::FClassFinder<UMWRoomLobbyWidget> LobbyWidget(TEXT("/Game/UI/Lobby/WBP_RoomLobby"));
	RoomLobbyWidgetClass = LobbyWidget.Class;
}

void AMWLobbyHUD::BeginPlay()
{
	Super::BeginPlay();
	auto* PC = GetOwningPlayerController();
	const auto* App = GetGameInstance<UMWGameInstance>();
	// Standalone 타이틀은 기존 L_Title Level Blueprint가 관리합니다. 로비 화면만 이곳에서 생성합니다.
	if (!PC || !PC->IsLocalController() || !App || !App->IsLobbyWorld(GetWorld()) || GetNetMode() == NM_Standalone) return;
	if (RoomLobbyWidgetClass)
	{
		RoomLobbyWidget = CreateWidget<UMWRoomLobbyWidget>(PC, RoomLobbyWidgetClass);
		if (RoomLobbyWidget)
		{
			RoomLobbyWidget->AddToViewport();
			FInputModeUIOnly InputMode;
			InputMode.SetWidgetToFocus(RoomLobbyWidget->TakeWidget());
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PC->SetInputMode(InputMode);
			PC->bShowMouseCursor = true;
		}
	}

	// GameState/자기 PlayerState가 늦게 도착해도 초기 연결을 놓치지 않습니다.
	GetWorldTimerManager().SetTimer(BindingRetryTimer, this, &ThisClass::RefreshLobbyScreen, 0.1f, true);
	RefreshLobbyScreen();
}

void AMWLobbyHUD::RefreshLobbyScreen()
{
	if (!RoomLobbyWidget) return;
	RefreshBindings();
	RoomLobbyWidget->Refresh();
	if (BoundGameState.IsValid() && GetOwningPlayerController() && GetOwningPlayerController()->GetPlayerState<AMWPlayerState>())
	{
		// 초기 연결 후에는 변경 이벤트로 갱신하고 주기적인 화면 조회는 중단합니다.
		GetWorldTimerManager().ClearTimer(BindingRetryTimer);
	}
}

void AMWLobbyHUD::RefreshBindings()
{
	auto* GS = GetWorld()->GetGameState<AMWLobbyGameState>();
	auto* PC = Cast<AMWLobbyPlayerController>(GetOwningPlayerController());
	if (BoundGameState.Get() != GS || BoundController.Get() != PC)
	{
		ClearBindings();
		BoundGameState = GS;
		BoundController = PC;
		if (GS) GS->OnLobbyChanged.AddUniqueDynamic(this, &ThisClass::HandleLobbyChanged);
		if (PC) PC->OnLobbyActionErrorChanged.AddUniqueDynamic(this, &ThisClass::HandleActionErrorChanged);
	}
	if (!GS) return;

	// 입퇴장으로 참가자가 바뀌면 이탈자의 구독을 해제하고 새 참가자의 구독을 추가합니다.
	for (int32 Index = BoundPlayers.Num() - 1; Index >= 0; --Index)
	{
		auto* Player = BoundPlayers[Index].Get();
		if (!Player || !GS->PlayerArray.Contains(Player))
		{
			if (Player) Player->OnPlayerStateChanged.RemoveDynamic(this, &ThisClass::HandlePlayerStateChanged);
			BoundPlayers.RemoveAt(Index);
		}
	}
	for (APlayerState* Player : GS->PlayerArray)
	{
		if (auto* PS = Cast<AMWPlayerState>(Player))
		{
			if (!BoundPlayers.Contains(PS))
			{
				BoundPlayers.Add(PS);
				PS->OnPlayerStateChanged.AddUniqueDynamic(this, &ThisClass::HandlePlayerStateChanged);
			}
		}
	}
}

void AMWLobbyHUD::ClearBindings()
{
	if (auto* GS = BoundGameState.Get()) GS->OnLobbyChanged.RemoveDynamic(this, &ThisClass::HandleLobbyChanged);
	if (auto* PC = BoundController.Get()) PC->OnLobbyActionErrorChanged.RemoveDynamic(this, &ThisClass::HandleActionErrorChanged);
	for (const auto& WeakPlayer : BoundPlayers)
		if (auto* PS = WeakPlayer.Get()) PS->OnPlayerStateChanged.RemoveDynamic(this, &ThisClass::HandlePlayerStateChanged);
	BoundPlayers.Reset();
	BoundGameState.Reset();
	BoundController.Reset();
}

void AMWLobbyHUD::HandleLobbyChanged() { RefreshLobbyScreen(); }
void AMWLobbyHUD::HandlePlayerStateChanged() { RefreshLobbyScreen(); }
void AMWLobbyHUD::HandleActionErrorChanged() { RefreshLobbyScreen(); }

void AMWLobbyHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(BindingRetryTimer);
	ClearBindings();
	if (RoomLobbyWidget) RoomLobbyWidget->RemoveFromParent();
	RoomLobbyWidget = nullptr;
	Super::EndPlay(EndPlayReason);
}
