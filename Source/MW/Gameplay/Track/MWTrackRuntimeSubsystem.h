#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Track/MWTrackRuntimeData.h"
#include "Subsystems/WorldSubsystem.h"
#include "MWTrackRuntimeSubsystem.generated.h"

class AMWVehiclePawn;
class UMWTrackRuntimeComponent;

// 현재 차량의 체크포인트·완주·경과 시간을 외부에서 조회하는 값이다.
USTRUCT(BlueprintType)
struct FMWTrackRaceProgress
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "MW|Race")
    int32 Checkpoints = 0;

    UPROPERTY(BlueprintReadOnly, Category = "MW|Race")
    bool bFinished = false;

    UPROPERTY(BlueprintReadOnly, Category = "MW|Race")
    float ElapsedSeconds = 0;
};

// 월드 트랙 연결과 서버 진행 기록을 관리하며 생성 이벤트 구독은 종료 때 해제한다.
UCLASS()
class MW_API UMWTrackRuntimeSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    UMWTrackRuntimeSubsystem();

    // 기존 배치 트랙을 연결하고 이후 생성 이벤트도 등록해 늦게 도착한 조각을 놓치지 않는다.
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;

    // 액터 생성 구독과 진행·카탈로그 참조를 이 월드에서 해제한다.
    virtual void Deinitialize() override;

    // 현재 차량에 기록된 진행 값을 반환한다. 조회로 점수를 확정하지 않는다.
    UFUNCTION(BlueprintPure, Category = "MW|Race")
    FMWTrackRaceProgress GetTrackRaceProgress(AMWVehiclePawn* Vehicle) const;

    // 서버에서 기록한 유효 복귀 Transform만 사용해 기존 차량을 복귀시킨다.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "MW|Race")
    bool RespawnAtTrackCheckpoint(AMWVehiclePawn* Vehicle);

    // 서버가 현재 진행과 중복 통과를 검사해 출발·체크포인트·완주 상태를 기록한다.
    void ProcessRaceTrigger(AMWVehiclePawn& Vehicle, AActor& Track, EMWTrackTrigger Trigger,
                            const TArray<FTransform>& RespawnPoints);

    // 배치 미리보기 상태를 실제 트랙 런타임에 전달한다.
    void SetTrackPreview(AActor& Actor, bool bPreview);
    // 복제로 생성된 승강기 컴포넌트에도 기존 카탈로그의 설정을 적용한다.
    void ConfigureRuntimeComponent(UMWTrackRuntimeComponent& Component);

protected:
    virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

private:
    struct FRacer
    {
        FMWTrackRaceProgress Progress;

        TSet<TWeakObjectPtr<AActor>> Passed;

        TArray<FTransform> RespawnTransforms;

        double StartTime = -1;
    };

    // 카탈로그에 대응하는 트랙에만 런타임을 연결하고 이미 있는 컴포넌트는 중복 생성하지 않는다.
    void AttachTrack(AActor* Actor, bool bInitialWorldActor = false);

    // 늦게 복제되거나 생성된 트랙에도 기존 카탈로그 연결을 적용한다.
    void OnActorSpawned(AActor* Actor);

    UPROPERTY()
    TObjectPtr<UMWTrackRuntimeData> Catalog;

    TMap<FName, const FMWTrackPieceRuntimeData*> Pieces;

    TMap<TWeakObjectPtr<AMWVehiclePawn>, FRacer> Racers;

    FDelegateHandle ActorSpawnHandle;
};
