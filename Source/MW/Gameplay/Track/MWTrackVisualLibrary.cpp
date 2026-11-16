// 복구 트랙의 기존 표시 그룹 전환을 재사용하는 도우미다. 설치 규칙은 변경하지 않는다.

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

// 기존 외형 그룹의 표시만 바꾸고 설치 승인이나 트랙 정의를 수정하지 않는다.
bool UMWTrackVisualLibrary::SetTrackWireframePreview(AActor* TrackVisual, bool bEnableWireframe)
{
    if (!IsValid(TrackVisual))
    {
        return false;
    }

    TInlineComponentArray<UStaticMeshComponent*> Meshes(TrackVisual);
    const bool bHasWireframe = Meshes.ContainsByPredicate(
        [](const UStaticMeshComponent* Mesh)
        {
            return Mesh->ComponentHasTag(TrackWireframeTag) && Mesh->GetStaticMesh() &&
                   !Mesh->ComponentHasTag(TrackSourceDisabledTag);
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

        if (Mesh->ComponentHasTag(ActiveTag) && Mesh->GetStaticMesh() && !Mesh->ComponentHasTag(TrackSourceDisabledTag))
        {
            MeshesToShow.Add(Mesh);
        }
        else
        {
            SetTrackMeshVisible(*Mesh, false);
        }
    }

    // 겹치는 면이 동시에 보이지 않도록 비활성 그룹을 먼저 숨긴 뒤 활성 그룹을 표시한다.
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
