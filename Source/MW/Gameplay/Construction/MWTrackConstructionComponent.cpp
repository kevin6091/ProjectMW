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
	// 상태 변경 시에만 작업한다. 공용 후보는 소유 GameState를 통해 복제한다.
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
	
	// 준비 도중 다시 호출되는 것을 막고, 조기 반환하더라도 원래 값으로 복원한다.
	TGuardValue<bool> PreparingGuard(bPreparingSelection, true);

	// 참가자 상태를 건드리기 전에 후보 정의를 모두 준비해 실패 시 부분 초기화를 피한다.
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
	
	// Ready 버튼 여부가 아니라 소유 Controller·세션 등록·개인 컴포넌트 준비 여부를 검사한다.
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
	
	// 다음 선택의 데이터를 지역 변수에서 완성한 뒤 마지막에 공개한다.
	// OfferId는 선택 안의 번호이고 BoardSlotIndex는 화면 배치용 0 기반 인덱스다.
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
	
	// 일반 게임에서는 시작 당시 참가자만 요청할 수 있다. 중도 참가자를 자동 편입하지 않는다.
	Participants.Reset();
	for (AMWPlayerState* PS : ReadyPlayers)
	{
		Participants.Add(TWeakObjectPtr<AMWPlayerState>(PS));
		PS->GetPlayerConstructionComponent()->ResetSelectionForServer(NewSession.SelectionSessionId, NewSession.OffersRevision);
	}
	// 준비가 끝난 뒤에 공개한다. 개인 상태와 이 속성의 복제 순서는 UI에서 가정하지 않는다.
	SelectionSession = NewSession;
	// 복제 갱신을 앞당기도록 요청한다. 즉시 모든 클라이언트에 전달되는 것은 아니다.
	GetOwner()->ForceNetUpdate();
	// 서버에서 값을 직접 바꾼 Listen Host도 OnRep를 기다리지 않고 화면을 갱신한다.
	OnSelectionSessionChanged.Broadcast();
	return true;
}

void UMWTrackConstructionComponent::EndSelection()
{
	if (!HasServerAuthority() || !SelectionSession.bIsOpen) return;
	SelectionSession.bIsOpen = false;
	// 확정 결과는 보존하되 선택 화면의 커서는 즉시 숨기고 만료 타이머를 정리한다.
	for (const auto& WeakPlayer : Participants)
		if (AMWPlayerState* PS = WeakPlayer.Get())
			if (UMWPlayerConstructionComponent* Player = PS->GetPlayerConstructionComponent())
				Player->ClearCursorForServer();
	Participants.Reset();
	GetOwner()->ForceNetUpdate();
	OnSelectionSessionChanged.Broadcast();
	// Offers와 개인 SelectedOfferId는 다음 배치 처리에서 읽도록 보존한다.
}

void UMWTrackConstructionComponent::OnRep_SelectionSession()
{
	// 원격 클라이언트도 서버의 직접 변경 경로와 같은 로컬 이벤트를 사용한다.
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
    // 세션·버전이 맞아야 그 화면에서 본 후보에 대한 요청으로 해석할 수 있다.
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

    // 공용 후보뿐 아니라 요청자의 개인 상태도 이번 선택에 초기화되어 있어야 한다.
    const FMWPlayerTrackSelection Current = Player->GetSelection();
    if (Current.SelectionSessionId != SessionId || Current.OffersRevision != Revision)
    {
        OutReason = EMWTrackSelectionRejectReason::NotParticipant;
        return false;
    }
    // Hover 해제·커서 공유는 후보 번호가 없어도 되지만, 아무것도 없는 상태의 확정은 거절한다.
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
    // 확정 뒤의 Hover는 무시하고, 실제 변경 요청에만 서버 처리 예산을 소비한다.
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
    // 기존 세션·등록·참가자 검사를 재사용한다. 커서는 특정 후보를 가리키지 않아도 유효하다.
    if (!ValidateRequest(Requester, SessionId, Revision, INDEX_NONE, true, Player, Reason)) return;
    Player->UpdateCursorForServer(Sequence, bVisible, WorldPosition);
}

FMWTrackSelectionReply UMWTrackConstructionComponent::ConfirmSelection(
    APlayerController* Requester, int32 SessionId, int32 Revision,
    int32 OfferId, int32 RequestId)
{
	// 실패해도 원본 요청의 식별자를 돌려줘 Controller가 대기 중인 요청과 대조할 수 있게 한다.
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

	// 접수 여부 검사보다 먼저 조회한다. 접수 종료 후 재전송된 요청에도 이전 결과를 돌려준다.
	FMWTrackSelectionReply Cached;
	if (Player->FindCachedReply(SessionId, RequestId, Cached))
	{
		if (Cached.OffersRevision == Revision && Cached.OfferId == OfferId)
			return Cached;
		// 같은 요청 ID에 다른 내용을 실어 보내는 경우는 재시도로 취급하지 않는다.
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

	// 같은 후보의 재확정은 성공으로 응답하되 상태 변경 이벤트를 다시 발생시키지 않는다.
	// 다른 플레이어가 같은 후보를 골랐는지는 검사하지 않으므로 공유 선택이 가능하다.
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
