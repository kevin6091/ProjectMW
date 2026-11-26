#pragma once

#include "GameFramework/Actor.h"
#include "MWWinnerVehicleShowcase.generated.h"

class AMWVehiclePawn;
class UMeshComponent;
class USkeletalMeshComponent;
class UNiagaraComponent;
class UNiagaraSystem;

USTRUCT()
struct FMWWinnerVehicleKey
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, meta = (ClampMin = "0", ClampMax = "1"))
    float Time = 0.f;

    UPROPERTY(EditAnywhere)
    FVector2D ScreenAnchor = FVector2D(.5, .5);

    UPROPERTY(EditAnywhere, meta = (ClampMin = "0.01", ClampMax = "1"))
    float WidthFraction = .05f;

    // 결승 카메라의 방향에 대한 차량 자세. 화면에 배치할 때 카메라 위치는 따로 계산한다.
    UPROPERTY(EditAnywhere)
    FRotator ViewRotation = FRotator::ZeroRotator;
};

USTRUCT()
struct FMWWinnerVehicleShot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, meta = (ClampMin = "0", Units = "s"))
    float LeadSeconds = 0.f;

    UPROPERTY(EditAnywhere)
    TArray<FMWWinnerVehicleKey> Keys;

    UPROPERTY(EditAnywhere)
    TObjectPtr<UNiagaraSystem> Trail;

    UPROPERTY(EditAnywhere, meta = (ClampMin = "0", ClampMax = "1"))
    FVector2D TrailTime = FVector2D::ZeroVector;

    bool IsValid() const;
};

// 로컬 결과 화면이 소유하는 차량 외형. 복제·충돌·Pawn 동작 없이 정해진 자세만 그린다.
UCLASS(NotPlaceable)
class AMWWinnerVehicleShowcase final : public AActor
{
    GENERATED_BODY()

public:
    AMWWinnerVehicleShowcase();
    bool Initialize(APlayerController& Controller, AMWVehiclePawn* Vehicle, TSubclassOf<AMWVehiclePawn> VehicleClass);
    bool Present(const FMWWinnerVehicleShot& Shot, float Alpha);

protected:
    void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void AddVisual(const UMeshComponent& Source, const FTransform& Part, USkeletalMeshComponent* LiveSkeletal);
    void UpdateTrail(const FMWWinnerVehicleShot& Shot, float Alpha);

    TWeakObjectPtr<APlayerController> Viewer;
    TWeakObjectPtr<AActor> RacerHiddenByShowcase;
    TWeakObjectPtr<UNiagaraComponent> ActiveTrail;
    FBox ModelBounds = FBox(ForceInit);
    bool bTrailStarted = false;
};
