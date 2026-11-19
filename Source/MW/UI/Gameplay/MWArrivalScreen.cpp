#include "UI/Gameplay/MWArrivalScreen.h"
#include "UI/Gameplay/MWGameplaySign.h"
#include "UI/Gameplay/MWGameplayHUDSubsystem.h"
#include "Gameplay/Construction/MWTrackSelectionBoard.h"
#include "Gameplay/Construction/MWTrackPreview.h"
#include "Gameplay/Construction/MWTrackPlacementViewComponent.h"
#include "Gameplay/Construction/MWPlayerConstructionComponent.h"
#include "System/MWPlayerState.h"
#include "Gameplay/Construction/MWTrackConstructionComponent.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "Player/MWGameplayPlayerController.h"
#include "System/GameState/MWGameplayGameState.h"
#include "System/MWGameInstance.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "MaterialShared.h"
#include "SceneViewExtension.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RHIGPUReadback.h"
#include "ScreenPass.h"
#include "RHICommandList.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SWindow.h"
#include "GenericPlatform/GenericWindow.h"
#include "Styling/CoreStyle.h"
#include "Blueprint/UserWidget.h"

#if WITH_EDITOR
#include "FrameGrabber.h"
#include "ImageUtils.h"
#include "Misc/PackageName.h"
#include "Widgets/SViewport.h"

// 명시적으로 요청한 PIE 검증에서만 해당 창의 첫 공개 backbuffer와 같은 시야의 후속 프레임을 보관한다.
static void CaptureArrival(UGameViewportClient& Client, const FGuid& Attempt)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("MWCaptureArrival"))) return;
    const auto Widget = Client.GetGameViewportWidget();
    const auto Viewport = Widget ? StaticCastSharedPtr<FSceneViewport>(Widget->GetViewportInterface().Pin()) : nullptr;
    if (!Viewport || Viewport.Get() != Client.GetGameViewport()) return;
    const auto Grabber = MakeShared<FFrameGrabber>(Viewport.ToSharedRef(), Viewport->GetSizeXY());
    const FString WorldName = FPackageName::GetShortName(Client.GetWorld()->GetOutermost()->GetName());
    const FString Prefix = FPaths::ProjectSavedDir() / TEXT("Screenshots/Arrival") /
        FString::Printf(TEXT("%u_%s_%s"), FPlatformProcess::GetCurrentProcessId(),
            *WorldName, *Attempt.ToString());
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Prefix), true);
    Grabber->StartCapturingFrames();
    Grabber->CaptureThisFrame(nullptr);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
        [Grabber, Prefix, Start = FPlatformTime::Seconds(), Written = 0, Second = false](float) mutable
        {
            for (auto& Frame : Grabber->GetCapturedFrames())
            {
                const FString Path = Prefix + (Written++ == 0 ? TEXT("_first.png") : TEXT("_later.png"));
                for (FColor& Color : Frame.ColorBuffer) Color.A = 255;
                FImageUtils::SaveImageByExtension(*Path, FImageView(Frame.ColorBuffer.GetData(), Frame.BufferSize.X, Frame.BufferSize.Y));
            }
            const double Elapsed = FPlatformTime::Seconds() - Start;
            if (!Second && Elapsed >= 2.0)
            {
                Grabber->CaptureThisFrame(nullptr);
                Second = true;
            }
            if (Written < 2 && Elapsed < 10.) return true;
            Grabber->Shutdown();
            return false;
        }));
}
#endif

// 이 시도에서 준비 확인이 끝난 뷰포트·카메라의 실제 장면 패스 뒤에 GPU fence를 넣는다.
class FMWArrivalFrame : public FSceneViewExtensionBase
{
public:
    FMWArrivalFrame(const FAutoRegister& Register, FViewport* InViewport, AActor& Camera, EMWRoomPhase InPhase,
                    int32 InSession, int32 InRevision)
        : FSceneViewExtensionBase(Register), Viewport(InViewport), Scene(Camera.GetWorld()->Scene),
          ViewOwner(&Camera), Phase(InPhase), Session(InSession), Revision(InRevision),
          Fence(RHICreateGPUFence(TEXT("MWArrivalScene"))) {}

    bool Matches(FViewport* InViewport, const AActor& Camera, EMWRoomPhase InPhase, int32 InSession, int32 InRevision) const
    {
        return Viewport == InViewport && Scene == Camera.GetWorld()->Scene &&
               ViewOwner.ActorUniqueId == FSceneViewOwner(&Camera).ActorUniqueId &&
               Phase == InPhase && Session == InSession && Revision == InRevision;
    }
    bool Complete() const { return Submitted->Load() && Fence->NumPendingWriteCommands.GetValue() == 0 && Fence->Poll(); }

    virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override
    {
        return Context.Viewport == Viewport && !Issued.Load();
    }
    virtual void PostRenderViewFamily_RenderThread(FRDGBuilder& Graph, FSceneViewFamily& Family) override
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
private:
    // 고정 선택 카메라에서만 장면 색을 비교한다. 가림막이나 HUD는 이 렌더 타깃에 포함되지 않는다.
    bool IsSelectionLightingStable(FRDGBuilder& Graph, FSceneViewFamily& Family, const FSceneView& View)
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

TSharedRef<SWidget> UMWArrivalScreen::CreateLoadingContent()
{
    if (UClass* Class = LoadingWidgetClass.LoadSynchronous())
        if (UUserWidget* Widget = CreateWidget<UUserWidget>(GetGameInstance(), Class)) return Widget->TakeWidget();
    return MWGameplayUI::FitDesignCanvas(SNew(SOverlay) + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
        [SNew(SMWGameplaySign).Label(NSLOCTEXT("MWGameplayUI","Loading","불러오는 중")).AnimateLoading(true)]);
}

void UMWArrivalScreen::Begin(APlayerController& Controller, FName Destination)
{
    auto* Client = GetGameInstance()->GetGameViewportClient();
    if (!Client || !Controller.IsLocalController()) return;
    Cancel();
    Source = Controller.GetWorld();
    ExpectedMap = Destination;
    Viewport = Client;
    Attempt = FGuid::NewGuid();
    VisibleSeconds = 0;
    bFailed = false;
    Cover = SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
        .BorderBackgroundColor(FLinearColor::FromSRGBColor(FColor(92,218,243))).Padding(0)[CreateLoadingContent()];
    Client->AddViewportWidgetContent(Cover.ToSharedRef(), 9999);
    Controller.SetInputMode(FInputModeUIOnly());
    if (GEngine)
    {
        TravelFailure = GEngine->OnTravelFailure().AddWeakLambda(this,
            [this](UWorld* World, ETravelFailure::Type, const FString& Reason) { HandleConnectionFailure(World, Reason); });
        NetworkFailure = GEngine->OnNetworkFailure().AddWeakLambda(this,
            [this](UWorld* World, UNetDriver*, ENetworkFailure::Type, const FString& Reason) { HandleConnectionFailure(World, Reason); });
    }
    // 월드를 바꾸는 사이에도 가림을 유지할 수 있도록 월드 틱 대신 코어 틱에서 검사한다.
    Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::Tick));
    UE_LOG(LogTemp, Display, TEXT("MWArrival covered attempt=%s world=%s map=%s t=%.6f"),
        *Attempt.ToString(), *Source->GetName(), *ExpectedMap.ToString(), FPlatformTime::Seconds());
}

bool UMWArrivalScreen::MaterialsReady(UWorld& World, const TArray<UMaterialInterface*>& Materials, FString& Pending)
{
    if (!World.Scene) { Pending = TEXT("destination scene"); return false; }
    for (UMaterialInterface* Material : Materials)
    {
        const FMaterialResource* Resource = Material ? Material->GetMaterialResource(World.Scene->GetShaderPlatform()) : nullptr;
        if (!Resource || !Resource->GetGameThreadShaderMap() || !Resource->GetGameThreadShaderMap()->IsValidForRendering())
        {
            Pending = FString::Printf(TEXT("shader %s resource=%d map=%d"), *GetPathNameSafe(Material), Resource != nullptr, Resource && Resource->GetGameThreadShaderMap());
            return false;
        }
        TArray<UTexture*> Textures;
        Material->GetUsedTextures(Textures, GetCurrentMaterialQualityLevelChecked());
        for (UTexture* Texture : Textures)
        {
            UTexture2D* Surface = Cast<UTexture2D>(Texture);
            if (!Surface) continue;
            // 공개 직후까지 높은 밉을 잠시 유지해 텍스처가 낮은 해상도로 바뀌는 순간을 줄인다.
            if (Surface->IsStreamable())
                Surface->SetForceMipLevelsToBeResident(3.f);
            if (!Surface->GetResource() || Surface->HasPendingInitOrStreaming() ||
                (Surface->IsStreamable() && !Surface->IsFullyStreamedIn()))
            {
                Pending = FString::Printf(TEXT("texture %s resident=%d"), *Surface->GetPathName(), Surface->GetNumResidentMips());
                return false;
            }
        }
    }
    return true;
}

bool UMWArrivalScreen::Tick(float Delta)
{
    UWorld* World = GetGameInstance()->GetWorld();
    UGameViewportClient* Client = Viewport.Get();
    if (!Client || !Cover) return false;
    auto* PC = Cast<AMWGameplayPlayerController>(GetGameInstance()->GetFirstLocalPlayerController(World));
    const auto Window = Client->GetWindow();
    // 그릴 수 없는 동안의 시간은 준비 실패 제한에 포함하지 않는다. 창이 돌아오면 새 프레임을 기다린다.
    if (!Window || !Window->IsVisible() || (Window->GetNativeWindow() && Window->GetNativeWindow()->IsMinimized()))
    {
        Frame.Reset();
        return true;
    }
    if (World && World != Source.Get() && World->HasBegunPlay() && World->GetNetMode() == NM_Standalone &&
        CastChecked<UMWGameInstance>(GetGameInstance())->IsLobbyWorld(World))
    {
        Cancel();
        return false;
    }
    if (bFailed || World == Source.Get()) return true;
    VisibleSeconds += Delta;
    Pending.Reset();
    auto* State = World ? World->GetGameState<AMWGameplayGameState>() : nullptr;
    AActor* Camera = PC ? PC->GetViewTarget() : nullptr;
    int32 Session = 0, Revision = 0;
    TArray<UMaterialInterface*> Materials;
    EMWRoomPhase Phase = State ? State->GetRoomPhase() : EMWRoomPhase::Traveling;
    if (!World || !State || !PC || PC->GetWorld() != World || !PC->GetLocalPlayer() ||
        PC->GetLocalPlayer()->ViewportClient != Client || !Camera || !Client->Viewport ||
        !State->GetRoomSettings().MapThemeId.IsValid() ||
        FName(*UWorld::RemovePIEPrefix(World->GetOutermost()->GetName())) != ExpectedMap)
        Pending = TEXT("destination world/controller");
    else if (Phase == EMWRoomPhase::Failed)
    {
        Pending = TEXT("server ended gameplay");
        ShowFailure(false, NSLOCTEXT("MWArrival", "GameStopped", "게임이 중단되었습니다. 방을 나가 주세요."));
        return true;
    }
    else if (Phase == EMWRoomPhase::PreparingConstruction)
    {
        const auto& Selection = State->GetConstructionComponent()->GetSelectionSession();
        Session = Selection.SelectionSessionId;
        Revision = Selection.OffersRevision;
        auto* Board = Cast<AMWTrackSelectionBoard>(Camera);
        if (!Board || !PC->TrackSelectionWidget || !PC->TrackSelectionWidget->IsInViewport())
            Pending = TEXT("selection camera/widget");
        else
            Board->CollectArrivalMaterials(Selection, Materials, Pending);
    }
    else if (Phase == EMWRoomPhase::TrackPlacement)
    {
        const auto& Placement = State->GetConstructionComponent()->GetPlacementSession();
        Session = Placement.SessionId;
        Revision = Placement.CourseRevision;
        const auto* PS = PC->GetPlayerState<AMWPlayerState>();
        if (!Cast<AMWTrackSelectionBoard>(Camera) || !Session || !PC->TrackSelectionWidget ||
            !PC->TrackSelectionWidget->IsInViewport() || !PS)
            Pending = TEXT("placement camera/session");
        else
        {
            const auto Local = PS->GetPlayerConstructionComponent()->GetPlacement();
            const auto* View = PC->FindComponentByClass<UMWTrackPlacementViewComponent>();
            if (Local.SessionId != Session || (Placement.bIsOpen && !Local.bPlaced &&
                (!View || !MWTrackPreview::CollectVisibleMaterials(View->GetArrivalGhost(Session, PS->GetLobbyMemberId()), Materials))))
                Pending = FString::Printf(TEXT("placement preview %s"), *Local.DefinitionAssetId.ToString());
            for (const auto& Placed : Placement.PlacedTracks)
                if (!MWTrackPreview::CollectVisibleMaterials(Placed.Actor, Materials))
                    Pending = FString::Printf(TEXT("placed track %d"), Placed.PieceId);
        }
    }
    else if (Phase == EMWRoomPhase::PreparingRace && !PC->GetPawn())
    {
        if (!World->GetSubsystem<UMWGameplayHUDSubsystem>()->HasWaitingBoard(*PC))
            Pending = TEXT("waiting board");
    }
    else if (Phase == EMWRoomPhase::PreparingRace || Phase == EMWRoomPhase::Countdown || Phase == EMWRoomPhase::Racing)
    {
        if (Camera != PC->GetPawn() || !Cast<AMWVehiclePawn>(Camera)) Pending = TEXT("vehicle camera");
        else
        {
            TInlineComponentArray<UMeshComponent*> Meshes(Camera);
            for (UMeshComponent* Mesh : Meshes)
                if (Mesh->IsVisible())
                    for (auto* Material : Mesh->GetMaterials()) Materials.AddUnique(Material);
        }
    }
    else Pending = TEXT("gameplay phase");

    if (Pending.IsEmpty()) CollectEnvironment.Broadcast(*World, Materials, Pending);
    if (Pending.IsEmpty()) MaterialsReady(*World, Materials, Pending);
    // 준비가 풀리거나 카메라·단계·리비전이 바뀌면 이전 장면의 렌더 완료를 재사용할 수 없다.
    if (!Pending.IsEmpty()) Frame.Reset();
    else if (!Frame || !Frame->Matches(Client->Viewport, *Camera, Phase, Session, Revision))
    {
        Frame = FSceneViewExtensions::NewExtension<FMWArrivalFrame>(Client->Viewport, *Camera, Phase, Session, Revision);
        UE_LOG(LogTemp, Display, TEXT("MWArrival ready attempt=%s world=%s camera=%s phase=%d session=%d revision=%d materials=%d t=%.6f"),
            *Attempt.ToString(), *World->GetName(), *Camera->GetName(), int32(Phase), Session, Revision, Materials.Num(), FPlatformTime::Seconds());
    }
    else if (Frame->Complete())
    {
        UE_LOG(LogTemp, Display, TEXT("MWArrival reveal attempt=%s world=%s camera=%s phase=%d session=%d revision=%d t=%.6f"),
            *Attempt.ToString(), *World->GetName(), *Camera->GetName(), int32(Phase), Session, Revision, FPlatformTime::Seconds());
        Cancel();
        PC->RestoreArrivalInput();
#if WITH_EDITOR
        CaptureArrival(*Client, Attempt);
#endif
        return false;
    }
    if (Pending.IsEmpty()) Pending = TEXT("destination GPU frame");
    if (VisibleSeconds > 45.f) ShowFailure();
    return true;
}

void UMWArrivalScreen::HandleConnectionFailure(UWorld* World, const FString& Reason)
{
    if (!World || World->GetGameInstance() != GetGameInstance() || !Cover) return;
    Pending = FString::Printf(TEXT("travel/network: %s"), *Reason);
    ShowFailure(false);
}

void UMWArrivalScreen::ShowFailure(bool bCanRetry, const FText& Message)
{
    bFailed = true;
    Frame.Reset();
    UE_LOG(LogTemp, Warning, TEXT("MWArrival failed attempt=%s pending=%s"), *Attempt.ToString(), *Pending);
    TSharedPtr<SButton> InitialFocus;
    Cover->SetHAlign(HAlign_Fill);
    Cover->SetVAlign(VAlign_Fill);
    Cover->SetContent(MWGameplayUI::RecoveryPanel(Message.IsEmpty()
        ? NSLOCTEXT("MWArrival","Failed","첫 화면을 준비할 수 없습니다.") : Message,
        NSLOCTEXT("MWArrival","Retry","첫 화면 다시 준비"), FOnClicked::CreateLambda([Weak = TWeakObjectPtr<UMWArrivalScreen>(this)]()
        {
            if (auto* Self = Weak.Get())
            {
                // 재시도는 현재 경기에서 전시와 첫 프레임만 다시 준비한다. 로비 출발을 다시 보고하지 않는다.
                if (auto* PC = Self->GetGameInstance()->GetFirstLocalPlayerController())
                {
                    if (auto* Board = Cast<AMWTrackSelectionBoard>(PC->GetViewTarget())) Board->RetryArrivalOffers();
                    PC->bShowMouseCursor = false;
                    PC->SetInputMode(FInputModeUIOnly());
                }
                Self->Frame.Reset();
                Self->VisibleSeconds = 0;
                Self->bFailed = false;
                Self->Cover->SetHAlign(HAlign_Fill);
                Self->Cover->SetVAlign(VAlign_Fill);
                Self->Cover->SetContent(Self->CreateLoadingContent());
            }
            return FReply::Handled();
        }),
        NSLOCTEXT("MWArrival","Leave","방 나가기"), FOnClicked::CreateLambda([Weak = TWeakObjectPtr<UMWArrivalScreen>(this)]()
        {
            if (auto* Self = Weak.Get())
                if (auto* Game = Cast<UMWGameInstance>(Self->GetGameInstance()))
                {
                    Game->RequestLeaveRoom();
                }
            return FReply::Handled();
        }), bCanRetry, InitialFocus));
    if (APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController())
    {
        FInputModeUIOnly Mode;
        Mode.SetWidgetToFocus(InitialFocus);
        PC->SetInputMode(Mode);
        PC->bShowMouseCursor = true;
    }
}

void UMWArrivalScreen::Cancel()
{
    if (GEngine)
    {
        GEngine->OnTravelFailure().Remove(TravelFailure);
        GEngine->OnNetworkFailure().Remove(NetworkFailure);
    }
    TravelFailure.Reset();
    NetworkFailure.Reset();
    FTSTicker::RemoveTicker(Ticker);
    Ticker.Reset();
    Frame.Reset();
    if (Viewport.IsValid() && Cover) Viewport->RemoveViewportWidgetContent(Cover.ToSharedRef());
    Cover.Reset();
    Viewport.Reset();
    Source.Reset();
}

void UMWArrivalScreen::Deinitialize()
{
    Cancel();
    Super::Deinitialize();
}
