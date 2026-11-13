#pragma once

#include "Components/InstancedStaticMeshComponent.h"
#include "MWVehicleSkidMarkComponent.generated.h"

// 바닥 접촉을 표시용 ISM 자국으로 만들며 원격 복제·차량 물리와 분리한다.
UCLASS()
class UMWVehicleSkidMarkComponent : public UInstancedStaticMeshComponent
{
    GENERATED_BODY()

public:
    UMWVehicleSkidMarkComponent();

    // 바퀴별 바닥 접촉을 확인하고 간격·면 방향이 이어질 때만 자국을 추가한다.
    void SampleWheel(int32 WheelIndex, const FVector& WheelCenter, double RadiusCm,
                     const UPrimitiveComponent& DriveCollision);

    void ResetTrail()
    {
        PreviousContacts.Reset();
    }

private:
    struct FWheelContact
    {
        FVector Position;

        FVector Normal;
    };

    // 오래된 인스턴스를 재사용해 차량당 자국 수를 제한한다.
    void AddMark(const FTransform& Transform);

    TMap<int32, FWheelContact> PreviousContacts;

    int32 NextMarkToReplace = 0;
};
