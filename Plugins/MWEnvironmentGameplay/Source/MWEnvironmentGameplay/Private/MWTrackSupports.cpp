#include "MWEnvironmentGameplay.h"

#include "CollisionQueryParams.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Gameplay/Construction/MWTrackConstructionComponent.h"
#include "Gameplay/Construction/MWTrackSnap.h"
#include "Gameplay/Track/MWTrackRuntimeComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "MWEnvironmentRestore.h"
#include "System/GameState/MWGameplayGameState.h"
#include "TimerManager.h"

namespace
{
    // 와이어프레임 받침과 비슷하게 양쪽 도로 가장자리보다 조금 안쪽에 세운다.
    constexpr double RoadWidthFraction = 0.85;
    constexpr int32 WidthMeasurements = 7;

    bool TraceRoadBottom(UPrimitiveComponent& Road, const FVector& At, FVector& Out)
    {
        const FBoxSphereBounds& Bounds = Road.Bounds;
        const FVector Bottom(At.X, At.Y, Bounds.Origin.Z - Bounds.BoxExtent.Z - 1.0);
        const FVector Top(At.X, At.Y, Bounds.Origin.Z + Bounds.BoxExtent.Z + 1.0);
        FHitResult Hit;
        if (!Road.LineTraceComponent(Hit, Bottom, Top, FCollisionQueryParams()))
        {
            return false;
        }
        Out = Hit.ImpactPoint;
        return true;
    }

    double RoadHalfWidth(UPrimitiveComponent& Road, const FVector& Center, const FVector& Across)
    {
        const FBoxSphereBounds& Bounds = Road.Bounds;
        double Inside = 0.0;
        double Outside = FVector::DotProduct(Bounds.BoxExtent, Across.GetAbs())
            - FVector::DotProduct(Center - Bounds.Origin, Across);
        for (int32 Index = 0; Index < WidthMeasurements; ++Index)
        {
            const double Distance = (Inside + Outside) * 0.5;
            FVector Unused;
            if (TraceRoadBottom(Road, Center + Across * Distance, Unused))
            {
                Inside = Distance;
            }
            else
            {
                Outside = Distance;
            }
        }
        return Inside;
    }

    bool FitScaffoldToRoad(const AActor& Piece, FVector& OutTop, FVector& OutAcross, double& OutWidth)
    {
        TInlineComponentArray<UPrimitiveComponent*> Components(&Piece);
        static const FName LegMarkerName(TEXT("MiddleLeg"));
        UPrimitiveComponent* const* LegMarker = Components.FindByPredicate([](const UPrimitiveComponent* Part)
        {
            return Part->GetFName() == LegMarkerName;
        });
        const FVector Preferred = LegMarker ? (*LegMarker)->GetComponentLocation() : Piece.GetActorLocation();
        UPrimitiveComponent* Road = nullptr;
        double Closest = TNumericLimits<double>::Max();
        for (UPrimitiveComponent* Part : Components)
        {
            if (!Part->ComponentHasTag(UMWTrackRuntimeComponent::FixedRoadTag) || !Part->IsQueryCollisionEnabled())
            {
                continue;
            }
            FVector Bottom;
            // 직선·경사는 BP의 프리뷰 받침 위치를 사용한다. 곡선은 고정 도로에서 접점을 구한다.
            if (!(LegMarker && TraceRoadBottom(*Part, Preferred, Bottom))
                && !TraceRoadBottom(*Part, Part->Bounds.Origin, Bottom))
            {
                continue;
            }
            const double Distance = FVector::DistSquared2D(Preferred, Bottom);
            if (Distance < Closest)
            {
                Closest = Distance;
                Road = Part;
                OutTop = Bottom;
            }
        }
        if (!Road)
        {
            return false;
        }

        OutAcross = Piece.GetActorForwardVector().GetSafeNormal2D();
        if (LegMarker)
        {
            OutAcross = (*LegMarker)->GetForwardVector().GetSafeNormal2D();
        }
        else
        {
            FMWTrackPortFrames Ports;
            if (MWTrackSnap::ReadPorts(Piece, Ports))
            {
                // 포트 사이 진행 방향을 기준으로 골조의 긴 축이 곡선의 도로를 가로지르게 한다.
                const FVector Direction = Piece.GetActorTransform().TransformVectorNoScale(
                    Ports[1].GetLocation() - Ports[0].GetLocation());
                OutAcross = FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal2D();
            }
        }

        const double Left = RoadHalfWidth(*Road, OutTop, -OutAcross);
        const double Right = RoadHalfWidth(*Road, OutTop, OutAcross);
        OutWidth = 2.0 * FMath::Min(Left, Right) * RoadWidthFraction;
        return OutWidth > UE_SMALL_NUMBER;
    }

    bool VerifySupports()
    {
        return FParse::Param(FCommandLine::Get(), TEXT("MWVerifyTrackPlacement"));
    }
}

void UMWEnvironmentGameplay::RefreshTrackSupports()
{
    double FloorZ;
    const auto* Construction = State ? State->GetConstructionComponent() : nullptr;
    if (!Construction || TrackSupportMesh.IsNull() || !Generator->TryGetFloorZ(FloorZ))
    {
        return;
    }

    const FMWTrackPlacementSession& Session = Construction->GetPlacementSession();
    if (SupportSessionId != Session.SessionId)
    {
        ResetTrackSupports();
        SupportSessionId = Session.SessionId;
    }
    bObservedOpenPlacement |= Session.bIsOpen;

    for (const FMWPlacedTrack& Piece : Session.PlacedTracks)
    {
        if (!Piece.ConnectedTo.IsValid() || !IsValid(Piece.Actor) || ResolvedSupportPieces.Contains(Piece.PieceId))
        {
            continue;
        }

        FVector Top;
        FVector Across;
        double Width;
        if (!FitScaffoldToRoad(*Piece.Actor, Top, Across, Width))
        {
            continue;
        }
        const double Height = Top.Z - FloorZ;
        if (Height <= UE_SMALL_NUMBER)
        {
            ResolvedSupportPieces.Add(Piece.PieceId);
            continue;
        }

        if (!TrackSupports)
        {
            UStaticMesh* Mesh = TrackSupportMesh.LoadSynchronous();
            if (!Mesh)
            {
                return;
            }
            TrackSupports = NewObject<UInstancedStaticMeshComponent>(Generator, TEXT("InstalledTrackSupports"), RF_Transient);
            TrackSupports->SetupAttachment(Generator->GetRootComponent());
            TrackSupports->SetStaticMesh(Mesh);
            TrackSupports->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
            TrackSupports->SetGenerateOverlapEvents(false);
            TrackSupports->SetCanEverAffectNavigation(false);
            Generator->AddInstanceComponent(TrackSupports);
            TrackSupports->RegisterComponent();
        }

        const FVector MeshSize = TrackSupports->GetStaticMesh()->GetBoundingBox().GetSize();
        if (MeshSize.X <= UE_SMALL_NUMBER || MeshSize.Z <= UE_SMALL_NUMBER)
        {
            return;
        }

        const double ScaleXY = Width / MeshSize.X;
        // 층을 이어 높은 조각에서도 가로대 사이 간격과 사선 각도가 과하게 늘어나지 않게 한다.
        const int32 Layers = FMath::Clamp(FMath::RoundToInt(Height / (MeshSize.Z * ScaleXY)), 1, 16);
        const double Duration = bObservedOpenPlacement ? TrackSupportBuildSeconds : 0.0;
        PendingSupportBuilds.Add({Piece.PieceId, Top, Across.Rotation().Quaternion(), Height, ScaleXY,
            GetWorld()->GetTimeSeconds(), Duration, Layers});
        ResolvedSupportPieces.Add(Piece.PieceId);
        if (VerifySupports())
        {
            UE_LOG(LogTemp, Display, TEXT("[MWTrackSupports] begin world=%s session=%d piece=%d width=%.2f layers=%d floor=%.2f top=%s duration=%.3f"),
                *GetWorld()->GetName(), Session.SessionId, Piece.PieceId, Width, Layers, FloorZ, *Top.ToCompactString(), Duration);
        }
    }

    if (!PendingSupportBuilds.IsEmpty() && !GetWorld()->GetTimerManager().IsTimerActive(SupportBuildTimer))
    {
        TickTrackSupportBuilds();
        if (!PendingSupportBuilds.IsEmpty())
        {
            GetWorld()->GetTimerManager().SetTimer(SupportBuildTimer, this, &ThisClass::TickTrackSupportBuilds, 1.0f / 60.0f, true);
        }
    }
}

void UMWEnvironmentGameplay::TickTrackSupportBuilds()
{
    const double Now = GetWorld()->GetTimeSeconds();
    const FBox Bounds = TrackSupports->GetStaticMesh()->GetBoundingBox();
    for (FTrackSupportBuild& Build : PendingSupportBuilds)
    {
        const double Progress = Build.Duration > 0.0 ? FMath::Clamp((Now - Build.StartedAt) / Build.Duration, 0.0, 1.0) : 1.0;
        const double Visible = Build.Height * FMath::InterpEaseOut(0.0, 1.0, Progress, 2.0);
        const double HeightPerLayer = Build.Height / Build.Layers;
        while (Build.FinishedLayers < Build.Layers)
        {
            const double Depth = Build.FinishedLayers * HeightPerLayer;
            const double LayerHeight = Progress >= 1.0 ? HeightPerLayer : FMath::Min(Visible - Depth, HeightPerLayer);
            if (LayerHeight <= UE_SMALL_NUMBER)
            {
                break;
            }
            const double ScaleZ = LayerHeight / Bounds.GetSize().Z;
            const FVector Anchor = Build.Top - FVector(0, 0, Depth + Bounds.Max.Z * ScaleZ);
            const FTransform Transform(Build.Rotation, Anchor, FVector(Build.ScaleXY, Build.ScaleXY, ScaleZ));
            if (Build.ActiveInstance == INDEX_NONE)
            {
                Build.ActiveInstance = TrackSupports->AddInstance(Transform, true);
            }
            else
            {
                TrackSupports->UpdateInstanceTransform(Build.ActiveInstance, Transform, true, true, true);
            }
            if (LayerHeight < HeightPerLayer)
            {
                break;
            }
            ++Build.FinishedLayers;
            Build.ActiveInstance = INDEX_NONE;
        }
    }
    PendingSupportBuilds.RemoveAll([this, Now](const FTrackSupportBuild& Build)
    {
        const bool bComplete = Build.FinishedLayers == Build.Layers;
        if (bComplete && VerifySupports())
        {
            UE_LOG(LogTemp, Display, TEXT("[MWTrackSupports] complete world=%s session=%d piece=%d layers=%d elapsed=%.3f"),
                *GetWorld()->GetName(), SupportSessionId, Build.PieceId, Build.Layers, Now - Build.StartedAt);
        }
        return bComplete;
    });
    if (PendingSupportBuilds.IsEmpty())
    {
        GetWorld()->GetTimerManager().ClearTimer(SupportBuildTimer);
    }
}

void UMWEnvironmentGameplay::ResetTrackSupports()
{
    GetWorld()->GetTimerManager().ClearTimer(SupportBuildTimer);
    PendingSupportBuilds.Reset();
    ResolvedSupportPieces.Reset();
    SupportSessionId = INDEX_NONE;
    bObservedOpenPlacement = false;
    if (TrackSupports)
    {
        TrackSupports->ClearInstances();
    }
}
