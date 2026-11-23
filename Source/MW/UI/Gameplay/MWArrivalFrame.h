#pragma once

#include "SceneViewExtension.h"
#include "RHIResources.h"

enum class EMWRoomPhase : uint8;
class FRHIGPUTextureReadback;

// 준비가 끝난 뷰포트와 카메라의 장면 패스에 GPU fence를 넣는다.
class FMWArrivalFrame : public FSceneViewExtensionBase
{
public:
    FMWArrivalFrame(const FAutoRegister& Register, FViewport* InViewport, AActor& Camera,
        EMWRoomPhase InPhase, int32 InSession, int32 InRevision);

    bool Matches(FViewport* InViewport, const AActor& Camera, EMWRoomPhase InPhase,
        int32 InSession, int32 InRevision) const;
    bool Complete() const;

    virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;
    virtual void PostRenderViewFamily_RenderThread(FRDGBuilder& Graph, FSceneViewFamily& Family) override;

private:
    bool IsSelectionLightingStable(FRDGBuilder& Graph, FSceneViewFamily& Family, const FSceneView& View);

    TSharedPtr<FRHIGPUTextureReadback, ESPMode::ThreadSafe> Sample;
    TArray<FColor> Reference;
    double StableSince = 0;
    int32 StableSamples = 0;
    int32 RenderedViews = 0;
    const FViewport* Viewport;
    const FSceneInterface* Scene;
    FSceneViewOwner ViewOwner;
    EMWRoomPhase Phase;
    int32 Session;
    int32 Revision;
    FGPUFenceRHIRef Fence;
    TSharedRef<TAtomic<bool>, ESPMode::ThreadSafe> Submitted = MakeShared<TAtomic<bool>, ESPMode::ThreadSafe>(false);
    TAtomic<bool> Issued{false};
};
