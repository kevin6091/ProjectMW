#include "Gameplay/Construction/MWPlayerConstructionComponent.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformTime.h"
#include "Net/UnrealNetwork.h"

namespace
{
    bool IsSameSelection(const FMWPlayerTrackSelection& A, const FMWPlayerTrackSelection& B)
    {
        return A.SelectionSessionId == B.SelectionSessionId
            && A.OffersRevision == B.OffersRevision
            && A.HoveredOfferId == B.HoveredOfferId
            && A.SelectedOfferId == B.SelectedOfferId
            && A.bConfirmed == B.bConfirmed;
    }
}

UMWPlayerConstructionComponent::UMWPlayerConstructionComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UMWPlayerConstructionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UMWPlayerConstructionComponent, Selection);
}

void UMWPlayerConstructionComponent::ResetSelectionForServer(int32 SessionId, int32 Revision)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    CachedSessionId = SessionId;
    ReplyCache.Reset();
    HoverTokens = 20.0;
    LastHoverBudgetTime = FPlatformTime::Seconds();

    FMWPlayerTrackSelection Empty;
    Empty.SelectionSessionId = SessionId;
    Empty.OffersRevision = Revision;
    ApplySelectionForServer(Empty);
}

void UMWPlayerConstructionComponent::ApplySelectionForServer(const FMWPlayerTrackSelection& NewSelection)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || IsSameSelection(Selection, NewSelection)) 
        return;

    Selection = NewSelection;
    GetOwner()->ForceNetUpdate();
    OnSelectionChanged.Broadcast(); // Listen Host 갱신
}

void UMWPlayerConstructionComponent::OnRep_Selection()
{
    OnSelectionChanged.Broadcast();
}

bool UMWPlayerConstructionComponent::FindCachedReply(int32 SessionId, int32 RequestId, FMWTrackSelectionReply& OutReply) const
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || SessionId != CachedSessionId) 
        return false;
    
    const FMWTrackSelectionReply* Found = ReplyCache.Find(RequestId);
    if (!Found) return false;
    OutReply = *Found;
    return true;
}

bool UMWPlayerConstructionComponent::CacheReplyForServer(const FMWTrackSelectionReply& Reply)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()
        || Reply.SelectionSessionId != CachedSessionId
        || Reply.RequestId <= 0 || ReplyCache.Contains(Reply.RequestId)
        || ReplyCache.Num() >= MaxCachedReplies) return false;

    ReplyCache.Add(Reply.RequestId, Reply);
    
    return true;
}

bool UMWPlayerConstructionComponent::ConsumeHoverBudgetForServer()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return false;
    
    const double Now = FPlatformTime::Seconds();
    const double Elapsed = FMath::Max(0.0, Now - LastHoverBudgetTime);
    LastHoverBudgetTime = Now;
    HoverTokens = FMath::Min(20.0, HoverTokens + Elapsed * 20.0);
    if (HoverTokens < 1.0) return false;
    HoverTokens -= 1.0;
    
    return true;
}
