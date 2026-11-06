#include "UI/MWTrackSelectionWidget.h"

#include "Components/Button.h"
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
	Button_Offer_0->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleOffer0);
	Button_Offer_1->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleOffer1);
	Button_Offer_2->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleOffer2);
	TryBindState();
}

void UMWTrackSelectionWidget::NativeDestruct()
{
	GetWorld()->GetTimerManager().ClearTimer(BindRetryTimer);
	if (BoundConstruction.IsValid())
	{
		BoundConstruction->OnSelectionSessionChanged.RemoveDynamic(this, &ThisClass::HandleStateChanged);
	}
	for (const auto& Player : BoundPlayers)
	{
		if (Player.IsValid()) Player->OnSelectionChanged.RemoveDynamic(this, &ThisClass::HandleStateChanged);
	}
	Button_Offer_0->OnClicked.RemoveDynamic(this, &ThisClass::HandleOffer0);
	Button_Offer_1->OnClicked.RemoveDynamic(this, &ThisClass::HandleOffer1);
	Button_Offer_2->OnClicked.RemoveDynamic(this, &ThisClass::HandleOffer2);
	BoundConstruction.Reset();
	BoundPlayers.Reset();
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
	bool bReady = Session.SelectionSessionId > 0 && GS && GS->PlayerArray.Num() >= 2;
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
	const FMWTrackSelectionSession Session = BoundConstruction.IsValid()
		? BoundConstruction->GetSelectionSession() : FMWTrackSelectionSession{};
	if (Session.SelectionSessionId != DisplayedSession.SelectionSessionId || Session.OffersRevision != DisplayedSession.OffersRevision)
	{
		bRequestPending = false;
		RequestError = FText::GetEmpty();
	}
	DisplayedSession = Session;
	const auto* PC = GetOwningPlayer<AMWGameplayPlayerController>();
	const auto* OwnPS = PC ? PC->GetPlayerState<AMWPlayerState>() : nullptr;
	const FMWPlayerTrackSelection Own = OwnPS ? OwnPS->GetPlayerConstructionComponent()->GetSelection() : FMWPlayerTrackSelection{};
	const bool bOwnReady = OwnPS && Session.SelectionSessionId > 0
		&& Own.SelectionSessionId == Session.SelectionSessionId && Own.OffersRevision == Session.OffersRevision;
	if (bOwnReady && Own.bConfirmed) bRequestPending = false;

	UButton* Buttons[] = {Button_Offer_0, Button_Offer_1, Button_Offer_2};
	UTextBlock* Names[] = {Text_OfferName_0, Text_OfferName_1, Text_OfferName_2};
	UTextBlock* States[] = {Text_OfferState_0, Text_OfferState_1, Text_OfferState_2};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Buttons); ++Index)
	{
		const auto* Offer = Session.Offers.FindByPredicate(
			[Index](const FMWTrackOffer& Item) { return Item.BoardSlotIndex == Index; });
		const bool bSelected = bOwnReady && Own.bConfirmed && Offer && Own.SelectedOfferId == Offer->OfferId;
		Names[Index]->SetText(GetOfferName(Offer ? Offer->OfferId : INDEX_NONE));
		States[Index]->SetText(FText::FromString(bSelected ? TEXT("내 선택 확정") : TEXT("트랙 후보")));
		Buttons[Index]->SetIsEnabled(Offer && Session.bIsOpen && bOwnReady && !Own.bConfirmed && !bRequestPending);
	}

	const auto* GS = GetWorld()->GetGameState<AMWGameplayGameState>();
	TArray<AMWPlayerState*> Players;
	if (GS)
	{
		for (APlayerState* State : GS->PlayerArray)
		{
			auto* PS = Cast<AMWPlayerState>(State);
			if (IsValid(PS) && !PS->IsInactive()) Players.Add(PS);
		}
	}
	Players.Sort([](const AMWPlayerState& A, const AMWPlayerState& B) { return A.GetLobbyMemberId() < B.GetLobbyMemberId(); });
	UTextBlock* PlayerNames[] = {Text_PlayerName_0, Text_PlayerName_1};
	UTextBlock* PlayerSelections[] = {Text_PlayerSelection_0, Text_PlayerSelection_1};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(PlayerNames); ++Index)
	{
		const auto* PS = Players.IsValidIndex(Index) ? Players[Index] : nullptr;
		const FMWPlayerTrackSelection Selection = PS ? PS->GetPlayerConstructionComponent()->GetSelection() : FMWPlayerTrackSelection{};
		const bool bConfirmed = PS && Selection.SelectionSessionId == Session.SelectionSessionId
			&& Selection.OffersRevision == Session.OffersRevision && Selection.bConfirmed;
		PlayerNames[Index]->SetText(PS ? FText::Format(FText::FromString(TEXT("P{0} {1}{2}")),
			PS->GetLobbyMemberId(), FText::FromString(PS->GetPlayerName()),
			FText::FromString(PS == OwnPS ? TEXT(" (나)") : TEXT(""))) : FText::FromString(TEXT("참가자 대기")));
		PlayerSelections[Index]->SetText(bConfirmed ? GetOfferName(Selection.SelectedOfferId) : FText::FromString(TEXT("선택 대기")));
	}
	FText Status = FText::FromString(TEXT("플레이어 접속 대기"));
	if (Session.SelectionSessionId > 0)
	{
		Status = FText::FromString(Session.bIsOpen ? TEXT("트랙 선택 중") : TEXT("트랙 선택 종료"));
	}
	if (bRequestPending) Status = FText::FromString(TEXT("서버 승인 대기"));
	if (Session.bIsOpen && !RequestError.IsEmpty()) Status = RequestError;
	Text_SessionStatus->SetText(Status);
}

void UMWTrackSelectionWidget::ConfirmSlot(int32 SlotIndex)
{
	auto* PC = GetOwningPlayer<AMWGameplayPlayerController>();
	const auto* Offer = DisplayedSession.Offers.FindByPredicate(
		[SlotIndex](const FMWTrackOffer& Item) { return Item.BoardSlotIndex == SlotIndex; });
	if (!PC || !Offer || !DisplayedSession.bIsOpen || bRequestPending) return;
	const int32 OfferId = Offer->OfferId;
	const int32 SessionId = DisplayedSession.SelectionSessionId;
	const int32 Revision = DisplayedSession.OffersRevision;
	bRequestPending = true;
	RequestError = FText::GetEmpty();
	Refresh();
	// 화면에 표시한 세션 키를 그대로 보내 오래된 클릭도 서버가 판별하게 한다.
	PC->RequestConfirmTrack(SessionId, Revision, OfferId);
}

void UMWTrackSelectionWidget::HandleSelectionReply(const FMWTrackSelectionReply& Reply)
{
	if (Reply.SelectionSessionId != DisplayedSession.SelectionSessionId || Reply.OffersRevision != DisplayedSession.OffersRevision) return;
	bRequestPending = false;
	RequestError = Reply.bAccepted ? FText::GetEmpty() : FText::Format(
		FText::FromString(TEXT("선택 거절: {0}")), UEnum::GetDisplayValueAsText(Reply.Reason));
	Refresh();
}

void UMWTrackSelectionWidget::HandleStateChanged() { TryBindState(); }
void UMWTrackSelectionWidget::HandleOffer0() { ConfirmSlot(0); }
void UMWTrackSelectionWidget::HandleOffer1() { ConfirmSlot(1); }
void UMWTrackSelectionWidget::HandleOffer2() { ConfirmSlot(2); }
