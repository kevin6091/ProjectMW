#include "Gameplay/Track/MWTrackVisualLibrary.h"

#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Gameplay/Track/MWTrackRuntimeSubsystem.h"

namespace
{
	const FName TrackNormalTag(TEXT("MWTrackNormal"));
	const FName TrackWireframeTag(TEXT("MWTrackWireframe"));
	const FName TrackSourceDisabledTag(TEXT("MWTrackSourceDisabled"));

	void SetTrackMeshVisible(UStaticMeshComponent& Mesh, bool bVisible)
	{
		Mesh.SetVisibility(bVisible);
		Mesh.SetHiddenInGame(!bVisible);
	}
}

bool UMWTrackVisualLibrary::SetTrackWireframePreview(AActor* TrackVisual, bool bEnableWireframe)
{
	if (!IsValid(TrackVisual))
	{
		return false;
	}

	TInlineComponentArray<UStaticMeshComponent*> Meshes(TrackVisual);
	const bool bHasWireframe = Meshes.ContainsByPredicate([](const UStaticMeshComponent* Mesh)
	{
		return Mesh->ComponentHasTag(TrackWireframeTag) && Mesh->GetStaticMesh()
			&& !Mesh->ComponentHasTag(TrackSourceDisabledTag);
	});
	const FName ActiveTag = bEnableWireframe && bHasWireframe ? TrackWireframeTag : TrackNormalTag;

	TInlineComponentArray<UStaticMeshComponent*> MeshesToShow;
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		const bool bIsNormal = Mesh->ComponentHasTag(TrackNormalTag);
		const bool bIsWireframe = Mesh->ComponentHasTag(TrackWireframeTag);
		if (!bIsNormal && !bIsWireframe)
		{
			continue;
		}

		if (Mesh->ComponentHasTag(ActiveTag) && Mesh->GetStaticMesh()
			&& !Mesh->ComponentHasTag(TrackSourceDisabledTag))
		{
			MeshesToShow.Add(Mesh);
		}
		else
		{
			SetTrackMeshVisible(*Mesh, false);
		}
	}

	// Hide the inactive group before showing the active group to avoid coplanar overlap.
	for (UStaticMeshComponent* Mesh : MeshesToShow)
	{
		SetTrackMeshVisible(*Mesh, true);
	}

	if (UWorld* World = TrackVisual->GetWorld())
	{
		if (UMWTrackRuntimeSubsystem* Runtime = World->GetSubsystem<UMWTrackRuntimeSubsystem>())
		{
			Runtime->SetTrackPreview(*TrackVisual, bEnableWireframe);
		}
	}
	return bEnableWireframe ? bHasWireframe : !MeshesToShow.IsEmpty();
}
