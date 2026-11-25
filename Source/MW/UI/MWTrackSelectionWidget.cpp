#include "UI/MWTrackSelectionWidget.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Gameplay/Construction/MWPlayerConstructionComponent.h"
#include "Gameplay/Construction/MWTrackConstructionComponent.h"
#include "Gameplay/Construction/MWTrackSelectionBoard.h"
#include "Player/MWGameplayPlayerController.h"
#include "System/GameState/MWGameplayGameState.h"
#include "System/MWPlayerState.h"
#include "TimerManager.h"
#include "Rendering/DrawElements.h"

#include "UI/Cursor/MWPointerCursor.h"
#include "UI/Cursor/MWCursorProjection.h"
#include "Player/Cursor/MWPlayerCursorComponent.h"
#include "UI/Cursor/MWRemoteCursorOverlay.h"
#include "UI/Common/MWParticipantPalette.h"

namespace
{
	constexpr float SnapContactPulseSeconds = 0.20f;
}

void UMWTrackSelectionWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
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
	RemoteCursors = nullptr;
	PointerClass = nullptr;
	if (Canvas_Cursors) Canvas_Cursors->ClearChildren();
	DisplayedSession = {};
	RequestError = FText::GetEmpty();
	bRequestPending = false;
	bShowingPlacement = false;
	PlacementEndpoint.Reset();
	ContactPulse.Reset();
	Super::NativeDestruct();
}

void UMWTrackSelectionWidget::ShowPlacementStatus(const FText& Status)
{
	if (!bShowingPlacement)
	{
		bShowingPlacement = true;
		GetWorld()->GetTimerManager().ClearTimer(BindRetryTimer);
	}
	if (!Text_SessionStatus->GetText().EqualTo(Status))
	{
		Text_SessionStatus->SetText(Status);
	}
}

void UMWTrackSelectionWidget::ShowPlacementEndpoint(const FVector* WorldPosition)
{
	// 끝점은 로컬 창의 카메라로 매 프레임 투영한다. 설치 판정에는 사용하지 않는다.
	if (!WorldPosition)
	{
		if (PlacementEndpoint.IsSet())
		{
			PlacementEndpoint.Reset();
			Invalidate(EInvalidateWidgetReason::Paint);
		}
		return;
	}

	if (!PlacementEndpoint.IsSet() || !PlacementEndpoint->Equals(*WorldPosition))
	{
		PlacementEndpoint = *WorldPosition;
#if WITH_EDITOR
		if (FParse::Param(FCommandLine::Get(), TEXT("MWVerifyTrackPlacement")))
		{
			UE_LOG(LogTemp, Display, TEXT("MWPlacementArrow world=%s tip=%s"),
				*GetWorld()->GetOutermost()->GetName(), *WorldPosition->ToCompactString());
		}
#endif
	}
}

void UMWTrackSelectionWidget::PulsePlacementEndpoint(const FVector& WorldPosition)
{
	ContactPulse = WorldPosition;
	ContactPulseAge = 0.0f;
}

int32 UMWTrackSelectionWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry,
	const FSlateRect& CullRect, FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style,
	bool bParentEnabled) const
{
	Layer = Super::NativePaint(Args, Geometry, CullRect, Elements, Layer, Style, bParentEnabled);
	const APlayerController* PC = GetOwningPlayer();
	const FPaintGeometry PaintGeometry = Geometry.ToPaintGeometry();

	FVector2D Point;
	if (PlacementEndpoint.IsSet() &&
		UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, *PlacementEndpoint, Point, true))
	{
		const FVector2D Tip = Point - FVector2D(0, 8);
		const FVector2D Top = Tip - FVector2D(0, 27);
		const TArray<FVector2d> Shaft{Top, Tip};
		const TArray<FVector2d> Head{Tip + FVector2D(-10, -10), Tip, Tip + FVector2D(10, -10)};
		const FLinearColor Outline(0.025f, 0.035f, 0.051f, 0.87f);
		for (const TArray<FVector2d>* Stroke : {&Shaft, &Head})
		{
			FSlateDrawElement::MakeLines(Elements, Layer + 1, PaintGeometry, *Stroke, ESlateDrawEffect::None, Outline, true, 8.0f);
			FSlateDrawElement::MakeLines(Elements, Layer + 2, PaintGeometry, *Stroke, ESlateDrawEffect::None, FLinearColor::White, true, 4.0f);
		}
		Layer += 2;
	}

	if (ContactPulse.IsSet() &&
		UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, *ContactPulse, Point, true))
	{
		// 접점 섬광이 퍼지며 사라진다. 설치 불꽃과 구분되도록 연결부에만 짧게 그린다.
		const float Progress = FMath::Clamp(ContactPulseAge / SnapContactPulseSeconds, 0.0f, 1.0f);
		const FLinearColor Glow(1.0f, 0.76f, 0.28f, 1.0f - Progress);
		for (int32 Ray = 0; Ray < 6; ++Ray)
		{
			const double Angle = UE_TWO_PI * (Ray + 0.5) / 6.0;
			const FVector2D Direction(FMath::Cos(Angle), FMath::Sin(Angle) * 0.6);
			const double Radius = FMath::Lerp(4.0, 26.0, Progress);
			const TArray<FVector2d> Line{Point + Direction * (Radius * 0.4), Point + Direction * Radius};
			FSlateDrawElement::MakeLines(Elements, Layer + 1, PaintGeometry, Line, ESlateDrawEffect::None, Glow, true, 3.0f);
		}
		++Layer;
	}
	return Layer;
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

void UMWTrackSelectionWidget::Refresh()
{
	if (bShowingPlacement) return;
	const auto Session = BoundConstruction.IsValid() ? BoundConstruction->GetSelectionSession() : FMWTrackSelectionSession{};
	if (Session.SelectionSessionId != DisplayedSession.SelectionSessionId || Session.OffersRevision != DisplayedSession.OffersRevision)
	{
		bRequestPending = false;
		AcknowledgedOffer = INDEX_NONE;
		RequestError = FText::GetEmpty();
	}
	DisplayedSession = Session;
	const auto* PC = GetOwningPlayer<AMWGameplayPlayerController>();
	const auto* OwnPS = PC ? PC->GetPlayerState<AMWPlayerState>() : nullptr;
	const auto* GS = GetWorld()->GetGameState<AMWGameplayGameState>();
	const auto* OwnConstruction = OwnPS ? OwnPS->GetPlayerConstructionComponent() : nullptr;
	const FMWPlayerTrackSelection OwnSelection = OwnConstruction ? OwnConstruction->GetSelection() : FMWPlayerTrackSelection{};
	const bool bOwnConfirmed = OwnSelection.SelectionSessionId == Session.SelectionSessionId
		&& OwnSelection.OffersRevision == Session.OffersRevision && OwnSelection.bConfirmed;

	if (bOwnConfirmed) bRequestPending = false;
	FString Status = Session.bIsOpen ? TEXT("좌클릭 · 트랙 선택") : TEXT("참가자 대기 중");
	if (GS && GS->GetRoomPhase() == EMWRoomPhase::Failed)
	{
		Status = TEXT("게임을 계속 진행할 수 없습니다.");
	}
	else if (Session.SelectionSessionId > 0 && !Session.bIsOpen) Status = TEXT("모든 플레이어의 트랙 선택이 끝났습니다.");
	else if (bOwnConfirmed || AcknowledgedOffer != INDEX_NONE) Status = TEXT("선택 완료 · 다른 참가자 대기 중");
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
	if (PlacementEndpoint.IsSet() || ContactPulse.IsSet())
	{
		ContactPulseAge += DeltaSeconds;
		if (ContactPulseAge >= SnapContactPulseSeconds)
		{
			ContactPulse.Reset();
		}
		Invalidate(EInvalidateWidgetReason::Paint);
	}
	auto* PC = GetOwningPlayer();
	const auto* GS = GetWorld()->GetGameState<AMWGameplayGameState>();
	if (!PC || !GS || !Canvas_Cursors) return;
	if (!RemoteCursors || !PointerClass) return;
	if (bShowingPlacement)
	{
		const auto& Session = GS->GetConstructionComponent()->GetPlacementSession();
		if (auto* Own = PC->GetPlayerState<AMWPlayerState>(); Own && Session.bIsOpen && MWCursorProjection::HasActiveViewportPointer(*PC))
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
			const auto* Board = Cast<AMWTrackSelectionBoard>(PC->GetViewTarget());
			if (!Board) continue;
			WorldPosition = Board->ProjectSelectionCursor(Sample.WorldPosition);
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
