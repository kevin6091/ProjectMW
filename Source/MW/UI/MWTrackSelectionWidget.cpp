#include "UI/MWTrackSelectionWidget.h"

#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Gameplay/Construction/MWPlayerConstructionComponent.h"
#include "Gameplay/Construction/MWTrackConstructionComponent.h"
#include "Gameplay/Track/MWTrackRuntimeSubsystem.h"
#include "Player/MWGameplayPlayerController.h"
#include "System/GameState/MWGameplayGameState.h"
#include "System/MWPlayerState.h"
#include "TimerManager.h"
#include "UI/Cursor/MWPointerCursor.h"
#include "UI/Cursor/MWCursorProjection.h"
#include "Player/Cursor/MWPlayerCursorComponent.h"
#include "UI/Cursor/MWRemoteCursorOverlay.h"
#include "UI/Common/MWParticipantPalette.h"

void UMWTrackSelectionWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (PlacementStatusSpace)
	{
		PlacementStatusSpace->SetVisibility(ESlateVisibility::Collapsed);
	}
	RemoteCursors = CreateWidget<UMWRemoteCursorOverlay>(GetOwningPlayer(), UMWRemoteCursorOverlay::StaticClass());
	PointerClass = GetDefault<UMWRemoteCursorOverlay>()->PointerWidgetClass.LoadSynchronous();
	if (RemoteCursors)
	{
		auto* Layer = Canvas_Cursors->AddChildToCanvas(RemoteCursors);
		Layer->SetAnchors(FAnchors(0, 0, 1, 1));
		Layer->SetOffsets(FMargin(0));
		Layer->SetZOrder(100);
	}
	TryBindState();
}

void UMWTrackSelectionWidget::NativeDestruct()
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(BindRetryTimer);
	if (BoundConstruction.IsValid())
	{
		BoundConstruction->OnSelectionSessionChanged.RemoveDynamic(this, &ThisClass::HandleStateChanged);
	}
	for (const auto& Player : BoundPlayers)
	{
		if (Player.IsValid()) Player->OnSelectionChanged.RemoveDynamic(this, &ThisClass::HandleStateChanged);
	}
	BoundConstruction.Reset();
	BoundPlayers.Reset();
	OfferPlates.Reset();
	RemoteCursors = nullptr;
	PointerClass = nullptr;
	if (Canvas_Cursors) Canvas_Cursors->ClearChildren();
	DisplayedSession = {};
	RequestError = FText::GetEmpty();
	bRequestPending = false;
	bShowingPlacement = false;
	Super::NativeDestruct();
}

void UMWTrackSelectionWidget::ShowPlacementStatus(const FText& Players, const FText& Status)
{
	if (!bShowingPlacement)
	{
		bShowingPlacement = true;
		// 배치 문구가 바뀌어도 참가자 목록이 밀리지 않도록 디자이너에서 지정한 공간을 확보한다.
		if (PlacementStatusSpace)
		{
			PlacementStatusSpace->SetVisibility(ESlateVisibility::Hidden);
		}
		GetWorld()->GetTimerManager().ClearTimer(BindRetryTimer);
		// 선택 후보 이름표만 감춘다. 같은 캔버스의 참가자 포인터는 배치 단계에도 필요하다.
		for (const auto& Pair : OfferPlates) Pair.Value->SetVisibility(ESlateVisibility::Collapsed);
		if (Text_Title)
		{
			Text_Title->SetText(FText::FromString(TEXT("트랙 배치")));
		}
	}
	if (Text_Players && !Text_Players->GetText().EqualTo(Players))
	{
		Text_Players->SetText(Players);
	}
	if (!Text_SessionStatus->GetText().EqualTo(Status))
	{
		Text_SessionStatus->SetText(Status);
	}
}

void UMWTrackSelectionWidget::TryBindState()
{
	if (bShowingPlacement) return;
	const auto* GS = GetWorld()->GetGameState<AMWGameplayGameState>();
	auto* Construction = GS ? GS->GetConstructionComponent() : nullptr;
	if (Construction != BoundConstruction.Get())
	{
		if (BoundConstruction.IsValid())
		{
			BoundConstruction->OnSelectionSessionChanged.RemoveDynamic(this, &ThisClass::HandleStateChanged);
		}
		BoundConstruction = Construction;
		if (Construction)
		{
			Construction->OnSelectionSessionChanged.AddUniqueDynamic(this, &ThisClass::HandleStateChanged);
		}
	}

	const FMWTrackSelectionSession Session = Construction ? Construction->GetSelectionSession() : FMWTrackSelectionSession{};
	// 참가 인원은 GameMode가 결정한다. UI 준비 조건에 2인 테스트 규칙을 넣지 않는다.
	bool bReady = Session.SelectionSessionId > 0 && GS && !GS->PlayerArray.IsEmpty();
	if (GS)
	{
		for (APlayerState* State : GS->PlayerArray)
		{
			auto* PS = Cast<AMWPlayerState>(State);
			auto* Player = PS ? PS->GetPlayerConstructionComponent() : nullptr;
			if (!Player || PS->IsInactive()) continue;
			if (Session.bIsPlacementReselection && !Construction->IsPlacementReselectionPendingFor(*PS)) continue;
			if (!BoundPlayers.Contains(Player))
			{
				BoundPlayers.Add(Player);
				Player->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleStateChanged);
			}
			const FMWPlayerTrackSelection Selection = Player->GetSelection();
			bReady &= Selection.SelectionSessionId == Session.SelectionSessionId
				&& Selection.OffersRevision == Session.OffersRevision && PS->GetLobbyMemberId() > 0;
		}
	}
	const auto* PC = GetOwningPlayer<AMWGameplayPlayerController>();
	bReady &= PC && PC->GetPlayerState<AMWPlayerState>();
	Refresh();
	// 전원 초기 상태를 받은 뒤에는 RepNotify/호스트 이벤트로만 화면을 갱신한다.
	if (bReady)
	{
		GetWorld()->GetTimerManager().ClearTimer(BindRetryTimer);
	}
	else if (!GetWorld()->GetTimerManager().IsTimerActive(BindRetryTimer))
	{
		GetWorld()->GetTimerManager().SetTimer(BindRetryTimer, this, &ThisClass::TryBindState, .1f, true);
	}
}

FText UMWTrackSelectionWidget::GetOfferName(int32 OfferId) const
{
	const auto* Offer = DisplayedSession.Offers.FindByPredicate(
		[OfferId](const FMWTrackOffer& Item) { return Item.OfferId == OfferId; });
	if (!Offer) return FText::FromString(TEXT("트랙 조각"));
	if (const auto* Runtime = GetWorld()->GetSubsystem<UMWTrackRuntimeSubsystem>())
	{
		const FText Name = Runtime->GetTrackDisplayName(Offer->TrackClass);
		if (!Name.IsEmpty()) return Name;
	}
	return FText::Format(NSLOCTEXT("MWTrackSelection", "UnnamedOffer", "트랙 {0}"), FText::AsNumber(OfferId));
}

void UMWTrackSelectionWidget::Refresh()
{
	if (bShowingPlacement) return;
	const auto Session = BoundConstruction.IsValid() ? BoundConstruction->GetSelectionSession() : FMWTrackSelectionSession{};
	if (Session.SelectionSessionId != DisplayedSession.SelectionSessionId || Session.OffersRevision != DisplayedSession.OffersRevision)
	{
		for (const auto& Pair : OfferPlates)
		{
			Pair.Value->RemoveFromParent();
		}
		OfferPlates.Reset();
		bRequestPending = false;
		AcknowledgedOffer = INDEX_NONE;
		RequestError = FText::GetEmpty();
	}
	DisplayedSession = Session;
	const auto* PC = GetOwningPlayer<AMWGameplayPlayerController>();
	const auto* OwnPS = PC ? PC->GetPlayerState<AMWPlayerState>() : nullptr;
	const auto* GS = GetWorld()->GetGameState<AMWGameplayGameState>();
	TArray<AMWPlayerState*> Players;
	if (GS)
		for (APlayerState* State : GS->PlayerArray)
			if (auto* PS = Cast<AMWPlayerState>(State)) Players.Add(PS);
	Players.Sort([](const AMWPlayerState& A, const AMWPlayerState& B) { return A.GetLobbyMemberId() < B.GetLobbyMemberId(); });
	TArray<FString> Lines;
	bool bOwnConfirmed = false;
	for (const auto* PS : Players)
	{
		const auto Selection = PS->GetPlayerConstructionComponent()->GetSelection();
		const bool bSame = Selection.SelectionSessionId == Session.SelectionSessionId && Selection.OffersRevision == Session.OffersRevision;
		const int32 OfferId = Selection.bConfirmed ? Selection.SelectedOfferId : Selection.HoveredOfferId;
		FString State = TEXT("선택 중");
		if (bSame && !Selection.bConfirmed && OfferId != INDEX_NONE)
			State = GetOfferName(OfferId).ToString();
		if (!bSame) State = TEXT("참여 준비 중");
		const FString Name = PS->GetPlayerName();
		if (bSame && Selection.bConfirmed)
			State = TEXT("선택 완료");
		Lines.Add(FString::Printf(TEXT("%s%s  ·  %s"), *Name, PS == OwnPS ? TEXT(" (나)") : TEXT(""), *State));
		bOwnConfirmed |= PS == OwnPS && bSame && Selection.bConfirmed;
	}
	if (Text_Players)
	{
		Text_Players->SetText(FText::FromString(FString::Join(Lines, TEXT("\n"))));
	}

	// 이름표 브러시를 지정한 구형 WBP만 판을 그린다. cs:80은 후보명 판을 사용하지 않는다.
	const TConstArrayView<FMWTrackOffer> NamedOffers = OfferPlateBrush.GetResourceObject() ? MakeArrayView(Session.Offers) : TConstArrayView<FMWTrackOffer>{};
	// 점유를 확정하면 전시 액터와 함께 이름표도 제거한다.
	for (const FMWTrackOffer& Offer : NamedOffers)
	{
		if (Offer.ClaimedByMemberId != INDEX_NONE)
		{
			if (UBorder* Plate = OfferPlates.FindRef(Offer.OfferId))
			{
				Plate->RemoveFromParent();
				OfferPlates.Remove(Offer.OfferId);
			}
			continue;
		}
		auto& Plate = OfferPlates.FindOrAdd(Offer.OfferId);
		if (!Plate)
		{
			Plate = CreateOfferPlate();
		}
		UTextBlock* Label = CastChecked<UTextBlock>(Plate->GetContent());
		Label->SetText(GetOfferName(Offer.OfferId));
	}
	if (bOwnConfirmed) bRequestPending = false;
	FString Status = Session.bIsOpen ? TEXT("[좌클릭]  트랙 고르기") : TEXT("다른 플레이어를 기다리고 있습니다.");
	if (GS && GS->GetRoomPhase() == EMWRoomPhase::Failed)
	{
		Status = TEXT("게임을 계속 진행할 수 없습니다.");
	}
	else if (Session.SelectionSessionId > 0 && !Session.bIsOpen) Status = TEXT("모든 플레이어의 트랙 선택이 끝났습니다.");
	else if (bOwnConfirmed || AcknowledgedOffer != INDEX_NONE) Status = TEXT("선택 완료 · 참가자 대기 중");
	if (Session.IsWaitingForReselectionOffers())
	{
		Text_SessionStatus->SetText(NSLOCTEXT("MWTrackSelection", "RetryPlacementOffers",
			"배치할 수 있는 트랙이 없어 다시 선택합니다\n좌클릭으로 후보 다시 준비"));
		return;
	}
	const FText StatusText = Session.bIsOpen && Session.bIsPlacementReselection && !bOwnConfirmed && AcknowledgedOffer == INDEX_NONE
		? NSLOCTEXT("MWTrackSelection", "PlacementReselection", "배치할 수 있는 트랙이 없어 다시 선택합니다\n좌클릭으로 선택")
		: FText::FromString(Status);
	Text_SessionStatus->SetText(Session.bIsOpen && !RequestError.IsEmpty() ? RequestError : StatusText);
}

UBorder* UMWTrackSelectionWidget::CreateOfferPlate()
{
	UBorder* Plate = WidgetTree->ConstructWidget<UBorder>();
	// 판면 색은 한 곳에서만 곱해야 호버 시 원본 텍스처의 명암이 유지된다.
	FSlateBrush Brush = OfferPlateBrush;
	Brush.TintColor = FLinearColor::White;
	Plate->SetBrush(Brush);
	Plate->SetBrushColor(OfferPlateBrush.TintColor.GetSpecifiedColor());
	Plate->SetPadding(OfferPlatePadding);
	Plate->SetVisibility(ESlateVisibility::Collapsed);

	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
	// 공용 WBP에서 참가자 목록을 숨겨도 후보 이름표는 기존 글꼴로 그린다.
	Label->SetFont(Text_Players ? Text_Players->GetFont() : Text_SessionStatus->GetFont());
	Label->SetColorAndOpacity(FLinearColor::White);
	Label->SetJustification(ETextJustify::Center);
	Plate->SetContent(Label);

	auto* PlateSlot = Canvas_Cursors->AddChildToCanvas(Plate);
	PlateSlot->SetAlignment(FVector2D(.5, 1));
	PlateSlot->SetAutoSize(true);
	return Plate;
}

void UMWTrackSelectionWidget::PositionOfferLabel(int32 OfferId, const FVector& WorldPosition, bool bHovered)
{
	UBorder* Plate = OfferPlates.FindRef(OfferId);
	if (!Plate || bShowingPlacement)
	{
		return;
	}

	FVector2D Screen;
	if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(), WorldPosition, Screen, true))
	{
		Plate->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const FLinearColor Color = bHovered ? OfferHoverColor : OfferPlateBrush.TintColor.GetSpecifiedColor();
	if (Plate->GetBrushColor() != Color)
	{
		Plate->SetBrushColor(Color);
	}
	auto* LabelSlot = CastChecked<UCanvasPanelSlot>(Plate->Slot);
	if (!LabelSlot->GetPosition().Equals(Screen))
	{
		LabelSlot->SetPosition(Screen);
	}
	Plate->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UMWTrackSelectionWidget::ConfirmOffer(int32 OfferId)
{
	auto* PC = GetOwningPlayer<AMWGameplayPlayerController>();
	const auto* PS = PC ? PC->GetPlayerState<AMWPlayerState>() : nullptr;
	if (!PS || !DisplayedSession.bIsOpen || bRequestPending || AcknowledgedOffer != INDEX_NONE
		|| PS->GetPlayerConstructionComponent()->GetSelection().bConfirmed) return;
	if (!DisplayedSession.Offers.ContainsByPredicate([OfferId](const FMWTrackOffer& Offer)
	{
		return Offer.OfferId == OfferId && Offer.ClaimedByMemberId == INDEX_NONE;
	})) return;
	const int32 SessionId = DisplayedSession.SelectionSessionId;
	const int32 Revision = DisplayedSession.OffersRevision;
	bRequestPending = true;
	RequestError = FText::GetEmpty();
	Refresh();
	PC->RequestConfirmTrack(SessionId, Revision, OfferId);
}

void UMWTrackSelectionWidget::HandleSelectionReply(const FMWTrackSelectionReply& Reply)
{
	if (Reply.SelectionSessionId != DisplayedSession.SelectionSessionId || Reply.OffersRevision != DisplayedSession.OffersRevision) return;
	bRequestPending = false;
	if (Reply.bAccepted) AcknowledgedOffer = Reply.OfferId;
	RequestError = Reply.bAccepted ? FText::GetEmpty() : FText::Format(
		FText::FromString(TEXT("선택을 처리하지 못했습니다: {0}")), UEnum::GetDisplayValueAsText(Reply.Reason));
	Refresh();
}

void UMWTrackSelectionWidget::HandleStateChanged() { TryBindState(); }

void UMWTrackSelectionWidget::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{
	Super::NativeTick(Geometry, DeltaSeconds);
	auto* PC = GetOwningPlayer();
	const auto* GS = GetWorld()->GetGameState<AMWGameplayGameState>();
	if (!PC || !GS || !Canvas_Cursors) return;
	if (!RemoteCursors || !PointerClass) return;
	if (bShowingPlacement)
	{
		const auto& Session = GS->GetConstructionComponent()->GetPlacementSession();
		if (auto* Own = PC->GetPlayerState<AMWPlayerState>(); Own && Session.bIsOpen)
		{
			// 각 창의 해상도와 카메라가 달라도 같은 트랙 위치에 포인터를 표시하도록 배치 평면의 월드 좌표를 보낸다.
			FVector Point = FVector::ZeroVector;
			const bool bVisible = MWCursorProjection::SampleWorldPlane(*PC, Session.BuildOrigin, FVector::UpVector, Point);
			Own->GetCursorComponent()->SubmitLocalCursor(MWCursorContexts::TrackPlacement, bVisible, Point);
		}
	}
	RemoteCursors->BeginCursorFrame();
	for (APlayerState* State : GS->PlayerArray)
	{
		const auto* PS = Cast<AMWPlayerState>(State);
		if (!PS || PS->IsInactive() || PS == PC->PlayerState || PS->GetLobbyMemberId() <= 0) continue;
		FVector WorldPosition;
		int32 StreamId;
		if (bShowingPlacement)
		{
			const auto Sample = PS->GetCursorComponent()->GetCursorState();
			if (!Sample.bVisible || Sample.Context != MWCursorContexts::TrackPlacement || Sample.Space != EMWCursorSpace::World) continue;
			WorldPosition = Sample.Position;
			StreamId = Sample.StreamId;
		}
		else
		{
			// 복제된 포인터가 늦게 도착해도 이전 후보 배치 위의 위치를 현재 선택판에 표시하지 않는다.
			const auto Sample = PS->GetPlayerConstructionComponent()->GetCursor();
			if (!Sample.bIsVisible || Sample.SelectionSessionId != DisplayedSession.SelectionSessionId ||
				Sample.OffersRevision != DisplayedSession.OffersRevision) continue;
			WorldPosition = Sample.WorldPosition;
			StreamId = Sample.SelectionSessionId;
		}
		FVector2D Position;
		if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, WorldPosition, Position, true)) continue;

		const int32 Member = PS->GetLobbyMemberId();
		auto* Marker = Cast<UMWVehiclePlayerMarkerWidget>(RemoteCursors->FindCursorVisual(Member));
		if (!Marker)
		{
			Marker = CreateWidget<UMWVehiclePlayerMarkerWidget>(PC, PointerClass);
			if (!Marker) continue;
			Marker->TakeWidget();
		}
		FMWVehiclePlayerPresentation View;
		View.PlayerName = FText::FromString(PS->GetPlayerName());
		View.MarkerText = FText::Format(NSLOCTEXT("MWTrack", "Member", "P{0}"), FText::AsNumber(Member));
		View.PlayerTint = GetDefault<UMWParticipantPalette>()->GetColorForMember(Member);
		View.bConfirmed = !bShowingPlacement && PS->GetPlayerConstructionComponent()->GetSelection().bConfirmed;
		Marker->SetPresentation(View, true);
		const FMWCursorVisualLayout Layout{FVector2D(MWPointerDesignSize),
			FVector2D(RemoteCursors->PointerDisplaySize), MWGetPointerHotspot(Marker)};
		RemoteCursors->PresentCursor(Member, StreamId, Position, Marker, Layout, DeltaSeconds);
	}
	RemoteCursors->EndCursorFrame();
}
