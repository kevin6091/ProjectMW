#include "UI/MWLobbyHUD.h"

#include "Player/MWLobbyPlayerController.h"
#include "System/GameState/MWLobbyGameState.h"
#include "System/MWGameInstance.h"
#include "System/MWPlayerState.h"
#include "UI/MWLobbyPresentation.h"
#include "UI/MWRoomLobbyWidget.h"
#include "UI/MWVehicleSelectionWidget.h"
#include "UI/MWVehicleSelectionPreviewActor.h"
#include "MWGameplayTags.h"
#include "Components/WidgetComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AMWLobbyHUD::AMWLobbyHUD()
{
	// 기존 Frontend 디자인이 적용된 Widget을 재사용하며 새 화면을 만들지 않습니다.
	static ConstructorHelpers::FClassFinder<UMWRoomLobbyWidget> LobbyWidget(TEXT("/Game/UI/Lobby/WBP_RoomLobby"));
	RoomLobbyWidgetClass = LobbyWidget.Class;
	static ConstructorHelpers::FClassFinder<UMWVehicleSelectionWidget> SelectionWidget(
		TEXT("/Game/UI/Lobby/VehicleSelection/WBP_VehicleSelection_Preview"));
	VehicleSelectionWidgetClass = SelectionWidget.Class;
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
			RoomLobbyWidget->SetIsFocusable(true);
			for (TActorIterator<AActor> It(GetWorld()); It; ++It)
			{
				if (auto* Presentation = Cast<IMWLobbyPresentation>(*It))
				{
					LobbyPresentation = TWeakInterfacePtr<IMWLobbyPresentation>(*It);
					Presentation->SetLobbyWidgets(RoomLobbyWidget, nullptr);
					break;
				}
			}
			if (!LobbyPresentation.IsValid())
			{
				RoomLobbyWidget->AddToViewport();
				FInputModeUIOnly InputMode;
				InputMode.SetWidgetToFocus(RoomLobbyWidget->TakeWidget());
				InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
				PC->SetInputMode(InputMode);
				PC->bShowMouseCursor = true;
			}
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
	if (const auto* GS = BoundGameState.Get())
	{
		if (GS->GetRoomPhase() == EMWRoomPhase::VehicleSelection && !VehicleSelectionWidget)
			OpenVehicleSelection();
		if (GS->GetRoomPhase() == EMWRoomPhase::Failed || GS->GetRoomPhase() == EMWRoomPhase::Lobby)
			CloseVehicleSelection();
	}
	if (VehicleSelectionWidget)
		VehicleSelectionWidget->RefreshPlayers();
	else
		RoomLobbyWidget->Refresh();
	if (BoundGameState.IsValid() && GetOwningPlayerController() && GetOwningPlayerController()->GetPlayerState<AMWPlayerState>())
	{
		// 초기 연결 후에는 변경 이벤트로 갱신하고 주기적인 화면 조회는 중단합니다.
		GetWorldTimerManager().ClearTimer(BindingRetryTimer);
	}
}

void AMWLobbyHUD::OpenVehicleSelection()
{
	auto* PC = BoundController.Get();
	if (!PC || !VehicleSelectionWidgetClass) return;

	VehicleSelectionWidget = CreateWidget<UMWVehicleSelectionWidget>(PC, VehicleSelectionWidgetClass);
	if (!VehicleSelectionWidget) return;

	// Use the authored layout with actual server-backed selection.
	VehicleSelectionWidget->bPreviewOnly = false;
	VehicleSelectionWidget->OnSelectionRequested.AddUniqueDynamic(this, &ThisClass::HandleVehicleSelectionRequested);
	const auto AddThumbnail = [this](FGameplayTag Id, const TCHAR* Name)
	{
		const FString Path = FString::Printf(
			TEXT("/Game/UI/Lobby/VehicleSelection/T_MW_VehiclePreview_%s.T_MW_VehiclePreview_%s"), Name, Name);
		VehicleSelectionWidget->VehicleThumbnails.Add(Id, TSoftObjectPtr<UTexture2D>(FSoftObjectPath(Path)));
	};
	AddThumbnail(MWGameplayTags::Asset_Vehicle_SMIET, TEXT("Smiet"));
	AddThumbnail(MWGameplayTags::Asset_Vehicle_MAMMOTH, TEXT("Mammoth"));
	AddThumbnail(MWGameplayTags::Asset_Vehicle_UFOLOGY, TEXT("Ufology"));
	AddThumbnail(MWGameplayTags::Asset_Vehicle_DOUG, TEXT("Doug"));
	AddThumbnail(MWGameplayTags::Asset_Vehicle_FRED, TEXT("Fred"));

	RoomLobbyWidget->SetVisibility(ESlateVisibility::Collapsed);
	if (auto* Presentation = LobbyPresentation.Get())
	{
		Presentation->SetLobbyWidgets(RoomLobbyWidget, VehicleSelectionWidget);
		return;
	}
	for (TActorIterator<AMWVehicleSelectionPreviewActor> It(GetWorld()); It; ++It)
	{
		UWidgetComponent* Surface = It->PreviewSurface;
		if (!Surface || !Surface->GetReceiveHardwareInput() || !IsValid(It->PreviewViewTarget)) continue;
		VehicleSelectionSurface = Surface;
		PreviousViewTarget = PC->GetViewTarget();
		constexpr float CameraBlendSeconds = 0.45f;
		PC->SetViewTargetWithBlend(It->PreviewViewTarget, CameraBlendSeconds);
		GetWorldTimerManager().SetTimer(VehicleCameraTimer, this, &ThisClass::PresentVehicleSelection,
			CameraBlendSeconds, false);
		return;
	}
	PresentVehicleSelection();
}

void AMWLobbyHUD::PresentVehicleSelection()
{
	auto* PC = BoundController.Get();
	if (!PC || !VehicleSelectionWidget) return;
	if (auto* Surface = VehicleSelectionSurface.Get())
	{
		Surface->SetOwnerPlayer(PC->GetLocalPlayer());
		Surface->SetWidget(VehicleSelectionWidget);
		Surface->SetVisibility(true);
		Surface->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(Mode);
	}
	else
	{
		VehicleSelectionWidget->SetIsFocusable(true);
		VehicleSelectionWidget->AddToViewport();
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(VehicleSelectionWidget->TakeWidget());
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Mode);
	}
	PC->bShowMouseCursor = true;
}

void AMWLobbyHUD::CloseVehicleSelection()
{
	if (!VehicleSelectionWidget) return;
	GetWorldTimerManager().ClearTimer(VehicleCameraTimer);
	VehicleSelectionWidget->OnSelectionRequested.RemoveDynamic(this, &ThisClass::HandleVehicleSelectionRequested);
	if (auto* Presentation = LobbyPresentation.Get())
	{
		Presentation->SetLobbyWidgets(RoomLobbyWidget, nullptr);
		VehicleSelectionWidget = nullptr;
		return;
	}
	if (auto* Surface = VehicleSelectionSurface.Get())
	{
		Surface->SetWidget(nullptr);
		Surface->SetVisibility(false);
		Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	VehicleSelectionWidget->RemoveFromParent();
	VehicleSelectionWidget = nullptr;
	VehicleSelectionSurface.Reset();
	if (!GetWorld()->bIsTearingDown && RoomLobbyWidget)
	{
		RoomLobbyWidget->SetVisibility(ESlateVisibility::Visible);
		if (auto* PC = BoundController.Get())
		{
			if (auto* ViewTarget = PreviousViewTarget.Get()) PC->SetViewTarget(ViewTarget);
			FInputModeUIOnly Mode;
			Mode.SetWidgetToFocus(RoomLobbyWidget->TakeWidget());
			Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PC->SetInputMode(Mode);
		}
	}
	PreviousViewTarget.Reset();
}

void AMWLobbyHUD::HandleVehicleSelectionRequested(FGameplayTag VehicleAssetID)
{
	if (auto* PC = BoundController.Get()) PC->RequestConfirmVehicle(VehicleAssetID);
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
void AMWLobbyHUD::HandleActionErrorChanged()
{
	if (auto* PC = BoundController.Get(); VehicleSelectionWidget && PC && !PC->GetLobbyActionError().IsEmpty())
		VehicleSelectionWidget->FinishSelectionRequest(FText::FromString(PC->GetLobbyActionError()));
	RefreshLobbyScreen();
}

void AMWLobbyHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(BindingRetryTimer);
	if (auto* Presentation = LobbyPresentation.Get()) Presentation->SetLobbyWidgets(nullptr, nullptr);
	LobbyPresentation.Reset();
	CloseVehicleSelection();
	ClearBindings();
	if (RoomLobbyWidget) RoomLobbyWidget->RemoveFromParent();
	RoomLobbyWidget = nullptr;
	Super::EndPlay(EndPlayReason);
}
