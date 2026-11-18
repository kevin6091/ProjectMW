#include "UI/MWTrackSelectionWidget.h"

#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Data/MWTrackDefinitionData.h"
#include "Engine/World.h"
#include "Gameplay/Construction/MWPlayerConstructionComponent.h"
#include "Gameplay/Construction/MWTrackConstructionComponent.h"
#include "Player/MWGameplayPlayerController.h"
#include "System/GameState/MWGameplayGameState.h"
#include "System/MWAssetManager.h"
#include "System/MWPlayerState.h"
#include "TimerManager.h"

void UMWTrackSelectionWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (PlacementStatusSpace)
	{
		PlacementStatusSpace->SetVisibility(ESlateVisibility::Collapsed);
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
	RemoteCursorLabels.Reset();
	SmoothedCursorPositions.Reset();
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
		Canvas_Cursors->SetVisibility(ESlateVisibility::Collapsed);
		if (Text_Title)
		{
			Text_Title->SetText(FText::FromString(TEXT("트랙 배치")));
		}
	}
	if (!Text_Players->GetText().EqualTo(Players))
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
	const auto* Definition = UMWAssetManager::Get().LoadAssetSynchronous<UMWTrackDefinitionData>(Offer->DefinitionAssetId);
	return Definition && !Definition->GetDisplayName().IsEmpty()
		? Definition->GetDisplayName() : FText::FromString(TEXT("트랙 조각"));
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
	TMap<int32, TArray<FString>> ConfirmedNames;
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
		{
			ConfirmedNames.FindOrAdd(Selection.SelectedOfferId).Add(Name);
			State = TEXT("선택 완료");
		}
		Lines.Add(FString::Printf(TEXT("%s%s  ·  %s"), *Name, PS == OwnPS ? TEXT(" (나)") : TEXT(""), *State));
		bOwnConfirmed |= PS == OwnPS && bSame && Selection.bConfirmed;
	}
	Text_Players->SetText(FText::FromString(FString::Join(Lines, TEXT("\n"))));

	// 이름과 확정자는 선택 상태 변경 때만 갱신한다. 매 프레임에는 화면 위치와 호버색만 맞춘다.
	for (const FMWTrackOffer& Offer : Session.Offers)
	{
		auto& Plate = OfferPlates.FindOrAdd(Offer.OfferId);
		if (!Plate)
		{
			Plate = CreateOfferPlate();
		}
		UTextBlock* Label = CastChecked<UTextBlock>(Plate->GetContent());
		FString Caption = GetOfferName(Offer.OfferId).ToString();
		if (const auto* Names = ConfirmedNames.Find(Offer.OfferId))
		{
			Caption += TEXT("\n") + FString::Join(*Names, TEXT(", ")) + TEXT(" · 선택 완료");
		}
		Label->SetText(FText::FromString(Caption));
	}
	if (bOwnConfirmed) bRequestPending = false;
	FString Status = Session.bIsOpen ? TEXT("[좌클릭]  트랙 고르기") : TEXT("다른 플레이어를 기다리고 있습니다.");
	if (GS && GS->GetRoomPhase() == EMWRoomPhase::Failed)
	{
		Status = TEXT("게임을 계속 진행할 수 없습니다.");
	}
	else if (Session.SelectionSessionId > 0 && !Session.bIsOpen) Status = TEXT("모든 플레이어의 트랙 선택이 끝났습니다.");
	else if (bOwnConfirmed || AcknowledgedOffer != INDEX_NONE) Status = TEXT("선택 완료 · 참가자 대기 중");
	else if (bRequestPending) Status = TEXT("선택 확인 중…");
	Text_SessionStatus->SetText(Session.bIsOpen && !RequestError.IsEmpty() ? RequestError : FText::FromString(Status));
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
	Label->SetFont(Text_Players->GetFont());
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
	if (!DisplayedSession.Offers.ContainsByPredicate([OfferId](const FMWTrackOffer& Offer) { return Offer.OfferId == OfferId; })) return;
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
	if (bShowingPlacement) return;
	auto* PC = GetOwningPlayer();
	const auto* GS = GetWorld()->GetGameState<AMWGameplayGameState>();
	if (!PC || !GS || !Canvas_Cursors) return;
	TSet<int32> Present;
	const FLinearColor Colors[] = {FLinearColor(.2f, .85f, 1), FLinearColor(1, .55f, .2f), FLinearColor(.5f, 1, .35f), FLinearColor(1, .4f, .8f)};
	for (APlayerState* State : GS->PlayerArray)
	{
		const auto* PS = Cast<AMWPlayerState>(State);
		if (!PS || PS == PC->GetPlayerState<AMWPlayerState>() || PS->GetLobbyMemberId() <= 0) continue;
		const int32 Id = PS->GetLobbyMemberId();
		Present.Add(Id);
		const auto RemoteCursor = PS->GetPlayerConstructionComponent()->GetCursor();
		UTextBlock* Label = RemoteCursorLabels.FindRef(Id);
		if (!RemoteCursor.bIsVisible || RemoteCursor.SelectionSessionId != DisplayedSession.SelectionSessionId || RemoteCursor.OffersRevision != DisplayedSession.OffersRevision)
		{
			if (Label) Label->SetVisibility(ESlateVisibility::Collapsed);
			SmoothedCursorPositions.Remove(Id);
			continue;
		}
		if (!Label)
		{
			Label = WidgetTree->ConstructWidget<UTextBlock>();
			Label->SetFont(Text_Players->GetFont());
			Label->SetColorAndOpacity(FSlateColor(Colors[(Id - 1) % UE_ARRAY_COUNT(Colors)]));
			Label->SetShadowColorAndOpacity(FLinearColor::Black);
			Label->SetShadowOffset(FVector2D(1, 1));
			auto* CursorSlot = Canvas_Cursors->AddChildToCanvas(Label);
			CursorSlot->SetAutoSize(true);
			CursorSlot->SetZOrder(100);
			RemoteCursorLabels.Add(Id, Label);
		}
		const FVector* Previous = SmoothedCursorPositions.Find(Id);
		const FVector Position = Previous ? FMath::VInterpTo(*Previous, FVector(RemoteCursor.WorldPosition), DeltaSeconds, 18) : FVector(RemoteCursor.WorldPosition);
		SmoothedCursorPositions.Add(Id, Position);
		FVector2D Screen;
		if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Position, Screen, false))
		{
			CastChecked<UCanvasPanelSlot>(Label->Slot)->SetPosition(Screen);
			Label->SetText(FText::FromString(FString::Printf(TEXT("+ P%d %s"), Id, *PS->GetPlayerName())));
			Label->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else Label->SetVisibility(ESlateVisibility::Collapsed);
	}
	for (auto It = RemoteCursorLabels.CreateIterator(); It; ++It)
	{
		if (Present.Contains(It.Key())) continue;
		It.Value()->RemoveFromParent();
		SmoothedCursorPositions.Remove(It.Key());
		It.RemoveCurrent();
	}
}
