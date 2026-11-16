// 서버의 아이템 지급·사용 상태를 소유하고 로컬 UI와 외형 표시에는 확정된 값만 전달한다.

#include "Gameplay/Items/MWItemComponent.h"

#include "Engine/World.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "Net/UnrealNetwork.h"
#include "System/GameState/MWGameplayGameState.h"
#include "System/MWDriveTestGameMode.h"

namespace
{
    constexpr double BoosterDurationSeconds = 2.0;
}

UMWItemComponent::UMWItemComponent()
{
    SetIsReplicatedByDefault(true);
    LoadBoosterVisualAssets();
}

// 실제 구독한 경기 GameState를 보관하고 현재 복제 외형을 초기 표시한다.
void UMWItemComponent::BeginPlay()
{
    Super::BeginPlay();
    if (AMWGameplayGameState* State = GetWorld()->GetGameState<AMWGameplayGameState>())
    {
        // 종료 시 다른 월드를 조회하지 않고 실제로 구독했던 상태 객체에서 연결을 해제한다.
        ObservedGameState = State;
        State->OnGameplayStateChanged.AddUniqueDynamic(this, &ThisClass::HandleGameStateChanged);
    }
    OnRep_BoosterPresentation();
}

// 구독했던 실제 GameState에서 이벤트를 해제하고 차량의 부스터 표시를 모두 정리한다.
void UMWItemComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (AMWGameplayGameState* State = ObservedGameState.Get())
    {
        State->OnGameplayStateChanged.RemoveDynamic(this, &ThisClass::HandleGameStateChanged);
    }
    ObservedGameState.Reset();
    RemoveBoosterVisual();
    Super::EndPlay(EndPlayReason);
}

// 기존 복제 계약에 따라 소유자 전용 값과 다른 플레이어에게 보일 상태를 등록한다.
void UMWItemComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(UMWItemComponent, CurrentItemId, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UMWItemComponent, GrantId, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UMWItemComponent, BoosterEndsAt, COND_OwnerOnly);
    DOREPLIFETIME(UMWItemComponent, BoosterPresentation);
}

// 기존 경기 단계 또는 명시된 주행 테스트 환경인지 확인한다. UI 상태로 사용을 승인하지 않는다.
bool UMWItemComponent::IsItemPlayAllowed() const
{
    const UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }
    if (const AMWGameplayGameState* State = World->GetGameState<AMWGameplayGameState>())
    {
        return State->GetRoomPhase() == EMWRoomPhase::Racing;
    }
    return World->GetAuthGameMode<AMWDriveTestGameMode>() != nullptr;
}

// 부스터 종료 판정을 화면별 시간 대신 GameState의 서버 시각에 맞춘다.
double UMWItemComponent::GetServerTime() const
{
    const UWorld* World = GetWorld();
    if (const AGameStateBase* State = World->GetGameState())
    {
        return State->GetServerWorldTimeSeconds();
    }
    return World->GetTimeSeconds();
}

// 현재 서버 시각 기준의 남은 시간으로 부스터 활성 여부를 판단한다.
bool UMWItemComponent::IsBoosterActive() const
{
    return GetBoosterTimeRemaining() > 0.f;
}

// 복제된 종료 시각에서 남은 초를 계산하고 이미 끝났으면 0을 반환한다.
float UMWItemComponent::GetBoosterTimeRemaining() const
{
    return BoosterEndsAt > 0.0 && GetWorld() ? static_cast<float>(FMath::Max(0.0, BoosterEndsAt - GetServerTime()))
                                             : 0.f;
}

// 서버 권한·소유 차량·허용 단계·빈 슬롯을 검사한 뒤 지급 ID와 보유 상태를 갱신한다.
bool UMWItemComponent::TryGrantBooster()
{
    const AMWVehiclePawn* Vehicle = Cast<AMWVehiclePawn>(GetOwner());
    if (!Vehicle || !Vehicle->HasAuthority() || !Vehicle->GetController() || !IsItemPlayAllowed() ||
        CurrentItemId != EMWItemId::None)
    {
        return false;
    }
    ++GrantId;
    SetCurrentItem(EMWItemId::Booster);
    return true;
}

// 로컬 소유 차량의 실제 보유 아이템만 서버 사용 요청으로 전달한다.
void UMWItemComponent::RequestUseItem()
{
    const AMWVehiclePawn* Vehicle = Cast<AMWVehiclePawn>(GetOwner());
    if (Vehicle && Vehicle->IsLocallyControlled() && CurrentItemId == EMWItemId::Booster)
    {
        ServerUseItem(GrantId);
    }
}

// 서버가 지급 ID·보유 상태·단계를 재검사해 중복 사용을 막고 종료 시각을 확정한다.
void UMWItemComponent::ServerUseItem_Implementation(uint32 ExpectedGrantId)
{
    const AMWVehiclePawn* Vehicle = Cast<AMWVehiclePawn>(GetOwner());
    if (!Vehicle || !Vehicle->GetController() || !IsItemPlayAllowed() || CurrentItemId != EMWItemId::Booster ||
        ExpectedGrantId != GrantId || IsBoosterActive())
    {
        return;
    }
    BoosterEndsAt = GetServerTime() + BoosterDurationSeconds;
    SetCurrentItem(EMWItemId::None);
}

// 허용 단계를 벗어나면 서버가 보유·부스터 시간을 초기화한다.
void UMWItemComponent::ResetItemState()
{
    AActor* Owner = GetOwner();
    if (Owner && Owner->HasAuthority() && (CurrentItemId != EMWItemId::None || BoosterEndsAt > 0.0))
    {
        BoosterEndsAt = 0.0;
        SetCurrentItem(EMWItemId::None);
    }
}

// 경기 상태 변경을 받아 아이템 사용 범위가 끝났는지 확인한다.
void UMWItemComponent::HandleGameStateChanged()
{
    if (!IsItemPlayAllowed())
    {
        ResetItemState();
    }
}

// 보유 값과 외형 상태를 함께 갱신하고 실제 변경만 소유자 UI에 알린다.
void UMWItemComponent::SetCurrentItem(EMWItemId ItemId)
{
    const bool bChanged = CurrentItemId != ItemId;
    CurrentItemId = ItemId;
    const bool bHeld = ItemId == EMWItemId::Booster;
    if (BoosterPresentation.bHeld != bHeld || BoosterPresentation.FiringEndsAt != BoosterEndsAt)
    {
        BoosterPresentation.bHeld = bHeld;
        BoosterPresentation.FiringEndsAt = BoosterEndsAt;
        OnRep_BoosterPresentation();
    }
    GetOwner()->ForceNetUpdate();
    if (bChanged)
    {
        OnRep_CurrentItem();
    }
}

// 서버가 보낸 보유 상태를 슬롯 구독자에게 전달한다. UI가 지급 결과를 다시 정하지 않는다.
void UMWItemComponent::OnRep_CurrentItem()
{
    OnHeldItemChanged.Broadcast(CurrentItemId);
}
