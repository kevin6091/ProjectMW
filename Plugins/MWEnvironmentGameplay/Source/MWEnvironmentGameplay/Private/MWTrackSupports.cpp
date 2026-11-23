#include "MWEnvironmentGameplay.h"

#include "CollisionQueryParams.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Gameplay/Construction/MWTrackConstructionComponent.h"
#include "Gameplay/Construction/MWTrackSnap.h"
#include "Gameplay/Track/MWTrackRuntimeComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "MWEnvironmentRestore.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "System/GameState/MWGameplayGameState.h"
#include "TimerManager.h"

namespace
{
    // 와이어프레임 받침과 비슷하게 양쪽 도로 가장자리보다 조금 안쪽에 세운다.
    constexpr double RoadWidthFraction = 0.85;
    constexpr int32 WidthMeasurements = 7;

    bool TraceRoad(UPrimitiveComponent& Road, const FVector& At, FVector& Out, bool bFromBelow = true)
    {
        const FBoxSphereBounds& Bounds = Road.Bounds;
        const FVector Bottom(At.X, At.Y, Bounds.Origin.Z - Bounds.BoxExtent.Z - 1.0);
        const FVector Top(At.X, At.Y, Bounds.Origin.Z + Bounds.BoxExtent.Z + 1.0);
        FHitResult Hit;
        if (!Road.LineTraceComponent(Hit, bFromBelow ? Bottom : Top, bFromBelow ? Top : Bottom, FCollisionQueryParams()))
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
            if (TraceRoad(Road, Center + Across * Distance, Unused))
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

    bool FitScaffoldToRoad(const AActor& Piece, FVector& OutTop, FVector& OutAcross, double& OutWidth,
        FVector& OutRoadSurface)
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
            if (!(LegMarker && TraceRoad(*Part, Preferred, Bottom))
                && !TraceRoad(*Part, Part->Bounds.Origin, Bottom))
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
        OutRoadSurface = OutTop;
        TraceRoad(*Road, OutTop, OutRoadSurface, /* bFromBelow */ false);

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
        if (const UStaticMeshComponent* Marker = LegMarker ? Cast<UStaticMeshComponent>(*LegMarker) : nullptr)
        {
            if (const UStaticMesh* Mesh = Marker->GetStaticMesh())
            {
                // BP의 받침 폭이 있으면 그 폭을 우선하되 도로 아래에 들어오는 범위만 사용한다.
                const double AuthoredWidth = Mesh->GetBoundingBox().GetSize().X *
                    FMath::Abs(Marker->GetComponentScale().X);
                OutWidth = FMath::Min(OutWidth, AuthoredWidth);
            }
        }
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
        FVector RoadSurface;
        double Width;
        if (!FitScaffoldToRoad(*Piece.Actor, Top, Across, Width, RoadSurface))
        {
            continue;
        }
        const double Height = Top.Z - FloorZ;
        if (Height <= UE_SMALL_NUMBER)
        {
            ResolvedSupportPieces.Add(Piece.PieceId);
            continue;
        }

        if (!ActiveTrackSupportMesh)
        {
            ActiveTrackSupportMesh = TrackSupportMesh.LoadSynchronous();
            if (!ActiveTrackSupportMesh)
            {
                return;
            }
        }

        const FVector MeshSize = ActiveTrackSupportMesh->GetBoundingBox().GetSize();
        if (MeshSize.X <= UE_SMALL_NUMBER || MeshSize.Z <= UE_SMALL_NUMBER)
        {
            return;
        }

        const double ScaleXY = Width / MeshSize.X;
        // 층을 이어 높은 조각에서도 가로대 사이 간격과 사선 각도가 과하게 늘어나지 않게 한다.
        const int32 Layers = FMath::Clamp(FMath::RoundToInt(Height / (MeshSize.Z * ScaleXY)), 1, 16);
        const double StartedAt = GetWorld()->GetTimeSeconds();
        const double Duration = bObservedOpenPlacement ? FMath::Max(0.0f, TrackSupportRevealSeconds) : 0.0;
        const FRotator Rotation = Across.Rotation();
        PendingSupportBuilds.Add({Piece.PieceId, Top, Rotation.Quaternion(), Height, ScaleXY, StartedAt, Duration, Layers});
        ResolvedSupportPieces.Add(Piece.PieceId);

        // 도로가 설치되면 섬광을 먼저 재생하고, 연기 사이로 완성된 골조를 내보낸다.
        UNiagaraComponent* Burst = nullptr;
        if (bObservedOpenPlacement && !TrackLaidEffect.IsNull())
        {
            if (UNiagaraSystem* Effect = TrackLaidEffect.LoadSynchronous())
            {
                // 골조 위쪽의 실제 노면에서 재생해 경사에서도 섬광이 도로에 묻히지 않게 한다.
                Burst = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Effect, RoadSurface, Rotation, FVector(ScaleXY));
                if (Burst)
                {
                    ActiveTrackLaidEffects.RemoveAll([](const TWeakObjectPtr<UNiagaraComponent>& Item) { return !Item.IsValid(); });
                    ActiveTrackLaidEffects.Add(Burst);
                }
            }
        }
        if (VerifySupports())
        {
            UE_LOG(LogTemp, Display, TEXT("[MWTrackSupports] begin world=%s session=%d piece=%d width=%.2f layers=%d floor=%.2f top=%s delay=%.3f burst=%d"),
                *GetWorld()->GetOutermost()->GetName(), Session.SessionId, Piece.PieceId,
                Width, Layers, FloorZ, *Top.ToCompactString(), Duration, Burst != nullptr);
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
    const FBox Bounds = ActiveTrackSupportMesh->GetBoundingBox();
    PendingSupportBuilds.RemoveAll([this, Now, &Bounds](const FTrackSupportBuild& Build)
    {
        const double Elapsed = Now - Build.StartedAt;
        if (Elapsed < Build.Duration)
        {
            return false;
        }

        const double HeightPerLayer = Build.Height / Build.Layers;
        const double ScaleZ = HeightPerLayer / Bounds.GetSize().Z;
        for (int32 Layer = 0; Layer < Build.Layers; ++Layer)
        {
            const FVector Anchor = Build.Top - FVector(0, 0, Layer * HeightPerLayer + Bounds.Max.Z * ScaleZ);
            const FTransform Transform(Build.Rotation, Anchor, FVector(Build.ScaleXY, Build.ScaleXY, ScaleZ));
            // 공유 재질에 인스턴싱 사용 플래그가 없어도 일반 메시 렌더링 경로에서 원래 재질을 쓴다.
            auto* Section = NewObject<UStaticMeshComponent>(Generator, NAME_None, RF_Transient);
            Section->SetupAttachment(Generator->GetRootComponent());
            Section->SetStaticMesh(ActiveTrackSupportMesh);
            Section->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
            Section->SetGenerateOverlapEvents(false);
            Section->SetCanEverAffectNavigation(false);
            Section->ComponentTags.Add(TEXT("MWInstalledTrackSupport"));
            Section->SetWorldTransform(Transform);
            Generator->AddInstanceComponent(Section);
            Section->RegisterComponent();
            TrackSupportComponents.Add(Section);
        }
        if (VerifySupports())
        {
            UE_LOG(LogTemp, Display, TEXT("[MWTrackSupports] complete world=%s session=%d piece=%d layers=%d elapsed=%.3f"),
                *GetWorld()->GetOutermost()->GetName(), SupportSessionId, Build.PieceId, Build.Layers, Elapsed);
        }
        return true;
    });
    if (PendingSupportBuilds.IsEmpty())
    {
        GetWorld()->GetTimerManager().ClearTimer(SupportBuildTimer);
    }
}

void UMWEnvironmentGameplay::ResetTrackSupports()
{
    GetWorld()->GetTimerManager().ClearTimer(SupportBuildTimer);
    for (const TWeakObjectPtr<UNiagaraComponent>& Effect : ActiveTrackLaidEffects)
    {
        if (UNiagaraComponent* Component = Effect.Get())
        {
            Component->DestroyComponent();
        }
    }
    ActiveTrackLaidEffects.Reset();
    PendingSupportBuilds.Reset();
    ResolvedSupportPieces.Reset();
    SupportSessionId = INDEX_NONE;
    bObservedOpenPlacement = false;
    for (UStaticMeshComponent* Section : TrackSupportComponents)
    {
        Section->DestroyComponent();
    }
    TrackSupportComponents.Reset();
    ActiveTrackSupportMesh = nullptr;
}
