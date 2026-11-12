#include "Gameplay/Construction/MWTrackConstructionComponent.h"

#include "Data/MWTrackDefinitionData.h"
#include "Gameplay/Construction/MWPlayerConstructionComponent.h"
#include "System/MWAssetManager.h"
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
	const TArray<FGameplayTag>& DefinitionIds, 
	const TArray<AMWPlayerState*>& SelectionPlayers,
	FText& OutError)
{
	OutError = FText::GetEmpty();
	AGameStateBase* GS = Cast<AGameStateBase>(GetOwner());
	
	if (!HasServerAuthority() || !GS || bPreparingSelection || SelectionSession.bIsOpen || PlacementSession.bIsOpen)
	{
		OutError = FText::FromString(TEXT("트랙 선택을 시작할 수 없습니다."));
		return false;
	}
	
	// 최초 예제는 공유 후보 세 개로 고정한다.
	if (DefinitionIds.Num() != 3 || NextSelectionSessionId == MAX_int32)
	{
		OutError = FText::FromString(TEXT("후보 설정을 확인해 주세요."));
		return false;
	}
	
	TGuardValue<bool> PreparingGuard(bPreparingSelection, true);

	// 실제 상태를 초기화하기 전에 모든 정의를 준비한다.
	UMWAssetManager& Assets = UMWAssetManager::Get();
	
	for (const FGameplayTag DefinitionId : DefinitionIds)
	{
		const UMWTrackDefinitionData* Definition =
			Assets.LoadAssetSynchronous<UMWTrackDefinitionData>(DefinitionId);

		if (!Definition)
		{
			OutError = FText::FromString(TEXT("트랙 정의를 불러오지 못했습니다."));
			return false;
		}

		if (!Definition->ValidateDefinition(OutError))
		{
			return false;
		}
	}
	
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
	NewSession.SelectionSessionId = ++NextSelectionSessionId;
	NewSession.OffersRevision = 1;
	NewSession.bIsOpen = true;

	for (int32 Index = 0; Index < DefinitionIds.Num(); ++Index)
	{
		FMWTrackOffer Offer;
		Offer.OfferId = Index + 1;
		Offer.DefinitionAssetId = DefinitionIds[Index];
		Offer.BoardSlotIndex = Index;

		NewSession.Offers.Add(Offer);
	}

	Participants.Reset();

	for (AMWPlayerState* PS : SelectionPlayers)
	{
		Participants.Add(TWeakObjectPtr<AMWPlayerState>(PS));

		PS->GetPlayerConstructionComponent()->ResetSelectionForServer(
			NewSession.SelectionSessionId,
			NewSession.OffersRevision);
	}
	
	// 개인 상태 준비 후 공용 상태를 공개한다.
	// 실제 클라이언트 수신 순서는 화면 구현에서 따로 처리한다.
	SelectionSession = NewSession;

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
	GetOwner()->ForceNetUpdate();
	OnSelectionSessionChanged.Broadcast();
	// Offers와 개인 SelectedOfferId는 다음 배치 처리에서 읽도록 보존한다.
}

void UMWTrackConstructionComponent::OnRep_SelectionSession()
{
	OnSelectionSessionChanged.Broadcast();
}

bool UMWTrackConstructionComponent::AreAllSelectionsConfirmed() const
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

FMWTrackSelectionReply UMWTrackConstructionComponent::ConfirmSelection(
    APlayerController* Requester, int32 SessionId, int32 Revision,
    int32 OfferId, int32 RequestId)
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
	
	if (Next.bConfirmed && Next.SelectedOfferId != OfferId)
	{
		Reply.Reason = EMWTrackSelectionRejectReason::AlreadyConfirmed;
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
		Player->ApplySelectionForServer(Next);
		if (AreAllSelectionsConfirmed())
		{
			OnAllSelectionsConfirmed.Broadcast();
		}
	}
	
	return Reply;
}
