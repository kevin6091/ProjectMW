#include "Gameplay/Construction/MWPlayerConstructionComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

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
    DOREPLIFETIME(UMWPlayerConstructionComponent, Cursor);
}

void UMWPlayerConstructionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(CursorExpiryTimer);
    Super::EndPlay(EndPlayReason);
}

void UMWPlayerConstructionComponent::ResetSelectionForServer(int32 SessionId, int32 Revision)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    // 캐시는 선택 단위로 비운다. 이전 선택의 요청 ID가 새 선택의 결과와 섞이지 않게 한다.
    CachedSessionId = SessionId;
    ReplyCache.Reset();
    HoverTokens = 20.0;
    LastHoverBudgetTime = FPlatformTime::Seconds();

    // 새 선택에 이전 위치나 타이머를 넘기지 않는다. 공용·개인 데이터의 식별자도 맞춘다.
    ClearCursorForServer();
    Cursor = FMWPlayerConstructionCursor();
    Cursor.SelectionSessionId = SessionId;
    Cursor.OffersRevision = Revision;
    LastCursorSequence = 0;
    CursorTokens = 30.0;
    LastCursorBudgetTime = LastHoverBudgetTime;

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

void UMWPlayerConstructionComponent::UpdateCursorForServer(int32 Sequence, bool bVisible, const FVector& WorldPosition)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !GetWorld()
        || Sequence <= LastCursorSequence) return;
    // 표시 전용이어도 잘못된 좌표는 저장하지 않고 NetQuantize의 전송 범위 안으로 제한한다.
    if (bVisible && (WorldPosition.ContainsNaN() || WorldPosition.GetAbsMax() > 1000000.0)) return;

    const double Now = FPlatformTime::Seconds();
    CursorTokens = FMath::Min(30.0, CursorTokens + FMath::Max(0.0, Now - LastCursorBudgetTime) * 30.0);
    LastCursorBudgetTime = Now;
    if (CursorTokens < 1.0) return;
    CursorTokens -= 1.0;
    LastCursorSequence = Sequence;

    if (!bVisible)
    {
        ClearCursorForServer();
        return;
    }

    // 같은 위치도 주기적으로 받는다. 마지막 이동/숨김 패킷이 유실되거나 송신이 멈춘 경우를 구분한다.
    GetWorld()->GetTimerManager().SetTimer(CursorExpiryTimer, this,
        &ThisClass::ClearCursorForServer, 0.75f, false);
    if (Cursor.bIsVisible && Cursor.WorldPosition.Equals(WorldPosition, 0.5)) return;
    Cursor.WorldPosition = WorldPosition;
    Cursor.bIsVisible = true;
    GetOwner()->ForceNetUpdate();
    // OnSelectionChanged는 발생시키지 않는다. 커서는 선택판 재구성 없이 별도로 표시한다.
}

void UMWPlayerConstructionComponent::ClearCursorForServer()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(CursorExpiryTimer);
    if (!Cursor.bIsVisible) return;
    Cursor.bIsVisible = false;
    GetOwner()->ForceNetUpdate();
}