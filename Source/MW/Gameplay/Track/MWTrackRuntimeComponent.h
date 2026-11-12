#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Gameplay/Track/MWTrackRuntimeData.h"
#include "MWTrackRuntimeComponent.generated.h"

class AMWVehiclePawn;
class UPrimitiveComponent;
class UStaticMeshComponent;
class UAudioComponent;
class UBoxComponent;
class UChildActorComponent;

// 트랙과 수명을 같이하는 복구 기믹이다. 미리보기와 실제 경기 동작을 구분한다.
UCLASS()
class UMWTrackRuntimeComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMWTrackRuntimeComponent();

    // 복구 데이터의 값과 에셋 참조를 소유한 뒤 충돌·움직임·교통을 구성한다.
    void Configure(const FMWTrackPieceRuntimeData& Data);

    // 미리보기 조각에서 실제 경기 트리거·충돌·음향이 동작하지 않도록 표시 상태를 구분한다.
    void SetPreview(bool bInPreview);

    // 로컬 이동축과 현재 높이로 발판의 월드 이동량을 조회한다.
    FVector GetLiftWorldOffset() const;

    // 설정된 실제 트랙에만 원본 시간 기반 움직임·기차·롤러·교통을 갱신한다.
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // 기존 월드 카탈로그의 복구 설정을 준비하고 현재 트랙 수명에 연결한다.
    virtual void BeginPlay() override;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    struct FMovedPart
    {
        TWeakObjectPtr<USceneComponent> Component;

        FTransform Rest;
    };

    struct FMotion
    {
        FMWTrackMotionData Data;

        TArray<FMovedPart> Parts;
    };

    struct FRiderTrigger
    {
        const FMWTrackShapeData* Shape = nullptr;

        TSet<TWeakObjectPtr<AMWVehiclePawn>> Riders;
    };

    struct FTrafficSlot
    {
        TWeakObjectPtr<UChildActorComponent> Visual;

        TWeakObjectPtr<UBoxComponent> Hitbox;

        FVector BoundsCenter;

        FRotator VisualCorrection;

        int32 LaneIndex = 0;

        double OffsetSeconds = 0;

        bool bActive = false;
    };

    struct FTrafficLaneState
    {
        double Phase = 0;

        double DriveSeconds = 0;
    };

    // 유효한 차량과 현재 단계·권한을 검사해 원래 트랙 효과를 실행한다.
    UFUNCTION()
    void OnTrigger(UPrimitiveComponent* Overlapped, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
                   int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);

    // 같은 차량의 다른 충돌 본체가 남았는지 확인한 뒤 발판 탑승 관계를 해제한다.
    UFUNCTION()
    void OnRiderEndOverlap(UPrimitiveComponent* Overlapped, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
                           int32 OtherBodyIndex);

    // 복구한 로컬 변환과 형상 데이터로 실제 트랙의 충돌 또는 트리거를 만든다.
    UPrimitiveComponent* CreateShape(const FMWTrackShapeData& Shape);

    // 원본 레일과 시간 설정에 맞춰 기차 부품·충돌·경고음을 갱신한다.
    void UpdateTrain(double Time);

    // 기차의 외형과 충돌 활성 여부를 같은 상태로 맞춘다.
    void SetTrainActive(bool bActive);

    // 차선·차종 가중치·슬롯 수로 복구 교통 외형과 충돌을 준비한다.
    void ConfigureTraffic();

    // 원본 차선 시간과 이동 상태를 사용해 교통 슬롯을 갱신한다.
    void UpdateTraffic(double Time);

    // 현재 슬롯의 교통 외형과 충돌 표시를 함께 바꾼다.
    void SetTrafficActive(FTrafficSlot& Slot, bool bActive);

    // 기존 경고음의 활성 상태를 전환하고 정지 시 재생 참조를 정리한다.
    void SetTrainBell(bool bActive);

    // 복제된 시작 시각으로 발판 높이를 구하고 안전한 탑승자 사본에 이동량을 적용한다.
    void UpdateLift(double Time, float DeltaTime);

    // 원본 축·기준 자세·진행 시간으로 복구 트랙 부품을 움직인다.
    void ApplyMotions(double Time, float GateAlpha);

    // 실제 롤러에 닿은 차량에 기존 표면 속도 API로 이동 효과를 전달한다.
    void ApplyRollers();

    // 모든 탑승 차량의 발판 참조와 선행 틱 관계를 해제한다.
    void ClearRiders();

    // 경기 연출이 호스트·원격에서 같은 시간축을 쓰도록 서버 시각을 조회한다.
    double GetRaceTime() const;

    // 설정 값과 그 안의 에셋 참조는 이 컴포넌트의 수명 동안 보관한다.
    UPROPERTY(Transient)
    // 설정 배열 원소를 가리키는 포인터 대신 값을 보관해 카탈로그 조회 수명과 분리한다.
    FMWTrackPieceRuntimeData Piece;

    bool bConfigured = false;

    TMap<TWeakObjectPtr<UPrimitiveComponent>, const FMWTrackShapeData*> Triggers;

    TMap<TWeakObjectPtr<UPrimitiveComponent>, FRiderTrigger> RiderTriggers;

    TArray<TWeakObjectPtr<UActorComponent>> GeneratedComponents;

    TArray<TWeakObjectPtr<UPrimitiveComponent>> RoadCollision;

    TArray<FMotion> Motions;

    TArray<TWeakObjectPtr<UStaticMeshComponent>> Train;

    TWeakObjectPtr<UPrimitiveComponent> LiftRide;

    TWeakObjectPtr<UPrimitiveComponent> TrainHitbox;

    TWeakObjectPtr<UAudioComponent> Bell;

    TArray<FTrafficSlot> Traffic;

    TArray<FTrafficLaneState> TrafficLaneStates;

    bool bTrainActive = false;

    bool bBellActive = false;

    FVector TrainCenter = FVector::ZeroVector;
    // 작동 시작은 서버가 결정하고 각 기기는 복제된 서버 시각으로 같은 높이를 계산한다.
    UPROPERTY(Replicated)
    double LiftActiveAt = -1;

    float LiftHeight = 0;

    bool bPreview = false;
};
