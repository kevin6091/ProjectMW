// 로컬 바퀴 접지점으로 제한된 개수의 자국 인스턴스를 표시한다. 차량 이동 물리와 분리한다.

#include "Gameplay/Vehicle/Visual/MWVehicleSkidMarkComponent.h"

#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    constexpr int32 MaxMarksPerVehicle = 384;
    constexpr double PlaneSizeCm = 100.0;
    constexpr double SurfaceLiftCm = 0.8;
    constexpr double MaxGapCm = 160.0;
}

UMWVehicleSkidMarkComponent::UMWVehicleSkidMarkComponent()
{
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Rubber(
        TEXT("/Game/Gameplay/Vehicles/Art/Materials/M_MW_TireSkid.M_MW_TireSkid"));
    if (Plane.Succeeded() && Rubber.Succeeded())
    {
        SetStaticMesh(Plane.Object);
        SetMaterial(0, Rubber.Object);
    }

    SetMobility(EComponentMobility::Movable);
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetGenerateOverlapEvents(false);
    SetCanEverAffectNavigation(false);
    SetCastShadow(false);
    SetReceivesDecals(false);
}

// 바퀴별 바닥 접촉을 확인하고 간격·면 방향이 이어질 때만 자국을 추가한다.
void UMWVehicleSkidMarkComponent::SampleWheel(int32 WheelIndex, const FVector& WheelCenter, double RadiusCm,
                                              const UPrimitiveComponent& DriveCollision)
{
    UWorld* World = GetWorld();
    if (!World || !GetStaticMesh())
    {
        return;
    }

    // 차체가 접지 중이어도 바퀴는 가장자리 밖에 있을 수 있어 바퀴별로 바닥을 확인한다.
    const FVector Probe = FVector::UpVector * RadiusCm;
    FHitResult Hit;
    const FCollisionQueryParams Query(SCENE_QUERY_STAT(MWTireSkid), true, GetOwner());
    const FCollisionResponseParams Responses(DriveCollision.GetCollisionResponseToChannels());
    if (!World->LineTraceSingleByChannel(Hit, WheelCenter + Probe * 0.25, WheelCenter - Probe * 1.6,
                                         DriveCollision.GetCollisionObjectType(), Query, Responses) ||
        !Hit.IsValidBlockingHit() || Hit.ImpactNormal.Z < 0.5 || Cast<APawn>(Hit.GetActor()))
    {
        PreviousContacts.Remove(WheelIndex);
        return;
    }

    const FWheelContact Current{Hit.ImpactPoint, Hit.ImpactNormal};
    if (const FWheelContact* Previous = PreviousContacts.Find(WheelIndex))
    {
        const FVector Delta = Current.Position - Previous->Position;
        const double Distance = Delta.Length();
        if (Distance < RadiusCm * 0.5)
        {
            return;
        }

        if (Distance <= MaxGapCm && FVector::DotProduct(Previous->Normal, Current.Normal) >= 0.9)
        {
            const FVector Normal = (Previous->Normal + Current.Normal).GetSafeNormal();
            const FVector Center = (Previous->Position + Current.Position) * 0.5 + Normal * SurfaceLiftCm;
            const double WidthCm = RadiusCm * 0.65;
            const FQuat Rotation = FRotationMatrix::MakeFromXZ(Delta, Normal).ToQuat();
            const FVector Scale((Distance + WidthCm * 0.2) / PlaneSizeCm, WidthCm / PlaneSizeCm, 1.0);
            AddMark(FTransform(Rotation, Center, Scale));
        }
    }

    PreviousContacts.Add(WheelIndex, Current);
}

// 오래된 인스턴스를 재사용해 차량당 자국 수를 제한한다.
void UMWVehicleSkidMarkComponent::AddMark(const FTransform& Transform)
{
    // 긴 경기에서도 표시 비용이 계속 늘지 않도록 오래된 월드 자국부터 재사용한다.
    if (GetInstanceCount() < MaxMarksPerVehicle)
    {
        AddInstance(Transform, /*월드 좌표=*/true);
        return;
    }
    UpdateInstanceTransform(NextMarkToReplace, Transform, /*월드 좌표=*/true, /*렌더 상태 갱신=*/true);
    NextMarkToReplace = (NextMarkToReplace + 1) % MaxMarksPerVehicle;
}
