#include "MWEnvironmentGameplay.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "Gameplay/Construction/MWTrackConstructionComponent.h"
#include "Gameplay/Construction/MWTrackSnap.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "System/GameState/MWGameplayGameState.h"
#include "TimerManager.h"

namespace
{
    constexpr double PeakTimeFraction = 0.35;

    bool VerifyPlacementPop()
    {
        return FParse::Param(FCommandLine::Get(), TEXT("MWVerifyTrackPlacement"));
    }
}

void UMWEnvironmentGameplay::RefreshTrackPlacementPops()
{
    const auto* Construction = State ? State->GetConstructionComponent() : nullptr;
    if (!Construction || TrackPlacementPopSeconds <= 0.0f || TrackPlacementPopScale <= 0.0f)
    {
        return;
    }

    const auto& Session = Construction->GetPlacementSession();
    if (PlacementPopSessionId != Session.SessionId)
    {
        ResetTrackPlacementPops();
        PlacementPopSessionId = Session.SessionId;
    }
    const bool bAnimateNewPieces = bSawOpenPlacementForPop;
    bSawOpenPlacementForPop |= Session.bIsOpen;

    const FGameplayTag CourseCaps = FGameplayTag::RequestGameplayTag(TEXT("Asset.Track.Course"), false);
    for (const FMWPlacedTrack& Piece : Session.PlacedTracks)
    {
        AActor* Track = Piece.Actor.Get();
        if (!IsValid(Track) || PresentedPopPieces.Contains(Piece.PieceId))
        {
            continue;
        }
        PresentedPopPieces.Add(Piece.PieceId);

        FMWTrackPortFrames Ports;
        if (!bAnimateNewPieces || !Piece.ConnectedTo.IsValid() || Piece.DefinitionAssetId.MatchesTag(CourseCaps)
            || Piece.ConnectedPortIndex < 0 || Piece.ConnectedPortIndex >= Ports.Num()
            || !MWTrackSnap::ReadPorts(*Track, Ports))
        {
            continue;
        }

        FTrackPlacementPop Pop;
        Pop.PieceId = Piece.PieceId;
        Pop.StartedAt = GetWorld()->GetTimeSeconds();
        // 접합한 포트를 중심으로 표시 복제본만 확대해 도로의 연결점과 충돌을 보존한다.
        Pop.ContactPoint = Piece.WorldTransform.TransformPosition(Ports[Piece.ConnectedPortIndex].GetLocation());
        const TInlineComponentArray<UStaticMeshComponent*> Meshes(Track);
        for (UStaticMeshComponent* Original : Meshes)
        {
            if (!Original->IsVisible() || Original->bHiddenInGame || !Original->GetStaticMesh())
            {
                continue;
            }

            auto* Visual = NewObject<UStaticMeshComponent>(Track, NAME_None, RF_Transient);
            Visual->SetMobility(EComponentMobility::Movable);
            Visual->SetStaticMesh(Original->GetStaticMesh());
            Visual->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
            Visual->SetGenerateOverlapEvents(false);
            Visual->SetCanEverAffectNavigation(false);
            Visual->SetCastShadow(Original->CastShadow);
            for (int32 Index = 0; Index < Original->GetNumMaterials(); ++Index)
            {
                Visual->SetMaterial(Index, Original->GetMaterial(Index));
            }
            const FTransform RestTransform = Original->GetComponentTransform();
            Visual->SetWorldTransform(RestTransform);
            Track->AddInstanceComponent(Visual);
            Visual->RegisterComponent();
            Original->SetVisibility(false);
            Pop.Surfaces.Add({Original, Visual, RestTransform});
        }
        if (!Pop.Surfaces.IsEmpty())
        {
            if (VerifyPlacementPop())
            {
                UE_LOG(LogTemp, Display, TEXT("MWPlacementPop begin world=%s piece=%d meshes=%d pivot=%s actor=%s"),
                    *GetWorld()->GetOutermost()->GetName(), Piece.PieceId, Pop.Surfaces.Num(),
                    *Pop.ContactPoint.ToCompactString(), *Track->GetActorTransform().ToHumanReadableString());
            }
            TrackPlacementPops.Add(MoveTemp(Pop));
        }
    }

    if (!TrackPlacementPops.IsEmpty() && !GetWorld()->GetTimerManager().IsTimerActive(PlacementPopTimer))
    {
        GetWorld()->GetTimerManager().SetTimer(PlacementPopTimer, this, &ThisClass::TickTrackPlacementPops, 1.0f / 60.0f, true);
    }
}

void UMWEnvironmentGameplay::TickTrackPlacementPops()
{
    const double Now = GetWorld()->GetTimeSeconds();
    TrackPlacementPops.RemoveAll([this, Now](FTrackPlacementPop& Pop)
    {
        const double Progress = FMath::Clamp((Now - Pop.StartedAt) / TrackPlacementPopSeconds, 0.0, 1.0);
        if (Progress >= 1.0)
        {
            RestoreTrackPopSurfaces(Pop);
            if (VerifyPlacementPop())
            {
                UE_LOG(LogTemp, Display, TEXT("MWPlacementPop end world=%s piece=%d elapsed=%.3f"),
                    *GetWorld()->GetOutermost()->GetName(), Pop.PieceId, Now - Pop.StartedAt);
            }
            return true;
        }

        const double Alpha = Progress < PeakTimeFraction
            ? FMath::InterpEaseOut(0.0, 1.0, Progress / PeakTimeFraction, 2.0)
            : FMath::InterpEaseInOut(1.0, 0.0, (Progress - PeakTimeFraction) / (1.0 - PeakTimeFraction), 2.0);
        const double Scale = 1.0 + TrackPlacementPopScale * Alpha;
        for (const FTrackPopSurface& Surface : Pop.Surfaces)
        {
            if (auto* Visual = Surface.Visual.Get())
            {
                FTransform Pose = Surface.RestTransform;
                Pose.SetLocation(Pop.ContactPoint + Scale * (Pose.GetLocation() - Pop.ContactPoint));
                Pose.MultiplyScale3D(FVector(Scale));
                Visual->SetWorldTransform(Pose);
            }
        }
        if (Progress >= PeakTimeFraction && !Pop.bLoggedPeak && VerifyPlacementPop())
        {
            Pop.bLoggedPeak = true;
            UE_LOG(LogTemp, Display, TEXT("MWPlacementPop turn world=%s piece=%d elapsed=%.3f scale=%.4f"),
                *GetWorld()->GetOutermost()->GetName(), Pop.PieceId, Now - Pop.StartedAt, Scale);
        }
        return false;
    });
    if (TrackPlacementPops.IsEmpty())
    {
        GetWorld()->GetTimerManager().ClearTimer(PlacementPopTimer);
    }
}

void UMWEnvironmentGameplay::RestoreTrackPopSurfaces(const FTrackPlacementPop& Pop) const
{
    for (const FTrackPopSurface& Surface : Pop.Surfaces)
    {
        if (auto* Original = Surface.Original.Get())
        {
            Original->SetVisibility(true);
        }
        if (auto* Visual = Surface.Visual.Get())
        {
            if (AActor* Actor = Visual->GetOwner())
            {
                Actor->RemoveInstanceComponent(Visual);
            }
            Visual->DestroyComponent();
        }
    }
}

void UMWEnvironmentGameplay::ResetTrackPlacementPops()
{
    GetWorld()->GetTimerManager().ClearTimer(PlacementPopTimer);
    for (const FTrackPlacementPop& Pop : TrackPlacementPops)
    {
        RestoreTrackPopSurfaces(Pop);
    }
    TrackPlacementPops.Reset();
    PresentedPopPieces.Reset();
    bSawOpenPlacementForPop = false;
    PlacementPopSessionId = INDEX_NONE;
}
