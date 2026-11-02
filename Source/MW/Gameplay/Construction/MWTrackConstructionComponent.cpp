#include "Gameplay/Construction/MWTrackConstructionComponent.h"
#include "Gameplay/Construction/MWPlayerConstructionComponent.h"
#include "Data/MWTrackPieceDefinition.h"
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
}

bool UMWTrackConstructionComponent::BeginSelection(const TArray<FGameplayTag>& DefinitionIds, FText& OutError)
{
	OutError = FText::GetEmpty();
	AGameStateBase* GS = Cast<AGameStateBase>(GetOwner());
	
	if (!HasServerAuthority() || !GS || bPreparingSelection || SelectionSession.bIsOpen)
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

	UMWAssetManager& Assets = UMWAssetManager::Get();
	for (const FGameplayTag Id : DefinitionIds)
	{
		const UMWTrackPieceDefinition* Definition = Assets.LoadAssetSynchronous<UMWTrackPieceDefinition>(Id);
		
		if (!Definition || Definition->PieceId.IsNone() || !Definition->Mesh)
		{
			OutError = FText::FromString(TEXT("트랙 정의나 메시가 없습니다."));
			return false;
		}
	}
	
	TArray<AMWPlayerState*> ReadyPlayers;
	for (APlayerState* BasePS : GS->PlayerArray)
	{
		AMWPlayerState* PS = Cast<AMWPlayerState>(BasePS);
		APlayerController* PC = PS ? Cast<APlayerController>(PS->GetOwner()) : nullptr;
		if (PS && PC && PC->GetPlayerState<AMWPlayerState>() == PS
			&& PS->GetLobbyMemberId() > 0 && PS->IsSessionRegistrationConfirmed()
			&& PS->GetPlayerConstructionComponent())
		{
			ReadyPlayers.Add(PS);
		}
	}
	
	if (ReadyPlayers.IsEmpty())
	{
		OutError = FText::FromString(TEXT("선택에 참여할 플레이어가 없습니다."));
		return false;
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
	for (AMWPlayerState* PS : ReadyPlayers)
	{
		Participants.Add(TWeakObjectPtr<AMWPlayerState>(PS));
		PS->GetPlayerConstructionComponent()->ResetSelectionForServer(NewSession.SelectionSessionId, NewSession.OffersRevision);
	}
	// 준비가 끝난 뒤에 공개한다. 개인 상태와 이 속성의 복제 순서는 UI에서 가정하지 않는다.
	SelectionSession = NewSession;
	GetOwner()->ForceNetUpdate();
	OnSelectionSessionChanged.Broadcast();
	return true;
}

void UMWTrackConstructionComponent::EndSelection()
{
	if (!HasServerAuthority() || !SelectionSession.bIsOpen) return;
	SelectionSession.bIsOpen = false;
	Participants.Reset();
	GetOwner()->ForceNetUpdate();
	OnSelectionSessionChanged.Broadcast();
	// Offers와 개인 SelectedOfferId는 다음 배치 처리에서 읽도록 보존한다.
}

void UMWTrackConstructionComponent::OnRep_SelectionSession()
{
	OnSelectionSessionChanged.Broadcast();
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
        OutReason = EMWTrackSelectionRejectReason::StaleSession;
    else if (SelectionSession.OffersRevision != Revision)
        OutReason = EMWTrackSelectionRejectReason::StaleOffers;
    else if (!SelectionSession.bIsOpen)
        OutReason = EMWTrackSelectionRejectReason::NotOpen;
    else if (!GS->PlayerArray.Contains(PS) || !PS->IsSessionRegistrationConfirmed()
        || !Participants.Contains(TWeakObjectPtr<AMWPlayerState>(PS)))
        OutReason = EMWTrackSelectionRejectReason::NotParticipant;
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
		Player->ApplySelectionForServer(Next);
	return Reply;
}
