#include "UI/Gameplay/MWArrivalFrame.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RHIGPUReadback.h"
#include "RHICommandList.h"
#include "ScreenPass.h"
#include "System/MWRoomTypes.h"

FMWArrivalFrame::FMWArrivalFrame(const FAutoRegister& Register, FViewport* InViewport, AActor& Camera, EMWRoomPhase InPhase,
                int32 InSession, int32 InRevision)
    : FSceneViewExtensionBase(Register), Viewport(InViewport), Scene(Camera.GetWorld()->Scene),
      ViewOwner(&Camera), Phase(InPhase), Session(InSession), Revision(InRevision),
      Fence(RHICreateGPUFence(TEXT("MWArrivalScene")))
{
}

bool FMWArrivalFrame::Matches(FViewport* InViewport, const AActor& Camera, EMWRoomPhase InPhase, int32 InSession, int32 InRevision) const
{
    return Viewport == InViewport && Scene == Camera.GetWorld()->Scene &&
           ViewOwner.ActorUniqueId == FSceneViewOwner(&Camera).ActorUniqueId &&
           Phase == InPhase && Session == InSession && Revision == InRevision;
}

bool FMWArrivalFrame::Complete() const
{
    return Submitted->Load() && Fence->NumPendingWriteCommands.GetValue() == 0 && Fence->Poll();
}

bool FMWArrivalFrame::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
    return Context.Viewport == Viewport && !Issued.Load();
}

void FMWArrivalFrame::PostRenderViewFamily_RenderThread(FRDGBuilder& Graph, FSceneViewFamily& Family)
{
    if (Family.RenderTarget != Viewport || Family.Scene != Scene || Issued.Load()) return;
    for (const FSceneView* View : Family.Views)
    {
        if (!View->bIsSceneCapture && View->ViewActor.ActorUniqueId == ViewOwner.ActorUniqueId)
        {
            if (Phase == EMWRoomPhase::PreparingConstruction && !IsSelectionLightingStable(Graph, Family, *View)) continue;
            Graph.AddPass(RDG_EVENT_NAME("MWArrivalSceneComplete"), ERDGPassFlags::NeverCull,
                [Signal = Fence, Done = Submitted](FRHICommandListImmediate& Commands)
                {
                    Commands.WriteGPUFence(Signal);
                    Done->Store(true);
                });
            Issued.Store(true);
            return;
        }
    }
}

// 고정 선택 카메라에서만 장면 색을 비교한다. 가림막이나 HUD는 이 렌더 타깃에 포함되지 않는다.
bool FMWArrivalFrame::IsSelectionLightingStable(FRDGBuilder& Graph, FSceneViewFamily& Family, const FSceneView& View)
{
    constexpr int32 Width = 24, Height = 16;
    // UE 5.8의 실시간 하늘 캡처는 하늘·구름의 여섯 면과 열 개의 후처리 단계로 나뉜다.
    // 직전 미완성 캡처까지 교체된 다음 이 뷰의 실제 색 수렴을 검사한다.
    static const auto* Slicing = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SkyLight.RealTimeReflectionCapture.TimeSlice"));
    static const auto* Faces = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SkyLight.RealTimeReflectionCapture.TimeSlice.SkyCloudCubeFacePerFrame"));
    const int32 FacesPerFrame = FMath::Clamp(Faces ? Faces->GetValueOnRenderThread() : 2, 1, 6);
    const int32 CycleFrames = 10 + 2 * FMath::DivideAndRoundUp(6, FacesPerFrame);
    if (Slicing && Slicing->GetValueOnRenderThread() && ++RenderedViews < 2 * CycleFrames) return false;
    if (Sample)
    {
        if (!Sample->IsReady()) return false;
        int32 Pitch = 0;
        auto* Pixels = static_cast<const FColor*>(Sample->Lock(Pitch));
        if (!Pixels) return false;
        TArray<FColor> Current;
        for (int32 Y = 0; Y < Height; ++Y) Current.Append(Pixels + Y * Pitch, Width);
        Sample->Unlock();
        Sample.Reset();

        int32 Difference = 0;
        if (Reference.Num() == Current.Num())
        {
            for (int32 Index = 0; Index < Current.Num(); ++Index)
            {
                const FColor& A = Current[Index];
                const FColor& B = Reference[Index];
                Difference += FMath::Abs(int32(A.R)-B.R) + FMath::Abs(int32(A.G)-B.G) + FMath::Abs(int32(A.B)-B.B);
            }
        }
        if (Reference.IsEmpty() || Difference > 3 * Width * Height * 3)
        {
            Reference = MoveTemp(Current);
            StableSince = FPlatformTime::Seconds();
            StableSamples = 0;
        }
        else if (++StableSamples >= 4 && FPlatformTime::Seconds() - StableSince >= .45)
            return true;
    }

    FRDGTextureRef SceneColor = Family.RenderTarget->GetRenderTargetTexture(Graph);
    if (!SceneColor) return false;
    const FIntPoint Size(Width, Height);
    auto Desc = FRDGTextureDesc::Create2D(Size, PF_B8G8R8A8, FClearValueBinding::None, TexCreate_RenderTargetable | TexCreate_ShaderResource);
    auto Small = Graph.CreateTexture(Desc, TEXT("MWArrival.SceneSample"));
    AddDrawTexturePass(Graph, FScreenPassViewInfo(View), SceneColor, Small,
        View.UnscaledViewRect.Min, View.UnscaledViewRect.Size(), FIntPoint::ZeroValue, Size, TStaticSamplerState<SF_Bilinear>::GetRHI());
    Sample = MakeShared<FRHIGPUTextureReadback>(TEXT("MWArrival.SceneReadback"));
    AddEnqueueCopyPass(Graph, Sample.Get(), Small);
    // Graph가 실행되기 전에 이동 시도가 무효화되어도 readback을 유지한다.
    Graph.AddPass(RDG_EVENT_NAME("MWArrival.KeepSample"), ERDGPassFlags::NeverCull,
        [Keep = Sample](FRHICommandListImmediate&) {});
    return false;
}
