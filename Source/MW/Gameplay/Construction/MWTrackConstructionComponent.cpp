#include "Gameplay/Construction/MWTrackConstructionComponent.h"

#include "Gameplay/Construction/MWPlayerConstructionComponent.h"
#include "Gameplay/Construction/MWTrackPlacementClearance.h"
#include "Player/Cursor/MWPlayerCursorComponent.h"
#include "Gameplay/Track/MWTrackRuntimeSubsystem.h"
#include "LogChannel.h"
#include "System/MWPlayerState.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

namespace
{
	UMWPlayerConstructionComponent* FindPlayerConstruction(APlayerController* PC)
	{
		AMWPlayerState* PS = PC ? PC->GetPlayerState<AMWPlayerState>() : nullptr;
		return PS ? PS->GetPlayerConstructionComponent() : nullptr;
	}
}

UMWTrackConstructionComponent::UMWTrackConstructionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

bool UMWTrackConstructionComponent::HasServerAuthority() const
{
	return GetOwner() && GetOwner()->HasAuthority();
}

void UMWTrackConstructionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UMWTrackConstructionComponent, SelectionSession);
	DOREPLIFETIME(UMWTrackConstructionComponent, PlacementSession);
}

bool UMWTrackConstructionComponent::BeginSelection(
	const TArray<AMWPlayerState*>& SelectionPlayers,
	FText& OutError)
{
	return BeginSelectionInternal(SelectionPlayers, OutError, false);
}

bool UMWTrackConstructionComponent::IsPlacementReselectionPendingFor(const AMWPlayerState& Player) const
{
	if (!PlacementSession.bIsOpen || !SelectionSession.bIsPlacementReselection)
	{
		return false;
	}
	if (!SelectionSession.ReselectionMemberIds.Contains(Player.GetLobbyMemberId()))
	{
		return false;
	}
	const auto& Placement = Player.GetPlayerConstructionComponent()->GetPlacement();
	return SelectionSession.bIsOpen || Placement.SessionId != SelectionSession.SelectionSessionId || Placement.TrackClass.IsNull();
}

bool UMWTrackConstructionComponent::BeginSelectionInternal(const TArray<AMWPlayerState*>& SelectionPlayers,
	FText& OutError, bool bPlacementReselection)
{
	OutError = FText::GetEmpty();
	AGameStateBase* GS = Cast<AGameStateBase>(GetOwner());
	
	if (!HasServerAuthority() || !GS || bPreparingSelection || SelectionSession.bIsOpen
		|| bPlacementReselection != PlacementSession.bIsOpen
		|| (bPlacementReselection && (!RoundStartEnd.Id.IsValid() || PlacementSession.BaseCoursePieceCount <= 0)))
	{
		OutError = FText::FromString(TEXT("트랙 선택을 시작할 수 없습니다."));
		return false;
	}
	
	if (SelectionPlayers.IsEmpty() || SelectionPlayers.Num() > MWTrackSelection::OfferCount || NextSelectionSessionId == MAX_int32)
	{
		OutError = FText::FromString(TEXT("후보 설정을 확인해 주세요."));
		return false;
	}
	
	TGuardValue<bool> PreparingGuard(bPreparingSelection, true);

	// GameMode가 전달한 목록도 실제 월드의 정상 참가자인지 검사한다.
	TSet<int32> SeenMemberIds;
	
	for (AMWPlayerState* PS : SelectionPlayers)
	{
		APlayerController* PC = IsValid(PS) ? Cast<APlayerController>(PS->GetOwner()) : nullptr;

		if (!IsValid(PS)
			|| PS->IsInactive()
			|| !IsValid(PC)
			|| PC->GetWorld() != GetWorld()
			|| PC->GetPlayerState<AMWPlayerState>() != PS
			|| !GS->PlayerArray.Contains(PS)
			|| PS->GetLobbyMemberId() <= 0
			|| !PS->GetPlayerConstructionComponent())
		{
			OutError = FText::FromString(TEXT("트랙 선택 참가자 정보를 확인할 수 없습니다."));
			return false;
		}

		if (SeenMemberIds.Contains(PS->GetLobbyMemberId()))
		{
			OutError = FText::FromString(TEXT("중복된 참가자가 있습니다."));
			return false;
		}

		SeenMemberIds.Add(PS->GetLobbyMemberId());
	}
	
	FMWTrackSelectionSession NewSession;
	const auto* Runtime = GetWorld()->GetSubsystem<UMWTrackRuntimeSubsystem>();
	auto Pool = Runtime ? Runtime->GetSelectionPool() : TArray<TSoftClassPtr<AActor>>{};
	const int32 PoolSize = Pool.Num();
	TMap<int32, FPreparedTrack> PreparedOffers;
	while (!Pool.IsEmpty() && NewSession.Offers.Num() < MWTrackSelection::OfferCount)
	{
		// 뽑은 항목은 풀에서 제거한다. 여섯 칸의 순서와 클래스는 서버가 한 번만 정한다.
		const int32 Index = FMath::RandRange(0, Pool.Num() - 1);
		const TSoftClassPtr<AActor> TrackClass = Pool[Index];
		Pool.RemoveAtSwap(Index);
		const UClass* Class = TrackClass.LoadSynchronous();
		if (!Class || Class->HasAnyClassFlags(CLASS_Abstract))
		{
			UE_LOG(LogMWServer, Warning, TEXT("[MWTrackSelection] 후보 클래스를 불러올 수 없습니다. %s"), *TrackClass.ToString());
			continue;
		}

		const int32 Slot = NewSession.Offers.Num();
		if (bPlacementReselection)
		{
			// 다시 배치할 지점은 현재 막힌 끝점이 아니라 이번 라운드가 처음 열린 끝점이다.
			const FTransform& End = RoundStartEnd.WorldFrame;
			FPreparedTrack Prepared;
			FText PrepareError;
			if (!PrepareTrack(TrackClass, End.GetLocation(), false, Prepared, PrepareError)
				|| !MWTrackPlacementClearance::FitsEitherPort(Prepared.GroundPoints, PlacementSession.Ground, Prepared.Ports, End))
			{
				Prepared.Destroy();
				continue;
			}
			PreparedOffers.Add(Slot + 1, MoveTemp(Prepared));
		}

		FMWTrackOffer& Offer = NewSession.Offers.AddDefaulted_GetRef();
		Offer.OfferId = Slot + 1;
		Offer.TrackClass = TrackClass;
		Offer.BoardSlotIndex = Slot;
	}
	const int32 RequiredOffers = bPlacementReselection ? SelectionPlayers.Num() : MWTrackSelection::OfferCount;
	if (NewSession.Offers.Num() < RequiredOffers)
	{
		for (const auto& Entry : PreparedOffers) Entry.Value.Destroy();
		OutError = bPlacementReselection
			? NSLOCTEXT("MWTrackSelection", "NoCompatibleOffers", "다시 배치할 수 있는 트랙 후보가 부족합니다.")
			: FText::FromString(TEXT("선택 가능한 트랙 후보 여섯 개를 준비하지 못했습니다."));
		return false;
	}

	NewSession.SelectionSessionId = ++NextSelectionSessionId;
	NewSession.OffersRevision = 1;
	NewSession.bIsOpen = true;
	NewSession.bIsPlacementReselection = bPlacementReselection;

	Participants.Reset();

	for (AMWPlayerState* PS : SelectionPlayers)
	{
		Participants.Add(TWeakObjectPtr<AMWPlayerState>(PS));
		if (bPlacementReselection)
		{
			NewSession.ReselectionMemberIds.Add(PS->GetLobbyMemberId());
			PS->GetCursorComponent()->CloseStreamForServer();
		}

		PS->GetPlayerConstructionComponent()->ResetSelectionForServer(
			NewSession.SelectionSessionId,
			NewSession.OffersRevision);
	}
	
	// 개인 상태 준비 후 공용 상태를 공개한다.
	// 실제 클라이언트 수신 순서는 화면 구현에서 따로 처리한다.
	ReselectionOffers = MoveTemp(PreparedOffers);
	SelectionSession = MoveTemp(NewSession);
	if (bPlacementReselection)
	{
		RollbackRoundPlacementForReselection(SelectionPlayers);
	}
	UE_LOG(LogMWServer, Log, TEXT("[MWTrackSelection] Opened session=%d pool=%d offers=%d"),
		SelectionSession.SelectionSessionId, PoolSize, SelectionSession.Offers.Num());

	GetOwner()->ForceNetUpdate();
	OnSelectionSessionChanged.Broadcast();
	
	return true;
}

void UMWTrackConstructionComponent::EndSelection()
{
	if (!HasServerAuthority() || !SelectionSession.bIsOpen) return;
	SelectionSession.bIsOpen = false;
	for (const auto& Participant : Participants)
	{
		if (Participant.IsValid()) Participant->GetPlayerConstructionComponent()->ClearCursorForServer();
	}
	Participants.Reset();
	for (const auto& Entry : ReselectionOffers) Entry.Value.Destroy();
	ReselectionOffers.Reset();
	GetOwner()->ForceNetUpdate();
	OnSelectionSessionChanged.Broadcast();
	// Offers와 개인 SelectedOfferId는 다음 배치 처리에서 읽도록 보존한다.
}

void UMWTrackConstructionComponent::OnRep_SelectionSession()
{
	OnSelectionSessionChanged.Broadcast();
}

bool UMWTrackConstructionComponent::IsAllSelectionsConfirmed() const
{
	if (!SelectionSession.bIsOpen || Participants.IsEmpty())
	{
		return false;
	}

	for (const auto& Participant : Participants)
	{
		const AMWPlayerState* PS = Participant.Get();
		if (!IsValid(PS) || PS->IsInactive())
		{
			return false;
		}

		const FMWPlayerTrackSelection Selection = PS->GetPlayerConstructionComponent()->GetSelection();
		if (!Selection.bConfirmed || Selection.SelectionSessionId != SelectionSession.SelectionSessionId
			|| Selection.OffersRevision != SelectionSession.OffersRevision)
		{
			return false;
		}
	}
	return true;
}

bool UMWTrackConstructionComponent::ValidateRequest(APlayerController* Requester,
    int32 SessionId, int32 Revision, int32 OfferId, bool bAllowNone,
    UMWPlayerConstructionComponent*& OutPlayer,
    EMWTrackSelectionRejectReason& OutReason) const
{
    OutPlayer = nullptr;
    OutReason = EMWTrackSelectionRejectReason::None;
    const AGameStateBase* GS = Cast<AGameStateBase>(GetOwner());
    AMWPlayerState* PS = Requester ? Requester->GetPlayerState<AMWPlayerState>() : nullptr;
    UMWPlayerConstructionComponent* Player = FindPlayerConstruction(Requester);

    if (!HasServerAuthority() || !GS || !PS || !Player || Requester->GetWorld() != GetWorld())
    {
        OutReason = EMWTrackSelectionRejectReason::Unavailable;
        return false;
    }

    if (SelectionSession.SelectionSessionId != SessionId)
    {
        OutReason = EMWTrackSelectionRejectReason::StaleSession;
    }
    else if (SelectionSession.OffersRevision != Revision)
    {
        OutReason = EMWTrackSelectionRejectReason::StaleOffers;
    }
    else if (!SelectionSession.bIsOpen)
    {
        OutReason = EMWTrackSelectionRejectReason::NotOpen;
    }
    else if (PS->IsInactive() 
    	|| PS->GetOwner() != Requester
		|| !GS->PlayerArray.Contains(PS)
		|| !Participants.Contains(TWeakObjectPtr<AMWPlayerState>(PS)))
    {
    	OutReason = EMWTrackSelectionRejectReason::NotParticipant;
    }

    if (OutReason != EMWTrackSelectionRejectReason::None) return false;

    const FMWPlayerTrackSelection Current = Player->GetSelection();

    if (Current.SelectionSessionId != SessionId || Current.OffersRevision != Revision)
    {
        OutReason = EMWTrackSelectionRejectReason::NotParticipant;
        return false;
    }

    const bool bValidOffer = (bAllowNone && OfferId == INDEX_NONE)
        || SelectionSession.Offers.ContainsByPredicate([OfferId](const FMWTrackOffer& Offer)
        {
            return Offer.OfferId == OfferId;
        });

    if (!bValidOffer)
    {
        OutReason = EMWTrackSelectionRejectReason::InvalidOffer;
        return false;
    }

    OutPlayer = Player;
    return true;
}

void UMWTrackConstructionComponent::UpdateHover(APlayerController* Requester,
    int32 SessionId, int32 Revision, int32 OfferId)
{
    UMWPlayerConstructionComponent* Player = nullptr;
    EMWTrackSelectionRejectReason Reason;

    if (!ValidateRequest(Requester, SessionId, Revision, OfferId, true, Player, Reason))
        return;

    // 지연된 Hover 요청이 이미 가져간 조각을 다시 가리키지 않게 한다.
    const auto* Offer = SelectionSession.Offers.FindByPredicate([OfferId](const FMWTrackOffer& Item)
    {
        return Item.OfferId == OfferId;
    });
    if (Offer && Offer->ClaimedByMemberId != INDEX_NONE)
    {
        OfferId = INDEX_NONE;
    }

    FMWPlayerTrackSelection Current = Player->GetSelection();

    if (Current.bConfirmed || Current.HoveredOfferId == OfferId
        || !Player->ConsumeHoverBudgetForServer()) return;

    Current.HoveredOfferId = OfferId;
    Player->ApplySelectionForServer(Current);
}

void UMWTrackConstructionComponent::UpdateCursor(APlayerController* Requester,
	int32 SessionId, int32 Revision, int32 Sequence, bool bVisible, const FVector& WorldPosition)
{
	UMWPlayerConstructionComponent* Player = nullptr;
	EMWTrackSelectionRejectReason Reason;

	if (ValidateRequest(Requester, SessionId, Revision, INDEX_NONE, true, Player, Reason))
	{
		Player->UpdateCursorForServer(Sequence, bVisible, WorldPosition);
	}
}

FMWTrackSelectionReply UMWTrackConstructionComponent::ConfirmSelection(APlayerController* Requester, int32 SessionId, int32 Revision, int32 OfferId, int32 RequestId)
{
	FMWTrackSelectionReply Reply;
	Reply.SelectionSessionId = SessionId;
	Reply.OffersRevision = Revision;
	Reply.OfferId = OfferId;
	Reply.RequestId = RequestId;

	UMWPlayerConstructionComponent* Player = FindPlayerConstruction(Requester);

	if (!HasServerAuthority() || !Player)
	{
		Reply.Reason = EMWTrackSelectionRejectReason::Unavailable;
		return Reply;
	}

	if (RequestId <= 0)
	{
		Reply.Reason = EMWTrackSelectionRejectReason::InvalidRequest;
		return Reply;
	}

	FMWTrackSelectionReply Cached;

	if (Player->FindCachedReply(SessionId, RequestId, Cached))
	{
		if (Cached.OffersRevision == Revision && Cached.OfferId == OfferId)
			return Cached;
		Reply.Reason = EMWTrackSelectionRejectReason::RequestConflict;
		return Reply;
	}

	UMWPlayerConstructionComponent* ValidatedPlayer = nullptr;

	if (!ValidateRequest(Requester, SessionId, Revision, OfferId, false,
		ValidatedPlayer, Reply.Reason))
	{
		Player->CacheReplyForServer(Reply);
		return Reply;
	}

	if (Player->GetCachedReplyCount() >= UMWPlayerConstructionComponent::MaxCachedReplies)
	{
		Reply.Reason = EMWTrackSelectionRejectReason::TooManyRequests;
		return Reply;
	}

	FMWPlayerTrackSelection Next = Player->GetSelection();
	const bool bApplySelection = !Next.bConfirmed;
	auto* Offer = SelectionSession.Offers.FindByPredicate([OfferId](const FMWTrackOffer& Item)
	{
		return Item.OfferId == OfferId;
	});
	check(Offer);

	if (Next.bConfirmed && Next.SelectedOfferId != OfferId)
	{
		Reply.Reason = EMWTrackSelectionRejectReason::AlreadyConfirmed;
	}
	else if (!Next.bConfirmed && Offer->ClaimedByMemberId != INDEX_NONE)
	{
		// 화면에서 아직 사라지지 않았어도 서버에서 먼저 승인한 참가자만 가져간다.
		Reply.Reason = EMWTrackSelectionRejectReason::OfferTaken;
	}
	else
	{
		Reply.bAccepted = true;
		Next.HoveredOfferId = OfferId;
		Next.SelectedOfferId = OfferId;
		Next.bConfirmed = true;
	}

	// 상태 변경 이벤트에서 재진입해도 중복 결과가 먼저 준비돼 있게 한다.
	if (!Player->CacheReplyForServer(Reply))
	{
		Reply.bAccepted = false;
		Reply.Reason = EMWTrackSelectionRejectReason::Unavailable;
		return Reply;
	}

	if (Reply.bAccepted && bApplySelection)
	{
		// 개인 상태 알림보다 먼저 소유자를 기록해 재진입한 요청도 중복 선택을 거절한다.
		Offer->ClaimedByMemberId = Requester->GetPlayerState<AMWPlayerState>()->GetLobbyMemberId();
		Player->ApplySelectionForServer(Next);
		// 후보 구성은 그대로다. 리비전을 유지하면서 소유 상태만 각 선택판에 전달한다.
		GetOwner()->ForceNetUpdate();
		OnSelectionSessionChanged.Broadcast();
		if (IsAllSelectionsConfirmed())
		{
			if (SelectionSession.bIsPlacementReselection)
			{
				ResumePlacementAfterReselection();
			}
			else
			{
				OnAllSelectionsConfirmed.Broadcast(SelectionSession.SelectionSessionId);
			}
		}
	}

	return Reply;
}
