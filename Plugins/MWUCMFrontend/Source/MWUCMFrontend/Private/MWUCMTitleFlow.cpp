#include "MWUCMTitleFlow.h"

#include "Algo/Count.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Components/WidgetComponent.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Containers/Ticker.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "UObject/UObjectGlobals.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/PlayerController.h"
#include "UCMTitleFidelityDirector.h"
#include "Modules/ModuleManager.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "UI/MWCreateRoomWidget.h"
#include "UI/MWRoomBrowserWidget.h"
#include "UI/MWRoomLobbyWidget.h"
#include "UI/MWVehicleSelectionWidget.h"
#include "UI/MWVehiclePlayerMarkerWidget.h"
#include "UI/MWPointerCursor.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "UI/MWVehicleSelectionPreviewActor.h"
#include "System/MultiplayerSubSystem/MWMultiplayerSubsystem.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, MWUCMFrontend)

namespace
{
constexpr double ReturnFadeOutSeconds = .28;
constexpr double ReturnFadeInSeconds = .32;
constexpr double ReturnPaintWarningSeconds = 2.;
constexpr double ReturnSessionWarningSeconds = 6.;
constexpr double ReturnPreparationTimeoutSeconds = 8.;
constexpr float VehiclePreviewBoardScale = 1.33f;

void FitVehiclePreview(UMWVehicleSelectionWidget* Widget)
{
    if (!Widget) return;

    // The UCM preview inherits the normal vehicle layout. Fit only this instance
    // to the world board's 1920 x 1080 canvas.
    const auto FindCanvasSlot = [Widget](FName Name) -> UCanvasPanelSlot*
    {
        UWidget* Child = Widget->GetWidgetFromName(Name);
        return Child ? Cast<UCanvasPanelSlot>(Child->Slot) : nullptr;
    };
    if (auto* Cards = FindCanvasSlot(TEXT("AlignmentCardGroup")))
    {
        Cards->SetPosition(FVector2D(60, 210));
        Cards->SetSize(FVector2D(1800, 440));
    }

    const FName LegendWidgets[] = { TEXT("Legend_Select"), TEXT("Legend_Cancel"), TEXT("SelectionStatePanel"),
        TEXT("SelectionLegendRow"), TEXT("ArcadeLegendStrip"), TEXT("ArcadeLegendBolt0"), TEXT("ArcadeLegendBolt1"),
        TEXT("ArcadeLegendBolt2"), TEXT("ArcadeLegendBolt3") };
    for (FName Name : LegendWidgets)
    {
        if (auto* Slot = FindCanvasSlot(Name))
            Slot->SetPosition(Slot->GetPosition() - FVector2D(0, 70));
    }
    // Card indicators inherit the enlarged card scale. The moving cursor is
    // drawn by Slate and keeps its original viewport size.
    Widget->SetWorldPreviewMarkerScales(1.f / VehiclePreviewBoardScale, .58f);
}

void RecordRoomReturn(const TCHAR* Event)
{
    CSV_EVENT_GLOBAL_NOLOG(TEXT("MW_RETURN_%s"), Event);
    UE_LOG(LogTemp, Display, TEXT("MW_RETURN %s MS=%.3f FRAME=%llu"),
        Event, FPlatformTime::Seconds() * 1000., GFrameCounter);
}

class SMWRoomReturnCover : public SBorder
{
public:
    using FArguments = SBorder::FArguments;

    void Construct(const FArguments& Args, FSimpleDelegate InOnPainted)
    {
        OnPainted = MoveTemp(InOnPainted);
        SBorder::Construct(Args);
    }

    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry,
        const FSlateRect& CullRect, FSlateWindowElementList& Elements, int32 Layer,
        const FWidgetStyle& Style, bool bParentEnabled) const override
    {
        const int32 Result = SBorder::OnPaint(Args, Geometry, CullRect, Elements, Layer, Style, bParentEnabled);
        OnPainted.ExecuteIfBound();
        return Result;
    }

private:
    FSimpleDelegate OnPainted;
};
}

void UMWUCMIntroSession::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UMWMultiplayerSubsystem>();
    if (auto* Sessions = GetGameInstance()->GetSubsystem<UMWMultiplayerSubsystem>())
    {
        Sessions->GetTitleTravelGate().BindUObject(this, &ThisClass::DeferRoomReturnTravel);
        Sessions->OnSessionStateChanged.AddUniqueDynamic(this, &ThisClass::HandleRoomPresentationState);
        Sessions->OnSessionError.AddUniqueDynamic(this, &ThisClass::HandleRoomReturnError);
    }
    PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &ThisClass::CoverMapBoundary);
    PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::OnReturnMapLoaded);
    CoverTickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::TickReturnCover));
}

void UMWUCMIntroSession::Deinitialize()
{
    PendingReturnTravel.Unbind();
    if (auto* Sessions = GetGameInstance()->GetSubsystem<UMWMultiplayerSubsystem>())
    {
        if (Sessions->GetTitleTravelGate().IsBoundToObject(this)) Sessions->GetTitleTravelGate().Unbind();
        Sessions->OnSessionStateChanged.RemoveDynamic(this, &ThisClass::HandleRoomPresentationState);
        Sessions->OnSessionError.RemoveDynamic(this, &ThisClass::HandleRoomReturnError);
    }
    RemoveReturnCover();
    if (CursorViewport.IsValid() && FrontendCursorSlate.IsValid())
    {
        int32 Index = 0;
        bool bRestored = false;
        for (const auto Type : {EMouseCursor::Default, EMouseCursor::Hand, EMouseCursor::TextEditBeam})
        {
            if (CursorViewport->GetSoftwareCursorWidget(Type) == FrontendCursorSlate)
            {
                CursorViewport->SetSoftwareCursorWidget(Type, PreviousCursors[Index]);
                bRestored = true;
            }
            ++Index;
        }
        if (bRestored) CursorViewport->SetUseSoftwareCursorWidgets(bPreviousSoftwareCursors);
    }
    FrontendCursorSlate.Reset();
    FrontendCursor = nullptr;
    FTSTicker::GetCoreTicker().RemoveTicker(CoverTickHandle);
    FCoreUObjectDelegates::PreLoadMapWithContext.Remove(PreLoadMapHandle);
    FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
    Super::Deinitialize();
}

void UMWUCMIntroSession::InstallFrontendCursor()
{
    if (FrontendCursorSlate.IsValid()) return;
    auto* Viewport = GetGameInstance()->GetGameViewportClient();
    auto* Class = PointerWidgetClass.LoadSynchronous();
    if (!Viewport || !Class) return;
    FrontendCursor = CreateWidget<UMWVehiclePlayerMarkerWidget>(GetGameInstance(), Class);
    if (!FrontendCursor) return;
    FrontendCursor->TakeWidget();
    FMWVehiclePlayerPresentation Player;
    Player.bLocal = true;
    Player.PlayerTint = FLinearColor(.8f, .05f, .08f, 1.f);
    FrontendCursor->SetPresentation(Player);
    FrontendCursor->SetRenderOpacity(1.f);
    FrontendCursor->SetVisibility(ESlateVisibility::HitTestInvisible);
    const TWeakObjectPtr<UMWUCMIntroSession> WeakThis(this);
    FrontendCursorSlate = MWMakePointerCursor(FrontendCursor, TAttribute<FVector2D>::CreateLambda([WeakThis]()
    {
        return FVector2D(154.834854f) * (WeakThis.IsValid() ? UWidgetLayoutLibrary::GetViewportScale(WeakThis.Get()) : 1.f);
    }));
    CursorViewport = Viewport;
    bPreviousSoftwareCursors = Viewport->GetIsUsingSoftwareCursorWidgets();
    for (const auto Type : {EMouseCursor::Default, EMouseCursor::Hand, EMouseCursor::TextEditBeam})
    {
        PreviousCursors.Add(Viewport->GetSoftwareCursorWidget(Type));
        Viewport->SetSoftwareCursorWidget(Type, FrontendCursorSlate);
    }
    Viewport->SetUseSoftwareCursorWidgets(true);
    UE_LOG(LogTemp, Display, TEXT("MW_UCM FRONTEND_CURSOR_INSTALLED VEHICLE_ART_ARROW_TIP_HOTSPOT"));
}

void UMWUCMIntroSession::SetRoomPresentationWorld(UWorld* World)
{
    if (World && World->GetGameInstance() == GetGameInstance() && World->GetNetMode() != NM_DedicatedServer)
        RoomPresentationWorld = World;
}

void UMWUCMIntroSession::SetReturnPhase(EReturnCoverPhase Phase)
{
    static const TCHAR* const Names[] = {TEXT("Hidden"), TEXT("FadingOut"), TEXT("WaitingForPaint"),
        TEXT("WaitingForSession"), TEXT("Traveling"), TEXT("WorldReady"), TEXT("FadingIn"), TEXT("AwaitingInput"), TEXT("Failed")};
    UE_LOG(LogTemp, Display, TEXT("MW_RETURN PHASE %s -> %s MS=%.3f"), Names[int32(ReturnCoverPhase)],
        Names[int32(Phase)], FPlatformTime::Seconds() * 1000.);
    ReturnCoverPhase = Phase;
    CoverChangedAt = FPlatformTime::Seconds();
}

void UMWUCMIntroSession::EnsureReturnCover()
{
    auto* Viewport = GetGameInstance()->GetGameViewportClient();
    if (!Viewport) return;
    if (ReturnCover && CoverViewport.Get() == Viewport) return;
    if (ReturnCover && CoverViewport.IsValid()) CoverViewport->RemoveViewportWidgetContent(ReturnCover.ToSharedRef());
    ReturnCover = SNew(SMWRoomReturnCover, FSimpleDelegate::CreateUObject(this, &ThisClass::CoverPainted))
        .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
        .BorderBackgroundColor(FLinearColor(0, 0, 0, CoverAlpha)).Visibility(EVisibility::HitTestInvisible);
    CoverViewport = Viewport;
    Viewport->AddViewportWidgetContent(ReturnCover.ToSharedRef(), 10000);
    OpaquePaintFrame = MAX_uint64;
    if (!StatusMessage.IsEmpty()) ShowReturnStatus(StatusMessage, bStatusRetry);
}

bool UMWUCMIntroSession::BeginRoomReturnCover(UWorld* World)
{
    if (!World || World != RoomPresentationWorld.Get() || !GetGameInstance()->GetGameViewportClient()) return false;
    if (ReturnCoverPhase != EReturnCoverPhase::Hidden && ReturnCoverPhase != EReturnCoverPhase::Failed) return false;
    if (ReturnCoverPhase == EReturnCoverPhase::Failed && World != ReturnSourceWorld.Get()) return false;
    ReturnSourceWorld = World;
    ReturnTargetWorld.Reset();
    ReturnController.Reset();
    ReturnViewTarget.Reset();
    ReturnMenuRoot.Reset();
    PreparationFailure.Reset();
    ExpectedReturnMap = NAME_None;
    EnsureReturnCover();
    ClearReturnStatus();
    RevealFromAlpha = CoverAlpha;
    OpaquePaintFrame = MAX_uint64;
    SetReturnPhase(EReturnCoverPhase::FadingOut);
    RecordRoomReturn(TEXT("FADE_OUT_START"));
    return true;
}

void UMWUCMIntroSession::HandleRoomPresentationState(EMWSessionState PreviousState, EMWSessionState NewState)
{
    if (NewState == EMWSessionState::Leaving)
        BeginRoomReturnCover(GetGameInstance()->GetWorld());
    else if (NewState == EMWSessionState::CleanupRequired && IsRoomReturnCovered())
        FailReturn(TEXT("SessionCleanupRequired"), NSLOCTEXT("MWReturn", "CleanupFailed", "방 종료를 완료하지 못했습니다."), true);
}

void UMWUCMIntroSession::HandleRoomReturnError(const FString& Error)
{
    if (ReturnCoverPhase == EReturnCoverPhase::Failed && bStatusRetry && !Error.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("MW_RETURN SESSION_ERROR %s"), *Error);
        ShowReturnStatus(FText::FromString(Error), true);
    }
}

bool UMWUCMIntroSession::DeferRoomReturnTravel(FName Map, FSimpleDelegate Travel)
{
    if (!Travel.IsBound() || !ReturnSourceWorld.IsValid() || ReturnSourceWorld.Get() != GetGameInstance()->GetWorld()
        || (ReturnCoverPhase != EReturnCoverPhase::FadingOut && ReturnCoverPhase != EReturnCoverPhase::WaitingForPaint
            && ReturnCoverPhase != EReturnCoverPhase::WaitingForSession)) return false;
    if (!PendingReturnTravel.IsBound())
    {
        ExpectedReturnMap = Map;
        PendingReturnTravel = MoveTemp(Travel);
        RecordRoomReturn(TEXT("TITLE_TRAVEL_AUTHORIZED"));
    }
    return true;
}

void UMWUCMIntroSession::CoverPainted()
{
    if ((ReturnCoverPhase != EReturnCoverPhase::WaitingForPaint && ReturnCoverPhase != EReturnCoverPhase::WaitingForSession)
        || OpaquePaintFrame != MAX_uint64 || CoverAlpha < 1.f) return;
    OpaquePaintFrame = GFrameCounter;
    RecordRoomReturn(TEXT("OPAQUE_SLATE_PAINT"));
}

void UMWUCMIntroSession::CoverMapBoundary(const FWorldContext& Context, const FString& MapName)
{
    if (ReturnCoverPhase == EReturnCoverPhase::Traveling && Context.OwningGameInstance == GetGameInstance())
    {
        RecordRoomReturn(TEXT("MAP_LOAD_START"));
    }
}

void UMWUCMIntroSession::OnReturnMapLoaded(UWorld* World)
{
    if (ReturnCoverPhase != EReturnCoverPhase::Traveling || !World || World->GetGameInstance() != GetGameInstance()) return;
    const FName Package(*UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()));
    if (World == ReturnSourceWorld.Get() || World->GetNetMode() != NM_Standalone || Package != ExpectedReturnMap)
    {
        FailReturn(TEXT("UnexpectedReturnWorld"), NSLOCTEXT("MWReturn", "WrongWorld", "타이틀 맵을 확인할 수 없습니다. 게임을 다시 실행해 주세요."));
        return;
    }
    ReturnTargetWorld = World;
    SetReturnPhase(EReturnCoverPhase::WorldReady);
    RecordRoomReturn(TEXT("WORLD_READY"));
}

bool UMWUCMIntroSession::IsReturnWorldReady(const UWorld* World) const
{
    return ReturnCoverPhase == EReturnCoverPhase::WorldReady && World && ReturnTargetWorld.Get() == World
        && GetGameInstance()->GetWorld() == World;
}

void UMWUCMIntroSession::SetPreparationFailure(const UWorld* World, const TCHAR* Reason)
{
    if (IsReturnWorldReady(World) && PreparationFailure != Reason)
    {
        PreparationFailure = Reason;
        UE_LOG(LogTemp, Display, TEXT("MW_RETURN PREPARATION_WAIT %s"), Reason);
    }
}

void UMWUCMIntroSession::RevealReturnedWorld(APlayerController* PC, AActor* ViewTarget, UUserWidget* Root)
{
    if (!IsValid(PC) || !IsReturnWorldReady(PC->GetWorld())) return;
    ReturnController = PC;
    ReturnViewTarget = ViewTarget;
    ReturnMenuRoot = Root;
    if (!IsReadyViewCurrent()) return;
    RecordRoomReturn(TEXT("CAMERA_AND_MENU_READY"));
    RevealFromAlpha = CoverAlpha;
    SetReturnPhase(EReturnCoverPhase::FadingIn);
    RecordRoomReturn(TEXT("FADE_IN_START"));
}

bool UMWUCMIntroSession::IsReadyViewCurrent() const
{
    const auto* World = ReturnTargetWorld.Get();
    const auto* PC = ReturnController.Get();
    const auto* Camera = ReturnViewTarget.Get();
    const auto* Root = ReturnMenuRoot.Get();
    return World && World == GetGameInstance()->GetWorld() && PC && PC->IsLocalController() && PC->GetWorld() == World
        && Camera && Camera->GetWorld() == World && PC->GetViewTarget() == Camera && Root && Root->IsInViewport()
        && Root->GetOwningPlayer() == PC;
}

bool UMWUCMIntroSession::FinishReturnPresentation(UWorld* World)
{
    if (ReturnCoverPhase != EReturnCoverPhase::AwaitingInput || World != ReturnTargetWorld.Get() || !IsReadyViewCurrent())
        return false;
    RemoveReturnCover();
    return true;
}

void UMWUCMIntroSession::ShowReturnStatus(const FText& Message, bool bRetry)
{
    StatusMessage = Message;
    bStatusRetry = bRetry;
    if (!ReturnCover) return;
    ReturnCover->SetVisibility(EVisibility::Visible);
    TSharedPtr<SButton> ExitButton;
    const FSlateFontInfo MessageFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 26);
    const FSlateFontInfo ButtonFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 22);
    ReturnCover->SetHAlign(HAlign_Center);
    ReturnCover->SetVAlign(VAlign_Center);
    ReturnCover->SetContent(
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(12.f)
        [SNew(STextBlock).Font(MessageFont).Text(Message).WrapTextAt(480.f).Justification(ETextJustify::Center)]
        + SVerticalBox::Slot().AutoHeight().Padding(12.f)
        [SNew(SButton).HAlign(HAlign_Center).ContentPadding(FMargin(16.f, 8.f))
            .Visibility(bRetry ? EVisibility::Visible : EVisibility::Collapsed).OnClicked_UObject(this, &ThisClass::RetryReturn)
            [SNew(STextBlock).Font(ButtonFont).Text(NSLOCTEXT("MWReturn", "Retry", "세션 정리 다시 시도"))]]
        + SVerticalBox::Slot().AutoHeight().Padding(12.f)
        [SAssignNew(ExitButton, SButton).HAlign(HAlign_Center).ContentPadding(FMargin(16.f, 8.f))
            .OnClicked_UObject(this, &ThisClass::ExitAfterReturnFailure)
            [SNew(STextBlock).Font(ButtonFont).Text(NSLOCTEXT("MWReturn", "Exit", "게임 종료"))]]);
    if (FrontendCursor) FrontendCursor->SetRenderOpacity(1.f);
    if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetKeyboardFocus(ExitButton);
}

void UMWUCMIntroSession::ClearReturnStatus()
{
    StatusMessage = FText::GetEmpty();
    bStatusRetry = false;
    if (!ReturnCover) return;
    ReturnCover->SetContent(SNullWidget::NullWidget);
    ReturnCover->SetVisibility(EVisibility::HitTestInvisible);
}

void UMWUCMIntroSession::FailReturn(const TCHAR* Reason, const FText& Message, bool bRetry)
{
    PendingReturnTravel.Unbind();
    UE_LOG(LogTemp, Warning, TEXT("MW_RETURN FAILED %s PREPARATION=%s"), Reason, *PreparationFailure);
    if (ReturnCoverPhase != EReturnCoverPhase::Failed) SetReturnPhase(EReturnCoverPhase::Failed);
    SetReturnCoverAlpha(1.f);
    ShowReturnStatus(Message, bRetry);
}

FReply UMWUCMIntroSession::RetryReturn()
{
    if (auto* Sessions = GetGameInstance()->GetSubsystem<UMWMultiplayerSubsystem>(); Sessions && ReturnSourceWorld.IsValid()
        && ReturnSourceWorld.Get() == GetGameInstance()->GetWorld())
        Sessions->RetrySessionCleanup();
    return FReply::Handled();
}

FReply UMWUCMIntroSession::ExitAfterReturnFailure()
{
    UWorld* World = GetGameInstance()->GetWorld();
    if (auto* PC = GetGameInstance()->GetFirstLocalPlayerController(World))
        UKismetSystemLibrary::QuitGame(World, PC, EQuitPreference::Quit, false);
    else if (GIsEditor)
    {
        if (auto* Viewport = CoverViewport.Get()) Viewport->CloseRequested(Viewport->Viewport);
    }
    else FPlatformMisc::RequestExit(false);
    return FReply::Handled();
}

void UMWUCMIntroSession::SetReturnCoverAlpha(float Alpha)
{
    CoverAlpha = Alpha;
    if (ReturnCover) ReturnCover->SetBorderBackgroundColor(FLinearColor(0, 0, 0, Alpha));
    if (FrontendCursor) FrontendCursor->SetRenderOpacity(StatusMessage.IsEmpty() ? 1.f - Alpha : 1.f);
}

void UMWUCMIntroSession::RemoveReturnCover()
{
    if (CoverViewport.IsValid() && ReturnCover.IsValid())
        CoverViewport->RemoveViewportWidgetContent(ReturnCover.ToSharedRef());
    ReturnCover.Reset();
    CoverViewport.Reset();
    CoverAlpha = 0.f;
    ClearReturnStatus();
    PendingReturnTravel.Unbind();
    ReturnSourceWorld.Reset();
    ReturnTargetWorld.Reset();
    ReturnController.Reset();
    ReturnViewTarget.Reset();
    ReturnMenuRoot.Reset();
    if (IsRoomReturnCovered()) SetReturnPhase(EReturnCoverPhase::Hidden);
    if (FrontendCursor) FrontendCursor->SetRenderOpacity(1.f);
}

bool UMWUCMIntroSession::TickReturnCover(float DeltaTime)
{
    if (!IsRoomReturnCovered()) return true;
    EnsureReturnCover();
    const double Elapsed = FPlatformTime::Seconds() - CoverChangedAt;
    if (ReturnCoverPhase == EReturnCoverPhase::FadingOut)
    {
        const float Progress = FMath::SmoothStep(0.f, 1.f, float(Elapsed / ReturnFadeOutSeconds));
        SetReturnCoverAlpha(FMath::Lerp(RevealFromAlpha, 1.f, Progress));
        if (Progress >= 1.f)
        {
            SetReturnPhase(EReturnCoverPhase::WaitingForPaint);
            RecordRoomReturn(TEXT("FADE_OUT_COMPLETE"));
        }
    }
    else if (ReturnCoverPhase == EReturnCoverPhase::WaitingForPaint || ReturnCoverPhase == EReturnCoverPhase::WaitingForSession)
    {
        if (!ReturnSourceWorld.IsValid() || ReturnSourceWorld.Get() != GetGameInstance()->GetWorld())
        {
            FailReturn(TEXT("ReturnSourceWorldLost"), NSLOCTEXT("MWReturn", "SourceLost", "이전 로비를 확인할 수 없습니다. 게임을 다시 실행해 주세요."));
        }
        else if (OpaquePaintFrame != MAX_uint64 && GFrameCounter > OpaquePaintFrame)
        {
            if (PendingReturnTravel.IsBound())
            {
                ClearReturnStatus();
                SetReturnCoverAlpha(1.f);
                SetReturnPhase(EReturnCoverPhase::Traveling);
                FSimpleDelegate Travel = MoveTemp(PendingReturnTravel);
                PendingReturnTravel.Unbind();
                Travel.ExecuteIfBound();
            }
            else if (ReturnCoverPhase == EReturnCoverPhase::WaitingForPaint)
            {
                ClearReturnStatus();
                SetReturnCoverAlpha(1.f);
                SetReturnPhase(EReturnCoverPhase::WaitingForSession);
            }
            else if (Elapsed >= ReturnSessionWarningSeconds && StatusMessage.IsEmpty())
            {
                RecordRoomReturn(TEXT("SESSION_RESPONSE_DELAYED"));
                ShowReturnStatus(NSLOCTEXT("MWReturn", "WaitForSession", "방 종료 응답을 기다리는 중입니다."), false);
            }
        }
        else if (Elapsed >= ReturnPaintWarningSeconds && StatusMessage.IsEmpty())
        {
            RecordRoomReturn(TEXT("PAINT_ACK_DELAYED"));
            ShowReturnStatus(NSLOCTEXT("MWReturn", "WaitForPaint", "게임 창이 다시 그려지면 타이틀로 이동합니다."), false);
        }
    }
    else if (ReturnCoverPhase == EReturnCoverPhase::Traveling && Elapsed >= ReturnPreparationTimeoutSeconds)
        FailReturn(TEXT("TravelCompletionMissing"), NSLOCTEXT("MWReturn", "TravelTimeout", "타이틀 맵 이동을 확인할 수 없습니다. 게임을 다시 실행해 주세요."));
    else if (ReturnCoverPhase == EReturnCoverPhase::WorldReady && Elapsed >= ReturnPreparationTimeoutSeconds)
        FailReturn(TEXT("TitlePresentationNotReady"), NSLOCTEXT("MWReturn", "TitleNotReady", "타이틀 카메라 또는 메뉴를 준비하지 못했습니다. 게임을 다시 실행해 주세요."));
    else if ((ReturnCoverPhase == EReturnCoverPhase::FadingIn || ReturnCoverPhase == EReturnCoverPhase::AwaitingInput) && !IsReadyViewCurrent())
        FailReturn(TEXT("ReadyViewInvalidated"), NSLOCTEXT("MWReturn", "ViewLost", "타이틀 화면 연결이 끊겼습니다. 게임을 다시 실행해 주세요."));
    else if (ReturnCoverPhase == EReturnCoverPhase::FadingIn)
    {
        const float Progress = FMath::SmoothStep(0.f, 1.f, float(Elapsed / ReturnFadeInSeconds));
        SetReturnCoverAlpha(RevealFromAlpha * (1.f - Progress));
        if (Progress >= 1.f)
        {
            SetReturnPhase(EReturnCoverPhase::AwaitingInput);
            RecordRoomReturn(TEXT("FADE_IN_COMPLETE"));
        }
    }

    else if (ReturnCoverPhase == EReturnCoverPhase::AwaitingInput && Elapsed >= ReturnPreparationTimeoutSeconds)
        FailReturn(TEXT("TitleInputNotReady"), NSLOCTEXT("MWReturn", "InputNotReady", "메인 메뉴 입력을 연결하지 못했습니다. 게임을 다시 실행해 주세요."));
    else if (ReturnCoverPhase == EReturnCoverPhase::Failed && bStatusRetry
        && (!ReturnSourceWorld.IsValid() || ReturnSourceWorld.Get() != GetGameInstance()->GetWorld()))
    {
        RecordRoomReturn(TEXT("RECOVERY_SOURCE_WORLD_UNAVAILABLE"));
        ShowReturnStatus(NSLOCTEXT("MWReturn", "RetryWorldLost", "이전 로비가 닫혔습니다. 게임을 다시 실행해 주세요."), false);
    }
    return true;
}

void UMWUCMTitleRootWidget::NativeConstruct()
{
    Super::NativeConstruct();
}

void UMWUCMTitleRootWidget::NativeDestruct()
{
    Super::NativeDestruct();
}

void UMWUCMTitleRootWidget::ActivateSourceMenu(int32 Index)
{
    if (Index == 2) { HandleQuit(); return; }
    const FName ButtonName = Index == 0 ? TEXT("Button_CreateRoom") : TEXT("Button_JoinRoom");
    if (auto* Button = TitleScreen ? Cast<UButton>(TitleScreen->GetWidgetFromName(ButtonName)) : nullptr)
        if (Button->GetIsEnabled()) Button->OnClicked.Broadcast();
}

void UMWUCMTitleRootWidget::HandleQuit()
{
    UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

bool UMWUCMTitleRootWidget::CanPresentEntryMenu(bool bLoggedIn)
{
    if (IsValid(Flow)) Flow->IsMenuReady(bLoggedIn);
    // UCM's original signs render the menu. MW keeps the existing button handlers and forms.
    return false;
}

void UMWUCMTitleRootWidget::OnEntryPresentationUpdated(bool bLoggedIn, bool bMenuRequested)
{
    if (IsValid(Flow)) Flow->ApplyEntryPresentation(bLoggedIn, bMenuRequested);
}

UWidget* UMWUCMTitleRootWidget::ResolveEntryPresentation(UWidget* Screen, bool bMenuRequested)
{
    if (IsValid(Flow))
    {
        Flow->PresentEntryWidget(Screen, bMenuRequested);
        if (WorldMenuPlaceholder && bMenuRequested)
            return WorldMenuPlaceholder;
    }
    return Screen;
}

AMWUCMTitleFlow::AMWUCMTitleFlow()
{
    bStartAutomatically = false;
    bExternalMenuPresentation = true;
    MWWorldSurface = CreateDefaultSubobject<UUCMWorldWidgetComponent>(TEXT("MWExistingWorldSurface"));
    MWWorldSurface->SetupAttachment(RootComponent);
    MWWorldSurface->SetDrawSize(FVector2D(1920, 1080));
}

void AMWUCMTitleFlow::BeginPlay()
{
    Super::BeginPlay();
    if (auto* Session = GetGameInstance()->GetSubsystem<UMWUCMIntroSession>())
    {
        Session->InstallFrontendCursor();
        if (GetWorld()->GetNetMode() != NM_Standalone) Session->SetRoomPresentationWorld(GetWorld());
    }
    TitleCanvas->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MenuCanvas->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    // No input polling, prop motion or audio before the existing MW login succeeds.
    SetActorTickEnabled(false);
    PresentationHandle = PresentationChanged.AddUObject(this, &ThisClass::RefreshRoot);
    MenuActionHandle = ExternalMenuAction.AddUObject(this, &ThisClass::HandleSourceMenu);
    SetSourceMenuPresentation(false, false);
    MWWorldSurface->SetVisibility(false);
    MWWorldSurface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (IsValid(VehiclePreviewActor) && VehiclePreviewActor->GetWorld() == GetWorld())
        VehiclePreviewActor->SetActorScale3D(VehiclePreviewActor->GetActorScale3D() * VehiclePreviewBoardScale);
    FActorSpawnParameters CameraSpawn;
    CameraSpawn.Name = TEXT("MWUCM_WorldRouteCamera");
    WorldRouteCamera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, CameraSpawn);
    if (WorldRouteCamera)
    {
        WorldRouteCamera->GetCameraComponent()->SetConstraintAspectRatio(true);
        WorldRouteCamera->GetCameraComponent()->SetAspectRatio(16.f / 9.f);
    }
    InitializeHandle = GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::InitializeRoot);
}

void AMWUCMTitleFlow::InitializeRoot()
{
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    auto* Session = GetGameInstance()->GetSubsystem<UMWUCMIntroSession>();
    if (!IsValid(PC) || !PC->IsLocalController() || PC->GetWorld() != GetWorld() || !RootWidgetClass)
    {
        if (Session) Session->SetPreparationFailure(GetWorld(), RootWidgetClass ? TEXT("LocalControllerMissing") : TEXT("RootWidgetClassMissing"));
        if (RootWidgetClass && Session && Session->IsReturnWorldReady(GetWorld()))
            InitializeHandle = GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::InitializeRoot);
        return;
    }
    // A room reloads this same UCM world through MW's unmodified session travel.
    // The existing controller owns its real lobby widget and registration state.
    if (GetWorld()->GetNetMode() != NM_Standalone)
    {
        if (Session)
        {
            Session->bIntroStarted = true;
        }
        bStarted = true;
        RestoreTitlePose();
        PreviewStage = 2;
        TitleCanvas->SetVisibility(false);
        SetSourceMenuPresentation(false, false);
        StartFidelity();
        SetActorTickEnabled(true);
        StartWorldRoute(TEXT("Lobby"), false, TEXT("Lobby"));
        UE_LOG(LogTemp, Display, TEXT("MW_UCM ROOM_WORLD_LOBBY_NO_INTRO"));
        return;
    }
    RootWidget = CreateWidget<UMWUCMTitleRootWidget>(PC, RootWidgetClass);
    if (!RootWidget)
    {
        if (Session) Session->SetPreparationFailure(GetWorld(), TEXT("RootWidgetCreationFailed"));
        return;
    }
    RootWidget->Flow = this;
    RootWidget->AddToViewport();
}

bool AMWUCMTitleFlow::IsMenuReady(bool bLoggedIn)
{
    if (!bLoggedIn) return false;
    if (!bStarted)
    {
        auto* Session = GetGameInstance()->GetSubsystem<UMWUCMIntroSession>();
        if (!IntroSequence || !Session || (Session->IsReturnWorldReady(GetWorld()) && (!IsValid(PreviewCamera) || !IsValid(MenuCamera))))
        {
            UE_LOG(LogTemp, Error, TEXT("MW_UCM Missing sequence/session lifetime; keeping menu gated"));
            if (Session) Session->SetPreparationFailure(GetWorld(), IntroSequence ? TEXT("TitleCameraMissing") : TEXT("IntroSequenceMissing"));
            return false;
        }
        bStarted = true;
        const bool bAlreadyStarted = Session->bIntroStarted;
        Session->bIntroStarted = true;
        SetActorTickEnabled(true);
        StartFidelity();
        if (bAlreadyStarted)
        {
            RestoreTitlePose();
            // RestoreTitlePose changes the view target before the usual camera tick.
            // Evaluate that camera now so the source blend captures the intended outgoing POV.
            if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
            {
                if (PC->PlayerCameraManager) PC->PlayerCameraManager->UpdateCamera(0.f);
            }
            ReturnCameraStartedAt = GetWorld()->GetTimeSeconds();
            // This is a programmatic camera transition, even while player input
            // remains blocked by the cover across the newly loaded world.
            {
                TGuardValue<bool> AllowTransition(bSuppressPreviewInput, false);
                AcceptPreviewInput();
            }
            if (Session->IsReturnWorldReady(GetWorld())) RecordRoomReturn(TEXT("UCM_DOLLY_STARTED"));
            UE_LOG(LogTemp, Display, TEXT("MW_UCM SESSION_RETURN_NO_INTRO"));
        }
        else
        {
            ReplayPreview();
            UE_LOG(LogTemp, Display, TEXT("MW_UCM LOGIN_SUCCESS_INTRO_ONCE"));
        }
    }
    return PreviewStage == 2;
}

void AMWUCMTitleFlow::ApplyEntryPresentation(bool bLoggedIn, bool bMenuRequested)
{
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC) return;
    if (!bLoggedIn)
    {
        PC->bShowMouseCursor = true;
        PC->SetInputMode(FInputModeUIOnly());
        return;
    }
    if (!bStarted) return;
    const bool bMain = bMenuRequested && ActiveRegion == TEXT("Main")
        && (!bWorldRouteActive || bWorldRoutePresentationReady);
    bMainPresentationRequested = bMain;
    const auto* IntroSession = GetGameInstance()->GetSubsystem<UMWUCMIntroSession>();
    const bool bCovered = IntroSession && IntroSession->IsRoomReturnCovered();
    bSuppressPreviewInput = !bMain || bCovered;
    SetSourceMenuPresentation(bMain && (PreviewStage == 2 || PreviewStage == 3), bMain && PreviewStage == 2 && !bCovered);
    if (!bMain) TitleCanvas->SetVisibility(false);
    TitleCanvas->SetCollisionEnabled(bMain && PreviewStage == 1
        ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
    if (bMain && PreviewStage == 0)
    {
        PC->bShowMouseCursor = false;
        PC->SetInputMode(FInputModeGameOnly());
    }
    else if (ActiveRegion != TEXT("RoomPanel")) UseWorldMouse();
}

void AMWUCMTitleFlow::RefreshRoot()
{
    if (!IsValid(RootWidget) || bRefreshing) return;
    TGuardValue<bool> Guard(bRefreshing, true);
    RootWidget->RefreshEntryScreen();
}

void AMWUCMTitleFlow::HandleSourceMenu(FName Action)
{
    if (!IsValid(RootWidget) || bSuppressPreviewInput || PreviewStage != 2) return;
    UE_LOG(LogTemp, Display, TEXT("MW_UCM MENU_HANDLER ACTION=%s MONOTONIC_MS=%.3f"), *Action.ToString(), FPlatformTime::Seconds() * 1000.0);
    const int32 Index = SourceMenuEntries.IndexOfByPredicate([&](const auto& Entry){ return Entry.ActionId == Action; });
    if (Index >= 0 && Index < 3) RootWidget->ActivateSourceMenu(Index);
}

void AMWUCMTitleFlow::EndPlay(const EEndPlayReason::Type Reason)
{
    MWWorldSurface->SetWidget(nullptr);
    if (WorldRouteCamera) WorldRouteCamera->Destroy();
    GetWorld()->GetTimerManager().ClearTimer(InitializeHandle);
    PresentationChanged.Remove(PresentationHandle);
    ExternalMenuAction.Remove(MenuActionHandle);
    if (IsValid(RootWidget))
    {
        RootWidget->RemoveFromParent();
        RootWidget->Flow = nullptr;
        RootWidget = nullptr;
    }
    Super::EndPlay(Reason);
}

void AMWUCMTitleFlow::StartFidelity()
{
    for (TActorIterator<AUCMTitleFidelityDirector> It(GetWorld()); It; ++It)
    {
        It->ResetFidelityRuntime();
        It->SetActorTickEnabled(true);
    }
}

const FMWUCMWorldRoute* AMWUCMTitleFlow::FindRoute(FName Name) const
{
    return WorldRoutes.FindByPredicate([&](const auto& Route) { return Route.Name == Name; });
}

void AMWUCMTitleFlow::UseWorldMouse()
{
    if (auto* PC = GetWorld()->GetFirstPlayerController())
    {
        PC->bShowMouseCursor = true;
        FInputModeGameAndUI Mode;
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        Mode.SetHideCursorDuringCapture(false);
        PC->SetInputMode(Mode);
    }
}

bool AMWUCMTitleFlow::StartWorldRoute(FName Name, bool bReverse, FName Destination)
{
    const auto* Route = FindRoute(Name);
    auto* PC = GetWorld()->GetFirstPlayerController();
    if (!Route || !PC || !PC->PlayerCameraManager || !WorldRouteCamera) return false;
    const auto& Samples = bReverse ? Route->Reverse : Route->Forward;
    if (Samples.Num() < 2) return false;
    RunningRoute = Name;
    bRouteReverse = bReverse;
    ActiveRegion = Destination;
    WorldRouteTime = 0;
    const bool bAlreadyUsingRouteCamera = PC->GetViewTarget() == WorldRouteCamera;
    WorldRouteOutgoing = bAlreadyUsingRouteCamera ? WorldRouteCamera->GetActorTransform()
        : FTransform(PC->PlayerCameraManager->GetCameraRotation(), PC->PlayerCameraManager->GetCameraLocation());
    WorldRouteOutgoingFOV = bAlreadyUsingRouteCamera ? WorldRouteCamera->GetCameraComponent()->FieldOfView
        : PC->PlayerCameraManager->GetFOVAngle();
    // A newly loaded room starts at the original online-menu end of the lobby path.
    if (Name == TEXT("Lobby") && !RootWidget)
    {
        WorldRouteOutgoing = Samples[0].Pose;
        WorldRouteOutgoingFOV = FMath::RadiansToDegrees(2.f * FMath::Atan(FMath::Tan(FMath::DegreesToRadians(Route->VerticalFOV) * .5f) * 16.f / 9.f));
    }
    WorldRouteCamera->SetActorTransform(WorldRouteOutgoing);
    WorldRouteCamera->GetCameraComponent()->SetFieldOfView(WorldRouteOutgoingFOV);
    PC->SetViewTarget(WorldRouteCamera);
    bWorldRouteActive = true;
    bWorldRoutePresentationReady = false;
    bWorldRouteBlendCompleteLogged = false;
    bSuppressPreviewInput = true;
    TitleCanvas->SetVisibility(false);
    TitleCanvas->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetSourceMenuPresentation(false, false);
    MWWorldSurface->SetVisibility(false);
    MWWorldSurface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    UseWorldMouse();
    UE_LOG(LogTemp, Display, TEXT("MW_UCM WORLD_ROUTE_BEGIN %s %s SOURCE=%s MONOTONIC_MS=%.3f"), *Name.ToString(), bReverse ? TEXT("REVERSE") : TEXT("FORWARD"), *Route->Source, FPlatformTime::Seconds() * 1000.0);
    return true;
}

void AMWUCMTitleFlow::UpdateWorldRoute(float DeltaTime)
{
    if (!bWorldRouteActive || !WorldRouteCamera) return;
    const auto* Route = FindRoute(RunningRoute);
    if (!Route) return;
    const auto& Samples = bRouteReverse ? Route->Reverse : Route->Forward;
    WorldRouteTime += DeltaTime;
    int32 Index = 0;
    while (Index + 1 < Samples.Num() && Samples[Index + 1].Time < WorldRouteTime) ++Index;
    const auto& A = Samples[Index];
    const auto& B = Samples[FMath::Min(Index + 1, Samples.Num() - 1)];
    const float Alpha = FMath::Clamp((WorldRouteTime - A.Time) / FMath::Max(B.Time - A.Time, .00001f), 0.f, 1.f);
    FTransform Pose;
    Pose.Blend(A.Pose, B.Pose, Alpha);
    const float U = FMath::Clamp(WorldRouteTime / FMath::Max(SourceBrainBlendSeconds, .00001f), 0.f, 1.f);
    if (U >= 1.f && !bWorldRouteBlendCompleteLogged)
    {
        bWorldRouteBlendCompleteLogged = true;
        UE_LOG(LogTemp, Display, TEXT("MW_UCM WORLD_ROUTE_BLEND_COMPLETE REGION=%s MONOTONIC_MS=%.3f"), *ActiveRegion.ToString(), FPlatformTime::Seconds() * 1000.0);
    }
    const float Blend = U * U * (3.f - 2.f * U);
    FTransform Blended;
    Blended.Blend(WorldRouteOutgoing, Pose, Blend);
    WorldRouteCamera->SetActorTransform(Blended);
    const float FOV = FMath::RadiansToDegrees(2.f * FMath::Atan(FMath::Tan(FMath::DegreesToRadians(Route->VerticalFOV) * .5f) * 16.f / 9.f));
    WorldRouteCamera->GetCameraComponent()->SetFieldOfView(FMath::Lerp(WorldRouteOutgoingFOV, FOV, Blend));
    // The main signs are world anchored, so reveal them when the original
    // camera blend completes and let them move with its remaining damping.
    // Other world panels still wait for the camera to settle to avoid exposing
    // a close-up or an overlapping panel while their camera is travelling.
    const FTransform& Endpoint = Samples.Last().Pose;
    const float EndpointDistanceCm = FVector::Distance(Blended.GetLocation(), Endpoint.GetLocation());
    const float EndpointAngleDegrees = FMath::RadiansToDegrees(Blended.GetRotation().AngularDistance(Endpoint.GetRotation()));
    const bool bCameraSettled = EndpointDistanceCm <= 3.f && EndpointAngleDegrees <= .1f;
    const bool bPresentationEligible = U >= 1.f && (ActiveRegion == TEXT("Main") || bCameraSettled);
    if (!bWorldRoutePresentationReady && bPresentationEligible)
    {
        bWorldRoutePresentationReady = true;
        if (ActiveRegion == TEXT("Main"))
        {
            bSuppressPreviewInput = false;
            SetSourceMenuPresentation(true, true);
        }
        else if (ActiveRegion == TEXT("Vehicle") && VehiclePreviewActor)
        {
            VehiclePreviewActor->PreviewSurface->SetVisibility(true);
            VehiclePreviewActor->PreviewSurface->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        }
        else if (PresentedWidget && ActiveRegion == TEXT("Lobby"))
        {
            MWWorldSurface->SetVisibility(true);
            MWWorldSurface->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        }
        if (ActiveRegion != TEXT("RoomPanel")) UseWorldMouse();
        UE_LOG(LogTemp, Display, TEXT("MW_UCM WORLD_ROUTE_PRESENTABLE REGION=%s SECONDS=%.3f ENDPOINT_DISTANCE_CM=%.3f ENDPOINT_ANGLE_DEG=%.3f MONOTONIC_MS=%.3f"), *ActiveRegion.ToString(), WorldRouteTime, EndpointDistanceCm, EndpointAngleDegrees, FPlatformTime::Seconds() * 1000.0);
    }
    if (WorldRouteTime < Samples.Last().Time) return;
    bWorldRouteActive = false;
    if (ActiveRegion == TEXT("Main"))
    {
        bSuppressPreviewInput = false;
        // The sampled reverse path ends at a different pose from MenuCamera.
        // Retain the evaluated endpoint to avoid a delayed view-target snap.
        SetSourceMenuPresentation(true, true);
    }
    else if (ActiveRegion == TEXT("Vehicle") && VehiclePreviewActor)
    {
        VehiclePreviewActor->PreviewSurface->SetVisibility(true);
        VehiclePreviewActor->PreviewSurface->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    }
    else if (PresentedWidget)
    {
        MWWorldSurface->SetVisibility(true);
        MWWorldSurface->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    }
    if (ActiveRegion != TEXT("RoomPanel")) UseWorldMouse();
    UE_LOG(LogTemp, Display, TEXT("MW_UCM WORLD_ROUTE_COMPLETE REGION=%s MONOTONIC_MS=%.3f"), *ActiveRegion.ToString(), FPlatformTime::Seconds() * 1000.0);
}

void AMWUCMTitleFlow::SetWorldWidget(UUserWidget* Widget, FName Anchor)
{
    if (!Widget) return;
    const auto* Route = FindRoute(Anchor);
    auto* PC = GetWorld()->GetFirstPlayerController();
    if (!Route || !PC) return;
    if (PresentedWidget != Widget)
    {
        Widget->RemoveFromParent();
        PresentedWidget = Widget;
        MWWorldSurface->SetOwnerPlayer(PC->GetLocalPlayer());
        MWWorldSurface->SetWidget(Widget);
        // Applying GameAndUI releases mouse capture. Do it once when the
        // presentation changes, not every frame between press and release.
        UseWorldMouse();
    }
    const FTransform& PanelPose = Route->WidgetPose;
    if (!MWWorldSurface->GetComponentTransform().Equals(PanelPose))
        MWWorldSurface->SetWorldTransform(PanelPose);
    const bool bShowWidget = !bWorldRouteActive || bWorldRoutePresentationReady;
    if (MWWorldSurface->IsVisible() != bShowWidget)
        MWWorldSurface->SetVisibility(bShowWidget);
    const ECollisionEnabled::Type DesiredCollision = bShowWidget ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision;
    if (MWWorldSurface->GetCollisionEnabled() != DesiredCollision)
        MWWorldSurface->SetCollisionEnabled(DesiredCollision);
    if (Widget->GetVisibility() != ESlateVisibility::SelfHitTestInvisible)
        Widget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void AMWUCMTitleFlow::PresentEntryWidget(UWidget* Screen, bool bMenuRequested)
{
    if (!bStarted) return;
    if (auto* Widget = Cast<UUserWidget>(Screen); Cast<UMWCreateRoomWidget>(Widget) || Cast<UMWRoomBrowserWidget>(Widget))
    {
        // Preserve MW's existing screen window; only its UCM background camera changes.
        if (ActiveRegion != TEXT("RoomPanel")) StartWorldRoute(TEXT("RoomPanel"), false, TEXT("RoomPanel"));
    }
    else if (bMenuRequested && ActiveRegion == TEXT("RoomPanel"))
    {
        MWWorldSurface->SetWidget(nullptr);
        PresentedWidget = nullptr;
        StartWorldRoute(TEXT("RoomPanel"), true, TEXT("Main"));
    }
}

void AMWUCMTitleFlow::UpdateLobbyPresentation()
{
    if (RootWidget || !bStarted) return;
    auto* PC = GetWorld()->GetFirstPlayerController();
    TArray<UUserWidget*> Widgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, UMWRoomLobbyWidget::StaticClass(), false);
    for (auto* Widget : Widgets)
    {
        if (Widget->GetOwningPlayer() != PC || (!Widget->IsInViewport() && Widget != LobbyWidget)) continue;
        LobbyWidget = Widget;
        if (ActiveRegion == TEXT("Lobby")) SetWorldWidget(Widget, TEXT("Lobby"));
        break;
    }
}

bool AMWUCMTitleFlow::ShowVehicleSelection()
{
    if (!bStarted || (bWorldRouteActive && !bWorldRoutePresentationReady) || !VehiclePreviewActor || !GetWorld()->IsPlayInEditor()) return false;
    auto* PC = GetWorld()->GetFirstPlayerController();
    if (!PC) return false;
    const FName Route = ActiveRegion == TEXT("Lobby") ? TEXT("Vehicle") : TEXT("MainVehicle");
    if (!FindRoute(Route)) return false;
    if (!StartWorldRoute(Route, false, TEXT("Vehicle"))) return false;
    VehiclePreviewActor->PreviewViewTarget = WorldRouteCamera;
    VehiclePreviewActor->OnPreviewClosed.AddUniqueDynamic(this, &ThisClass::CloseVehicleSelection);
    if (!VehiclePreviewActor->OpenPreview())
    {
        CloseVehicleSelection();
        return false;
    }
    FitVehiclePreview(Cast<UMWVehicleSelectionWidget>(VehiclePreviewActor->PreviewSurface->GetUserWidgetObject()));
    VehiclePreviewActor->PreviewSurface->SetVisibility(false);
    VehiclePreviewActor->PreviewSurface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    return true;
}

void AMWUCMTitleFlow::CloseVehicleSelection()
{
    if (ActiveRegion != TEXT("Vehicle")) return;
    if (VehiclePreviewActor && VehiclePreviewActor->PreviewSurface->IsVisible())
    {
        VehiclePreviewActor->ClosePreview();
        return;
    }
    MWWorldSurface->SetWidget(nullptr);
    PresentedWidget = nullptr;
    if (LobbyWidget)
    {
        StartWorldRoute(TEXT("Vehicle"), true, TEXT("Lobby"));
        SetWorldWidget(LobbyWidget, TEXT("Lobby"));
    }
    else StartWorldRoute(TEXT("MainVehicle"), true, TEXT("Main"));
}

void AMWUCMTitleFlow::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    UpdateWorldRoute(DeltaTime);
    UpdateLobbyPresentation();
    auto* Session = GetGameInstance()->GetSubsystem<UMWUCMIntroSession>();
    if (!Session || !bMainPresentationRequested) return;

    if (Session->IsReturnWorldReady(GetWorld()))
    {
        if (!IsValid(RootWidget) || !RootWidget->IsInViewport() || !IsValid(MenuCamera))
        {
            Session->SetPreparationFailure(GetWorld(), IsValid(MenuCamera) ? TEXT("MenuRootMissing") : TEXT("MenuCameraMissing"));
            return;
        }
        TInlineComponentArray<UUCMWorldWidgetComponent*> Canvases(this);
        const int32 VisibleMenuCount = Algo::CountIf(Canvases, [](const UUCMWorldWidgetComponent* Canvas)
        {
            return Canvas->GetName().StartsWith(TEXT("SourceMenu_")) && Canvas->IsRegistered()
                && Canvas->IsVisible() && Canvas->GetUserWidgetObject();
        });
        APlayerController* PC = GetWorld()->GetFirstPlayerController();
        const APlayerCameraManager* Camera = PC ? PC->PlayerCameraManager.Get() : nullptr;
        if ((PreviewStage == 3 || PreviewStage == 2) && VisibleMenuCount > 0 && VisibleMenuCount == SourceMenuEntries.Num()
            && PC && PC->IsLocalController() && PC->GetWorld() == GetWorld() && PC->GetViewTarget() == MenuCamera
            && Camera && Camera->GetCameraCacheTime() > ReturnCameraStartedAt)
        {
            const FMinimalViewInfo& View = Camera->GetCameraCacheView();
            if (!View.Location.ContainsNaN() && !View.Rotation.ContainsNaN() && FMath::IsFinite(View.FOV) && View.FOV > 0.f)
            {
                Session->RevealReturnedWorld(PC, MenuCamera, RootWidget);
                return;
            }
        }
        Session->SetPreparationFailure(GetWorld(), VisibleMenuCount != SourceMenuEntries.Num() || VisibleMenuCount == 0
            ? TEXT("MenuCanvasesNotReady") : TEXT("MenuCameraNotReady"));
    }
    if (PreviewStage == 2 && Session->FinishReturnPresentation(GetWorld()))
    {
        RefreshRoot();
        if (!bSuppressPreviewInput) RecordRoomReturn(TEXT("MENU_INPUT_ENABLED"));
    }
}
