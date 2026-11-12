#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "UObject/SoftObjectPtr.h"
#include "MWItemComponent.generated.h"

class ALevelSequenceActor;
class AMWGameplayGameState;
class ULevelSequence;
class UStaticMesh;
class UStaticMeshComponent;
class USoundBase;

// 다른 플레이어가 볼 외형 상태다. 보유 아이템과 사용 요청 검증 값은 소유자용 경로와 구분한다.
USTRUCT()
struct FMWBoosterPresentation
{
    GENERATED_BODY()

    UPROPERTY()
    bool bHeld = false;

    UPROPERTY()
    double FiringEndsAt = 0.0;
};

UENUM(BlueprintType)
enum class EMWItemId : uint8
{
    None,
    Booster
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMWHeldItemChanged, EMWItemId, ItemId);

// 차량과 수명을 같이하는 서버 아이템 상태와 표시 연결이다.
UCLASS()
class MW_API UMWItemComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMWItemComponent();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintPure, Category = "MW|Items")
    EMWItemId GetCurrentItemId() const
    {
        return CurrentItemId;
    }

    UPROPERTY(BlueprintAssignable, Category = "MW|Items")
    FMWHeldItemChanged OnHeldItemChanged;

    // 서버 권한·소유 차량·허용 단계·빈 슬롯을 검사한 뒤 지급 ID와 보유 상태를 갱신한다.
    bool TryGrantBooster();

    // 로컬 소유 차량의 실제 보유 아이템만 서버 사용 요청으로 전달한다.
    void RequestUseItem();

    // 허용 단계를 벗어나면 서버가 보유·부스터 시간을 초기화한다.
    void ResetItemState();

    // 현재 서버 시각 기준의 남은 시간으로 부스터 활성 여부를 판단한다.
    bool IsBoosterActive() const;

    // 복제된 종료 시각에서 남은 초를 계산하고 이미 끝났으면 0을 반환한다.
    float GetBoosterTimeRemaining() const;

protected:
    // 실제 구독한 경기 GameState를 보관하고 현재 복제 외형을 초기 표시한다.
    virtual void BeginPlay() override;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // 차체 메시로 장착 위치를 찾을 수 없는 차량은 BP에 지정한 Pawn 로컬 변환을 사용한다.
    UPROPERTY(EditDefaultsOnly, Category = "MW|Items|Booster Visual")
    bool bOverrideBoosterMount = false;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Items|Booster Visual", meta = (EditCondition = "bOverrideBoosterMount"))
    FTransform BoosterMountTransform = FTransform::Identity;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Items|Booster Visual",
              meta = (EditCondition = "bOverrideBoosterMount", ClampMin = "0.01"))
    float BoosterExhaustLengthScale = 6.2f;

private:
    // 기존 경기 단계 또는 명시된 주행 테스트 환경인지 확인한다. UI 상태로 사용을 승인하지 않는다.
    bool IsItemPlayAllowed() const;

    // 부스터 종료 판정을 화면별 시간 대신 GameState의 서버 시각에 맞춘다.
    double GetServerTime() const;

    // 보유 값과 외형 상태를 함께 갱신하고 실제 변경만 소유자 UI에 알린다.
    void SetCurrentItem(EMWItemId ItemId);

    // 기본 외형과 음원을 준비하되 원본 시퀀스는 CDO 시점에 강제로 로드하지 않는다.
    void LoadBoosterVisualAssets();

    // 원래 차량 장착표 또는 BP의 명시적 override로 로컬 부스터 외형을 붙인다.
    bool SpawnBoosterVisual();

    // 복구 시퀀스의 루트와 부품 바인딩을 현재 장착 외형으로 연결한다.
    void PlayBoosterAnimation(const TSoftObjectPtr<ULevelSequence>& SequenceAsset, bool bLoop = false);

    // 현재 시퀀스 재생과 바인딩 참조를 정리한다.
    void StopBoosterAnimation();

    // 노즐 기준으로 배기 외형과 사용 음향을 시작한다. 서버 속도를 직접 변경하지 않는다.
    void StartBoosterJet();

    // 배기 표시와 재생 음향을 함께 정리한다.
    void StopBoosterJet();

    // 기존 부스터 사용 음원을 이어 재생한다. 이번 주석 정리에서 음량·재생 정책은 바꾸지 않는다.
    void PlayBoosterFireLoop();

    // 시퀀스가 바꾼 외형 자세를 보관한 기준으로 되돌린다.
    void RestoreBoosterPose();

    // 표시 타이머·시퀀스·배기·음향·생성 액터를 해제해 이전 차량 참조가 남지 않게 한다.
    void RemoveBoosterVisual();

    // 서버 종료 시각과 외형 상태로 장착·사용·분리 연출을 맞춘다.
    UFUNCTION()
    void OnRep_BoosterPresentation();

    UPROPERTY(ReplicatedUsing = OnRep_BoosterPresentation)
    FMWBoosterPresentation BoosterPresentation;

    UPROPERTY()
    TSubclassOf<AActor> BoosterVisualClass;

    UPROPERTY()
    TSoftObjectPtr<ULevelSequence> BoosterAttach;

    UPROPERTY()
    TSoftObjectPtr<ULevelSequence> BoosterFireStart;

    UPROPERTY()
    TSoftObjectPtr<ULevelSequence> BoosterFireLoop;

    UPROPERTY()
    TSoftObjectPtr<ULevelSequence> BoosterDetach;

    UPROPERTY()
    TObjectPtr<UStaticMesh> BoosterJetMesh;

    UPROPERTY()
    TObjectPtr<USoundBase> BoosterFireSound;

    UPROPERTY(Transient)
    TObjectPtr<AActor> BoosterVisual;

    UPROPERTY(Transient)
    TObjectPtr<ALevelSequenceActor> BoosterSequenceActor;

    UPROPERTY(Transient)
    TObjectPtr<UStaticMeshComponent> BoosterJet;

    FTimerHandle BoosterExpiryTimer;

    FTimerHandle BoosterAnimationTimer;

    double DisplayedFiringEnd = 0.0;

    bool bBoosterDetaching = false;
    // 구독 대상의 수명은 소유하지 않는다. 맵 이동으로 먼저 파괴되면 참조도 무효화된다.
    TWeakObjectPtr<AMWGameplayGameState> ObservedGameState;

    // 경기 상태 변경을 받아 아이템 사용 범위가 끝났는지 확인한다.
    UFUNCTION()
    void HandleGameStateChanged();

    // 서버가 보낸 보유 상태를 슬롯 구독자에게 전달한다. UI가 지급 결과를 다시 정하지 않는다.
    UFUNCTION()
    void OnRep_CurrentItem();

    UFUNCTION(Server, Reliable)
    void ServerUseItem(uint32 ExpectedGrantId);

    UPROPERTY(ReplicatedUsing = OnRep_CurrentItem)
    EMWItemId CurrentItemId = EMWItemId::None;

    UPROPERTY(Replicated)
    uint32 GrantId = 0;

    UPROPERTY(Replicated)
    double BoosterEndsAt = 0.0;
};
