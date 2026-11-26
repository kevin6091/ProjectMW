#include "Gameplay/Items/MWItemComponent.h"

#include "Components/BoxComponent.h"
#include "Data/MWItemDefinitionData.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "System/MWDriveTestGameMode.h"
#include "TimerManager.h"

void UMWItemComponent::ResolveBurstShot()
{
    const auto* Vehicle = Cast<AMWVehiclePawn>(GetOwner());
    const auto* Hull = Vehicle ? Cast<UBoxComponent>(Vehicle->GetRootComponent()) : nullptr;
    const UMWItemDefinitionData* Definition = GetMountedItemDefinition(MountedPresentation.ItemId);
    if (!Hull || !Vehicle->HasAuthority() || !Vehicle->GetController() || !IsItemPlayAllowed()
        || !Definition || !Definition->UsesBurstFire() || BurstShotsRemaining == 0)
    {
        StopBurst();
        return;
    }

    const FMWItemBurstFireData& Burst = Definition->BurstFire;
    UWorld* World = GetWorld();
    const FTransform HullTransform(Hull->GetComponentQuat(), Hull->GetComponentLocation());
    const FVector Start = HullTransform.TransformPositionNoScale(Burst.TraceStartOffset + FVector(Hull->GetScaledBoxExtent().X, 0.0, 0.0));
    const FVector Forward = Hull->GetForwardVector();
    const FVector End = Start + Forward * Burst.Range;

    const FCollisionQueryParams Query(SCENE_QUERY_STAT(MWItemBurst), false, Vehicle);
    const FCollisionResponseParams Responses(Hull->GetCollisionResponseToChannels());
    FHitResult Hit;
    const bool bBlocked = World->LineTraceSingleByChannel(Hit, Start, End, Hull->GetCollisionObjectType(), Query, Responses);
    auto* Target = bBlocked ? Cast<AMWVehiclePawn>(Hit.GetActor()) : nullptr;
    const bool bHitVehicle = Target && Target->CanDrive();
    if (bHitVehicle)
    {
        Target->ApplyItemHit(Forward.GetSafeNormal2D(), Burst.KnockbackSpeed, 0.f, 0.f);
    }

#if ENABLE_DRAW_DEBUG
    if (AMWDriveTestGameMode::IsItemPickupTestMap(*World))
    {
        MulticastDrawBurstTrace(Start, bBlocked ? Hit.ImpactPoint : End, bBlocked, bHitVehicle);
    }
#endif

    if (--BurstShotsRemaining == 0)
    {
        StopBurst();
    }
}

void UMWItemComponent::StopBurst()
{
    BurstShotsRemaining = 0;
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(BurstShotTimer);
    }
}

void UMWItemComponent::MulticastDrawBurstTrace_Implementation(FVector_NetQuantize Start, FVector_NetQuantize End,
    bool bBlocked, bool bHitVehicle)
{
#if ENABLE_DRAW_DEBUG
    UWorld* World = GetWorld();
    if (World && World->GetNetMode() != NM_DedicatedServer)
    {
        const FColor Color = bHitVehicle ? FColor::Green : bBlocked ? FColor::Yellow : FColor::Red;
        DrawDebugLine(World, Start, End, Color, false, 0.8f, SDPG_Foreground, 2.5f);
    }
#endif
}
