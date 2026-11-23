#include "UI/Gameplay/MWArrivalScreen.h"
#include "UI/Gameplay/MWArrivalFrame.h"
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
#include "Streaming/StreamableRenderResourceState.h"
#include "Engine/World.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "MaterialShared.h"
#include "SceneInterface.h"
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
    MapTravelStartedAt = FPlatformTime::Seconds();
    Viewport = Client;
    Attempt = FGuid::NewGuid();
    VisibleSeconds = 0;
    QualityWaitStartedAt.Reset();
    Failure = EFailureState::None;
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

void UMWArrivalScreen::CheckMaterials(UWorld& World, const TArray<UMaterialInterface*>& Materials,
    FString& Pending, FString& QualityPending)
{
    if (!World.Scene)
    {
        Pending = TEXT("destination scene");
        return;
    }

    TSet<const UTexture2D*> CheckedTextures;
    TArray<UTexture*> Textures;
    for (UMaterialInterface* Material : Materials)
    {
        const FMaterialResource* Resource = Material ? Material->GetMaterialResource(World.Scene->GetShaderPlatform()) : nullptr;
        if (!Resource || !Resource->GetGameThreadShaderMap() || !Resource->GetGameThreadShaderMap()->IsValidForRendering())
        {
            Pending = FString::Printf(TEXT("shader %s resource=%d map=%d"), *GetPathNameSafe(Material), Resource != nullptr, Resource && Resource->GetGameThreadShaderMap());
            return;
        }

        Textures.Reset();
        Material->GetUsedTextures(Textures, GetCurrentMaterialQualityLevelChecked());
        for (UTexture* Texture : Textures)
        {
            const UTexture2D* Surface = Cast<UTexture2D>(Texture);
            if (!Surface || CheckedTextures.Contains(Surface)) continue;
            CheckedTextures.Add(Surface);
            if (!Surface->GetResource() || Surface->HasPendingRenderResourceInitialization()
#if WITH_EDITOR
                || Surface->IsDefaultTexture()
#endif
            )
            {
                Pending = FString::Printf(TEXT("texture resource %s"), *Surface->GetPathName());
                return;
            }

            // 가상 텍스처의 타일 상주 상태는 일반 텍스처의 밉 개수와 다르다. 실제 장면 프레임에서 확인한다.
            const auto& State = Surface->GetStreamableResourceState();
            if (Surface->IsCurrentlyVirtualTextured() || !State.IsValid() || !State.bSupportsStreaming) continue;
            if (State.NumResidentLODs < State.NumNonStreamingLODs)
            {
                Pending = FString::Printf(TEXT("texture base %s resident=%d required=%d"), *Surface->GetPathName(),
                    State.NumResidentLODs, State.NumNonStreamingLODs);
                return;
            }

            // 선택 밉은 설치되지 않을 수 있다. 설치된 밉을 기다리는 동안에도 엔진의 스트리밍 예산을 따른다.
            if (QualityPending.IsEmpty() && State.NumResidentLODs < State.NumNonOptionalLODs)
            {
                QualityPending = FString::Printf(TEXT("texture quality %s resident=%d installed=%d"), *Surface->GetPathName(),
                    State.NumResidentLODs, State.NumNonOptionalLODs);
            }
        }
    }
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
    if (Failure == EFailureState::NeedsUserAction) return true;

    const FName WorldPackage = World ? FName(*UWorld::RemovePIEPrefix(World->GetOutermost()->GetName())) : NAME_None;
    if (MapTravelStartedAt.IsSet())
    {
        if (World && World != Source.Get() && !ExpectedMap.IsNone() && WorldPackage == ExpectedMap)
        {
            MapTravelStartedAt.Reset();
            // 로컬 시간 초과 뒤에도 같은 목적지가 열리면 기존 첫 화면 준비 경로를 다시 시작한다.
            if (Failure == EFailureState::MapTravelTimeout)
            {
                UE_LOG(LogTemp, Display, TEXT("MWArrival resumed late travel attempt=%s world=%s t=%.6f"),
                    *Attempt.ToString(), *WorldPackage.ToString(), FPlatformTime::Seconds());
                RestartPreparation(GetGameInstance()->GetFirstLocalPlayerController(World));
            }
        }
        else
        {
            // 서버의 출발 준비 보고 제한 이후에도 클라이언트가 출발 월드에 남을 수 있다.
            if (Failure == EFailureState::None &&
                FPlatformTime::Seconds() - MapTravelStartedAt.GetValue() >= FMath::Max(1.f, MapTravelTimeoutSeconds))
            {
                Pending = FString::Printf(TEXT("map travel timeout source=%s current=%s destination=%s"),
                    *GetNameSafe(Source.Get()), *WorldPackage.ToString(), *ExpectedMap.ToString());
                ShowFailure(false, NSLOCTEXT("MWArrival", "MapTravelTimeout",
                    "경기 맵으로 이동하지 못했습니다. 방을 나가 주세요."), EFailureState::MapTravelTimeout);
            }
            return true;
        }
    }
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
        WorldPackage != ExpectedMap)
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
        const auto Board = World->GetSubsystem<UMWGameplayHUDSubsystem>()->GetWaitingBoardReadiness(*PC);
        if (Board == EMWWaitingBoardReadiness::Unavailable)
        {
            Pending = TEXT("waiting board asset/binding");
            ShowFailure(false, NSLOCTEXT("MWArrival", "WaitingBoardUnavailable",
                "경기 대기판을 불러올 수 없습니다. 방을 나가 주세요."));
            return true;
        }
        if (Board == EMWWaitingBoardReadiness::Pending)
            Pending = TEXT("waiting board roster");
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
    FString QualityPending;
    if (Pending.IsEmpty()) CheckMaterials(*World, Materials, Pending, QualityPending);
    if (!Pending.IsEmpty())
    {
        QualityWaitStartedAt.Reset();
    }
    else if (!Frame && !QualityPending.IsEmpty())
    {
        if (!QualityWaitStartedAt) QualityWaitStartedAt = VisibleSeconds;
        const double QualityWait = VisibleSeconds - QualityWaitStartedAt.GetValue();
        if (QualityWait < FMath::Max(0.f, FirstViewQualityWaitSeconds))
            Pending = QualityPending;
        else
            UE_LOG(LogTemp, Display, TEXT("MWArrival quality fallback attempt=%s wait=%.2fs %s"),
                *Attempt.ToString(), QualityWait, *QualityPending);
    }
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

void UMWArrivalScreen::RestartPreparation(APlayerController* Controller)
{
    // 가림막은 유지하고 목적지의 재질과 실제 프레임을 확인한 뒤에만 입력을 복구한다.
    Frame.Reset();
    VisibleSeconds = 0;
    QualityWaitStartedAt.Reset();
    Failure = EFailureState::None;
    Cover->SetContent(CreateLoadingContent());
    if (Controller)
    {
        Controller->bShowMouseCursor = false;
        Controller->SetInputMode(FInputModeUIOnly());
    }
}

void UMWArrivalScreen::ShowFailure(bool bCanRetry, const FText& Message, EFailureState NewFailure)
{
    Failure = NewFailure;
    Frame.Reset();
    UE_LOG(LogTemp, Warning, TEXT("MWArrival failed attempt=%s pending=%s"), *Attempt.ToString(), *Pending);
    TSharedPtr<SButton> InitialFocus;
    Cover->SetHAlign(HAlign_Fill);
    Cover->SetVAlign(VAlign_Fill);
    Cover->SetContent(MWGameplayUI::RecoveryPanel(Message.IsEmpty()
        ? NSLOCTEXT("MWArrival","Failed","첫 화면을 준비할 수 없습니다.") : Message,
        NSLOCTEXT("MWArrival","Retry","첫 화면 다시 준비"), FOnClicked::CreateLambda([Weak = TWeakObjectPtr<UMWArrivalScreen>(this)]()
        {
            if (auto* Self = Weak.Get(); Self && Self->Cover)
            {
                // 재시도는 현재 경기에서 전시와 첫 프레임만 다시 준비한다. 로비 출발을 다시 보고하지 않는다.
                auto* PC = Self->GetGameInstance()->GetFirstLocalPlayerController();
                if (PC)
                    if (auto* Board = Cast<AMWTrackSelectionBoard>(PC->GetViewTarget())) Board->RetryArrivalOffers();
                Self->RestartPreparation(PC);
            }
            return FReply::Handled();
        }),
        NSLOCTEXT("MWArrival","Leave","방 나가기"), FOnClicked::CreateLambda([Weak = TWeakObjectPtr<UMWArrivalScreen>(this)]()
        {
            if (auto* Self = Weak.Get())
                if (auto* Game = Cast<UMWGameInstance>(Self->GetGameInstance()))
                {
                    // 방을 나가기로 했으면 늦게 들어온 맵이 세션 정리 화면을 덮지 않게 한다.
                    Self->Failure = EFailureState::NeedsUserAction;
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
    MapTravelStartedAt.Reset();
}

void UMWArrivalScreen::Deinitialize()
{
    Cancel();
    Super::Deinitialize();
}
