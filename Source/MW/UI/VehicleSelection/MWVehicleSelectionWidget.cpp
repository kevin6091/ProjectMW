#include "UI/VehicleSelection/MWVehicleSelectionWidget.h"

#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Widgets/SWindow.h"
#include "Engine/GameViewportClient.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/UniformGridSlot.h"
#include "GameFramework/PlayerController.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Data/MWVehicleDefinitionData.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "LogChannel.h"
#include "MWGameplayTags.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "System/MWAssetManager.h"
#include "System/GameState/MWLobbyGameState.h"
#include "System/MWPlayerState.h"
#include "UI/VehicleSelection/MWVehicleCardWidget.h"
#include "UI/VehicleSelection/MWVehiclePlayerEntryWidget.h"
#include "UI/VehicleSelection/MWVehiclePlayerMarkerWidget.h"
#include "UI/Cursor/MWPointerCursor.h"
#include "UI/Cursor/MWCursorProjection.h"
#include "UI/Cursor/MWRemoteCursorOverlay.h"

bool UMWVehicleSelectionWidget::SampleSharedCursor(const UWidgetComponent* Surface, FVector2D& OutNormalizedPosition) const
{
	APlayerController* PC = GetOwningPlayer();
	return !bPreviewOnly && PC && CursorLayer
		&& MWCursorProjection::SampleNormalized(*this, *CursorLayer, Surface, *PC, OutNormalizedPosition);
}

bool UMWVehicleSelectionWidget::ProjectSharedCursor(const UWidgetComponent* Surface, FVector2D Position, FVector2D& OutViewportPosition) const
{
	APlayerController* PC = GetOwningPlayer();
	return PC && CursorLayer && MWCursorProjection::ToViewport(*this, *CursorLayer, Surface, Position, *PC, OutViewportPosition);
}

UUserWidget* UMWVehicleSelectionWidget::CreateSharedCursorVisual() const
{
	if (!IsValid(LocalPlayerCursor) || !GetOwningPlayer()) return nullptr;
	// main의 자기 커서와 같은 BP 클래스를 재사용한다. 원형·화살표·상태 문구를 복제 제작하지 않는다.
	UMWVehiclePlayerMarkerWidget* Visual = CreateWidget<UMWVehiclePlayerMarkerWidget>(GetOwningPlayer(), LocalPlayerCursor->GetClass());
	if (Visual) Visual->TakeWidget();
	return Visual;
}

bool UMWVehicleSelectionWidget::ConfigureSharedCursorVisual(UUserWidget& Visual, const AMWPlayerState* Player,
	FMWCursorVisualLayout& OutLayout) const
{
	if (!Player || !IsValid(LocalPlayerCursor) || !Visual.IsA(LocalPlayerCursor->GetClass())) return false;
	UMWVehiclePlayerMarkerWidget* Marker = Cast<UMWVehiclePlayerMarkerWidget>(&Visual);
	const UCanvasPanelSlot* LocalSlot = Cast<UCanvasPanelSlot>(LocalPlayerCursor->Slot);
	if (!Marker || !LocalSlot) return false;
	const int32* SlotIndex = PlayerSlots.Find(TWeakObjectPtr<AMWPlayerState>(const_cast<AMWPlayerState*>(Player)));
	if (!SlotIndex || !ParticipantViews.IsValidIndex(*SlotIndex)) return false;
	FMWVehiclePlayerPresentation View = ParticipantViews[*SlotIndex];
	View.bLocal = false;
	Marker->SetPresentation(View, /*bShowCursorName=*/true);
	// 소프트웨어 커서는 DPI 배율을 곱한 픽셀 크기, 오버레이는 DPI 적용 전 논리 크기를 사용한다.
	OutLayout.DesignSize = FVector2D(MWPointerDesignSize);
	OutLayout.DisplaySize = LocalSlot->GetSize() * FMath::Clamp(RemoteCursorScale, 0.f, 1.f);
	OutLayout.Hotspot = MWGetPointerHotspot(Marker);
	return true;
}

void UMWVehicleSelectionWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ResetPointerHit();
	if (IsValid(LocalPlayerCursor)) LocalPlayerCursor->SetVisibility(ESlateVisibility::Collapsed);
	if (!IsValid(Button_Confirm) || !IsValid(Button_Reset) || !IsValid(Button_Back) ||
		!IsValid(Button_Refresh) || !IsValid(Grid_Vehicles) || !IsValid(Box_Players) ||
		!IsValid(Text_Status) || !IsValid(Text_Error) || !IsValid(Text_RegisteredCount) || !IsValid(PreviewNotice)) return;
	for (UButton* Button : {Button_Confirm.Get(), Button_Reset.Get(), Button_Back.Get(), Button_Refresh.Get(),
		Button_NextPreviewScenario.Get(), Button_NextPreviewLocalPlayer.Get(), Button_SecretVehicle.Get()})
	{
		if (Button) Button->SetClickMethod(EButtonClickMethod::PreciseClick);
	}
	Button_Confirm->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleConfirm);
	if (IsValid(Button_SecretVehicle))
		Button_SecretVehicle->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSecretVehicleChosen);
	Button_Reset->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleReset);
	Button_Back->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBack);
	Button_Refresh->OnClicked.AddUniqueDynamic(this, &ThisClass::RefreshVehicles);

	if (IsValid(Button_NextPreviewScenario)) Button_NextPreviewScenario->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleNextPreviewScenario);
	if (IsValid(Button_NextPreviewLocalPlayer)) Button_NextPreviewLocalPlayer->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleNextPreviewLocalPlayer);
	if (bPreviewOnly && PreviewScenarios.IsValidIndex(DefaultPreviewScenarioIndex))
	{
		ActivePreviewScenarioIndex = DefaultPreviewScenarioIndex;
		ActivePreview = PreviewScenarios[ActivePreviewScenarioIndex];
	}
	RefreshPreviewControls();
	RefreshVehicles();
}

void UMWVehicleSelectionWidget::NativeDestruct()
{
	ReleaseSoftwareCursor();
	PendingRequest = ESelectionRequest::None;
	ResetPointerHit();
	if (TSharedPtr<SWindow> Window = CursorWindow.Pin())
	{
		Window->GetOnWindowDeactivatedEvent().Remove(WindowDeactivatedHandle);
	}
	WindowDeactivatedHandle.Reset();
	CursorWindow.Reset();
	if (IsValid(Button_Confirm)) Button_Confirm->OnClicked.RemoveDynamic(this, &ThisClass::HandleConfirm);
	if (IsValid(Button_SecretVehicle))
		Button_SecretVehicle->OnClicked.RemoveDynamic(this, &ThisClass::HandleSecretVehicleChosen);
	if (IsValid(Button_Reset)) Button_Reset->OnClicked.RemoveDynamic(this, &ThisClass::HandleReset);
	if (IsValid(Button_Back)) Button_Back->OnClicked.RemoveDynamic(this, &ThisClass::HandleBack);
	if (IsValid(Button_Refresh)) Button_Refresh->OnClicked.RemoveDynamic(this, &ThisClass::RefreshVehicles);
	BoundGameState.Reset();
	if (IsValid(Button_NextPreviewScenario)) Button_NextPreviewScenario->OnClicked.RemoveDynamic(this, &ThisClass::HandleNextPreviewScenario);
	if (IsValid(Button_NextPreviewLocalPlayer)) Button_NextPreviewLocalPlayer->OnClicked.RemoveDynamic(this, &ThisClass::HandleNextPreviewLocalPlayer);
	ParticipantViews.Reset();
	PlayerSlots.Reset();
	ActivePreview = FMWVehiclePreviewScenario();
	ActivePreviewScenarioIndex = INDEX_NONE;
	ClearCards();
	PlayerEntries.Reset();
	if (IsValid(Box_Players)) Box_Players->ClearChildren();
	Super::NativeDestruct();
}

FReply UMWVehicleSelectionWidget::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FReply Reply = Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
	if (Reply.IsEventHandled() || InMouseEvent.IsTouchEvent()) return Reply;
	// Check the board hit before a child button consumes the click.
	UpdateLocalCursor(InMouseEvent);
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton && bCursorInUI && CancelLocalSelection())
	{
		return FReply::Handled();
	}
	return Reply;
}

FReply UMWVehicleSelectionWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	UpdateLocalCursor(InMouseEvent);
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void UMWVehicleSelectionWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	UpdateLocalCursor(InMouseEvent);
}

void UMWVehicleSelectionWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	ResetPointerHit();
	Super::NativeOnMouseLeave(InMouseEvent);
}

void UMWVehicleSelectionWidget::BindCursorWindow()
{
	UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
	if (CursorWindow.Pin() == Window) return;
	if (TSharedPtr<SWindow> Previous = CursorWindow.Pin())
	{
		Previous->GetOnWindowDeactivatedEvent().Remove(WindowDeactivatedHandle);
	}
	WindowDeactivatedHandle.Reset();
	CursorWindow = Window;
	if (Window)
	{
		WindowDeactivatedHandle = Window->GetOnWindowDeactivatedEvent().AddUObject(this, &ThisClass::ResetPointerHit);
	}
}

void UMWVehicleSelectionWidget::ResetPointerHit()
{
	bCursorInUI = false;
}

void UMWVehicleSelectionWidget::RefreshLocalCursor()
{
	const FMWVehiclePlayerPresentation* LocalView = ParticipantViews.FindByPredicate(
		[](const FMWVehiclePlayerPresentation& View) { return View.bOccupied && View.bLocal; });
	if (!LocalView)
	{
		ReleaseSoftwareCursor();
		return;
	}

	// The selection screen owns the viewport cursor even outside the board's hit area.
	RegisterSoftwareCursor();
	if (IsValid(SoftwareCursor)) SoftwareCursor->SetPresentation(*LocalView, /*bShowCursorName=*/true);
}

FVector2D UMWVehicleSelectionWidget::GetSoftwareCursorSize() const
{
	const UCanvasPanelSlot* CursorSlot = IsValid(LocalPlayerCursor)
		? Cast<UCanvasPanelSlot>(LocalPlayerCursor->Slot) : nullptr;
	// Reuse the Designer marker dimensions and the project's DPI rule, not pixel offsets.
	return CursorSlot ? CursorSlot->GetSize() * UWidgetLayoutLibrary::GetViewportScale(this) : FVector2D::ZeroVector;
}

void UMWVehicleSelectionWidget::RegisterSoftwareCursor()
{
	if (IsValid(SoftwareCursor) || !IsValid(LocalPlayerCursor)) return;
	APlayerController* Owner = GetOwningPlayer();
	UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (!IsValid(Owner) || !Owner->IsLocalController() || !IsValid(Viewport)) return;
	SoftwareCursor = CreateWidget<UMWVehiclePlayerMarkerWidget>(Owner, LocalPlayerCursor->GetClass());
	if (!IsValid(SoftwareCursor)) return;
	SoftwareCursor->SetVisibility(ESlateVisibility::HitTestInvisible);
	const TWeakObjectPtr<UMWVehicleSelectionWidget> WeakThis(this);
	SoftwareCursorSlate = MWMakePointerCursor(SoftwareCursor, TAttribute<FVector2D>::CreateLambda([WeakThis]()
	{
		return WeakThis.IsValid() ? WeakThis->GetSoftwareCursorSize() : FVector2D::ZeroVector;
	}));
	CursorViewport = Viewport;
	PreviousSoftwareCursor = Viewport->GetSoftwareCursorWidget(EMouseCursor::Default);
	bPreviouslyUsedSoftwareCursors = Viewport->GetIsUsingSoftwareCursorWidgets();
	Viewport->SetSoftwareCursorWidget(EMouseCursor::Default, SoftwareCursorSlate);
	Viewport->SetUseSoftwareCursorWidgets(true);
	// Slate draws the cursor above the world. NativeDestruct restores the previous mapping.
}

void UMWVehicleSelectionWidget::ReleaseSoftwareCursor()
{
	if (UGameViewportClient* Viewport = CursorViewport.Get())
	{
		// A later owner may have replaced the mapping; never overwrite that owner's cursor.
		if (SoftwareCursorSlate.IsValid() && Viewport->GetSoftwareCursorWidget(EMouseCursor::Default) == SoftwareCursorSlate)
		{
			Viewport->SetSoftwareCursorWidget(EMouseCursor::Default, PreviousSoftwareCursor);
			Viewport->SetUseSoftwareCursorWidgets(bPreviouslyUsedSoftwareCursors);
		}
	}
	CursorViewport.Reset();
	SoftwareCursorSlate.Reset();
	PreviousSoftwareCursor.Reset();
	SoftwareCursor = nullptr;
}

void UMWVehicleSelectionWidget::UpdateLocalCursor(const FPointerEvent& MouseEvent)
{
	if (MouseEvent.IsTouchEvent() || !IsValid(CursorLayer) || !IsValid(LocalPlayerCursor)) return;
	BindCursorWindow();
	const TSharedPtr<SWindow> Window = CursorWindow.Pin();
	if (!Window || !Window->IsActive())
	{
		ResetPointerHit();
		return;
	}
	UCanvasPanelSlot* CursorSlot = Cast<UCanvasPanelSlot>(LocalPlayerCursor->Slot);
	if (!CursorSlot) return;
	// WidgetComponent hardware hit testing supplies a virtual pointer in its Slate window.
	// Convert through the actual layer geometry, including Designer scale and transforms.
	const FGeometry& Geometry = CursorLayer->GetCachedGeometry();
	const FVector2D Position = Geometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	const FVector2D Size = Geometry.GetLocalSize();
	if (Size.X <= 0.f || Size.Y <= 0.f || Position.X < 0.f || Position.Y < 0.f ||
		Position.X > Size.X || Position.Y > Size.Y)
	{
		ResetPointerHit();
		return;
	}
	bCursorInUI = true;
}

void UMWVehicleSelectionWidget::ClearCards()
{
	for (UMWVehicleCardWidget* Card : Cards)
	{
		if (!IsValid(Card)) continue;
		Card->OnChosen.RemoveDynamic(this, &ThisClass::HandleCardChosen);
		Card->OnHoverChanged.RemoveDynamic(this, &ThisClass::HandleCardHover);
	}
	SelectedCard.Reset();
	SecretVehicleCard = nullptr;
	Cards.Reset();
	if (IsValid(Grid_Vehicles)) Grid_Vehicles->ClearChildren();
	if (IsValid(Wrap_Vehicles)) Wrap_Vehicles->ClearChildren();
}

void UMWVehicleSelectionWidget::AddCard(UMWVehicleCardWidget* Card)
{
	if (!IsValid(Card) || ColumnCount <= 0) return;
	if (IsValid(Wrap_Vehicles))
	{
		UWrapBoxSlot* CardSlot = Wrap_Vehicles->AddChildToWrapBox(Card);
		CardSlot->SetHorizontalAlignment(CardHorizontalAlignment);
		CardSlot->SetVerticalAlignment(CardVerticalAlignment);
	}
	else
	{
		const int32 Index = Cards.Num();
		UUniformGridSlot* CardSlot = Grid_Vehicles->AddChildToUniformGrid(Card, Index / ColumnCount, Index % ColumnCount);
		CardSlot->SetHorizontalAlignment(CardHorizontalAlignment);
		CardSlot->SetVerticalAlignment(CardVerticalAlignment);
	}
	Card->OnChosen.AddUniqueDynamic(this, &ThisClass::HandleCardChosen);
	Card->OnHoverChanged.AddUniqueDynamic(this, &ThisClass::HandleCardHover);
	if (CardMarkerScale != 1.f) ApplyCardMarkerScale(Card);
	Cards.Add(Card);
}

void UMWVehicleSelectionWidget::SetWorldPreviewMarkerScales(float PinnedScale, float CardScale)
{
	if (IsValid(LocalPlayerCursor))
	{
		const FVector2D Hotspot = MWGetPointerHotspot(LocalPlayerCursor);
		if (UCanvasPanelSlot* CursorSlot = Cast<UCanvasPanelSlot>(LocalPlayerCursor->Slot))
			CursorSlot->SetAlignment(Hotspot);
		LocalPlayerCursor->SetRenderTransformPivot(Hotspot);
		LocalPlayerCursor->SetRenderScale(FVector2D(PinnedScale));
	}
	CardMarkerScale = CardScale;
	for (UMWVehicleCardWidget* Card : Cards)
		ApplyCardMarkerScale(Card);
}

void UMWVehicleSelectionWidget::ApplyCardMarkerScale(UMWVehicleCardWidget* Card) const
{
	if (UWidget* Markers = Card->GetWidgetFromName(TEXT("Grid_PlayerMarkers")))
		Markers->SetRenderScale(FVector2D(CardMarkerScale));
}

void UMWVehicleSelectionWidget::RefreshVehicles()
{
	if (IsRequestPending()) return;
	if (!IsValid(Grid_Vehicles) || !IsValid(Text_Error) || !IsValid(Text_RegisteredCount) || !GetOwningPlayer()) return;
	const FGameplayTag PreviousSelection = GetSelectedVehicleAssetId();
	ClearCards();
	Text_Error->SetText(FText());
	UMWAssetManager& Assets = UMWAssetManager::Get();
	// Keep the authored card order. The existing native tags identify the gameplay assets.
	const TArray<FGameplayTag> AssetIds{
		MWGameplayTags::Asset_Vehicle_SMIET, MWGameplayTags::Asset_Vehicle_MAMMOTH,
		MWGameplayTags::Asset_Vehicle_UFOLOGY, MWGameplayTags::Asset_Vehicle_DOUG,
		MWGameplayTags::Asset_Vehicle_FRED};
	TMap<FGameplayTag, UMWVehicleDefinitionData*> Definitions;
	for (const FGameplayTag Id : AssetIds)
	{
		UMWVehicleDefinitionData* Definition = Assets.LoadAssetSynchronous<UMWVehicleDefinitionData>(Id);
		if (!IsValid(Definition))
		{
			UE_LOG(LogDefault, Warning, TEXT("Vehicle UI: catalog entry %s is not a vehicle definition."), *Id.ToString());
			Text_Error->SetText(InvalidVehicleText);
			continue;
		}
		Definitions.Add(Id, Definition);
	}
	Text_RegisteredCount->SetText(FText::Format(RegisteredCountFormat, FText::AsNumber(Definitions.Num())));
	if (!CardWidgetClass || ColumnCount <= 0)
	{
		Text_Error->SetText(InvalidVehicleText);
		RefreshPlayers();
		RefreshControls();
		return;
	}
	if (bPreviewOnly)
	{
		for (const FMWVehiclePreviewCard& Sample : PreviewCards)
		{
			UMWVehicleCardWidget* Card = CreateWidget<UMWVehicleCardWidget>(GetOwningPlayer(), CardWidgetClass);
			if (!Card) continue;
			Card->SetPreview(Sample.DisplayName, Sample.Thumbnail, !Sample.bUnavailable, Sample.UnavailableReason);
			AddCard(Card);
		}
	}
	else
	{
		for (const FGameplayTag Id : AssetIds)
		{
			UMWVehicleDefinitionData* const* Found = Definitions.Find(Id);
			if (!Found) continue;
			AddCard(CreateVehicleCard(Id, **Found));
		}
		if (Button_SecretVehicle && SecretVehicleAssetId.IsValid())
		{
			if (auto* Definition = Assets.LoadAssetSynchronous<UMWVehicleDefinitionData>(SecretVehicleAssetId))
				SecretVehicleCard = CreateVehicleCard(SecretVehicleAssetId, *Definition);
		}
	}
	if (auto* Card = FindVehicleCard(PreviousSelection); Card && Card->IsAvailable())
	{
		SelectedCard = Card;
		Card->SetSelected(true);
	}
	RefreshPreviewSelection();
	RefreshPlayers();
	RefreshControls();
}

UMWVehicleCardWidget* UMWVehicleSelectionWidget::CreateVehicleCard(FGameplayTag Id, UMWVehicleDefinitionData& Definition)
{
	UClass* PawnClass = Definition.GetPawnClass().LoadSynchronous();
	TArray<FText> Errors;
	const bool bValid = PawnClass && !PawnClass->HasAnyClassFlags(CLASS_Abstract) && Definition.ValidateDefinition(Errors);
	auto* Card = CreateWidget<UMWVehicleCardWidget>(GetOwningPlayer(), CardWidgetClass);
	if (Card) Card->SetVehicle(Id, &Definition, VehicleThumbnails.FindRef(Id), bValid, bValid ? FText() : InvalidVehicleText);
	return Card;
}

UMWVehicleCardWidget* UMWVehicleSelectionWidget::FindVehicleCard(FGameplayTag Id) const
{
	if (!Id.IsValid()) return nullptr;
	if (SecretVehicleCard && SecretVehicleCard->GetVehicleAssetId() == Id) return SecretVehicleCard;
	for (UMWVehicleCardWidget* Card : Cards)
	{
		if (Card->GetVehicleAssetId() == Id) return Card;
	}
	return nullptr;
}

FGameplayTag UMWVehicleSelectionWidget::GetSelectedVehicleAssetId() const
{
	const UMWVehicleCardWidget* Card = SelectedCard.Get();
	return !bPreviewOnly && Card ? Card->GetVehicleAssetId() : FGameplayTag::EmptyTag;
}

FText UMWVehicleSelectionWidget::GetSelectedDisplayName() const
{
	const UMWVehicleCardWidget* Card = SelectedCard.Get();
	return Card ? Card->GetDisplayName() : FText();
}

FMWVehiclePreviewParticipant* UMWVehicleSelectionWidget::FindLocalPreviewParticipant()
{
	return ActivePreview.Participants.FindByPredicate([this](const FMWVehiclePreviewParticipant& Player)
	{
		return Player.SlotIndex == ActivePreview.LocalSlotIndex;
	});
}

const FMWVehiclePreviewParticipant* UMWVehicleSelectionWidget::FindLocalPreviewParticipant() const
{
	return ActivePreview.Participants.FindByPredicate([this](const FMWVehiclePreviewParticipant& Player)
	{
		return Player.SlotIndex == ActivePreview.LocalSlotIndex;
	});
}

bool UMWVehicleSelectionWidget::IsPreviewLocked() const
{
	const FMWVehiclePreviewParticipant* Player = FindLocalPreviewParticipant();
	return bPreviewOnly && Player && Player->bConfirmed;
}

bool UMWVehicleSelectionWidget::AreAllPreviewParticipantsConfirmed() const
{
	if (!bPreviewOnly || ActivePreview.Participants.IsEmpty()) return false;
	for (const FMWVehiclePreviewParticipant& Player : ActivePreview.Participants)
	{
		if (!Player.bConfirmed || !Cards.IsValidIndex(Player.PreviewCardIndex) ||
			!IsValid(Cards[Player.PreviewCardIndex]) || !Cards[Player.PreviewCardIndex]->IsAvailable()) return false;
	}
	return true;
}

FLinearColor UMWVehicleSelectionWidget::GetParticipantTint(int32 SlotIndex) const
{
	return ParticipantTints.IsValidIndex(SlotIndex) ? ParticipantTints[SlotIndex] : FallbackParticipantTint;
}

FLinearColor UMWVehicleSelectionWidget::GetParticipantTintForMember(int32 MemberId) const
{
	return MemberId > 0 && !ParticipantTints.IsEmpty()
		? GetParticipantTint((MemberId - 1) % ParticipantTints.Num())
		: FallbackParticipantTint;
}

FMWVehiclePlayerPresentation UMWVehicleSelectionWidget::MakeWaitingPresentation() const
{
	FMWVehiclePlayerPresentation View;
	View.PlayerName = WaitingPlayerText;
	View.PlayerTint = WaitingSlotTint;
	return View;
}

FMWVehiclePlayerPresentation UMWVehicleSelectionWidget::MakePreviewPresentation(const FMWVehiclePreviewParticipant& Player) const
{
	FMWVehiclePlayerPresentation View;
	View.PlayerName = Player.DisplayName;
	View.bOccupied = true;
	View.bLocal = Player.SlotIndex == ActivePreview.LocalSlotIndex;
	View.bConfirmed = Player.bConfirmed;
	View.PlayerTint = GetParticipantTint(Player.SlotIndex);
	View.MarkerText = FText::Format(ParticipantMarkerFormat, FText::AsNumber(Player.SlotIndex + 1));
	View.StatusText = Player.bConfirmed ? PreviewPlayerLockedText : PreviewPlayerSelectingText;
	View.VehicleName = UnselectedVehicleText;
	if (PreviewCards.IsValidIndex(Player.PreviewCardIndex))
	{
		View.VehicleName = PreviewCards[Player.PreviewCardIndex].DisplayName;
		View.Thumbnail = PreviewCards[Player.PreviewCardIndex].Thumbnail;
	}
	return View;
}

void UMWVehicleSelectionWidget::SelectPreviewScenario(int32 Index)
{
	if (!bPreviewOnly || !PreviewScenarios.IsValidIndex(Index) || IsRequestPending()) return;
	ActivePreviewScenarioIndex = Index;
	ActivePreview = PreviewScenarios[Index];
	if (IsValid(Text_Error)) Text_Error->SetText(FText());
	RefreshPreviewControls();
	RefreshPreviewSelection();
	RefreshPlayers();
	RefreshControls();
}

void UMWVehicleSelectionWidget::SelectPreviewLocalSlot(int32 SlotIndex)
{
	if (!bPreviewOnly || IsRequestPending() || !ActivePreview.Participants.ContainsByPredicate(
		[SlotIndex](const FMWVehiclePreviewParticipant& Player) { return Player.SlotIndex == SlotIndex; })) return;
	ActivePreview.LocalSlotIndex = SlotIndex;
	RefreshPreviewControls();
	RefreshPreviewSelection();
	RefreshPlayers();
	RefreshControls();
}

void UMWVehicleSelectionWidget::RefreshPreviewControls()
{
	if (IsValid(PreviewControls)) PreviewControls->SetVisibility(bPreviewOnly && bShowPreviewControls
		? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (IsValid(Button_NextPreviewScenario)) Button_NextPreviewScenario->SetIsEnabled(bPreviewOnly && !PreviewScenarios.IsEmpty());
	if (IsValid(Button_NextPreviewLocalPlayer)) Button_NextPreviewLocalPlayer->SetIsEnabled(bPreviewOnly && !ActivePreview.Participants.IsEmpty());
	if (IsValid(Text_PreviewScenario)) Text_PreviewScenario->SetText(ActivePreview.Label);
	if (IsValid(Text_PreviewLocalPlayer))
	{
		const FMWVehiclePreviewParticipant* Player = FindLocalPreviewParticipant();
		Text_PreviewLocalPlayer->SetText(Player ? FText::Format(PreviewLocalSlotFormat,
			FText::Format(ParticipantMarkerFormat, FText::AsNumber(Player->SlotIndex + 1)), Player->DisplayName) : FText());
	}
	if (IsValid(Text_PreviewCapacity))
	{
		Text_PreviewCapacity->SetVisibility(bPreviewOnly ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		Text_PreviewCapacity->SetText(FText::Format(PreviewCapacityFormat,
			FText::AsNumber(ActivePreview.Participants.Num()), FText::AsNumber(ActivePreview.Capacity)));
	}
}

void UMWVehicleSelectionWidget::SetPreviewControlsVisible(bool bVisible)
{
	bShowPreviewControls = bVisible;
	RefreshPreviewControls();
}

void UMWVehicleSelectionWidget::HandleNextPreviewScenario()
{
	if (bPreviewOnly && !PreviewScenarios.IsEmpty()) SelectPreviewScenario((ActivePreviewScenarioIndex + 1) % PreviewScenarios.Num());
}

void UMWVehicleSelectionWidget::HandleNextPreviewLocalPlayer()
{
	if (!bPreviewOnly || ActivePreview.Participants.IsEmpty()) return;
	const int32 Current = ActivePreview.Participants.IndexOfByPredicate([this](const FMWVehiclePreviewParticipant& Player)
	{
		return Player.SlotIndex == ActivePreview.LocalSlotIndex;
	});
	SelectPreviewLocalSlot(ActivePreview.Participants[(Current + 1) % ActivePreview.Participants.Num()].SlotIndex);
}

void UMWVehicleSelectionWidget::RefreshPreviewSelection()
{
	if (!bPreviewOnly) return;
	SelectedCard.Reset();
	for (UMWVehicleCardWidget* Card : Cards) if (IsValid(Card)) Card->SetSelected(false);
	const FMWVehiclePreviewParticipant* Player = FindLocalPreviewParticipant();
	if (Player && Cards.IsValidIndex(Player->PreviewCardIndex) && Cards[Player->PreviewCardIndex]->IsAvailable())
	{
		SelectedCard = Cards[Player->PreviewCardIndex];
		SelectedCard->SetSelected(true);
	}
}

void UMWVehicleSelectionWidget::RefreshPlayers()
{
	if (!IsValid(Box_Players) || !PlayerEntryWidgetClass || !GetOwningPlayer()) return;
	AMWLobbyGameState* State = !bPreviewOnly && GetWorld() ? GetWorld()->GetGameState<AMWLobbyGameState>() : nullptr;
	if (BoundGameState.Get() != State)
	{
		PlayerSlots.Reset();
		BoundGameState = State;
	}
	ParticipantViews.Reset();
	if (bPreviewOnly) BuildPreviewParticipants();
	else BuildLobbyParticipants(State);
	if (!bPreviewOnly)
	{
		if (const auto* OwnState = GetOwningPlayer()->GetPlayerState<AMWPlayerState>())
		{
			const bool bConfirmed = OwnState->GetVehicleAssetID().IsValid();
			if (bConfirmed && PendingRequest == ESelectionRequest::Confirm)
			{
				PendingRequest = ESelectionRequest::None;
			}
			else if (!bConfirmed && PendingRequest == ESelectionRequest::Cancel)
			{
				PendingRequest = ESelectionRequest::None;
				ClearLocalSelectionDisplay();
			}
		}
	}
	RenderParticipants();
	if (!bPreviewOnly) RefreshControls();
}

void UMWVehicleSelectionWidget::BuildPreviewParticipants()
{
	if (ActivePreview.Capacity > 0)
	{
		ParticipantViews.Init(MakeWaitingPresentation(), ActivePreview.Capacity);
		TSet<int32> OccupiedSlots;
		for (const FMWVehiclePreviewParticipant& Player : ActivePreview.Participants)
		{
			if (!ParticipantViews.IsValidIndex(Player.SlotIndex) || OccupiedSlots.Contains(Player.SlotIndex))
			{
				if (IsValid(Text_Error)) Text_Error->SetText(InvalidVehicleText);
				continue;
			}
			OccupiedSlots.Add(Player.SlotIndex);
			ParticipantViews[Player.SlotIndex] = MakePreviewPresentation(Player);
		}
	}
}

void UMWVehicleSelectionWidget::BuildLobbyParticipants(AMWLobbyGameState* State)
{
	if (!State || State->GetRoomSettings().MaxPlayers <= 0) return;
	ParticipantViews.Init(MakeWaitingPresentation(), State->GetRoomSettings().MaxPlayers);
	TArray<AMWPlayerState*> Players;
	for (APlayerState* Player : State->PlayerArray)
		if (AMWPlayerState* MWPlayer = Cast<AMWPlayerState>(Player)) Players.Add(MWPlayer);
	Players.Sort([](const AMWPlayerState& A, const AMWPlayerState& B) { return A.GetLobbyMemberId() < B.GetLobbyMemberId(); });
	for (auto It = PlayerSlots.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || !Players.Contains(It.Key().Get()) || !ParticipantViews.IsValidIndex(It.Value())) It.RemoveCurrent();
	}
	TSet<int32> OccupiedSlots;
	for (const auto& Pair : PlayerSlots) OccupiedSlots.Add(Pair.Value);
	for (AMWPlayerState* Player : Players)
	{
		const TWeakObjectPtr<AMWPlayerState> Key(Player);
		if (!PlayerSlots.Contains(Key))
		{
			for (int32 Index = 0; Index < ParticipantViews.Num(); ++Index)
			{
				if (OccupiedSlots.Contains(Index)) continue;
				PlayerSlots.Add(Key, Index);
				OccupiedSlots.Add(Index);
				break;
			}
		}
		const int32* Seat = PlayerSlots.Find(Key);

		if (!Seat) continue;

		FMWVehiclePlayerPresentation& View = ParticipantViews[*Seat];

		View.PlayerName = FText::FromString(Player->GetPlayerName());

		const int32 MemberId = Player->GetLobbyMemberId();

		View.PlayerTint = GetParticipantTintForMember(MemberId);

		View.bOccupied = true;

		View.bLocal = GetOwningPlayer()->PlayerState == Player;

		View.MarkerText = FText::Format(ParticipantMarkerFormat, FText::AsNumber(*Seat + 1));

		const FGameplayTag ConfirmedId = Player->GetVehicleAssetID();

		View.bConfirmed = ConfirmedId.IsValid();

		View.StatusText = View.bConfirmed ? SelectionCompleteText : UnselectedVehicleText;

		if (View.bConfirmed)
		{
			if (const auto* Card = FindVehicleCard(ConfirmedId))
			{
				View.VehicleName = Card->GetDisplayName();
				View.Thumbnail = Card->GetThumbnail();
			}
		}
	}
}

void UMWVehicleSelectionWidget::RenderParticipants()
{
	if (PlayerEntries.Num() != ParticipantViews.Num())
	{
		Box_Players->ClearChildren();
		PlayerEntries.Reset();
		for (int32 Index = 0; Index < ParticipantViews.Num(); ++Index)
		{
			UMWVehiclePlayerEntryWidget* Entry = CreateWidget<UMWVehiclePlayerEntryWidget>(GetOwningPlayer(), PlayerEntryWidgetClass);
			if (!Entry) break;
			Box_Players->AddChildToHorizontalBox(Entry)->SetPadding(PlayerEntryPadding);
			PlayerEntries.Add(Entry);
		}
	}
	for (int32 Index = 0; Index < PlayerEntries.Num(); ++Index) PlayerEntries[Index]->SetPresentation(ParticipantViews[Index]);
	RefreshLocalCursor();
}

bool UMWVehicleSelectionWidget::IsSelectionLocked() const
{
	if (bPreviewOnly) return IsPreviewLocked();
	const auto* PC = GetOwningPlayer();
	const auto* PS = PC ? PC->GetPlayerState<AMWPlayerState>() : nullptr;
	const auto* State = BoundGameState.Get();
	return !PS || PS->GetVehicleAssetID().IsValid() || !State ||
		State->GetRoomPhase() != EMWRoomPhase::VehicleSelection;
}

void UMWVehicleSelectionWidget::RefreshControls()
{
	if (!IsValid(Button_Confirm) || !IsValid(Button_Reset) || !IsValid(Button_Back) ||
		!IsValid(Button_Refresh) || !IsValid(Text_Status) || !IsValid(PreviewNotice)) return;
	const bool bLocked = IsSelectionLocked() || IsRequestPending();
	const bool bLocalPreview = !bPreviewOnly || FindLocalPreviewParticipant() != nullptr;
	const bool bSelected = SelectedCard.IsValid() && SelectedCard->IsAvailable();
	if (IsValid(Button_SecretVehicle))
	{
		const bool bCanChooseSecret = !bPreviewOnly && !bLocked && SecretVehicleCard && SecretVehicleCard->IsAvailable();
		Button_SecretVehicle->SetVisibility(bCanChooseSecret ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	Button_Confirm->SetVisibility(ESlateVisibility::Collapsed);
	Button_Confirm->SetIsEnabled(bSelected && !bLocked && bLocalPreview && (bPreviewOnly || OnSelectionRequested.IsBound()));
	Button_Reset->SetVisibility(ESlateVisibility::Collapsed);
	Button_Reset->SetIsEnabled(false);
	Button_Back->SetIsEnabled(bPreviewOnly && !IsRequestPending());
	Button_Back->SetVisibility(bPreviewOnly ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	Button_Refresh->SetIsEnabled(!IsRequestPending());
	PreviewNotice->SetVisibility(ESlateVisibility::Collapsed);
	for (UMWVehicleCardWidget* Card : Cards) if (IsValid(Card)) Card->SetInteractionLocked(bLocked || !bLocalPreview);
	const bool bAllPreviewComplete = AreAllPreviewParticipantsConfirmed();
	// Preview completion is presentation only. No server approval is inferred here.
	Text_Status->SetText(IsRequestPending() ? RequestPendingText : bAllPreviewComplete ? PreviewLockedText :
		IsSelectionLocked() ? SelectionCompleteText : Cards.IsEmpty() ? EmptyCatalogText :
		bSelected && !bPreviewOnly ? ConfirmVehicleText : ChooseVehicleText);
	if (IsValid(Text_SelectionDetail))
	{
		Text_SelectionDetail->SetText(IsSelectionLocked() && !bAllPreviewComplete ? WaitingForPlayersText : FText());
		Text_SelectionDetail->SetVisibility(Text_SelectionDetail->GetText().IsEmpty()
			? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (IsValid(Legend_Cancel))
	{
		const bool bCanCancel = bPreviewOnly ? bSelected && !bAllPreviewComplete : GetCancelableVehicleId().IsValid();
		Legend_Cancel->SetVisibility((bPreviewOnly ? !bAllPreviewComplete : bCanCancel)
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		Legend_Cancel->SetIsEnabled(bCanCancel && !IsRequestPending());
	}
	if (IsValid(Text_Error)) Text_Error->SetVisibility(Text_Error->GetText().IsEmpty()
		? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UMWVehicleSelectionWidget::HandleCardChosen(UMWVehicleCardWidget* Card)
{
	if (!IsValid(Card) || (Card != SecretVehicleCard && !Cards.Contains(Card)) ||
		!Card->IsAvailable() || IsSelectionLocked() || IsRequestPending()) return;
	if (bPreviewOnly)
	{
		FMWVehiclePreviewParticipant* Player = FindLocalPreviewParticipant();
		if (!Player) return;
		Player->PreviewCardIndex = Cards.IndexOfByKey(Card);
		// Development participants only: no server request or VehicleAssetID is created.
		Player->bConfirmed = true;
	}
	if (UMWVehicleCardWidget* Previous = SelectedCard.Get()) Previous->SetSelected(false);
	SelectedCard = Card;
	Card->SetSelected(true);
	if (bPreviewOnly)
	{
		RefreshPlayers();
		RefreshControls();
	}
	else
	{
		HandleConfirm();
	}
}

void UMWVehicleSelectionWidget::HandleSecretVehicleChosen()
{
	HandleCardChosen(SecretVehicleCard);
}

void UMWVehicleSelectionWidget::HandleCardHover(UMWVehicleCardWidget* Card, bool bHovered)
{
	if (!bPreviewOnly || IsPreviewLocked() || IsRequestPending() || !IsValid(Card)) return;
	const FMWVehiclePreviewParticipant* Player = FindLocalPreviewParticipant();
	if (!Player || Player->bConfirmed || !PlayerEntries.IsValidIndex(Player->SlotIndex)) return;
	if (bHovered && Card->IsAvailable())
	{
		FMWVehiclePlayerPresentation View = MakePreviewPresentation(*Player);
		View.VehicleName = Card->GetDisplayName();
		View.Thumbnail = Card->GetThumbnail();
		View.bHoverPreview = true;
		PlayerEntries[Player->SlotIndex]->SetPresentation(View);
	}
	else if (ParticipantViews.IsValidIndex(Player->SlotIndex)) PlayerEntries[Player->SlotIndex]->SetPresentation(ParticipantViews[Player->SlotIndex]);
}

void UMWVehicleSelectionWidget::HandleConfirm()
{
	UMWVehicleCardWidget* Card = SelectedCard.Get();
	if (!Card || !Card->IsAvailable() || IsSelectionLocked() || IsRequestPending()) return;
	if (bPreviewOnly)
	{
		FMWVehiclePreviewParticipant* Player = FindLocalPreviewParticipant();
		if (!Player) return;
		Player->bConfirmed = true;
		RefreshPlayers();
		RefreshControls();
	}
	else if (OnSelectionRequested.IsBound() && Card->GetVehicleAssetId().IsValid())
	{
		PendingRequest = ESelectionRequest::Confirm;
		if (IsValid(Text_Error)) Text_Error->SetText(FText());
		RefreshControls();
		OnSelectionRequested.Broadcast(Card->GetVehicleAssetId());
	}
}

void UMWVehicleSelectionWidget::FinishSelectionRequest(const FText& Message)
{
	const bool bWasCancel = PendingRequest == ESelectionRequest::Cancel;
	PendingRequest = ESelectionRequest::None;
	if (!Message.IsEmpty() && !bPreviewOnly && !bWasCancel)
	{
		ClearLocalSelectionDisplay();
	}
	if (IsValid(Text_Error)) Text_Error->SetText(Message);
	RefreshPlayers();
	if (bPreviewOnly) RefreshControls();
}

FGameplayTag UMWVehicleSelectionWidget::GetCancelableVehicleId() const
{
	const auto* State = BoundGameState.Get();
	const auto* PC = GetOwningPlayer();
	const auto* PS = PC ? PC->GetPlayerState<AMWPlayerState>() : nullptr;
	return State && State->GetRoomPhase() == EMWRoomPhase::VehicleSelection && PS && OnSelectionCancelRequested.IsBound()
		? PS->GetVehicleAssetID() : FGameplayTag::EmptyTag;
}

void UMWVehicleSelectionWidget::ClearLocalSelectionDisplay()
{
	if (UMWVehicleCardWidget* Card = SelectedCard.Get()) Card->SetSelected(false);
	SelectedCard.Reset();
}

bool UMWVehicleSelectionWidget::CancelLocalSelection()
{
	if (IsRequestPending()) return false;
	if (!bPreviewOnly)
	{
		const FGameplayTag ExpectedVehicleID = GetCancelableVehicleId();
		if (!ExpectedVehicleID.IsValid()) return false;

		PendingRequest = ESelectionRequest::Cancel;
		if (IsValid(Text_Error)) Text_Error->SetText(FText());
		RefreshControls();
		OnSelectionCancelRequested.Broadcast(ExpectedVehicleID);
		return true;
	}

	if (AreAllPreviewParticipantsConfirmed()) return false;
	FMWVehiclePreviewParticipant* Player = FindLocalPreviewParticipant();
	if (!Player) return false;
	Player->bConfirmed = false;
	Player->PreviewCardIndex = INDEX_NONE;
	ClearLocalSelectionDisplay();
	if (IsValid(Text_Error)) Text_Error->SetText(FText());
	RefreshPlayers();
	RefreshControls();
	return true;
}

void UMWVehicleSelectionWidget::HandleReset()
{
	if (bPreviewOnly) CancelLocalSelection();
}

void UMWVehicleSelectionWidget::HandleBack()
{
	if (!IsRequestPending()) OnBackRequested.Broadcast();
}
