#include "MWEnvironmentGameplay.h"

#include "CollisionQueryParams.h"
#include "Components/MaterialBillboardComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Gameplay/Construction/MWTrackConstructionComponent.h"
#include "Gameplay/Construction/MWTrackSnap.h"
#include "Gameplay/Track/MWTrackRuntimeComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
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
    constexpr double TrackLaidSpriteLifeSeconds = 1.0;
    const FName ParticleAgeParameter(TEXT("Age"));
    const FName ParticleSeedParameter(TEXT("Seed"));

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

    TArray<FVector> SampleTrackSurfaces(const AActor& Piece, const FVector& Center, double Spacing)
    {
        constexpr int32 GridResolution = 9;
        constexpr int32 MaxEffectSites = 6;
        TArray<FVector> Candidates;
        TInlineComponentArray<UPrimitiveComponent*> Components(&Piece);
        for (UPrimitiveComponent* Road : Components)
        {
            if (!Road->ComponentHasTag(UMWTrackRuntimeComponent::FixedRoadTag) || !Road->IsQueryCollisionEnabled())
            {
                continue;
            }

            // 고정 노면과 접촉한 지점만 남긴다. 굽은 조각의 양 끝을 잇는 직선 위에는 도로가 없을 수 있다.
            const FBox Bounds = Road->Bounds.GetBox();
            for (int32 X = 0; X < GridResolution; ++X)
            {
                for (int32 Y = 0; Y < GridResolution; ++Y)
                {
                    const FVector Probe(
                        FMath::Lerp(Bounds.Min.X, Bounds.Max.X, (X + 0.5) / GridResolution),
                        FMath::Lerp(Bounds.Min.Y, Bounds.Max.Y, (Y + 0.5) / GridResolution),
                        Bounds.GetCenter().Z);
                    FVector Surface;
                    if (TraceRoad(*Road, Probe, Surface, /* bFromBelow */ false))
                    {
                        Candidates.Add(Surface);
                    }
                }
            }
        }

        TArray<FVector> Sites { Center };
        const double MinDistanceSquared = FMath::Square(Spacing);
        // 골조 접점에서 시작하고, 이미 선택한 모든 점에서 가장 먼 후보를 추가해 한쪽 끝으로 쏠리지 않게 한다.
        while (Sites.Num() < MaxEffectSites)
        {
            const FVector* Best = nullptr;
            double BestDistance = MinDistanceSquared;
            for (const FVector& Candidate : Candidates)
            {
                double Nearest = TNumericLimits<double>::Max();
                for (const FVector& Site : Sites)
                {
                    Nearest = FMath::Min(Nearest, FVector::DistSquared2D(Candidate, Site));
                }
                if (Nearest > BestDistance)
                {
                    Best = &Candidate;
                    BestDistance = Nearest;
                }
            }
            if (!Best)
            {
                break;
            }
            Sites.Add(*Best);
        }
        return Sites;
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
        // 재선택으로 세션이 바뀌어도 보존된 도로의 골조는 완성 상태로 복원한다.
        const bool bAnimateSupport = bObservedOpenPlacement && Piece.PieceId > Session.BaseCoursePieceCount;
        const double RevealDelay = bAnimateSupport ? FMath::Max(0.0f, TrackSupportRevealSeconds) : 0.0;
        const double AssemblyDuration = bAnimateSupport ? FMath::Max(0.0f, TrackSupportAssemblySeconds) : 0.0;
        const FRotator Rotation = Across.Rotation();
        PendingSupportBuilds.Add({Piece.PieceId, Top, Rotation.Quaternion(), Height, ScaleXY,
            StartedAt, RevealDelay, AssemblyDuration, Layers});
        ResolvedSupportPieces.Add(Piece.PieceId);

        if (bAnimateSupport)
        {
            PlayTrackLaidEffects(*Piece.Actor, Piece.PieceId, RoadSurface, Width, StartedAt);
        }
        if (VerifySupports())
        {
            UE_LOG(LogTemp, Display, TEXT("[MWTrackSupports] begin world=%s session=%d piece=%d width=%.2f layers=%d floor=%.2f top=%s delay=%.3f assembly=%.3f"),
                *GetWorld()->GetOutermost()->GetName(), Session.SessionId, Piece.PieceId,
                Width, Layers, FloorZ, *Top.ToCompactString(), RevealDelay, AssemblyDuration);
        }
    }

    if (!PendingSupportBuilds.IsEmpty() && !GetWorld()->GetTimerManager().IsTimerActive(SupportBuildTimer))
    {
        TickTrackSupportBuilds();
        if (!PendingSupportBuilds.IsEmpty() || !TrackLaidSprites.IsEmpty())
        {
            GetWorld()->GetTimerManager().SetTimer(SupportBuildTimer, this, &ThisClass::TickTrackSupportBuilds, 1.0f / 60.0f, true);
        }
    }
}

void UMWEnvironmentGameplay::PlayTrackLaidEffects(const AActor& Piece, int32 PieceId, const FVector& CenterSurface,
    double Width, double StartedAt)
{
    UMaterialInterface* Material = TrackLaidParticleMaterial.LoadSynchronous();
    if (!Material)
    {
        return;
    }

    const float Size = Width * FMath::Max(0.01f, TrackLaidParticleSizeFraction);
    const TArray<FVector> Sites = SampleTrackSurfaces(Piece, CenterSurface, FMath::Max(1.f, TrackLaidEffectSpacing));
    for (int32 Index = 0; Index < Sites.Num(); ++Index)
    {
        const FVector& Site = Sites[Index];
        auto* Sprite = NewObject<UMaterialBillboardComponent>(Generator, NAME_None, RF_Transient);
        Sprite->SetupAttachment(Generator->GetRootComponent());
        Sprite->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
        Sprite->SetCanEverAffectNavigation(false);
        auto* Instance = UMaterialInstanceDynamic::Create(Material, Sprite);
        Instance->SetScalarParameterValue(ParticleAgeParameter, 0.f);
        Instance->SetScalarParameterValue(ParticleSeedParameter, Index + 1.f);
        Sprite->AddElement(Instance, nullptr, false, Size, Size, nullptr);
        // 불티는 노면 위로 나오고 짙은 연기의 아래쪽은 골조 머리를 가릴 수 있다.
        Sprite->SetWorldLocation(Site);
        Generator->AddInstanceComponent(Sprite);
        Sprite->RegisterComponent();
        TrackLaidSprites.Add({Sprite, Instance, Site, Size * 0.18f, StartedAt});

        if (VerifySupports())
        {
            UE_LOG(LogTemp, Display, TEXT("[MWTrackSupports] site world=%s session=%d piece=%d index=%d/%d position=%s particle=1"),
                *GetWorld()->GetOutermost()->GetName(), SupportSessionId, PieceId, Index + 1, Sites.Num(), *Site.ToCompactString());
        }
    }
}

void UMWEnvironmentGameplay::TickTrackSupportBuilds()
{
    const double Now = GetWorld()->GetTimeSeconds();
    const FBox Bounds = ActiveTrackSupportMesh->GetBoundingBox();
    PendingSupportBuilds.RemoveAll([this, Now, &Bounds](FTrackSupportBuild& Build)
    {
        const double Elapsed = Now - Build.StartedAt;
        if (Elapsed < Build.RevealDelay)
        {
            return false;
        }

        const double HeightPerLayer = Build.Height / Build.Layers;
        const double ScaleZ = HeightPerLayer / Bounds.GetSize().Z;
        // 고정된 최종 위치에 위층부터 드러낸다. 프레임이 늦으면 경과한 층까지 따라간다.
        const double Progress = Build.AssemblyDuration > 0.0
            ? FMath::Clamp((Elapsed - Build.RevealDelay) / Build.AssemblyDuration, 0.0, 1.0) : 1.0;
        const int32 LayerCount = 1 + FMath::FloorToInt((Build.Layers - 1) * Progress);
        for (; Build.RevealedLayers < LayerCount; ++Build.RevealedLayers)
        {
            const FVector Anchor = Build.Top - FVector(0, 0, Build.RevealedLayers * HeightPerLayer + Bounds.Max.Z * ScaleZ);
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
            if (VerifySupports())
            {
                UE_LOG(LogTemp, Display, TEXT("[MWTrackSupports] layer world=%s session=%d piece=%d layer=%d/%d elapsed=%.3f bottom=%.2f"),
                    *GetWorld()->GetOutermost()->GetName(), SupportSessionId, Build.PieceId, Build.RevealedLayers + 1,
                    Build.Layers, Elapsed, Anchor.Z + Bounds.Min.Z * ScaleZ);
            }
        }
        if (Build.RevealedLayers < Build.Layers)
        {
            return false;
        }
        if (VerifySupports())
        {
            UE_LOG(LogTemp, Display, TEXT("[MWTrackSupports] complete world=%s session=%d piece=%d layers=%d elapsed=%.3f"),
                *GetWorld()->GetOutermost()->GetName(), SupportSessionId, Build.PieceId, Build.Layers, Elapsed);
        }
        return true;
    });
    TrackLaidSprites.RemoveAll([Now](const FTrackLaidSprite& Particle)
    {
        UMaterialBillboardComponent* Sprite = Particle.Component.Get();
        if (!Sprite)
        {
            return true;
        }
        const float Age = Now - Particle.StartedAt;
        if (Age >= TrackLaidSpriteLifeSeconds)
        {
            Sprite->DestroyComponent();
            return true;
        }
        // 섬광은 노면에서 시작하고, 연기가 퍼질 때는 노면의 깊이 판정에 잘리지 않게 위로 이동한다.
        const double Rise = Particle.RiseHeight * FMath::SmoothStep(0.08f, 0.45f, Age);
        Sprite->SetWorldLocation(Particle.Surface + FVector::UpVector * Rise);
        if (UMaterialInstanceDynamic* Material = Particle.Material.Get())
        {
            Material->SetScalarParameterValue(ParticleAgeParameter, Age);
        }
        return false;
    });
    if (PendingSupportBuilds.IsEmpty() && TrackLaidSprites.IsEmpty())
    {
        GetWorld()->GetTimerManager().ClearTimer(SupportBuildTimer);
    }
}

void UMWEnvironmentGameplay::ResetTrackSupports()
{
    GetWorld()->GetTimerManager().ClearTimer(SupportBuildTimer);
    for (const FTrackLaidSprite& Particle : TrackLaidSprites)
    {
        if (UMaterialBillboardComponent* Component = Particle.Component.Get())
        {
            Component->DestroyComponent();
        }
    }
    TrackLaidSprites.Reset();
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
