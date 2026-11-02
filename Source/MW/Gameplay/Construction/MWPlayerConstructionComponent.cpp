#include "Gameplay/Construction/MWPlayerConstructionComponent.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformTime.h"
#include "Net/UnrealNetwork.h"

namespace
{
    // 변경 없는 Hover나 재확정으로 불필요한 네트워크 갱신·UI 이벤트가 발생하지 않게 한다.
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
    // PlayerState에 상주하지만 매 프레임 계산하지 않고 상태 변화만 복제한다.
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
    // 캐시는 선택 단위로 비운다. 이전 선택의 요청 ID가 새 선택의 결과와 섞이지 않게 한다.
    CachedSessionId = SessionId;
    ReplyCache.Reset();
    HoverTokens = 20.0;
    LastHoverBudgetTime = FPlatformTime::Seconds();

    // 새 후보를 받았다는 표식만 적용하고 Hover·확정 값은 구조체 기본값으로 되돌린다.
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
    // 이 델리게이트 자체는 복제되지 않는다. 서버 화면을 위해 직접 발생시킨다.
    OnSelectionChanged.Broadcast(); // Listen Host 갱신
}

void UMWPlayerConstructionComponent::OnRep_Selection()
{
    // 원격 화면은 속성 복제를 받은 뒤 같은 변경 이벤트를 발생시킨다.
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
    // 기존 요청 결과는 덮어쓰지 않는다. 세션당 상한으로 캐시가 계속 커지는 것도 막는다.
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
    
    // 일정한 1초 구간을 세는 방식이 아니다. 경과 시간에 따라 최대 20개까지 토큰을 보충한다.
    const double Now = FPlatformTime::Seconds();
    const double Elapsed = FMath::Max(0.0, Now - LastHoverBudgetTime);
    LastHoverBudgetTime = Now;
    HoverTokens = FMath::Min(20.0, HoverTokens + Elapsed * 20.0);
    if (HoverTokens < 1.0) return false;
    HoverTokens -= 1.0;
    
    return true;
}
