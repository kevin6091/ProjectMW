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
	Marker->SetPresentation(View, /*닉네임 표시=*/true);
	// 소프트웨어 커서는 DPI 배율을 곱한 픽셀 크기, 오버레이는 DPI 적용 전 논리 크기를 사용한다.
	OutLayout.DesignSize = FVector2D(MWPointerDesignSize);
	OutLayout.DisplaySize = LocalSlot->GetSize() * FMath::Clamp(RemoteCursorScale, 0.f, 1.f);
	OutLayout.Hotspot = MWGetPointerHotspot(Marker);
	return true;
}

// 기존 카드·참가자·커서 이벤트를 연결하고 실제 방과 외형 Preview의 시작 상태를 구분한다.
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

    if (IsValid(Button_NextPreviewScenario))
        Button_NextPreviewScenario->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleNextPreviewScenario);
    if (IsValid(Button_NextPreviewLocalPlayer))
        Button_NextPreviewLocalPlayer->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleNextPreviewLocalPlayer);
    if (bPreviewOnly && PreviewScenarios.IsValidIndex(DefaultPreviewScenarioIndex))
    {
        ActivePreviewScenarioIndex = DefaultPreviewScenarioIndex;
        ActivePreview = PreviewScenarios[ActivePreviewScenarioIndex];
    }
    RefreshPreviewControls();
    RefreshVehicles();
}

// 소프트웨어 커서·버튼·참가자 상태 연결을 해제해 다음 화면의 입력을 덮어쓰지 않는다.
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
    if (IsValid(Button_Confirm))
        Button_Confirm->OnClicked.RemoveDynamic(this, &ThisClass::HandleConfirm);
    if (IsValid(Button_SecretVehicle))
        Button_SecretVehicle->OnClicked.RemoveDynamic(this, &ThisClass::HandleSecretVehicleChosen);
    if (IsValid(Button_Reset))
        Button_Reset->OnClicked.RemoveDynamic(this, &ThisClass::HandleReset);
    if (IsValid(Button_Back))
        Button_Back->OnClicked.RemoveDynamic(this, &ThisClass::HandleBack);
    if (IsValid(Button_Refresh))
        Button_Refresh->OnClicked.RemoveDynamic(this, &ThisClass::RefreshVehicles);
    BoundGameState.Reset();
    if (IsValid(Button_NextPreviewScenario))
        Button_NextPreviewScenario->OnClicked.RemoveDynamic(this, &ThisClass::HandleNextPreviewScenario);
    if (IsValid(Button_NextPreviewLocalPlayer))
        Button_NextPreviewLocalPlayer->OnClicked.RemoveDynamic(this, &ThisClass::HandleNextPreviewLocalPlayer);
    ParticipantViews.Reset();
    PlayerSlots.Reset();
    ActivePreview = FMWVehiclePreviewScenario();
    ActivePreviewScenarioIndex = INDEX_NONE;
    ClearCards();
    PlayerEntries.Reset();
    if (IsValid(Box_Players))
        Box_Players->ClearChildren();
    Super::NativeDestruct();
}

// 자식 버튼보다 먼저 게시판 입력을 확인해 실제 우클릭 취소 경로를 놓치지 않는다.
FReply UMWVehicleSelectionWidget::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry,
                                                                 const FPointerEvent& InMouseEvent)
{
    const FReply Reply = Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
    if (Reply.IsEventHandled() || InMouseEvent.IsTouchEvent())
        return Reply;
    // 자식 버튼이 클릭을 소비하기 전에 게시판 위 입력인지 확인한다.
    UpdateLocalCursor(InMouseEvent);
    if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton && bCursorInUI && CancelLocalSelection())
    {
        return FReply::Handled();
    }
    return Reply;
}

// 현재 실제 마우스 이벤트의 위치를 기존 커서 좌표 갱신에 전달한다.
FReply UMWVehicleSelectionWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    UpdateLocalCursor(InMouseEvent);
    return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

// 선택 화면에 들어온 포인터의 유효 상태를 반영하고 커서 표시를 갱신한다.
void UMWVehicleSelectionWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
    UpdateLocalCursor(InMouseEvent);
}

// 게시판 히트 상태를 비우되 Viewport 커서 수명은 이 선택 화면 정책을 따른다.
void UMWVehicleSelectionWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
    ResetPointerHit();
    Super::NativeOnMouseLeave(InMouseEvent);
}

// 실제 Slate 창의 커서 처리를 연결해 월드 위젯에서도 클릭 위치를 맞춘다.
void UMWVehicleSelectionWidget::BindCursorWindow()
{
    UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
    const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
    if (CursorWindow.Pin() == Window)
        return;
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

// 이전 게시판 히트 좌표를 무효화해 새 화면에서 오래된 클릭 지점을 쓰지 않게 한다.
void UMWVehicleSelectionWidget::ResetPointerHit()
{
    bCursorInUI = false;
}

// 현재 참가자 표시와 포인터 유효성으로 로컬 커서 외형을 갱신한다.
void UMWVehicleSelectionWidget::RefreshLocalCursor()
{
    const FMWVehiclePlayerPresentation* LocalView = ParticipantViews.FindByPredicate(
        [](const FMWVehiclePlayerPresentation& View)
        {
            return View.bOccupied && View.bLocal;
        });
    if (!LocalView)
    {
        ReleaseSoftwareCursor();
        return;
    }

    // 게시판 히트 영역 밖에서도 선택 화면이 Viewport 커서의 표시 수명을 관리한다.
    RegisterSoftwareCursor();
    if (IsValid(SoftwareCursor))
        SoftwareCursor->SetPresentation(*LocalView, /*닉네임 표시=*/true);
}

// 실제 Designer 마커 크기와 프로젝트 DPI를 사용해 화면 커서 크기를 얻는다.
FVector2D UMWVehicleSelectionWidget::GetSoftwareCursorSize() const
{
    const UCanvasPanelSlot* CursorSlot =
        IsValid(LocalPlayerCursor) ? Cast<UCanvasPanelSlot>(LocalPlayerCursor->Slot) : nullptr;
    // 고정 픽셀 오프셋 대신 Designer의 마커 크기와 프로젝트 DPI 규칙을 재사용한다.
    return CursorSlot ? CursorSlot->GetSize() * UWidgetLayoutLibrary::GetViewportScale(this) : FVector2D::ZeroVector;
}

// 기존 커서 설정을 보관하고 선택 화면 수명 동안 같은 외형과 클릭 끝점을 사용한다.
void UMWVehicleSelectionWidget::RegisterSoftwareCursor()
{
    if (IsValid(SoftwareCursor) || !IsValid(LocalPlayerCursor))
        return;
    APlayerController* Owner = GetOwningPlayer();
    UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
    if (!IsValid(Owner) || !Owner->IsLocalController() || !IsValid(Viewport))
        return;
    SoftwareCursor = CreateWidget<UMWVehiclePlayerMarkerWidget>(Owner, LocalPlayerCursor->GetClass());
    if (!IsValid(SoftwareCursor))
        return;
    SoftwareCursor->SetVisibility(ESlateVisibility::HitTestInvisible);
    const TWeakObjectPtr<UMWVehicleSelectionWidget> WeakThis(this);
    SoftwareCursorSlate = MWMakePointerCursor(SoftwareCursor, TAttribute<FVector2D>::CreateLambda(
                                                                  [WeakThis]()
                                                                  {
                                                                      return WeakThis.IsValid()
                                                                                 ? WeakThis->GetSoftwareCursorSize()
                                                                                 : FVector2D::ZeroVector;
                                                                  }));
    CursorViewport = Viewport;
    PreviousSoftwareCursor = Viewport->GetSoftwareCursorWidget(EMouseCursor::Default);
    bPreviouslyUsedSoftwareCursors = Viewport->GetIsUsingSoftwareCursorWidgets();
    Viewport->SetSoftwareCursorWidget(EMouseCursor::Default, SoftwareCursorSlate);
    Viewport->SetUseSoftwareCursorWidgets(true);
    // 커서는 Slate에서 월드 위에 그리며 위젯 종료 때 이전 커서 연결을 복구한다.
}

// 아직 이 화면이 설치한 커서일 때만 이전 설정을 복원해 다음 화면을 덮어쓰지 않는다.
void UMWVehicleSelectionWidget::ReleaseSoftwareCursor()
{
    if (UGameViewportClient* Viewport = CursorViewport.Get())
    {
        // 다른 화면이 이미 커서를 교체했다면 그 화면의 설정을 덮어쓰지 않는다.
        if (SoftwareCursorSlate.IsValid() &&
            Viewport->GetSoftwareCursorWidget(EMouseCursor::Default) == SoftwareCursorSlate)
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

// 월드 위젯의 실제 Geometry로 입력 좌표를 바꿔 커서 그림과 클릭 위치를 맞춘다.
void UMWVehicleSelectionWidget::UpdateLocalCursor(const FPointerEvent& MouseEvent)
{
    if (MouseEvent.IsTouchEvent() || !IsValid(CursorLayer) || !IsValid(LocalPlayerCursor))
        return;
    BindCursorWindow();
    const TSharedPtr<SWindow> Window = CursorWindow.Pin();
    if (!Window || !Window->IsActive())
    {
        ResetPointerHit();
        return;
    }
    UCanvasPanelSlot* CursorSlot = Cast<UCanvasPanelSlot>(LocalPlayerCursor->Slot);
    if (!CursorSlot)
        return;
    // WidgetComponent의 하드웨어 히트 테스트는 해당 Slate 창의 가상 포인터 좌표를 제공한다.
    // Designer의 크기·변환을 포함한 실제 레이어 Geometry로 좌표를 변환한다.
    const FGeometry& Geometry = CursorLayer->GetCachedGeometry();
    const FVector2D Position = Geometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
    const FVector2D Size = Geometry.GetLocalSize();
    if (Size.X <= 0.f || Size.Y <= 0.f || Position.X < 0.f || Position.Y < 0.f || Position.X > Size.X ||
        Position.Y > Size.Y)
    {
        ResetPointerHit();
        return;
    }
    bCursorInUI = true;
}

// 기존 카드 이벤트와 참조를 해제한 뒤 목록을 다시 만들 수 있게 비운다.
void UMWVehicleSelectionWidget::ClearCards()
{
    for (UMWVehicleCardWidget* Card : Cards)
    {
        if (!IsValid(Card))
            continue;
        Card->OnChosen.RemoveDynamic(this, &ThisClass::HandleCardChosen);
        Card->OnHoverChanged.RemoveDynamic(this, &ThisClass::HandleCardHover);
    }
    SelectedCard.Reset();
    SecretVehicleCard = nullptr;
    Cards.Reset();
    if (IsValid(Grid_Vehicles))
        Grid_Vehicles->ClearChildren();
    if (IsValid(Wrap_Vehicles))
        Wrap_Vehicles->ClearChildren();
}

// 작성된 행·그리드 배치에 카드를 넣고 기존 클릭·호버 알림을 연결한다.
void UMWVehicleSelectionWidget::AddCard(UMWVehicleCardWidget* Card)
{
    if (!IsValid(Card) || ColumnCount <= 0)
        return;
    if (IsValid(Wrap_Vehicles))
    {
        UWrapBoxSlot* CardSlot = Wrap_Vehicles->AddChildToWrapBox(Card);
        CardSlot->SetHorizontalAlignment(CardHorizontalAlignment);
        CardSlot->SetVerticalAlignment(CardVerticalAlignment);
    }
    else
    {
        const int32 Index = Cards.Num();
        UUniformGridSlot* CardSlot =
            Grid_Vehicles->AddChildToUniformGrid(Card, Index / ColumnCount, Index % ColumnCount);
        CardSlot->SetHorizontalAlignment(CardHorizontalAlignment);
        CardSlot->SetVerticalAlignment(CardVerticalAlignment);
    }
    Card->OnChosen.AddUniqueDynamic(this, &ThisClass::HandleCardChosen);
    Card->OnHoverChanged.AddUniqueDynamic(this, &ThisClass::HandleCardHover);
    if (CardMarkerScale != 1.f)
        ApplyCardMarkerScale(Card);
    Cards.Add(Card);
}

// 기존 외형 Preview 표시 배율만 조정하고 실제 서버 선택 상태는 변경하지 않는다.
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

// 현재 카드 안의 표시 요소에 설정된 Preview 배율을 적용한다.
void UMWVehicleSelectionWidget::ApplyCardMarkerScale(UMWVehicleCardWidget* Card) const
{
    if (UWidget* Markers = Card->GetWidgetFromName(TEXT("Grid_PlayerMarkers")))
        Markers->SetRenderScale(FVector2D(CardMarkerScale));
}

// 카탈로그의 실제 ID로 기존 카드 순서를 유지하고 비밀 차량은 별도 표시 모델로 보관한다.
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

// 카탈로그 ID와 실제 차량 정의를 기존 카드 클래스에 연결한다.
UMWVehicleCardWidget* UMWVehicleSelectionWidget::CreateVehicleCard(FGameplayTag Id,
                                                                   UMWVehicleDefinitionData& Definition)
{
    UClass* PawnClass = Definition.GetPawnClass().LoadSynchronous();
    TArray<FText> Errors;
    const bool bValid =
        PawnClass && !PawnClass->HasAnyClassFlags(CLASS_Abstract) && Definition.ValidateDefinition(Errors);
    auto* Card = CreateWidget<UMWVehicleCardWidget>(GetOwningPlayer(), CardWidgetClass);
    if (Card)
        Card->SetVehicle(Id, &Definition, VehicleThumbnails.FindRef(Id), bValid, bValid ? FText() : InvalidVehicleText);
    return Card;
}

// 비밀 차량 모델도 포함해 실제 ID에 대응하는 기존 카드만 찾는다.
UMWVehicleCardWidget* UMWVehicleSelectionWidget::FindVehicleCard(FGameplayTag Id) const
{
    if (!Id.IsValid())
        return nullptr;
    if (SecretVehicleCard && SecretVehicleCard->GetVehicleAssetId() == Id)
        return SecretVehicleCard;
    for (UMWVehicleCardWidget* Card : Cards)
    {
        if (Card->GetVehicleAssetId() == Id)
            return Card;
    }
    return nullptr;
}

// 화면에서 고른 모델의 실제 차량 ID를 조회하며 서버 확정을 대신하지 않는다.
FGameplayTag UMWVehicleSelectionWidget::GetSelectedVehicleAssetId() const
{
    const UMWVehicleCardWidget* Card = SelectedCard.Get();
    return !bPreviewOnly && Card ? Card->GetVehicleAssetId() : FGameplayTag::EmptyTag;
}

// 현재 선택 카드의 사용자용 이름만 조회한다.
FText UMWVehicleSelectionWidget::GetSelectedDisplayName() const
{
    const UMWVehicleCardWidget* Card = SelectedCard.Get();
    return Card ? Card->GetDisplayName() : FText();
}

// 개발용 참가자 목록에서 지정된 로컬 슬롯을 찾는다. 실제 서버 참가자를 만들지 않는다.
FMWVehiclePreviewParticipant* UMWVehicleSelectionWidget::FindLocalPreviewParticipant()
{
    return ActivePreview.Participants.FindByPredicate(
        [this](const FMWVehiclePreviewParticipant& Player)
        {
            return Player.SlotIndex == ActivePreview.LocalSlotIndex;
        });
}

// 개발용 참가자 목록에서 지정된 로컬 슬롯을 찾는다. 실제 서버 참가자를 만들지 않는다.
const FMWVehiclePreviewParticipant* UMWVehicleSelectionWidget::FindLocalPreviewParticipant() const
{
    return ActivePreview.Participants.FindByPredicate(
        [this](const FMWVehiclePreviewParticipant& Player)
        {
            return Player.SlotIndex == ActivePreview.LocalSlotIndex;
        });
}

// 외형 Preview의 완료 조건을 확인한다. 서버의 실제 단계 판정과 구분한다.
bool UMWVehicleSelectionWidget::IsPreviewLocked() const
{
    const FMWVehiclePreviewParticipant* Player = FindLocalPreviewParticipant();
    return bPreviewOnly && Player && Player->bConfirmed;
}

// 개발용 참가자의 표시 완료 여부만 계산한다.
bool UMWVehicleSelectionWidget::AreAllPreviewParticipantsConfirmed() const
{
    if (!bPreviewOnly || ActivePreview.Participants.IsEmpty())
        return false;
    for (const FMWVehiclePreviewParticipant& Player : ActivePreview.Participants)
    {
        if (!Player.bConfirmed || !Cards.IsValidIndex(Player.PreviewCardIndex) ||
            !IsValid(Cards[Player.PreviewCardIndex]) || !Cards[Player.PreviewCardIndex]->IsAvailable())
            return false;
    }
    return true;
}

// 기존 팔레트 범위 안에서 표시 색을 조회하고 없는 경우 대체 색을 사용한다.
FLinearColor UMWVehicleSelectionWidget::GetParticipantTint(int32 SlotIndex) const
{
    return ParticipantTints.IsValidIndex(SlotIndex) ? ParticipantTints[SlotIndex] : FallbackParticipantTint;
}

// 화면별 자리 배정 순서 대신 공유 MemberId로 같은 플레이어 색을 고른다.
FLinearColor UMWVehicleSelectionWidget::GetParticipantTintForMember(int32 MemberId) const
{
    return MemberId > 0 && !ParticipantTints.IsEmpty() ? GetParticipantTint((MemberId - 1) % ParticipantTints.Num())
                                                       : FallbackParticipantTint;
}

// 빈 하단 슬롯에 사용할 대기 문구와 표시 색을 구성한다.
FMWVehiclePlayerPresentation UMWVehicleSelectionWidget::MakeWaitingPresentation() const
{
    FMWVehiclePlayerPresentation View;
    View.PlayerName = WaitingPlayerText;
    View.PlayerTint = WaitingSlotTint;
    return View;
}

// 개발용 참가자의 외형·선택 상태만 표시 값으로 변환한다.
FMWVehiclePlayerPresentation UMWVehicleSelectionWidget::MakePreviewPresentation(
    const FMWVehiclePreviewParticipant& Player) const
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

// 명시한 개발 시나리오로만 바꾸고 실제 방·차량 ID에는 요청하지 않는다.
void UMWVehicleSelectionWidget::SelectPreviewScenario(int32 Index)
{
    if (!bPreviewOnly || !PreviewScenarios.IsValidIndex(Index) || IsRequestPending())
        return;
    ActivePreviewScenarioIndex = Index;
    ActivePreview = PreviewScenarios[Index];
    if (IsValid(Text_Error))
        Text_Error->SetText(FText());
    RefreshPreviewControls();
    RefreshPreviewSelection();
    RefreshPlayers();
    RefreshControls();
}

// 외형 Preview에서 로컬 참가자로 표시할 슬롯을 고른다.
void UMWVehicleSelectionWidget::SelectPreviewLocalSlot(int32 SlotIndex)
{
    if (!bPreviewOnly || IsRequestPending() ||
        !ActivePreview.Participants.ContainsByPredicate(
            [SlotIndex](const FMWVehiclePreviewParticipant& Player)
            {
                return Player.SlotIndex == SlotIndex;
            }))
        return;
    ActivePreview.LocalSlotIndex = SlotIndex;
    RefreshPreviewControls();
    RefreshPreviewSelection();
    RefreshPlayers();
    RefreshControls();
}

// 개발 표시 조작부의 현재 시나리오·슬롯 상태를 갱신한다.
void UMWVehicleSelectionWidget::RefreshPreviewControls()
{
    if (IsValid(PreviewControls))
        PreviewControls->SetVisibility(bPreviewOnly && bShowPreviewControls ? ESlateVisibility::SelfHitTestInvisible
                                                                            : ESlateVisibility::Collapsed);
    if (IsValid(Button_NextPreviewScenario))
        Button_NextPreviewScenario->SetIsEnabled(bPreviewOnly && !PreviewScenarios.IsEmpty());
    if (IsValid(Button_NextPreviewLocalPlayer))
        Button_NextPreviewLocalPlayer->SetIsEnabled(bPreviewOnly && !ActivePreview.Participants.IsEmpty());
    if (IsValid(Text_PreviewScenario))
        Text_PreviewScenario->SetText(ActivePreview.Label);
    if (IsValid(Text_PreviewLocalPlayer))
    {
        const FMWVehiclePreviewParticipant* Player = FindLocalPreviewParticipant();
        Text_PreviewLocalPlayer->SetText(
            Player ? FText::Format(PreviewLocalSlotFormat,
                                   FText::Format(ParticipantMarkerFormat, FText::AsNumber(Player->SlotIndex + 1)),
                                   Player->DisplayName)
                   : FText());
    }
    if (IsValid(Text_PreviewCapacity))
    {
        Text_PreviewCapacity->SetVisibility(bPreviewOnly ? ESlateVisibility::HitTestInvisible
                                                         : ESlateVisibility::Collapsed);
        Text_PreviewCapacity->SetText(FText::Format(PreviewCapacityFormat,
                                                    FText::AsNumber(ActivePreview.Participants.Num()),
                                                    FText::AsNumber(ActivePreview.Capacity)));
    }
}

// 개발 검토용 조작부만 표시하거나 숨긴다.
void UMWVehicleSelectionWidget::SetPreviewControlsVisible(bool bVisible)
{
    bShowPreviewControls = bVisible;
    RefreshPreviewControls();
}

// 기존 개발 목록 범위에서 다음 외형 시나리오를 고른다.
void UMWVehicleSelectionWidget::HandleNextPreviewScenario()
{
    if (bPreviewOnly && !PreviewScenarios.IsEmpty())
        SelectPreviewScenario((ActivePreviewScenarioIndex + 1) % PreviewScenarios.Num());
}

// 개발 참가자 표시의 로컬 슬롯만 순환한다.
void UMWVehicleSelectionWidget::HandleNextPreviewLocalPlayer()
{
    if (!bPreviewOnly || ActivePreview.Participants.IsEmpty())
        return;
    const int32 Current = ActivePreview.Participants.IndexOfByPredicate(
        [this](const FMWVehiclePreviewParticipant& Player)
        {
            return Player.SlotIndex == ActivePreview.LocalSlotIndex;
        });
    SelectPreviewLocalSlot(ActivePreview.Participants[(Current + 1) % ActivePreview.Participants.Num()].SlotIndex);
}

// 개발용 선택을 현재 카드 표시 모델로 맞춘다. 실제 서버 선택은 만들지 않는다.
void UMWVehicleSelectionWidget::RefreshPreviewSelection()
{
    if (!bPreviewOnly)
        return;
    SelectedCard.Reset();
    for (UMWVehicleCardWidget* Card : Cards)
        if (IsValid(Card))
            Card->SetSelected(false);
    const FMWVehiclePreviewParticipant* Player = FindLocalPreviewParticipant();
    if (Player && Cards.IsValidIndex(Player->PreviewCardIndex) && Cards[Player->PreviewCardIndex]->IsAvailable())
    {
        SelectedCard = Cards[Player->PreviewCardIndex];
        SelectedCard->SetSelected(true);
    }
}

// 실제 방과 Preview 데이터 원본을 구분해 참가자 표시를 갱신한다.
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

// 외형 검토용 참가자만 구성한다. 실제 서버 차량 선택으로 등록하지 않는다.
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
                if (IsValid(Text_Error))
                    Text_Error->SetText(InvalidVehicleText);
                continue;
            }
            OccupiedSlots.Add(Player.SlotIndex);
            ParticipantViews[Player.SlotIndex] = MakePreviewPresentation(Player);
        }
    }
}

void UMWVehicleSelectionWidget::BuildLobbyParticipants(AMWLobbyGameState* State)
{
    if (!State || State->GetRoomSettings().MaxPlayers <= 0)
        return;
    ParticipantViews.Init(MakeWaitingPresentation(), State->GetRoomSettings().MaxPlayers);
    TArray<AMWPlayerState*> Players;
    for (APlayerState* Player : State->PlayerArray)
        if (AMWPlayerState* MWPlayer = Cast<AMWPlayerState>(Player))
            Players.Add(MWPlayer);
    Players.Sort(
        [](const AMWPlayerState& A, const AMWPlayerState& B)
        {
            return A.GetLobbyMemberId() < B.GetLobbyMemberId();
        });
    for (auto It = PlayerSlots.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid() || !Players.Contains(It.Key().Get()) || !ParticipantViews.IsValidIndex(It.Value()))
            It.RemoveCurrent();
    }
    TSet<int32> OccupiedSlots;
    for (const auto& Pair : PlayerSlots)
        OccupiedSlots.Add(Pair.Value);
    for (AMWPlayerState* Player : Players)
    {
        const TWeakObjectPtr<AMWPlayerState> Key(Player);
        if (!PlayerSlots.Contains(Key))
        {
            for (int32 Index = 0; Index < ParticipantViews.Num(); ++Index)
            {
                if (OccupiedSlots.Contains(Index))
                    continue;
                PlayerSlots.Add(Key, Index);
                OccupiedSlots.Add(Index);
                break;
            }
        }
        const int32* Seat = PlayerSlots.Find(Key);

        if (!Seat)
            continue;

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

// 기존 하단 참가자 슬롯과 이동 커서를 갱신한다. 카드 위 고정 마커는 새로 만들지 않는다.
void UMWVehicleSelectionWidget::RenderParticipants()
{
    if (PlayerEntries.Num() != ParticipantViews.Num())
    {
        Box_Players->ClearChildren();
        PlayerEntries.Reset();
        for (int32 Index = 0; Index < ParticipantViews.Num(); ++Index)
        {
            UMWVehiclePlayerEntryWidget* Entry =
                CreateWidget<UMWVehiclePlayerEntryWidget>(GetOwningPlayer(), PlayerEntryWidgetClass);
            if (!Entry)
                break;
            Box_Players->AddChildToHorizontalBox(Entry)->SetPadding(PlayerEntryPadding);
            PlayerEntries.Add(Entry);
        }
    }
    for (int32 Index = 0; Index < PlayerEntries.Num(); ++Index)
        PlayerEntries[Index]->SetPresentation(ParticipantViews[Index]);
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

// 실제 요청 대기·선택 잠금 상태로 입력을 조절하고 불필요한 Preview 안내를 숨긴다.
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
	// Preview의 완료는 표시용이며 서버 선택 승인으로 간주하지 않는다.
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

// 유효한 카드만 선택하고 실제 방에서는 기존 서버 승인 요청으로 즉시 전달한다.
void UMWVehicleSelectionWidget::HandleCardChosen(UMWVehicleCardWidget* Card)
{
    if (!IsValid(Card) || (Card != SecretVehicleCard && !Cards.Contains(Card)) || !Card->IsAvailable() ||
        IsSelectionLocked() || IsRequestPending())
        return;
    if (bPreviewOnly)
    {
        FMWVehiclePreviewParticipant* Player = FindLocalPreviewParticipant();
        if (!Player)
            return;
        Player->PreviewCardIndex = Cards.IndexOfByKey(Card);
        // 개발용 참가자 표시만 갱신한다. 서버 요청이나 실제 차량 ID를 만들지 않는다.
        Player->bConfirmed = true;
    }
    if (UMWVehicleCardWidget* Previous = SelectedCard.Get())
        Previous->SetSelected(false);
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

// 기존 비밀 차량 카드 모델을 같은 선택 요청 경로에 전달한다.
void UMWVehicleSelectionWidget::HandleSecretVehicleChosen()
{
    HandleCardChosen(SecretVehicleCard);
}

// 외형 Preview의 호버 미리보기만 갱신하고 확정 참가자의 표시를 덮어쓰지 않는다.
void UMWVehicleSelectionWidget::HandleCardHover(UMWVehicleCardWidget* Card, bool bHovered)
{
    if (!bPreviewOnly || IsPreviewLocked() || IsRequestPending() || !IsValid(Card))
        return;
    const FMWVehiclePreviewParticipant* Player = FindLocalPreviewParticipant();
    if (!Player || Player->bConfirmed || !PlayerEntries.IsValidIndex(Player->SlotIndex))
        return;
    if (bHovered && Card->IsAvailable())
    {
        FMWVehiclePlayerPresentation View = MakePreviewPresentation(*Player);
        View.VehicleName = Card->GetDisplayName();
        View.Thumbnail = Card->GetThumbnail();
        View.bHoverPreview = true;
        PlayerEntries[Player->SlotIndex]->SetPresentation(View);
    }
    else if (ParticipantViews.IsValidIndex(Player->SlotIndex))
        PlayerEntries[Player->SlotIndex]->SetPresentation(ParticipantViews[Player->SlotIndex]);
}

// Preview의 로컬 완료와 실제 서버 확정 요청을 구분한다.
void UMWVehicleSelectionWidget::HandleConfirm()
{
    UMWVehicleCardWidget* Card = SelectedCard.Get();
    if (!Card || !Card->IsAvailable() || IsSelectionLocked() || IsRequestPending())
        return;
    if (bPreviewOnly)
    {
        FMWVehiclePreviewParticipant* Player = FindLocalPreviewParticipant();
        if (!Player)
            return;
        Player->bConfirmed = true;
        RefreshPlayers();
        RefreshControls();
    }
    else if (OnSelectionRequested.IsBound() && Card->GetVehicleAssetId().IsValid())
    {
        PendingRequest = ESelectionRequest::Confirm;
        if (IsValid(Text_Error))
            Text_Error->SetText(FText());
        RefreshControls();
        OnSelectionRequested.Broadcast(Card->GetVehicleAssetId());
    }
}

// 확정·취소 응답을 구분해 로컬 대기를 해제하고 실제 참가자 상태로 다시 표시한다.
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

// 현재 단계와 소유자의 서버 확정 ID를 확인해 취소 가능한 대상만 반환한다.
FGameplayTag UMWVehicleSelectionWidget::GetCancelableVehicleId() const
{
    const auto* State = BoundGameState.Get();
    const auto* PC = GetOwningPlayer();
    const auto* PS = PC ? PC->GetPlayerState<AMWPlayerState>() : nullptr;
    return State && State->GetRoomPhase() == EMWRoomPhase::VehicleSelection && PS &&
                   OnSelectionCancelRequested.IsBound()
               ? PS->GetVehicleAssetID()
               : FGameplayTag::EmptyTag;
}

// 로컬 선택 강조와 카드 참조만 해제한다. 서버 취소 승인은 별도 요청 경로를 따른다.
void UMWVehicleSelectionWidget::ClearLocalSelectionDisplay()
{
    if (UMWVehicleCardWidget* Card = SelectedCard.Get())
        Card->SetSelected(false);
    SelectedCard.Reset();
}

// 실제 방은 서버 취소 응답을 기다리고 Preview만 로컬 표시를 해제한다.
bool UMWVehicleSelectionWidget::CancelLocalSelection()
{
    if (IsRequestPending())
        return false;
    if (!bPreviewOnly)
    {
        const FGameplayTag ExpectedVehicleID = GetCancelableVehicleId();
        if (!ExpectedVehicleID.IsValid())
            return false;

        PendingRequest = ESelectionRequest::Cancel;
        if (IsValid(Text_Error))
            Text_Error->SetText(FText());
        RefreshControls();
        OnSelectionCancelRequested.Broadcast(ExpectedVehicleID);
        return true;
    }

    if (AreAllPreviewParticipantsConfirmed())
        return false;
    FMWVehiclePreviewParticipant* Player = FindLocalPreviewParticipant();
    if (!Player)
        return false;
    Player->bConfirmed = false;
    Player->PreviewCardIndex = INDEX_NONE;
    ClearLocalSelectionDisplay();
    if (IsValid(Text_Error))
        Text_Error->SetText(FText());
    RefreshPlayers();
    RefreshControls();
    return true;
}

// 기존 선택 취소 경로를 재사용해 표시와 요청 대기 처리를 일치시킨다.
void UMWVehicleSelectionWidget::HandleReset()
{
    if (bPreviewOnly)
        CancelLocalSelection();
}

// 실제 요청 대기 중에는 중복 뒤로 요청을 보내지 않고 기존 화면 종료 알림을 사용한다.
void UMWVehicleSelectionWidget::HandleBack()
{
    if (!IsRequestPending())
        OnBackRequested.Broadcast();
}
