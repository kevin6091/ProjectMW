#include "Gameplay/Items/MWItemComponent.h"

#include "Components/BoxComponent.h"
#include "Data/MWItemDefinitionData.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"

void UMWItemComponent::ResolveHammerHit()
{
    auto* Attacker = Cast<AMWVehiclePawn>(GetOwner());
    const auto* Hull = Attacker ? Cast<UBoxComponent>(Attacker->GetRootComponent()) : nullptr;
    if (!Hull || !Attacker->HasAuthority() || !IsItemPlayAllowed() || !HammerDefinition)
    {
        return;
    }

    const FVector Forward = Attacker->GetActorForwardVector().GetSafeNormal2D();
    if (Forward.IsNearlyZero())
    {
        return;
    }
    const FMWItemAreaHitData& Hit = HammerDefinition->AreaHit;
    const FVector Center = Hull->GetComponentLocation() + Forward * (Hull->GetScaledBoxExtent().X + Hit.Radius);
    const FCollisionQueryParams Query(SCENE_QUERY_STAT(MWHammerHit), false, Attacker);
    TArray<FOverlapResult> Candidates;
    GetWorld()->OverlapMultiByObjectType(Candidates, Center, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn),
        FCollisionShape::MakeBox(FVector(Hit.Radius, Hit.Radius, Hit.HalfHeight)), Query);

    TSet<AMWVehiclePawn*> HitVehicles;
    for (const FOverlapResult& Candidate : Candidates)
    {
        auto* Target = Cast<AMWVehiclePawn>(Candidate.GetActor());
        const auto* TargetHull = Target ? Cast<UBoxComponent>(Target->GetRootComponent()) : nullptr;
        if (!TargetHull || Target == Attacker || !Target->CanDrive() || HitVehicles.Contains(Target))
        {
            continue;
        }

        // 지면 위에서 본 차량 박스의 가장 가까운 점을 원과 비교한다. 높이 범위도 별도로 적용한다.
        const FTransform TargetPlanar(FRotator(0.f, TargetHull->GetComponentRotation().Yaw, 0.f),
            TargetHull->GetComponentLocation());
        const FVector LocalCenter = TargetPlanar.InverseTransformPositionNoScale(Center);
        const FVector Extent = TargetHull->GetScaledBoxExtent();
        const double GapX = FMath::Max(FMath::Abs(LocalCenter.X) - Extent.X, 0.0);
        const double GapY = FMath::Max(FMath::Abs(LocalCenter.Y) - Extent.Y, 0.0);
        if (FMath::Abs(LocalCenter.Z) > Hit.HalfHeight + Extent.Z
            || FMath::Square(GapX) + FMath::Square(GapY) > FMath::Square(Hit.Radius))
        {
            continue;
        }

        HitVehicles.Add(Target);
        const FVector Away = (TargetHull->GetComponentLocation() - Attacker->GetActorLocation()).GetSafeNormal2D();
        Target->ApplyItemHit(Away.IsNearlyZero() ? Forward : Away, Hit.KnockbackSpeed, Hit.ControlLockDuration, Hit.SpinDegrees);
    }
}

