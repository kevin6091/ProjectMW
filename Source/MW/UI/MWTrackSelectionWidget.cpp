#include "UI/MWTrackSelectionWidget.h"

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
	RemoteCursorLabels.Reset();
	SmoothedCursorPositions.Reset();
	if (Canvas_Cursors) Canvas_Cursors->ClearChildren();
	DisplayedSession = {};
	RequestError = FText::GetEmpty();
	bRequestPending = false;
	Super::NativeDestruct();
}

void UMWTrackSelectionWidget::TryBindState()
{
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
	if (!Offer) return FText::FromString(TEXT("후보 데이터 대기"));
	const auto* Definition = UMWAssetManager::Get().LoadAssetSynchronous<UMWTrackDefinitionData>(Offer->DefinitionAssetId);
	return Definition && !Definition->GetDisplayName().IsEmpty()
		? Definition->GetDisplayName() : FText::FromName(Offer->DefinitionAssetId.GetTagName());
}

void UMWTrackSelectionWidget::Refresh()
{
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
	TArray<AMWPlayerState*> Players;
	if (GS)
		for (APlayerState* State : GS->PlayerArray)
			if (auto* PS = Cast<AMWPlayerState>(State)) Players.Add(PS);
	Players.Sort([](const AMWPlayerState& A, const AMWPlayerState& B) { return A.GetLobbyMemberId() < B.GetLobbyMemberId(); });
	FString Lines;
	bool bOwnConfirmed = false;
	for (const auto* PS : Players)
	{
		const auto Selection = PS->GetPlayerConstructionComponent()->GetSelection();
		const bool bSame = Selection.SelectionSessionId == Session.SelectionSessionId && Selection.OffersRevision == Session.OffersRevision;
		const int32 OfferId = Selection.bConfirmed ? Selection.SelectedOfferId : Selection.HoveredOfferId;
		FString State = TEXT("선택 중");
		if (bSame && OfferId != INDEX_NONE)
			State = GetOfferName(OfferId).ToString() + (Selection.bConfirmed ? TEXT(" · 선택 완료") : TEXT(" · 확인 중"));
		if (!bSame) State = TEXT("참여 준비 중");
		Lines += FString::Printf(TEXT("P%d %s%s · %s\n"), PS->GetLobbyMemberId(), *PS->GetPlayerName(), PS == OwnPS ? TEXT(" (나)") : TEXT(""), *State);
		bOwnConfirmed |= PS == OwnPS && bSame && Selection.bConfirmed;
	}
	Text_Players->SetText(FText::FromString(Lines));
	if (bOwnConfirmed) bRequestPending = false;
	FString Status = Session.bIsOpen ? TEXT("트랙 위에 마우스를 올리고 좌클릭으로 선택하세요.") : TEXT("다른 플레이어를 기다리고 있습니다.");
	if (Session.SelectionSessionId > 0 && !Session.bIsOpen) Status = TEXT("모든 플레이어의 트랙 선택이 끝났습니다.");
	else if (bOwnConfirmed || AcknowledgedOffer != INDEX_NONE) Status = TEXT("선택했습니다. 다른 플레이어의 선택을 기다립니다.");
	else if (bRequestPending) Status = TEXT("서버에서 선택을 확인하고 있습니다...");
	Text_SessionStatus->SetText(Session.bIsOpen && !RequestError.IsEmpty() ? RequestError : FText::FromString(Status));
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
