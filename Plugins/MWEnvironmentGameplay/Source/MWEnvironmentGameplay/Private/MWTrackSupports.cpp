#include "MWEnvironmentGameplay.h"

#include "CollisionQueryParams.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Gameplay/Construction/MWTrackConstructionComponent.h"
#include "Gameplay/Track/MWTrackRuntimeComponent.h"
#include "MWEnvironmentRestore.h"
#include "System/GameState/MWGameplayGameState.h"

namespace
{
    // 도로 중앙에서 아래로 훑으면서 고정 도로 형상에 닿는 지점을 찾는다.
    bool FindSupportTop(const AActor& Piece, FVector& OutTop)
    {
        TInlineComponentArray<UPrimitiveComponent*> Components(&Piece);
        const FVector TrackCenter = Piece.GetActorLocation();
        double BestDistance = TNumericLimits<double>::Max();
        for (UPrimitiveComponent* Component : Components)
        {
            if (!Component->ComponentHasTag(UMWTrackRuntimeComponent::FixedRoadTag) || !Component->IsQueryCollisionEnabled())
            {
                continue;
            }

            // 곡선의 액터 원점은 도로 밖에 있을 수 있으므로 움직이지 않는 충돌 형상의 중심을 사용한다.
            const FBoxSphereBounds& Bounds = Component->Bounds;
            const double Distance = FVector::DistSquared2D(TrackCenter, Bounds.Origin);
            if (Distance >= BestDistance)
            {
                continue;
            }

            FHitResult Hit;
            const FVector Vertical(0, 0, Bounds.BoxExtent.Z + 1.0);
            if (Component->LineTraceComponent(Hit, Bounds.Origin - Vertical, Bounds.Origin + Vertical, FCollisionQueryParams()))
            {
                BestDistance = Distance;
                OutTop = Hit.ImpactPoint;
            }
        }
        return BestDistance < TNumericLimits<double>::Max();
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
        SupportSessionId = Session.SessionId;
        ResolvedSupportPieces.Reset();
        if (TrackSupports)
        {
            TrackSupports->ClearInstances();
        }
    }

    for (const FMWPlacedTrack& Piece : Session.PlacedTracks)
    {
        // 시작 조각과 고스트는 대상이 아니다. 액터/충돌이 늦게 준비되면 다음 상태 점검에서 읽는다.
        if (!Piece.ConnectedTo.IsValid() || !IsValid(Piece.Actor) || ResolvedSupportPieces.Contains(Piece.PieceId))
        {
            continue;
        }

        FVector Top;
        if (!FindSupportTop(*Piece.Actor, Top))
        {
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

        const FBox MeshBox = TrackSupports->GetStaticMesh()->GetBoundingBox();
        const double MeshHeight = MeshBox.Max.Z - MeshBox.Min.Z;
        const double Height = Top.Z - FloorZ;
        if (MeshHeight > UE_SMALL_NUMBER && Height > UE_SMALL_NUMBER)
        {
            const double ScaleZ = Height / MeshHeight;
            const FVector Base(Top.X, Top.Y, FloorZ - MeshBox.Min.Z * ScaleZ);
            const FRotator Rotation(0, Piece.WorldTransform.Rotator().Yaw, 0);
            TrackSupports->AddInstance(FTransform(Rotation, Base, FVector(1, 1, ScaleZ)), true);
            UE_LOG(LogTemp, Verbose, TEXT("[MWTrackSupports] World=%s Session=%d Piece=%d FloorZ=%.1f Top=%s"),
                   *GetWorld()->GetName(), Session.SessionId, Piece.PieceId, FloorZ, *Top.ToCompactString());
        }
        ResolvedSupportPieces.Add(Piece.PieceId);
    }
}
