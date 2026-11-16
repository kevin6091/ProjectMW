#include "MWUCMTitleFlow.h"
#include "MWUCMWorldMusic.h"

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
#include "Widgets/Layout/SBox.h"
#include "InputCoreTypes.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
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
#include "Misc/PackageName.h"
#include "UI/Lobby/MWCreateRoomWidget.h"
#include "UI/Lobby/MWRoomBrowserWidget.h"
#include "UI/Lobby/MWRoomLobbyWidget.h"
#include "UI/Gameplay/MWGameplaySign.h"
#include "UI/VehicleSelection/MWVehicleSelectionWidget.h"
#include "UI/VehicleSelection/MWVehiclePlayerMarkerWidget.h"
#include "UI/Cursor/MWPointerCursor.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "UI/VehicleSelection/MWVehicleSelectionPreviewActor.h"
#include "System/MWGameInstance.h"
#include "System/MWPlayerState.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, MWUCMFrontend)

namespace
{
    class SMWHostLeaveConfirmation : public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(SMWHostLeaveConfirmation)
        {
        }
        SLATE_EVENT(FSimpleDelegate, OnConfirm)
        SLATE_EVENT(FSimpleDelegate, OnCancel)
        SLATE_END_ARGS()

        // 호스트 종료 안내와 확인·취소 버튼을 기준 디자인 비율로 배치한다.
        void Construct(const FArguments& Args)
        {
            OnConfirm = Args._OnConfirm;
            OnCancel = Args._OnCancel;
            const auto* Brush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
            const auto Action = [this](const FText& Key, const FText& Text, bool bConfirmed)
            {
                return SNew(SButton)
                    .IsFocusable(false)
                    .ButtonStyle(FCoreStyle::Get(), TEXT("NoBorder"))
                    .ContentPadding(FMargin(16.f, 2.f))
                    .OnClicked(
                        this, &SMWHostLeaveConfirmation::Choose,
                        bConfirmed)[SNew(SHorizontalBox) +
                                    SHorizontalBox::Slot().AutoWidth().VAlign(
                                        VAlign_Center)[SNew(SBox).WidthOverride(66.f).HeightOverride(
                                        54.f)[SNew(SBorder)
                                                  .BorderImage(&Keycap)
                                                  .HAlign(HAlign_Center)
                                                  .VAlign(VAlign_Center)
                                                  .Padding(0.f)[SNew(STextBlock)
                                                                    .Text(Key)
                                                                    .Font(Art.Font(14.f))
                                                                    .ColorAndOpacity(FLinearColor::White)]]] +
                                    SHorizontalBox::Slot()
                                        .AutoWidth()
                                        .VAlign(VAlign_Center)
                                        .Padding(13.f, 0.f, 0.f, 0.f)[SNew(STextBlock)
                                                                          .Text(Text)
                                                                          .Font(Art.Font(36.f))
                                                                          .ColorAndOpacity(FLinearColor::White)]];
            };

            ChildSlot
                [SNew(SBorder)
                     .BorderImage(Brush)
                     .BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, .77f))
                     .Padding(0.f)[MWGameplayUI::FitDesignCanvas(
                         SNew(SOverlay) +
                         SOverlay::Slot()
                             .HAlign(HAlign_Center)
                             .VAlign(VAlign_Center)
                                 [SNew(SMWGameplayPanel)
                                      .Size(FVector2D(960.f, 432.f))
                                      .Fill(FLinearColor::FromSRGBColor(FColor(16, 162, 220)))
                                      .Border(FLinearColor::FromSRGBColor(FColor(9, 46, 68)))
                                      .BorderWidth(6.f)
                                          [SNew(SVerticalBox) +
                                           SVerticalBox::Slot().AutoHeight().Padding(70.f, 86.f, 70.f, 20.f)
                                               [SNew(STextBlock)
                                                    .Text(NSLOCTEXT(
                                                        "MWLobby", "HostLeaveWarning",
                                                        "방을 종료하면 다른 모든 플레이어가\n로비에서 나갑니다. 정말 계속할까요?"))
                                                    .Justification(ETextJustify::Center)
                                                    .Font(Art.Font(32.f))
                                                    .ColorAndOpacity(FLinearColor::White)] +
                                           SVerticalBox::Slot()
                                               .AutoHeight()
                                               .HAlign(HAlign_Center)
                                               .Padding(0.f, 0.f, 0.f, 7.f)[Action(
                                                   NSLOCTEXT("MWLobby", "ConfirmKey", "SPACE"),
                                                   NSLOCTEXT("MWLobby", "ConfirmHostLeave", "확인"), true)] +
                                           SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Action(
                                               NSLOCTEXT("MWLobby", "CancelKey", "Esc"),
                                               NSLOCTEXT("MWLobby", "CancelHostLeave", "취소"), false)]]])]];
        }

        virtual bool SupportsKeyboardFocus() const override
        {
            return true;
        }

        // 확인·취소 입력만 이 대화상자에서 처리해 뒤쪽 메뉴로 전달되지 않게 한다.
        virtual FReply OnKeyDown(const FGeometry&, const FKeyEvent& Event) override
        {
            if (Event.IsRepeat())
                return FReply::Handled();
            if (Event.GetKey() == EKeys::SpaceBar)
                return Choose(true);
            if (Event.GetKey() == EKeys::Escape)
                return Choose(false);
            return FReply::Handled();
        }

        virtual FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent&) override
        {
            return FReply::Handled();
        }

        virtual FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent&) override
        {
            return FReply::Handled();
        }

    private:
        // 현재 선택한 확인 또는 취소 콜백을 한 경로로 전달한다.
        FReply Choose(bool bConfirmed)
        {
            const FSimpleDelegate Action = bConfirmed ? OnConfirm : OnCancel;
            Action.ExecuteIfBound();
            return FReply::Handled();
        }

        FSimpleDelegate OnConfirm;
        FSimpleDelegate OnCancel;
        FMWGameplayUIArt Art;
        FSlateRoundedBoxBrush Keycap{FLinearColor(.015f, .020f, .025f), 6.f};
    };
}

namespace
{
    constexpr double ReturnSessionWarningSeconds = 6.;
    constexpr double ReturnPreparationTimeoutSeconds = 8.;
    constexpr float VehiclePreviewBoardScale = 1.33f;

    // 기존 차량 선택 위젯 인스턴스만 게시판 구도에 맞추고 공유 WBP 디자인은 덮어쓰지 않는다.
    void FitVehiclePreview(UMWVehicleSelectionWidget* Widget)
    {
        if (!Widget)
            return;

        // UCM 화면도 기존 차량 레이아웃을 사용한다. 공유 클래스 대신 이 인스턴스만
        // 월드 게시판의 1920×1080 캔버스에 맞춘다.
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

        const FName LegendWidgets[] = {TEXT("Legend_Select"),        TEXT("Legend_Cancel"),
                                       TEXT("SelectionStatePanel"),  TEXT("SelectionLegendRow"),
                                       TEXT("Button_SecretVehicle"), TEXT("ArcadeLegendStrip"),
                                       TEXT("ArcadeLegendBolt0"),    TEXT("ArcadeLegendBolt1"),
                                       TEXT("ArcadeLegendBolt2"),    TEXT("ArcadeLegendBolt3")};
        for (FName Name : LegendWidgets)
        {
            if (auto* Slot = FindCanvasSlot(Name))
                Slot->SetPosition(Slot->GetPosition() - FVector2D(0, 70));
        }
        // 카드 표시는 확대된 카드 크기를 따르지만 움직이는 커서는
        // Slate에서 그리므로 원래 Viewport 크기를 유지한다.
        Widget->SetWorldPreviewMarkerScales(1.f / VehiclePreviewBoardScale, .58f);
    }

    // 실제 경과 시간과 렌더 프레임으로 복귀 지점을 기록한다. 캡처 간격을 프레임 시간으로 대신하지 않는다.
    void RecordRoomReturn(const TCHAR* Event)
    {
        CSV_EVENT_GLOBAL_NOLOG(TEXT("MW_RETURN_%s"), Event);
        UE_LOG(LogTemp, Display, TEXT("MW_RETURN %s MS=%.3f FRAME=%llu"), Event, FPlatformTime::Seconds() * 1000.,
               GFrameCounter);
    }

}

// 기존 세션 상태와 맵 로드 이벤트를 구독하고 복귀 상태 점검을 등록한다.
void UMWUCMIntroSession::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    if (auto* Sessions = Cast<UMWGameInstance>(GetGameInstance()))
    {
        Sessions->OnSessionStateChanged.AddUniqueDynamic(this, &ThisClass::HandleRoomPresentationState);
        Sessions->OnSessionError.AddUniqueDynamic(this, &ThisClass::HandleRoomReturnError);
    }
    PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &ThisClass::CoverMapBoundary);
    PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::OnReturnMapLoaded);
    CoverTickHandle =
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::TickReturnCover));
}

// 게임 인스턴스 종료 시 구독·가림막을 해제하고 교체 전 커서 설정을 복원한다.
void UMWUCMIntroSession::Deinitialize()
{
    if (auto* Sessions = Cast<UMWGameInstance>(GetGameInstance()))
    {
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
        if (bRestored)
            CursorViewport->SetUseSoftwareCursorWidgets(bPreviousSoftwareCursors);
    }
    FrontendCursorSlate.Reset();
    FrontendCursor = nullptr;
    FTSTicker::GetCoreTicker().RemoveTicker(CoverTickHandle);
    FCoreUObjectDelegates::PreLoadMapWithContext.Remove(PreLoadMapHandle);
    FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
    Super::Deinitialize();
}

// 차량 선택과 같은 커서 외형을 프런트엔드에 설치하고 기존 설정은 종료 시 복구한다.
void UMWUCMIntroSession::InstallFrontendCursor()
{
    if (FrontendCursorSlate.IsValid())
        return;
    auto* Viewport = GetGameInstance()->GetGameViewportClient();
    auto* Class = PointerWidgetClass.LoadSynchronous();
    if (!Viewport || !Class)
        return;
    FrontendCursor = CreateWidget<UMWVehiclePlayerMarkerWidget>(GetGameInstance(), Class);
    if (!FrontendCursor)
        return;
    FrontendCursor->TakeWidget();
    FMWVehiclePlayerPresentation Player;
    Player.bLocal = true;
    Player.PlayerTint = FLinearColor(.8f, .05f, .08f, 1.f);
    FrontendCursor->SetPresentation(Player);
    FrontendCursor->SetRenderOpacity(1.f);
    FrontendCursor->SetVisibility(ESlateVisibility::HitTestInvisible);
    const TWeakObjectPtr<UMWUCMIntroSession> WeakThis(this);
    FrontendCursorSlate = MWMakePointerCursor(
        FrontendCursor,
        TAttribute<FVector2D>::CreateLambda(
            [WeakThis]()
            {
                return FVector2D(154.834854f) *
                       (WeakThis.IsValid() ? UWidgetLayoutLibrary::GetViewportScale(WeakThis.Get()) : 1.f);
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

// 복귀를 시작할 수 있는 실제 로비 월드를 기록한다. 같은 게임 인스턴스인지 확인한다.
void UMWUCMIntroSession::SetRoomPresentationWorld(UWorld* World)
{
    if (World && World->GetGameInstance() == GetGameInstance() && World->GetNetMode() != NM_DedicatedServer)
        RoomPresentationWorld = World;
}

// 복귀 단계가 바뀐 이유를 추적할 수 있도록 이전·다음 상태와 실제 시간을 기록한다.
void UMWUCMIntroSession::SetReturnPhase(EReturnCoverPhase Phase)
{
    static const TCHAR* const Names[] = {TEXT("Hidden"), TEXT("WaitingForSession"), TEXT("Traveling"),
        TEXT("WorldReady"), TEXT("Failed")};
    UE_LOG(LogTemp, Display, TEXT("MW_RETURN PHASE %s -> %s MS=%.3f"), Names[int32(ReturnCoverPhase)],
           Names[int32(Phase)], FPlatformTime::Seconds() * 1000.);
    ReturnCoverPhase = Phase;
    CoverChangedAt = FPlatformTime::Seconds();
}

// 현재 게임 인스턴스의 Viewport에 가림막을 한 번만 연결한다.
void UMWUCMIntroSession::EnsureReturnCover()
{
    auto* Viewport = GetGameInstance()->GetGameViewportClient();
    if (!Viewport)
        return;
    if (ReturnCover && CoverViewport.Get() == Viewport)
        return;
    if (ReturnCover && CoverViewport.IsValid())
        CoverViewport->RemoveViewportWidgetContent(ReturnCover.ToSharedRef());
    ReturnCover = SNew(SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
        .BorderBackgroundColor(FLinearColor(0, 0, 0, .85f));
    CoverViewport = Viewport;
    Viewport->AddViewportWidgetContent(ReturnCover.ToSharedRef(), 10000);
}

void UMWUCMIntroSession::BeginRoomReturn(UWorld* World, bool bWasParticipant)
{
    if (!World || World != RoomPresentationWorld.Get() || !GetGameInstance()->GetGameViewportClient()) return;
    if (ReturnCoverPhase != EReturnCoverPhase::Hidden && ReturnCoverPhase != EReturnCoverPhase::Failed) return;
    if (ReturnCoverPhase == EReturnCoverPhase::Failed && World != ReturnSourceWorld.Get()) return;
    if (ReturnCoverPhase == EReturnCoverPhase::Hidden) bReturnToRoomBrowser = bWasParticipant;
    ReturnSourceWorld = World;
    ReturnTargetWorld.Reset();
    ReturnViewFOV = 0.f;
    PreparationFailure.Reset();
    ExpectedReturnMap = NAME_None;
    ClearReturnStatus();
    SetReturnPhase(EReturnCoverPhase::WaitingForSession);
}

// 기존 연결 상태가 방 종료로 바뀌면 복귀 표시를 시작한다. 세션 종료 자체를 구현하지 않는다.
void UMWUCMIntroSession::HandleRoomPresentationState(EMWSessionState PreviousState, EMWSessionState NewState)
{
    if (NewState == EMWSessionState::Leaving)
        BeginRoomReturn(GetGameInstance()->GetWorld(), PreviousState == EMWSessionState::Joined);
    else if (NewState == EMWSessionState::CleanupRequired && IsRoomReturnPending())
        FailReturn(TEXT("SessionCleanupRequired"),
                   NSLOCTEXT("MWReturn", "CleanupFailed", "방 종료를 완료하지 못했습니다."), true);
}

// 기존 연결 서비스의 정리 오류를 현재 복구 안내에 반영한다.
void UMWUCMIntroSession::HandleRoomReturnError(const FString& Error)
{
    if (ReturnCoverPhase == EReturnCoverPhase::Failed && bStatusRetry && !Error.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("MW_RETURN SESSION_ERROR %s"), *Error);
        ShowReturnStatus(FText::FromString(Error), true);
    }
}

// 이 게임 인스턴스의 이동 시작만 받아 예상 맵과 마지막 로비 시점을 기록한다.
void UMWUCMIntroSession::CoverMapBoundary(const FWorldContext& Context, const FString& MapName)
{
    if (Context.OwningGameInstance == GetGameInstance() && ReturnCoverPhase == EReturnCoverPhase::WaitingForSession)
    {
        ExpectedReturnMap = FName(*FPackageName::ObjectPathToPackageName(UWorld::RemovePIEPrefix(MapName)));
        UWorld* Source = ReturnSourceWorld.Get();
        if (Source && ExpectedReturnMap == FName(*UWorld::RemovePIEPrefix(Source->GetOutermost()->GetName())))
        {
            const auto* PC = GetGameInstance()->GetFirstLocalPlayerController(Source);
            if (const auto* Camera = PC ? PC->PlayerCameraManager.Get() : nullptr)
            {
                const FMinimalViewInfo& View = Camera->GetCameraCacheView();
                if (!View.Location.ContainsNaN() && !View.Rotation.ContainsNaN() && FMath::IsFinite(View.FOV) && View.FOV > 0.f)
                {
                    // Use the last visible lobby pose as the first view in the reloaded scene.
                    ReturnViewPose = FTransform(View.Rotation, View.Location);
                    ReturnViewFOV = View.FOV;
                    RecordRoomReturn(TEXT("LOBBY_CAMERA_SAVED"));
                }
            }
        }
        ClearReturnStatus();
        SetReturnPhase(EReturnCoverPhase::Traveling);
        RecordRoomReturn(TEXT("MAP_LOAD_START"));
    }
}

// 로드된 월드가 이전 월드와 다르고 예상 맵인지 확인한 뒤 준비 상태로 넘긴다.
void UMWUCMIntroSession::OnReturnMapLoaded(UWorld* World)
{
    if (ReturnCoverPhase != EReturnCoverPhase::Traveling || !World || World->GetGameInstance() != GetGameInstance())
        return;
    const FName Package(*UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()));
    if (World == ReturnSourceWorld.Get() || World->GetNetMode() != NM_Standalone || Package != ExpectedReturnMap)
    {
        FailReturn(TEXT("UnexpectedReturnWorld"),
                   NSLOCTEXT("MWReturn", "WrongWorld", "타이틀 맵을 확인할 수 없습니다. 게임을 다시 실행해 주세요."));
        return;
    }
    ReturnTargetWorld = World;
    SetReturnPhase(EReturnCoverPhase::WorldReady);
    RecordRoomReturn(TEXT("WORLD_READY"));
}

// 예상한 새 월드가 로드됐는지 확인한다. 메뉴 표시 준비까지 끝났다는 뜻은 아니다.
bool UMWUCMIntroSession::IsReturnWorldReady(const UWorld* World) const
{
    return ReturnCoverPhase == EReturnCoverPhase::WorldReady && World && ReturnTargetWorld.Get() == World &&
           GetGameInstance()->GetWorld() == World;
}

// 카메라나 위젯 준비가 막힌 이유를 기록한다. 준비되지 않은 화면을 강제로 노출하지 않는다.
void UMWUCMIntroSession::SetPreparationFailure(const UWorld* World, const TCHAR* Reason)
{
    if (IsReturnWorldReady(World) && PreparationFailure != Reason)
    {
        PreparationFailure = Reason;
        UE_LOG(LogTemp, Display, TEXT("MW_RETURN PREPARATION_WAIT %s"), Reason);
    }
}

bool UMWUCMIntroSession::GetReturnView(const UWorld* World, FTransform& Pose, float& FOV) const
{
    if (!IsReturnWorldReady(World) || ReturnViewFOV <= 0.f) return false;
    Pose = ReturnViewPose;
    FOV = ReturnViewFOV;
    return true;
}

bool UMWUCMIntroSession::FinishReturnPresentation(APlayerController* PC, AActor* ViewTarget, UUserWidget* Root)
{
    if (!IsValid(PC) || !PC->IsLocalController() || !IsReturnWorldReady(PC->GetWorld()) ||
        !IsValid(ViewTarget) || ViewTarget->GetWorld() != PC->GetWorld() || PC->GetViewTarget() != ViewTarget ||
        !IsValid(Root) || !Root->IsInViewport() || Root->GetOwningPlayer() != PC)
        return false;
    RemoveReturnCover();
    return true;
}

// 실패·대기 사유와 가능한 재시도·종료 동작을 표시하고 키보드 포커스를 안내로 옮긴다.
void UMWUCMIntroSession::ShowReturnStatus(const FText& Message, bool bRetry)
{
    StatusMessage = Message;
    bStatusRetry = bRetry;
    EnsureReturnCover();
    if (!ReturnCover)
        return;
    ReturnCover->SetVisibility(EVisibility::Visible);
    TSharedPtr<SButton> ExitButton;
    const FSlateFontInfo MessageFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 26);
    const FSlateFontInfo ButtonFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 22);
    ReturnCover->SetHAlign(HAlign_Center);
    ReturnCover->SetVAlign(VAlign_Center);
    ReturnCover->SetContent(
        SNew(SVerticalBox) +
        SVerticalBox::Slot().AutoHeight().Padding(12.f)
            [SNew(STextBlock).Font(MessageFont).Text(Message).WrapTextAt(480.f).Justification(ETextJustify::Center)] +
        SVerticalBox::Slot().AutoHeight().Padding(12.f)
            [SNew(SButton)
                 .HAlign(HAlign_Center)
                 .ContentPadding(FMargin(16.f, 8.f))
                 .Visibility(bRetry ? EVisibility::Visible : EVisibility::Collapsed)
                 .OnClicked_UObject(this, &ThisClass::RetryReturn)
                     [SNew(STextBlock).Font(ButtonFont).Text(NSLOCTEXT("MWReturn", "Retry", "세션 정리 다시 시도"))]] +
        SVerticalBox::Slot().AutoHeight().Padding(
            12.f)[SAssignNew(ExitButton, SButton)
                      .HAlign(HAlign_Center)
                      .ContentPadding(FMargin(16.f, 8.f))
                      .OnClicked_UObject(this, &ThisClass::ExitAfterReturnFailure)
                          [SNew(STextBlock).Font(ButtonFont).Text(NSLOCTEXT("MWReturn", "Exit", "게임 종료"))]]);
    if (FrontendCursor)
        FrontendCursor->SetRenderOpacity(1.f);
    if (FSlateApplication::IsInitialized())
        FSlateApplication::Get().SetKeyboardFocus(ExitButton);
}

// 기존 안내 내용을 지우고 필요하지 않은 상태 패널을 숨긴다.
void UMWUCMIntroSession::ClearReturnStatus()
{
    StatusMessage = FText::GetEmpty();
    bStatusRetry = false;
    if (!ReturnCover)
        return;
    ReturnCover->SetVisibility(EVisibility::Collapsed);
}

// 실패 이유를 남기고 재시도 가능 여부에 맞춰 안내를 표시한다.
void UMWUCMIntroSession::FailReturn(const TCHAR* Reason, const FText& Message, bool bRetry)
{
    UE_LOG(LogTemp, Warning, TEXT("MW_RETURN FAILED %s PREPARATION=%s"), Reason, *PreparationFailure);
    if (ReturnCoverPhase != EReturnCoverPhase::Failed)
        SetReturnPhase(EReturnCoverPhase::Failed);
    ShowReturnStatus(Message, bRetry);
}

// 이전 로비 월드가 아직 유효할 때만 기존 세션 정리 요청을 다시 보낸다.
FReply UMWUCMIntroSession::RetryReturn()
{
    if (auto* Sessions = Cast<UMWGameInstance>(GetGameInstance()); Sessions && ReturnSourceWorld.IsValid()
        && ReturnSourceWorld.Get() == GetGameInstance()->GetWorld())
        Sessions->RetrySessionCleanup();
    return FReply::Handled();
}

// 현재 유효한 로컬 컨트롤러 또는 Viewport를 통해 명시적으로 실행을 종료한다.
FReply UMWUCMIntroSession::ExitAfterReturnFailure()
{
    UWorld* World = GetGameInstance()->GetWorld();
    if (auto* PC = GetGameInstance()->GetFirstLocalPlayerController(World))
        UKismetSystemLibrary::QuitGame(World, PC, EQuitPreference::Quit, false);
    else if (GIsEditor)
    {
        if (auto* Viewport = CoverViewport.Get())
            Viewport->CloseRequested(Viewport->Viewport);
    }
    else
        FPlatformMisc::RequestExit(false);
    return FReply::Handled();
}

// 화면 가림막과 이전·새 월드 참조를 해제하고 커서 표시를 복구한다.
void UMWUCMIntroSession::RemoveReturnCover()
{
    if (CoverViewport.IsValid() && ReturnCover.IsValid())
        CoverViewport->RemoveViewportWidgetContent(ReturnCover.ToSharedRef());
    ReturnCover.Reset();
    CoverViewport.Reset();
    ClearReturnStatus();
    ReturnSourceWorld.Reset();
    ReturnTargetWorld.Reset();
    ReturnViewFOV = 0.f;
    bReturnToRoomBrowser = false;
    if (IsRoomReturnPending()) SetReturnPhase(EReturnCoverPhase::Hidden);
    if (FrontendCursor) FrontendCursor->SetRenderOpacity(1.f);
}

// 엔진 틱에서 응답·이동·표시 준비 지연을 확인한다. 게임 스레드 정지를 대신 처리하는 기능은 아니다.
bool UMWUCMIntroSession::TickReturnCover(float DeltaTime)
{
    if (!IsRoomReturnPending()) return true;
    const double Elapsed = FPlatformTime::Seconds() - CoverChangedAt;
    if (ReturnCoverPhase == EReturnCoverPhase::WaitingForSession)
    {
        if (!ReturnSourceWorld.IsValid() || ReturnSourceWorld.Get() != GetGameInstance()->GetWorld())
        {
            FailReturn(
                TEXT("ReturnSourceWorldLost"),
                NSLOCTEXT("MWReturn", "SourceLost", "이전 로비를 확인할 수 없습니다. 게임을 다시 실행해 주세요."));
        }
        else if (Elapsed >= ReturnSessionWarningSeconds && StatusMessage.IsEmpty())
        {
            ShowReturnStatus(NSLOCTEXT("MWReturn", "WaitForSession", "방 종료 응답을 기다리는 중입니다."), false);
        }
    }
    else if (ReturnCoverPhase == EReturnCoverPhase::Traveling && Elapsed >= ReturnPreparationTimeoutSeconds)
        FailReturn(
            TEXT("TravelCompletionMissing"),
            NSLOCTEXT("MWReturn", "TravelTimeout", "타이틀 맵 이동을 확인할 수 없습니다. 게임을 다시 실행해 주세요."));
    else if (ReturnCoverPhase == EReturnCoverPhase::WorldReady && Elapsed >= ReturnPreparationTimeoutSeconds)
        FailReturn(TEXT("TitlePresentationNotReady"),
                   NSLOCTEXT("MWReturn", "TitleNotReady",
                             "타이틀 카메라 또는 메뉴를 준비하지 못했습니다. 게임을 다시 실행해 주세요."));
    else if (ReturnCoverPhase == EReturnCoverPhase::Failed && bStatusRetry &&
             (!ReturnSourceWorld.IsValid() || ReturnSourceWorld.Get() != GetGameInstance()->GetWorld()))
    {
        RecordRoomReturn(TEXT("RECOVERY_SOURCE_WORLD_UNAVAILABLE"));
        ShowReturnStatus(NSLOCTEXT("MWReturn", "RetryWorldLost", "이전 로비가 닫혔습니다. 게임을 다시 실행해 주세요."),
                         false);
    }
    return true;
}

// 기존 루트의 생성 수명을 그대로 따르고 화면 기능을 중복 생성하지 않는다.
void UMWUCMTitleRootWidget::NativeConstruct()
{
    Super::NativeConstruct();
}

// 기존 루트가 등록한 위젯·요청 연결의 종료 처리를 유지한다.
void UMWUCMTitleRootWidget::NativeDestruct()
{
    Super::NativeDestruct();
}

// UCM 표지판 선택을 기존 MW의 방 생성·검색·종료 버튼 처리로 전달한다.
void UMWUCMTitleRootWidget::ActivateSourceMenu(int32 Index)
{
    auto* Music = GetWorld()->GetSubsystem<UMWUCMWorldMusic>();
    if (Index == 2)
    {
        if (Music)
            Music->PlayMenuConfirm();
        HandleQuit();
        return;
    }

    const FName ButtonName = Index == 0 ? TEXT("Button_CreateRoom") : TEXT("Button_JoinRoom");
    auto* Button = TitleScreen ? Cast<UButton>(TitleScreen->GetWidgetFromName(ButtonName)) : nullptr;
    if (!Button || !Button->GetIsEnabled())
        return;

    if (Music)
        Music->PlayMenuConfirm();
    Button->OnClicked.Broadcast();
}

// 기존 소유 플레이어의 정상 게임 종료 경로를 사용한다.
void UMWUCMTitleRootWidget::HandleQuit()
{
    UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

// MW 메인 화면의 중복 표시를 막고 실제 표지판 표시는 UCM 연출에 맡긴다.
bool UMWUCMTitleRootWidget::CanPresentEntryMenu(bool bLoggedIn)
{
    if (IsValid(Flow))
        Flow->IsMenuReady(bLoggedIn);
    // 메뉴는 UCM의 원래 표지판으로 표시하고 MW의 기존 버튼 처리와 입력 폼은 유지한다.
    return false;
}

// 기존 루트가 고른 로그인·메뉴 상태를 현재 월드 연출에 전달한다.
void UMWUCMTitleRootWidget::OnEntryPresentationUpdated(bool bLoggedIn, bool bMenuRequested)
{
    if (IsValid(Flow))
        Flow->ApplyEntryPresentation(bLoggedIn, bMenuRequested);
}

// 카메라 이동 중에는 대체 영역을 사용하고 준비된 기존 화면을 표시한다.
UWidget* UMWUCMTitleRootWidget::ResolveEntryPresentation(UWidget* Screen, bool bMenuRequested)
{
    if (IsValid(Flow))
    {
        Flow->PresentEntryWidget(Screen, bMenuRequested);
        if (WorldMenuPlaceholder && (bMenuRequested || !Flow->IsEntryPanelReady()))
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

// 기존 연출과 MW 위젯 이벤트를 연결하고 로컬 컨트롤러 준비 뒤 루트를 초기화한다.
void AMWUCMTitleFlow::BeginPlay()
{
    Super::BeginPlay();
    if (GetNetMode() == NM_DedicatedServer) return;
    if (auto* Session = GetGameInstance()->GetSubsystem<UMWUCMIntroSession>())
    {
        Session->InstallFrontendCursor();
        if (GetWorld()->GetNetMode() != NM_Standalone)
            Session->SetRoomPresentationWorld(GetWorld());
    }
    TitleCanvas->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MenuCanvas->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    // 기존 MW 로그인이 성공하기 전에는 메뉴 입력 확인·배경 동작·음악을 시작하지 않는다.
    SetActorTickEnabled(false);
    PresentationHandle = PresentationChanged.AddUObject(this, &ThisClass::RefreshRoot);
    MenuActionHandle = ExternalMenuAction.AddUObject(this, &ThisClass::HandleSourceMenu);
    SetMainMenuPresentation(false, false);
    MWWorldSurface->SetVisibility(false);
    MWWorldSurface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (IsValid(VehiclePreviewActor) && VehiclePreviewActor->GetWorld() == GetWorld())
        VehiclePreviewActor->SetActorScale3D(VehiclePreviewActor->GetActorScale3D() * VehiclePreviewBoardScale);
    FActorSpawnParameters CameraSpawn;
    CameraSpawn.Name = TEXT("MWUCM_WorldRouteCamera");
    WorldRouteCamera =
        GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, CameraSpawn);
    if (WorldRouteCamera)
    {
        WorldRouteCamera->GetCameraComponent()->SetConstraintAspectRatio(true);
        WorldRouteCamera->GetCameraComponent()->SetAspectRatio(16.f / 9.f);
    }
    InitializeHandle = GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::InitializeRoot);
}

// 현재 월드와 로컬 컨트롤러를 확인하고 Standalone 진입과 실제 네트워크 로비 표시를 구분한다.
void AMWUCMTitleFlow::InitializeRoot()
{
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    auto* Session = GetGameInstance()->GetSubsystem<UMWUCMIntroSession>();
    if (!IsValid(PC) || !PC->IsLocalController() || PC->GetWorld() != GetWorld() || !RootWidgetClass)
    {
        if (Session)
            Session->SetPreparationFailure(GetWorld(), RootWidgetClass ? TEXT("LocalControllerMissing")
                                                                       : TEXT("RootWidgetClassMissing"));
        if (!RootWidgetClass)
            UE_LOG(LogTemp, Error, TEXT("MW_UCM RootWidgetClass is missing in %s"), *GetPathName());
        if (RootWidgetClass)
            InitializeHandle = GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::InitializeRoot);
        return;
    }
    // The streamed scene presents the real HUD widgets in the network lobby.
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
        SetMainMenuPresentation(false, false);
        StartFidelity();
        SetActorTickEnabled(true);
        StartWorldRoute(TEXT("Lobby"), false, TEXT("Lobby"));
        UE_LOG(LogTemp, Display, TEXT("MW_UCM ROOM_WORLD_LOBBY_NO_INTRO"));
        return;
    }
    RootWidget = CreateWidget<UMWUCMTitleRootWidget>(PC, RootWidgetClass);
    if (!RootWidget)
    {
        UE_LOG(LogTemp, Error, TEXT("MW_UCM Failed to create %s in %s"), *GetNameSafe(RootWidgetClass.Get()), *GetPathName());
        if (Session) Session->SetPreparationFailure(GetWorld(), TEXT("RootWidgetCreationFailed"));
        return;
    }
    RootWidget->Flow = this;
    if (Session && Session->IsReturnWorldReady(GetWorld()))
    {
        const auto* App = GetGameInstance<UMWGameInstance>();
        IsMenuReady(App && (!App->IsLoginRequired() || App->IsLoggedIn()));
    }
    RootWidget->AddToViewport();
}

// 로그인 이후 인트로를 최초 한 번 시작하거나 복귀 경로를 선택하고 메뉴 준비 여부를 반환한다.
bool AMWUCMTitleFlow::IsMenuReady(bool bLoggedIn)
{
    if (!bLoggedIn)
        return false;
    if (!bStarted)
    {
        auto* Session = GetGameInstance()->GetSubsystem<UMWUCMIntroSession>();
        if (!IntroSequence || !Session ||
            (Session->IsReturnWorldReady(GetWorld()) && (!IsValid(PreviewCamera) || !IsValid(MenuCamera))))
        {
            UE_LOG(LogTemp, Error, TEXT("MW_UCM Missing sequence/session lifetime; keeping menu gated"));
            if (Session)
                Session->SetPreparationFailure(GetWorld(), IntroSequence ? TEXT("TitleCameraMissing")
                                                                         : TEXT("IntroSequenceMissing"));
            return false;
        }
        bStarted = true;
        const bool bAlreadyStarted = Session->bIntroStarted;
        Session->bIntroStarted = true;
        SetActorTickEnabled(true);
        StartFidelity();
        if (Session->IsReturnWorldReady(GetWorld()) && StartRoomReturn(*Session))
        {
            UE_LOG(LogTemp, Display, TEXT("MW_UCM SESSION_RETURN_NO_INTRO"));
        }
        else if (bAlreadyStarted)
        {
            RestoreTitlePose();
            // RestoreTitlePose는 일반 카메라 틱 전에 실제 시점 대상을 바꾼다.
            // 지금 카메라를 평가해 원본 전환이 올바른 출발 시점을 잡게 한다.
            if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
            {
                if (PC->PlayerCameraManager)
                    PC->PlayerCameraManager->UpdateCamera(0.f);
            }
            {
                TGuardValue<bool> AllowTransition(bSuppressPreviewInput, false);
                AcceptPreviewInput();
            }
            if (Session->IsReturnWorldReady(GetWorld()))
                RecordRoomReturn(TEXT("UCM_DOLLY_STARTED"));
            UE_LOG(LogTemp, Display, TEXT("MW_UCM SESSION_RETURN_NO_INTRO"));
        }
        else
        {
            ReplayPreview();
            ShowIntroSkip();
            UE_LOG(LogTemp, Display, TEXT("MW_UCM LOGIN_SUCCESS_INTRO_ONCE"));
        }
    }
    return PreviewStage == 2;
}

// 현재 인트로의 클릭 안내를 한 번만 붙이고 약한 액터 참조로 스킵을 전달한다.
void AMWUCMTitleFlow::ShowIntroSkip()
{
    auto* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
    if (!Viewport || IntroSkipOverlay.IsValid())
        return;
    IntroSkipOverlay =
        SNew(SVerticalBox).Visibility(EVisibility::SelfHitTestInvisible) +
        SVerticalBox::Slot()
            .VAlign(VAlign_Bottom)
            .HAlign(HAlign_Right)
            .Padding(24.f)[SNew(SButton)
                               .IsFocusable(false)
                               .ContentPadding(FMargin(18.f, 10.f))
                               .OnClicked_Lambda(
                                   [WeakThis = TWeakObjectPtr<AMWUCMTitleFlow>(this)]()
                                   {
                                       if (WeakThis.IsValid())
                                           WeakThis->SkipIntro();
                                       return FReply::Handled();
                                   })[SNew(STextBlock).Text(NSLOCTEXT("MWIntro", "Skip", "건너뛰기 · Space / 클릭"))]];
    Viewport->AddViewportWidgetContent(IntroSkipOverlay.ToSharedRef(), 50);
}

// 이 Flow가 붙인 인트로 스킵 화면을 제거하고 Slate 참조를 해제한다.
void AMWUCMTitleFlow::HideIntroSkip()
{
    if (!IntroSkipOverlay.IsValid())
        return;
    if (auto* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
        Viewport->RemoveViewportWidgetContent(IntroSkipOverlay.ToSharedRef());
    IntroSkipOverlay.Reset();
}

// 실제 인트로가 재생 중일 때만 넘긴다. 스킵 입력이 같은 프레임의 메뉴 실행으로 이어지지 않게 한다.
bool AMWUCMTitleFlow::SkipIntro()
{
    if (!bStarted || GetNetMode() != NM_Standalone || !SkipPreviewIntro())
        return false;
    IntroSkippedFrame = GFrameCounter;
    if (auto* PC = GetWorld()->GetFirstPlayerController())
        PC->FlushPressedKeys(); // 스킵에 쓴 클릭·Space가 타이틀 메뉴까지 실행하지 않게 한다.
    HideIntroSkip();
    RefreshRoot();
    return true;
}

bool AMWUCMTitleFlow::StartRoomReturn(const UMWUCMIntroSession& Session)
{
    auto* PC = GetWorld()->GetFirstPlayerController();
    const auto* LobbyRoute = FindRoute(TEXT("Lobby"));
    FTransform Pose;
    float FOV = 0.f;
    if (!PC || !PC->PlayerCameraManager || !WorldRouteCamera || !LobbyRoute || LobbyRoute->Reverse.Num() < 2 ||
        !Session.GetReturnView(GetWorld(), Pose, FOV))
        return false;

    PreviewStage = 2;
    WorldRouteCamera->SetActorTransform(Pose);
    WorldRouteCamera->GetCameraComponent()->SetFieldOfView(FOV);
    PC->SetViewTarget(WorldRouteCamera);
    return StartWorldRoute(TEXT("Lobby"), true, TEXT("ReturningFromLobby"));
}

// 메뉴 가시성·입력 허용과 음악의 메인 메뉴 상태를 함께 갱신한다.
void AMWUCMTitleFlow::SetMainMenuPresentation(bool bVisible, bool bInputEnabled)
{
    SetSourceMenuPresentation(bVisible, bInputEnabled);
    if (auto* Music = GetWorld()->GetSubsystem<UMWUCMWorldMusic>())
        Music->SetMainMenuVisible(bVisible);
}

// 입력이 허용된 메뉴 선택 변경에만 기존 강조 효과음을 재생한다.
void AMWUCMTitleFlow::OnSourceMenuSelectionChanged()
{
    if (!bSuppressPreviewInput)
        if (auto* Music = GetWorld()->GetSubsystem<UMWUCMWorldMusic>())
            Music->PlayMenuHighlight();
}

// 로그인·현재 영역·복귀 상태에 맞춰 메뉴 표시, 입력 모드, 음악 활성 상태를 갱신한다.
void AMWUCMTitleFlow::ApplyEntryPresentation(bool bLoggedIn, bool bMenuRequested)
{
    // 방 생성·검색 창이 메인 표지판을 숨겨도 같은 프런트엔드이므로 음악은 계속 재생한다.
    if (auto* Music = GetWorld()->GetSubsystem<UMWUCMWorldMusic>())
        Music->SetFrontendMusicActive(bLoggedIn && bStarted && PreviewStage >= 2);
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC)
        return;
    if (!bLoggedIn)
    {
        SetMainMenuPresentation(false, false);
        PC->bShowMouseCursor = true;
        PC->SetInputMode(FInputModeUIOnly());
        return;
    }
    if (!bStarted)
        return;
    const bool bMain =
        bMenuRequested && ActiveRegion == TEXT("Main") && (!bWorldRouteActive || bWorldRoutePresentationReady);
    const auto* IntroSession = GetGameInstance()->GetSubsystem<UMWUCMIntroSession>();
    const bool bReturning = IntroSession && IntroSession->IsRoomReturnPending();
    bSuppressPreviewInput = !bMain || bReturning;
    SetMainMenuPresentation(!bReturning && bMain && (PreviewStage == 2 || PreviewStage == 3),
        !bReturning && bMain && PreviewStage == 2);
    if (!bMain) TitleCanvas->SetVisibility(false);
    TitleCanvas->SetCollisionEnabled(bMain && PreviewStage == 1
        ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
    if (bMain && PreviewStage == 0)
    {
        PC->bShowMouseCursor = true;
        FInputModeGameAndUI Mode;
        Mode.SetHideCursorDuringCapture(false);
        PC->SetInputMode(Mode);
    }
    else if (ActiveRegion != TEXT("RoomPanel"))
        UseWorldMouse();
}

// 표시 이벤트가 다시 루트 갱신을 부르는 재진입을 막고 기존 화면 선택을 갱신한다.
void AMWUCMTitleFlow::RefreshRoot()
{
    if (!IsValid(RootWidget) || bRefreshing)
        return;
    TGuardValue<bool> Guard(bRefreshing, true);
    RootWidget->RefreshEntryScreen();
}

// 원본 메뉴 동작 이름을 기존 MW 버튼 인덱스로 연결한다.
void AMWUCMTitleFlow::HandleSourceMenu(FName Action)
{
    if (!IsValid(RootWidget) || bSuppressPreviewInput || PreviewStage != 2)
        return;
    UE_LOG(LogTemp, Display, TEXT("MW_UCM MENU_HANDLER ACTION=%s MONOTONIC_MS=%.3f"), *Action.ToString(),
           FPlatformTime::Seconds() * 1000.0);
    const int32 Index = SourceMenuEntries.IndexOfByPredicate(
        [&](const auto& Entry)
        {
            return Entry.ActionId == Action;
        });
    if (Index < 0 || Index >= 3)
        return;

    RootWidget->ActivateSourceMenu(Index);
}

// 이 월드에서 만든 카메라·위젯·타이머·이벤트 구독을 종료한다.
void AMWUCMTitleFlow::EndPlay(const EEndPlayReason::Type Reason)
{
    UnbindHostLeaveConfirmation();
    ClearVehicleDeparture();
    HideIntroSkip();
    SetMainMenuPresentation(false, false);
    MWWorldSurface->SetWidget(nullptr);
    if (WorldRouteCamera)
        WorldRouteCamera->Destroy();
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

// 이 월드의 기존 타이틀 배경 동작과 교통 연출을 시작한다.
void AMWUCMTitleFlow::StartFidelity()
{
    for (TActorIterator<AUCMTitleFidelityDirector> It(GetWorld()); It; ++It)
    {
        It->ResetFidelityRuntime();
        It->SetActorTickEnabled(true);
    }
}

// 원본 경로를 이름으로 조회한다. 반환 포인터는 WorldRoutes 내부 데이터를 가리킨다.
const FMWUCMWorldRoute* AMWUCMTitleFlow::FindRoute(FName Name) const
{
    return WorldRoutes.FindByPredicate(
        [&](const auto& Route)
        {
            return Route.Name == Name;
        });
}

// 월드 UI에서 커서를 보이게 하고 게임·UI 입력을 함께 받을 모드로 맞춘다.
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

// 경로·카메라를 검증한 뒤 이동을 시작하고 준비 전 위젯 입력을 막는다.
bool AMWUCMTitleFlow::StartWorldRoute(FName Name, bool bReverse, FName Destination)
{
    const auto* Route = FindRoute(Name);
    auto* PC = GetWorld()->GetFirstPlayerController();
    if (!Route || !PC || !PC->PlayerCameraManager || !WorldRouteCamera)
        return false;
    const auto& Samples = bReverse ? Route->Reverse : Route->Forward;
    if (Samples.Num() < 2)
        return false;
    RunningRoute = Name;
    bRouteReverse = bReverse;
    ActiveRegion = Destination;
    WorldRouteTime = 0;
    const bool bAlreadyUsingRouteCamera = PC->GetViewTarget() == WorldRouteCamera;
    WorldRouteOutgoing = bAlreadyUsingRouteCamera ? WorldRouteCamera->GetActorTransform()
                                                  : FTransform(PC->PlayerCameraManager->GetCameraRotation(),
                                                               PC->PlayerCameraManager->GetCameraLocation());
    WorldRouteOutgoingFOV = bAlreadyUsingRouteCamera ? WorldRouteCamera->GetCameraComponent()->FieldOfView
                                                     : PC->PlayerCameraManager->GetFOVAngle();
    // 새 로비 월드는 원본 로비 경로의 온라인 메뉴 쪽 끝에서 시작한다.
    if (Name == TEXT("Lobby") && !RootWidget)
    {
        WorldRouteOutgoing = Samples[0].Pose;
        WorldRouteOutgoingFOV = FMath::RadiansToDegrees(
            2.f * FMath::Atan(FMath::Tan(FMath::DegreesToRadians(Route->VerticalFOV) * .5f) * 16.f / 9.f));
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
    SetMainMenuPresentation(false, false);
    MWWorldSurface->SetVisibility(false);
    MWWorldSurface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    UseWorldMouse();
    UE_LOG(LogTemp, Display, TEXT("MW_UCM WORLD_ROUTE_BEGIN %s %s SOURCE=%s MONOTONIC_MS=%.3f"), *Name.ToString(),
           bReverse ? TEXT("REVERSE") : TEXT("FORWARD"), *Route->Source, FPlatformTime::Seconds() * 1000.0);
    return true;
}

// 원본 샘플 사이의 위치·회전·시야각을 보간하고 화면 표시 가능 시점을 갱신한다.
void AMWUCMTitleFlow::UpdateWorldRoute(float DeltaTime)
{
    if (!bWorldRouteActive || !WorldRouteCamera)
        return;
    const auto* Route = FindRoute(RunningRoute);
    if (!Route)
        return;
    const auto& Samples = bRouteReverse ? Route->Reverse : Route->Forward;
    WorldRouteTime += DeltaTime;
    int32 Index = 0;
    while (Index + 1 < Samples.Num() && Samples[Index + 1].Time < WorldRouteTime)
        ++Index;
    const auto& A = Samples[Index];
    const auto& B = Samples[FMath::Min(Index + 1, Samples.Num() - 1)];
    const float Alpha = FMath::Clamp((WorldRouteTime - A.Time) / FMath::Max(B.Time - A.Time, .00001f), 0.f, 1.f);
    FTransform Pose;
    Pose.Blend(A.Pose, B.Pose, Alpha);
    const float U = FMath::Clamp(WorldRouteTime / FMath::Max(SourceBrainBlendSeconds, .00001f), 0.f, 1.f);
    if (U >= 1.f && !bWorldRouteBlendCompleteLogged)
    {
        bWorldRouteBlendCompleteLogged = true;
        UE_LOG(LogTemp, Display, TEXT("MW_UCM WORLD_ROUTE_BLEND_COMPLETE REGION=%s MONOTONIC_MS=%.3f"),
               *ActiveRegion.ToString(), FPlatformTime::Seconds() * 1000.0);
    }
    const float Blend = U * U * (3.f - 2.f * U);
    FTransform Blended;
    Blended.Blend(WorldRouteOutgoing, Pose, Blend);
    WorldRouteCamera->SetActorTransform(Blended);
    const float FOV = FMath::RadiansToDegrees(
        2.f * FMath::Atan(FMath::Tan(FMath::DegreesToRadians(Route->VerticalFOV) * .5f) * 16.f / 9.f));
    WorldRouteCamera->GetCameraComponent()->SetFieldOfView(FMath::Lerp(WorldRouteOutgoingFOV, FOV, Blend));
    if (VehicleDepartureCover)
    {
        // 맵 이동이 현재 월드를 교체할 때까지 차량 게시판과 카메라를 유지한다.
        bWorldRouteActive = WorldRouteTime < Samples.Last().Time;
        return;
    }

    // 메인 표지판은 월드에 고정돼 있으므로 원본 카메라 전환이 끝나면 표시하고
    // 남은 카메라 감쇠 움직임은 그대로 이어 간다.
    // 다른 월드 패널은 카메라가 안정될 때까지 기다려
    // 이동 도중 패널이 지나치게 가깝거나 서로 겹친 모습이 드러나지 않게 한다.
    const FTransform& Endpoint = Samples.Last().Pose;
    const float EndpointDistanceCm = FVector::Distance(Blended.GetLocation(), Endpoint.GetLocation());
    const float EndpointAngleDegrees =
        FMath::RadiansToDegrees(Blended.GetRotation().AngularDistance(Endpoint.GetRotation()));
    const bool bCameraSettled = EndpointDistanceCm <= 3.f && EndpointAngleDegrees <= .1f;
    const auto* Session = GetGameInstance()->GetSubsystem<UMWUCMIntroSession>();
    const bool bReturning = Session && Session->IsReturnWorldReady(GetWorld());
    const bool bPresentationEligible = !bReturning && U >= 1.f && (ActiveRegion == TEXT("Main") || bCameraSettled);
    if (!bWorldRoutePresentationReady && bPresentationEligible)
    {
        bWorldRoutePresentationReady = true;
        if (ActiveRegion == TEXT("Main"))
        {
            bSuppressPreviewInput = false;
            SetMainMenuPresentation(true, true);
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
        if (ActiveRegion == TEXT("RoomPanel")) RefreshRoot();
        else UseWorldMouse();
        UE_LOG(LogTemp, Display, TEXT("MW_UCM WORLD_ROUTE_PRESENTABLE REGION=%s SECONDS=%.3f ENDPOINT_DISTANCE_CM=%.3f ENDPOINT_ANGLE_DEG=%.3f MONOTONIC_MS=%.3f"), *ActiveRegion.ToString(), WorldRouteTime, EndpointDistanceCm, EndpointAngleDegrees, FPlatformTime::Seconds() * 1000.0);
    }
    if (WorldRouteTime < Samples.Last().Time)
        return;
    bWorldRouteActive = false;
    if (bReturning && ActiveRegion == TEXT("ReturningFromLobby")) return;
    if (ActiveRegion == TEXT("Main") && !bReturning)
    {
        bSuppressPreviewInput = false;
        // 복구한 역방향 경로의 끝과 MenuCamera의 기본 시점은 서로 다르다.
        // 평가한 경로 끝을 유지해 잠시 뒤 시점이 갑자기 바뀌는 현상을 막는다.
        SetMainMenuPresentation(true, true);
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
    if (ActiveRegion != TEXT("RoomPanel"))
        UseWorldMouse();
    UE_LOG(LogTemp, Display, TEXT("MW_UCM WORLD_ROUTE_COMPLETE REGION=%s MONOTONIC_MS=%.3f"), *ActiveRegion.ToString(),
           FPlatformTime::Seconds() * 1000.0);
}

// 복구 경로의 위젯 배치 정보에 맞춰 기존 화면을 월드 표면에 붙인다.
void AMWUCMTitleFlow::SetWorldWidget(UUserWidget* Widget, FName Anchor)
{
    if (!Widget)
        return;
    const auto* Route = FindRoute(Anchor);
    auto* PC = GetWorld()->GetFirstPlayerController();
    if (!Route || !PC)
        return;
    if (PresentedWidget != Widget)
    {
        Widget->RemoveFromParent();
        PresentedWidget = Widget;
        MWWorldSurface->SetOwnerPlayer(PC->GetLocalPlayer());
        MWWorldSurface->SetWidget(Widget);
        // GameAndUI 설정은 마우스 캡처를 해제하므로 표시가 바뀔 때만 한 번 적용한다.
        // 버튼을 누르고 떼는 사이 매 프레임 다시 설정하면 클릭 처리가 끊길 수 있다.
        UseWorldMouse();
    }
    const FTransform& PanelPose = Route->WidgetPose;
    if (!MWWorldSurface->GetComponentTransform().Equals(PanelPose))
        MWWorldSurface->SetWorldTransform(PanelPose);
    const bool bShowWidget = !bWorldRouteActive || bWorldRoutePresentationReady;
    if (MWWorldSurface->IsVisible() != bShowWidget)
        MWWorldSurface->SetVisibility(bShowWidget);
    const ECollisionEnabled::Type DesiredCollision =
        bShowWidget ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision;
    if (MWWorldSurface->GetCollisionEnabled() != DesiredCollision)
        MWWorldSurface->SetCollisionEnabled(DesiredCollision);
    if (Widget->GetVisibility() != ESlateVisibility::SelfHitTestInvisible)
        Widget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

// 기존 생성·검색 창을 유지하고 해당 배경의 원본 진입·복귀 카메라만 연결한다.
void AMWUCMTitleFlow::PresentEntryWidget(UWidget* Screen, bool bMenuRequested)
{
    if (!bStarted)
        return;
    if (auto* Widget = Cast<UUserWidget>(Screen);
        Cast<UMWCreateRoomWidget>(Widget) || Cast<UMWRoomBrowserWidget>(Widget))
    {
        // MW의 기존 창을 유지하고 뒤에서 보이는 UCM 카메라만 바꾼다.
        if (ActiveRegion != TEXT("RoomPanel"))
            StartWorldRoute(TEXT("RoomPanel"), false, TEXT("RoomPanel"));
    }
    else if (bMenuRequested && ActiveRegion == TEXT("RoomPanel"))
    {
        MWWorldSurface->SetWidget(nullptr);
        PresentedWidget = nullptr;
        StartWorldRoute(TEXT("RoomPanel"), true, TEXT("Main"));
    }
}

// 기존 호스트 종료 버튼 연결을 잠시 확인 창으로 바꾸고 원래 콜백은 보관한다.
void AMWUCMTitleFlow::BindHostLeaveConfirmation()
{
    const auto* PC = LobbyWidget ? LobbyWidget->GetOwningPlayer() : nullptr;
    const auto* Player = PC ? PC->GetPlayerState<AMWPlayerState>() : nullptr;
    if (!Player || !Player->IsRoomHost())
        return;

    auto* Button = Cast<UButton>(LobbyWidget->GetWidgetFromName(TEXT("Button_Leave")));
    if (!Button)
        return;

    // 기존 위젯이 종료 콜백을 소유한다. UCM 월드 로비를 표시하는 동안만
    // 기존 연결을 확인 창으로 바꾸고 화면을 떠날 때 복원한다.
    static const FName LeaveHandler(TEXT("HandleLeaveClicked"));
    if (!Button->OnClicked.Contains(LobbyWidget.Get(), LeaveHandler))
        return;
    if (HostLeaveButton.IsValid() && HostLeaveButton.Get() != Button)
        UnbindHostLeaveConfirmation();
    OriginalHostLeave.BindUFunction(LobbyWidget, LeaveHandler);
    Button->OnClicked.Remove(OriginalHostLeave);
    Button->OnClicked.AddUniqueDynamic(this, &ThisClass::ShowHostLeaveConfirmation);
    HostLeaveButton = Button;
}

// 확인 창 연결을 제거하고 보관한 기존 종료 버튼 콜백을 돌려놓는다.
void AMWUCMTitleFlow::UnbindHostLeaveConfirmation()
{
    CloseHostLeaveConfirmation();
    if (auto* Button = HostLeaveButton.Get())
    {
        Button->OnClicked.RemoveDynamic(this, &ThisClass::ShowHostLeaveConfirmation);
        if (OriginalHostLeave.IsBound())
            Button->OnClicked.AddUnique(OriginalHostLeave);
    }
    HostLeaveButton.Reset();
    OriginalHostLeave.Unbind();
}

// 호스트 종료 버튼이 유효할 때 확인 창을 한 번만 띄우고 이전 키보드 포커스를 보관한다.
void AMWUCMTitleFlow::ShowHostLeaveConfirmation()
{
    if (!HostLeaveButton.IsValid() || !HostLeaveButton->GetIsEnabled() || HostLeaveConfirmation)
        return;
    auto* Viewport = GetGameInstance()->GetGameViewportClient();
    if (!Viewport || !FSlateApplication::IsInitialized())
        return;

    PreviousHostLeaveFocus = FSlateApplication::Get().GetKeyboardFocusedWidget();
    HostLeaveViewport = Viewport;
    HostLeaveConfirmation = SNew(SMWHostLeaveConfirmation)
                                .OnConfirm(FSimpleDelegate::CreateUObject(this, &ThisClass::ConfirmHostLeave))
                                .OnCancel(FSimpleDelegate::CreateUObject(this, &ThisClass::CloseHostLeaveConfirmation));
    Viewport->AddViewportWidgetContent(HostLeaveConfirmation.ToSharedRef(), 9000);
    FSlateApplication::Get().SetKeyboardFocus(HostLeaveConfirmation);
}

// 확인을 받은 경우에만 보관한 기존 방 종료 처리를 실행한다.
void AMWUCMTitleFlow::ConfirmHostLeave()
{
    CloseHostLeaveConfirmation();
    if (HostLeaveButton.IsValid() && HostLeaveButton->GetIsEnabled() && OriginalHostLeave.IsBound())
    {
        const FScriptDelegate Leave = OriginalHostLeave;
        Leave.ProcessDelegate<UObject>(nullptr);
    }
}

// 확인 창을 제거하고 열기 전의 키보드 포커스를 복구한다.
void AMWUCMTitleFlow::CloseHostLeaveConfirmation()
{
    if (!HostLeaveConfirmation)
        return;
    if (auto* Viewport = HostLeaveViewport.Get())
        Viewport->RemoveViewportWidgetContent(HostLeaveConfirmation.ToSharedRef());
    HostLeaveConfirmation.Reset();
    HostLeaveViewport.Reset();
    if (FSlateApplication::IsInitialized())
    {
        if (auto Previous = PreviousHostLeaveFocus.Pin())
            FSlateApplication::Get().SetKeyboardFocus(Previous);
    }
    PreviousHostLeaveFocus.Reset();
}

// 현재 로비·차량 위젯과 연출 영역을 맞추고 호스트 종료 확인 연결을 유지한다.
void AMWUCMTitleFlow::UpdateLobbyPresentation()
{
    if (!LobbyWidget || !bStarted) return;
    if (VehicleWidget && ActiveRegion == TEXT("Lobby") && !bWorldRouteActive)
    {
        if (VehiclePreviewActor && VehiclePreviewActor->PreviewSurface &&
            StartWorldRoute(TEXT("Vehicle"), false, TEXT("Vehicle")))
        {
            auto* Surface = VehiclePreviewActor->PreviewSurface.Get();
            Surface->SetOwnerPlayer(VehicleWidget->GetOwningLocalPlayer());
            Surface->SetWidget(VehicleWidget);
            FitVehiclePreview(VehicleWidget);
            Surface->SetVisibility(false);
            Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
        else if (!VehicleWidget->IsInViewport())
        {
            MWWorldSurface->SetVisibility(false);
            MWWorldSurface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            VehicleWidget->AddToViewport();
            if (auto* PC = VehicleWidget->GetOwningPlayer()) PC->SetInputMode(FInputModeUIOnly());
        }
    }
    else if (!VehicleWidget && ActiveRegion == TEXT("Lobby"))
    {
        SetWorldWidget(LobbyWidget, TEXT("Lobby"));
        BindHostLeaveConfirmation();
        if (HostLeaveConfirmation && (!HostLeaveButton.IsValid() || !HostLeaveButton->GetIsEnabled()))
            CloseHostLeaveConfirmation();
    }
}

UWidgetComponent* AMWUCMTitleFlow::GetVehicleSelectionSurface() const
{
    return VehiclePreviewActor ? VehiclePreviewActor->PreviewSurface.Get() : nullptr;
}

// 출발 경로를 시작하고 화면을 가린 뒤 완료를 알린다. 서버 이동 승인 자체는 하지 않는다.
bool AMWUCMTitleFlow::BeginVehicleDeparture(FSimpleDelegate OnCovered)
{
    auto* Viewport = GetGameInstance()->GetGameViewportClient();
    if (!Viewport || !StartWorldRoute(TEXT("VehicleDeparture"), false, TEXT("VehicleDeparture")))
    {
        UE_LOG(LogTemp, Warning, TEXT("MW_UCM VEHICLE_DEPARTURE_UNAVAILABLE"));
        return false;
    }

    OnVehicleDepartureCovered = MoveTemp(OnCovered);
    VehicleDepartureCover =
        SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(FLinearColor::Transparent)
            .Padding(0.f)[MWGameplayUI::FitDesignCanvas(
                SNew(SOverlay) +
                SOverlay::Slot()
                    .HAlign(HAlign_Center)
                    .VAlign(VAlign_Center)[SNew(SMWGameplaySign)
                                               .Label(NSLOCTEXT("MWGameplayUI", "Loading", "불러오는 중"))
                                               .AnimateLoading(true)])];
    VehicleDepartureCover->GetContent()->SetVisibility(EVisibility::Collapsed);
    VehicleDepartureViewport = Viewport;
    Viewport->AddViewportWidgetContent(VehicleDepartureCover.ToSharedRef(), 9999);

    if (auto* Surface = GetVehicleSelectionSurface())
        Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (VehicleWidget)
    {
        VehicleWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
        if (VehicleWidget->IsInViewport())
            VehicleWidget->RemoveFromParent();
    }
    auto* PC = GetWorld()->GetFirstPlayerController();
    PC->SetInputMode(FInputModeGameOnly());
    PC->bShowMouseCursor = false;
    return true;
}

// 기존 출발 경로의 화면 가림을 진행하고 준비 뒤 완료 콜백을 한 번 전달한다.
void AMWUCMTitleFlow::UpdateVehicleDeparture()
{
    if (!VehicleDepartureCover || !OnVehicleDepartureCovered.IsBound())
    {
        return;
    }
    if (bVehicleDepartureCovered)
    {
        // 불투명 가림이 반영될 차례를 준 뒤 이동 준비 완료를 보고한다.
        FSimpleDelegate OnCovered = MoveTemp(OnVehicleDepartureCovered);
        OnVehicleDepartureCovered.Unbind();
        UE_LOG(LogTemp, Display, TEXT("MW_UCM VEHICLE_DEPARTURE_COVERED SECONDS=%.3f"), WorldRouteTime);
        OnCovered.ExecuteIfBound();
        return;
    }

    // 원본 Unity LoadingController는 초당 0.4씩 화면 가림 알파를 높인다.
    constexpr float SourceFadeSeconds = 2.5f;
    const float Alpha = FMath::Clamp(WorldRouteTime / SourceFadeSeconds, 0.f, 1.f);
    bVehicleDepartureCovered = Alpha >= 1.f;
    VehicleDepartureCover->SetBorderBackgroundColor(bVehicleDepartureCovered
                                                        ? FLinearColor::FromSRGBColor(FColor(92, 218, 243))
                                                        : FLinearColor(0.f, 0.f, 0.f, Alpha));
    if (bVehicleDepartureCovered)
        VehicleDepartureCover->GetContent()->SetVisibility(EVisibility::HitTestInvisible);
}

// 출발 가림막과 완료 콜백을 해제해 이전 월드 요청이 남지 않게 한다.
void AMWUCMTitleFlow::ClearVehicleDeparture()
{
    if (!VehicleDepartureCover)
    {
        return;
    }
    if (auto* Viewport = VehicleDepartureViewport.Get())
    {
        Viewport->RemoveViewportWidgetContent(VehicleDepartureCover.ToSharedRef());
    }
    VehicleDepartureViewport.Reset();
    VehicleDepartureCover.Reset();
    OnVehicleDepartureCovered.Unbind();
    bVehicleDepartureCovered = false;
    bWorldRouteActive = false;
}

void AMWUCMTitleFlow::SetLobbyWidgets(UMWRoomLobbyWidget* Lobby, UMWVehicleSelectionWidget* Selection)
{
    if (!Selection && VehicleDepartureCover)
    {
        ClearVehicleDeparture();
        if (Lobby)
        {
            StartWorldRoute(TEXT("Vehicle"), true, TEXT("Lobby"));
        }
    }
    if (LobbyWidget != Lobby || Selection)
        UnbindHostLeaveConfirmation();
    LobbyWidget = Lobby;
    if (VehicleWidget != Selection && VehicleWidget)
    {
        VehicleWidget->RemoveFromParent();
        if (VehiclePreviewActor && VehiclePreviewActor->PreviewSurface)
        {
            auto* Surface = VehiclePreviewActor->PreviewSurface.Get();
            Surface->SetWidget(nullptr);
            Surface->SetVisibility(false);
            Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
    }
    VehicleWidget = Selection;
    if (!Lobby)
    {
        MWWorldSurface->SetWidget(nullptr);
        PresentedWidget = nullptr;
        return;
    }
    if (!Selection && ActiveRegion == TEXT("Vehicle"))
        StartWorldRoute(TEXT("Vehicle"), true, TEXT("Lobby"));
    UpdateLobbyPresentation();
}

// 기존 차량 선택 UI의 표시만 담당한다. 선택 승인과 게임 단계 변경은 기존 HUD·서버 경로를 따른다.
bool AMWUCMTitleFlow::ShowVehicleSelection()
{
    if (!bStarted || (bWorldRouteActive && !bWorldRoutePresentationReady) || !VehiclePreviewActor ||
        !GetWorld()->IsPlayInEditor())
        return false;
    auto* PC = GetWorld()->GetFirstPlayerController();
    if (!PC)
        return false;
    const FName Route = ActiveRegion == TEXT("Lobby") ? TEXT("Vehicle") : TEXT("MainVehicle");
    if (!FindRoute(Route))
        return false;
    if (!StartWorldRoute(Route, false, TEXT("Vehicle")))
        return false;
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

// 차량 게시판 표시와 관련 입력을 정리하고 기존 월드 화면으로 돌아간다.
void AMWUCMTitleFlow::CloseVehicleSelection()
{
    if (ActiveRegion != TEXT("Vehicle"))
        return;
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
    else
        StartWorldRoute(TEXT("MainVehicle"), true, TEXT("Main"));
}

// 스킵 입력을 먼저 처리한 뒤 기존 경로·출발·로비 표시를 현재 단계에 맞춰 갱신한다.
void AMWUCMTitleFlow::Tick(float DeltaTime)
{
    // 원본 타이틀·메뉴가 입력을 소비하기 전에 인트로 스킵부터 처리한다.
    if (bStarted && PreviewStage == 0 && GetNetMode() == NM_Standalone)
    {
        auto* PC = GetWorld()->GetFirstPlayerController();
        if (PC && (PC->WasInputKeyJustPressed(EKeys::SpaceBar) || PC->WasInputKeyJustPressed(EKeys::LeftMouseButton)))
            if (SkipIntro())
                return;
    }
    if (PreviewStage != 0)
        HideIntroSkip();
    if (IntroSkippedFrame != GFrameCounter)
        Super::Tick(DeltaTime);
    UpdateWorldRoute(DeltaTime);
    UpdateVehicleDeparture();
    UpdateLobbyPresentation();
    auto* Session = GetGameInstance()->GetSubsystem<UMWUCMIntroSession>();
    if (!Session || !Session->IsReturnWorldReady(GetWorld()) || bWorldRouteActive || PreviewStage != 2)
        return;

    const bool bRoomBrowser = Session->ShouldReturnToRoomBrowser();
    if (ActiveRegion == TEXT("Main") && StartWorldRoute(TEXT("RoomPanel"), false, TEXT("ReturningFromLobby")))
        return;

    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    AActor* ViewTarget = PC ? PC->GetViewTarget() : nullptr;
    if (ViewTarget && (ViewTarget == WorldRouteCamera || ViewTarget == MenuCamera) &&
        Session->FinishReturnPresentation(PC, ViewTarget, RootWidget))
    {
        ActiveRegion = TEXT("RoomPanel");
        if (bRoomBrowser)
        {
            RootWidget->RestoreRoomBrowser();
            RecordRoomReturn(TEXT("ROOM_BROWSER_RESTORED"));
        }
        else
        {
            RootWidget->RestoreCreateRoomForm();
            RecordRoomReturn(TEXT("CREATE_ROOM_FORM_RESTORED"));
        }
    }
}
