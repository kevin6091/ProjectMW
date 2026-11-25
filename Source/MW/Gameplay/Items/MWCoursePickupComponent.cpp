#include "Gameplay/Items/MWCoursePickupComponent.h"

#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "Gameplay/Construction/MWTrackSnap.h"
#include "Gameplay/Items/MWCoursePickupSettings.h"
#include "Gameplay/Items/MWPickupBox.h"
#include "Gameplay/Track/MWTrackRuntimeSubsystem.h"
#include "LogChannel.h"

namespace
{
    bool HasStationaryRoad(const FMWPlacedTrack& Track, const UMWTrackRuntimeSubsystem& Runtime,
                           const UMWCoursePickupSettings& Settings)
    {
        if (!IsValid(Track.Actor) || !Track.bRequiresCollision || Settings.ExcludedTracks.Contains(Track.TrackClass))
        {
            return false;
        }

        const FMWTrackDefinition* Data = Runtime.FindTrackDefinition(Track.TrackClass);
        return Data && Data->Motions.IsEmpty() && Data->TrafficLanes.IsEmpty() && Data->TrainParts.IsEmpty();
    }

    bool FindRoadSurface(const AActor& Track, const FVector& Candidate, const UMWCoursePickupSettings& Settings,
                         FVector& OutPoint)
    {
        const FVector Probe = FVector::UpVector * Settings.SurfaceSearchHeight;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(MWPickupRoad), true);
        FHitResult Hit;
        // 이 조각의 충돌만 검사하므로 아래 지형이나 위로 교차하는 다른 도로에 얹히지 않는다.
        if (!Track.ActorLineTraceSingle(Hit, Candidate + Probe, Candidate - Probe, ECC_Visibility, Query)
            || Hit.ImpactNormal.Z < Settings.MinSurfaceUpDot)
        {
            return false;
        }
        OutPoint = Hit.ImpactPoint;
        return true;
    }
}

void UMWCoursePickupComponent::Prepare(const FMWTrackPlacementSession& Course,
                                       TConstArrayView<APlayerStart*> PlayerStarts)
{
    Clear();
    UWorld* World = GetWorld();
    const auto& Settings = *GetDefault<UMWCoursePickupSettings>();
    if (!World || !GetOwner()->HasAuthority() || !Settings.bEnabled || Course.bIsOpen || Course.PlacedTracks.Num() < 2
        || !FMath::IsFinite(Settings.SurfaceSearchHeight) || Settings.SurfaceSearchHeight <= 0
        || Settings.LaneOffsets.IsEmpty())
    {
        return;
    }

    auto& Runtime = *World->GetSubsystem<UMWTrackRuntimeSubsystem>();
    FActorSpawnParameters Params;
    Params.Owner = GetOwner();
    Params.ObjectFlags |= RF_Transient;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    TArray<FVector> RowCenters;
    const auto SpawnRow = [&](AActor& Track, const FVector& Center, const FVector& Forward)
    {
        const FVector Direction = Forward.GetSafeNormal2D();
        if (Direction.IsNearlyZero()
            || RowCenters.ContainsByPredicate([&](const FVector& Existing)
               { return FVector::DistSquared(Existing, Center) < FMath::Square(Settings.RowSpacing); }))
        {
            return;
        }

        const FVector Right = FVector::CrossProduct(FVector::UpVector, Direction);
        const int32 Before = Boxes.Num();
        for (double Offset : Settings.LaneOffsets)
        {
            FVector RoadPoint;
            if (!FMath::IsFinite(Offset)
                || !FindRoadSurface(Track, Center + Right * Offset, Settings, RoadPoint))
            {
                continue;
            }
            const FVector BoxPosition = RoadPoint + FVector::UpVector * Settings.BoxOriginHeight;
            if (AMWPickupBox* Box = World->SpawnActor<AMWPickupBox>(BoxPosition, Direction.Rotation(), Params))
            {
                Boxes.Add(Box);
                UE_LOG(LogMWServer, Log, TEXT("[MWCoursePickup] session=%d box=%s track=%s lane=%.1f location=%s"),
                    Course.SessionId, *Box->GetName(), *Track.GetName(), Offset, *BoxPosition.ToCompactString());
            }
        }
        if (Boxes.Num() > Before)
        {
            RowCenters.Add(Center);
        }
    };

    // 첫 줄은 출발 차체가 닿지 않는 거리를 남기고 실제 시작 트랙 위에서 검사한다.
    const FMWPlacedTrack& Starter = Course.PlacedTracks[0];
    FTransform Exit;
    if (!PlayerStarts.IsEmpty() && IsValid(Starter.Actor) && MWTrackSnap::ReadFirstPort(*Starter.Actor, Exit))
    {
        Exit *= Starter.Actor->GetActorTransform();
        const FVector Forward = Exit.GetRotation().GetForwardVector().GetSafeNormal2D();
        double GridDistance = TNumericLimits<double>::Max();
        double GridZ = Exit.GetLocation().Z;
        for (const APlayerStart* Start : PlayerStarts)
        {
            const double Distance = FVector::DotProduct(Exit.GetLocation() - Start->GetActorLocation(), Forward);
            if (Distance < GridDistance)
            {
                GridDistance = Distance;
                GridZ = Start->GetActorLocation().Z;
            }
        }

        const double DistanceFromGrid = GridDistance * Settings.FirstRowFraction;
        if (DistanceFromGrid >= Settings.FirstRowMinClearance && GridDistance > DistanceFromGrid)
        {
            FVector Center = Exit.GetLocation() - Forward * (GridDistance - DistanceFromGrid);
            Center.Z = FMath::Lerp(GridZ, Exit.GetLocation().Z, Settings.FirstRowFraction);
            SpawnRow(*Starter.Actor, Center, Forward);
        }
    }

    // 포트 접선으로 곡선의 진행 방향을 추정한다. 각 차선의 최종 높이는 도로 충돌에서 읽는다.
    if (FMath::IsFinite(Settings.RowSpacing) && Settings.RowSpacing > 0)
    {
        for (int32 Index = 1; Index < Course.PlacedTracks.Num() - 1; ++Index)
        {
            const FMWPlacedTrack& Placed = Course.PlacedTracks[Index];
            FMWTrackPortFrames Ports;
            if (!HasStationaryRoad(Placed, Runtime, Settings)
                || (Placed.ConnectedPortIndex != 0 && Placed.ConnectedPortIndex != 1)
                || !MWTrackSnap::ReadPorts(*Placed.Actor, Ports))
            {
                continue;
            }

            const FTransform Entry = Ports[Placed.ConnectedPortIndex] * Placed.Actor->GetActorTransform();
            const FTransform End = Ports[1 - Placed.ConnectedPortIndex] * Placed.Actor->GetActorTransform();
            const FVector A = Entry.GetLocation();
            const FVector B = End.GetLocation();
            const double Length = FVector::Distance(A, B);
            if (Length <= 2 * Settings.SeamClearance)
            {
                continue;
            }

            const FVector EntryTangent = -Entry.GetRotation().GetForwardVector() * Length;
            const FVector EndTangent = End.GetRotation().GetForwardVector() * Length;
            const double Margin = Settings.SeamClearance / Length;
            const int32 Count = FMath::Max(1, FMath::CeilToInt(FMath::Min(
                (Length - 2 * Settings.SeamClearance) / Settings.RowSpacing, double(Settings.MaxRowsPerPiece))));
            for (int32 I = 0; I < Count; ++I)
            {
                const double Alpha = FMath::Lerp(Margin, 1.0 - Margin, (I + 0.5) / Count);
                SpawnRow(*Placed.Actor, FMath::CubicInterp(A, EntryTangent, B, EndTangent, Alpha),
                    FMath::CubicInterpDerivative(A, EntryTangent, B, EndTangent, Alpha));
            }
        }
    }
    UE_LOG(LogMWServer, Log, TEXT("[MWCoursePickup] prepared session=%d revision=%d rows=%d boxes=%d"),
        Course.SessionId, Course.CourseRevision, RowCenters.Num(), Boxes.Num());
}

void UMWCoursePickupComponent::Clear()
{
    for (const auto& Box : Boxes)
    {
        if (Box.IsValid())
        {
            Box->Destroy();
        }
    }
    Boxes.Reset();
}
