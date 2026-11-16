// 원본 인트로·메뉴 요소·카메라 경로를 재생하는 기반이며 실제 기능 요청은 MW 연결부에 전달한다.

#include "UCMTitlePreview.h"
#include "EngineUtils.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "MovieSceneObjectBindingID.h"
#include "Modules/ModuleManager.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "Misc/App.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "UObject/UObjectIterator.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "LevelSequencePlayer.h"
#include "LevelSequenceActor.h"
#include "LevelSequence.h"
#include "Serialization/JsonSerializer.h"
#include "Styling/CoreStyle.h"
#include "Fonts/CompositeFont.h"
#include "Engine/FontFace.h"
IMPLEMENT_MODULE(FDefaultModuleImpl, UCMTitlePreview)

// 복구 자료가 sRGB 색인지 설정에 따라 변환해 원본 UI 색의 기준을 맞춘다.
FLinearColor UUCMSourcedWidget::SourceTint(FLinearColor C) const
{
    return bSourceUITintsSRGB ? FLinearColor::FromSRGBColor(C.ToFColor(false)) : C;
}

// 원본 요소 데이터로 기존 UMG 캔버스를 구성하고 표시·버튼 이벤트를 연결한다.
TSharedRef<SWidget> UUCMSourcedWidget::RebuildWidget()
{
    if (!WidgetTree)
        WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SourceCanvas"));
    WidgetTree->RootWidget = Root;
    SourceVisuals.Reset();
    for (const FUCMSourceUIElement& E : Elements)
    {
        UWidget* Content = nullptr;
        if (E.Texture)
        {
            if (E.bSliced)
            {
                // Unity Sprite 테두리는 텍셀, Image 배치는 기준 캔버스 픽셀 단위여서 변환이 필요하다.
                // Slate Box는 텍스처 전체 크기를 쓰므로 원본 아홉 UV 영역을 명시적으로 구성한다.
                UCanvasPanel* Nine = WidgetTree->ConstructWidget<UCanvasPanel>();
                float Ratio = FMath::Max(E.SourcePixelsPerUnit, .0001f);
                float L = E.Texture->GetSizeX() * E.SliceMargin.Left / Ratio,
                      R = E.Texture->GetSizeX() * E.SliceMargin.Right / Ratio,
                      T = E.Texture->GetSizeY() * E.SliceMargin.Top / Ratio,
                      B = E.Texture->GetSizeY() * E.SliceMargin.Bottom / Ratio;
                float SX = FMath::Min(1.f, float(E.Size.X) / FMath::Max(L + R, .0001f)),
                      SY = FMath::Min(1.f, float(E.Size.Y) / FMath::Max(T + B, .0001f));
                L *= SX;
                R *= SX;
                T *= SY;
                B *= SY;
                float X[4] = {0, L, float(E.Size.X) - R, float(E.Size.X)},
                      Y[4] = {0, T, float(E.Size.Y) - B, float(E.Size.Y)};
                float U[4] = {0, E.SliceMargin.Left, 1 - E.SliceMargin.Right, 1},
                      V[4] = {0, E.SliceMargin.Top, 1 - E.SliceMargin.Bottom, 1};
                for (int32 J = 0; J < 3; J++)
                    for (int32 K = 0; K < 3; K++)
                    {
                        UImage* Part = WidgetTree->ConstructWidget<UImage>();
                        FSlateBrush Brush;
                        Brush.SetResourceObject(E.Texture);
                        Brush.DrawAs = ESlateBrushDrawType::Image;
                        Brush.SetUVRegion(FBox2D(FVector2D(U[K], V[J]), FVector2D(U[K + 1], V[J + 1])));
                        Part->SetBrush(Brush);
                        Part->SetColorAndOpacity(SourceTint(E.Color));
                        Part->SetVisibility(ESlateVisibility::HitTestInvisible);
                        auto SliceSlot = Nine->AddChildToCanvas(Part);
                        SliceSlot->SetPosition(FVector2D(X[K], Y[J]));
                        SliceSlot->SetSize(FVector2D(X[K + 1] - X[K], Y[J + 1] - Y[J]));
                    }
                Content = Nine;
            }
            else
            {
                UImage* I = WidgetTree->ConstructWidget<UImage>();
                I->SetBrushFromTexture(E.Texture);
                I->SetColorAndOpacity(SourceTint(E.Color));
                Content = I;
            }
        }
        else if (E.bContinuePrompt)
        {
            // 기존 월드 캔버스와 버튼 안에서 원본 근거가 있는 타이틀 안내만 표시한다.
            FString KeyText, HintText;
            E.Text.Split(TEXT(" "), &KeyText, &HintText);
            KeyText.TrimStartAndEndInline();
            HintText.TrimStartAndEndInline();
            auto MakeFont = [this](float Size)
            {
                FSlateFontInfo Font =
                    FCoreStyle::GetDefaultFontStyle("Bold", FMath::RoundToInt(Size * SourceFontPixelsToPoints));
                if (SourceFontFace)
                {
                    auto Composite = MakeShared<FCompositeFont>();
                    FTypefaceEntry Entry(TEXT("Default"));
                    Entry.Font = FFontData(SourceFontFace);
                    Composite->DefaultTypeface.Fonts.Add(Entry);
                    Font = FSlateFontInfo(Composite, Size * SourceFontPixelsToPoints);
                }
                return Font;
            };
            UBorder* Block =
                WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ContinuePromptBackground"));
            Block->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, 14.f));
            Block->SetBrushColor(FLinearColor(.004f, .010f, .075f, .96f));
            Block->SetPadding(FMargin(42, 20));
            Block->SetHorizontalAlignment(HAlign_Center);
            Block->SetVerticalAlignment(VAlign_Center);
            Block->SetClipping(EWidgetClipping::ClipToBounds);
            UHorizontalBox* Row =
                WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ContinuePromptRow"));
            Block->AddChild(Row);
            UBorder* KeyBase = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ContinueKeycapEdge"));
            KeyBase->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, 10.f));
            KeyBase->SetBrushColor(FLinearColor(.12f, .17f, .24f, 1));
            KeyBase->SetPadding(FMargin(3, 3, 3, 10));
            UBorder* KeyFace = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ContinueKeycapFace"));
            KeyFace->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, 7.f));
            KeyFace->SetBrushColor(FLinearColor(.86f, .91f, .98f, 1));
            KeyFace->SetPadding(FMargin(24, 6));
            KeyFace->SetVerticalAlignment(VAlign_Center);
            KeyBase->AddChild(KeyFace);
            UTextBlock* Key =
                WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ContinueKeycapText"));
            Key->SetText(FText::FromString(KeyText));
            Key->SetFont(MakeFont(E.FontSize * .66f));
            Key->SetColorAndOpacity(FSlateColor(FLinearColor(.009f, .016f, .03f, 1)));
            Key->SetAutoWrapText(false);
            Key->SetWrapTextAt(0);
            Key->SetJustification(ETextJustify::Center);
            KeyFace->AddChild(Key);
            UTextBlock* Hint =
                WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ContinueHintText"));
            Hint->SetText(FText::FromString(HintText));
            auto HintFont = MakeFont(E.FontSize);
            HintFont.OutlineSettings.OutlineSize = E.FontOutlineSize;
            HintFont.OutlineSettings.OutlineColor = FLinearColor::Black;
            Hint->SetFont(HintFont);
            Hint->SetColorAndOpacity(FSlateColor(FLinearColor::White));
            Hint->SetAutoWrapText(false);
            Hint->SetWrapTextAt(0);
            auto KeySlot = Row->AddChildToHorizontalBox(KeyBase);
            KeySlot->SetVerticalAlignment(VAlign_Center);
            KeySlot->SetPadding(FMargin(0, 0, 28, 0));
            auto HintSlot = Row->AddChildToHorizontalBox(Hint);
            HintSlot->SetVerticalAlignment(VAlign_Center);
            Content = Block;
        }
        else
        {
            UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>();
            T->SetText(FText::FromString(E.Text));
            T->SetColorAndOpacity(FSlateColor(SourceTint(E.Color)));
            T->SetJustification(E.SourceJustification == 0   ? ETextJustify::Left
                                : E.SourceJustification == 2 ? ETextJustify::Right
                                                             : ETextJustify::Center);
            FSlateFontInfo Font =
                FCoreStyle::GetDefaultFontStyle("Bold", FMath::RoundToInt(E.FontSize * SourceFontPixelsToPoints));
            if (SourceFontFace)
            {
                auto Composite = MakeShared<FCompositeFont>();
                FTypefaceEntry Entry(TEXT("Default"));
                Entry.Font = FFontData(SourceFontFace);
                Composite->DefaultTypeface.Fonts.Add(Entry);
                Font = FSlateFontInfo(Composite, E.FontSize * SourceFontPixelsToPoints);
            }
            Font.OutlineSettings.OutlineSize = E.FontOutlineSize;
            Font.OutlineSettings.OutlineColor = FLinearColor::Black;
            T->SetFont(Font);
            if (!E.TextShadowOffset.IsNearlyZero())
            {
                T->SetShadowOffset(E.TextShadowOffset);
                T->SetShadowColorAndOpacity(FLinearColor(0, 0, 0, .8f));
            }
            UBorder* Center = WidgetTree->ConstructWidget<UBorder>();
            Center->SetBrushColor(FLinearColor::Transparent);
            Center->SetVerticalAlignment(VAlign_Center);
            Center->SetHorizontalAlignment(HAlign_Fill);
            Center->AddChild(T);
            Content = Center;
        }
        SourceVisuals.Add(Content);
        UWidget* W = Content;
        if (E.bButton)
        {
            UUCMSourceActionButton* Button = WidgetTree->ConstructWidget<UUCMSourceActionButton>();
            FButtonStyle Style = Button->GetStyle();
            Style.Normal.DrawAs = ESlateBrushDrawType::NoDrawType;
            Style.Hovered.DrawAs = ESlateBrushDrawType::NoDrawType;
            Style.Pressed.DrawAs = ESlateBrushDrawType::NoDrawType;
            Style.Disabled.DrawAs = ESlateBrushDrawType::NoDrawType;
            Style.NormalPadding = FMargin(0);
            Style.PressedPadding = FMargin(0);
            Button->SetStyle(Style);
            if (auto BS = Cast<UButtonSlot>(Button->AddChild(Content)))
            {
                BS->SetHorizontalAlignment(HAlign_Fill);
                BS->SetVerticalAlignment(VAlign_Fill);
            }
            if (E.SourceActionId.IsNone())
                Button->OnClicked.AddDynamic(this, &UUCMSourcedWidget::ActivatePreviewButton);
            else
            {
                Button->SourceAction = E.SourceActionId;
                Button->SourceOwner = this;
                Button->OnClicked.AddDynamic(Button, &UUCMSourceActionButton::HandleSourceClick);
                Button->OnHovered.AddDynamic(Button, &UUCMSourceActionButton::HandleSourceHover);
            }
            W = Button;
        }
        if (!E.bButton)
            W->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        W->SetVisibility(E.bEnabled ? (E.bButton ? ESlateVisibility::Visible : ESlateVisibility::SelfHitTestInvisible)
                                    : ESlateVisibility::Collapsed);
        W->SetRenderTransformAngle(E.Rotation);
        UCanvasPanelSlot* CanvasSlot = Root->AddChildToCanvas(W);
        CanvasSlot->SetPosition(E.Position);
        CanvasSlot->SetSize(E.Size);
        CanvasSlot->SetAlignment(FVector2D(0, 0));
    }
    return Super::RebuildWidget();
}

// 기존 위젯의 실행 알림만 전달한다. 로그인이나 게임 시작을 직접 승인하지 않는다.
void UUCMSourcedWidget::ActivatePreviewButton()
{
    Activated.Broadcast();
}

// 원본 버튼 동작 ID를 소유 위젯의 기존 실행 경로로 전달한다.
void UUCMSourceActionButton::HandleSourceClick()
{
    if (SourceOwner)
        SourceOwner->HandleSourceAction(SourceAction);
}

// 호버한 동작 ID를 전달해 키보드와 같은 메뉴 선택 상태를 사용한다.
void UUCMSourceActionButton::HandleSourceHover()
{
    if (SourceOwner)
        SourceOwner->SourceActionHovered.Broadcast(SourceAction);
}

// 복구 동작 ID를 외부 소비자에게 알리고 실제 기능 처리는 기존 MW에 맡긴다.
void UUCMSourcedWidget::HandleSourceAction(FName Action)
{
    SourceActionActivated.Broadcast(Action);
}

// 현재 동작 ID와 맞는 원본 요소만 강조해 메뉴 선택을 표시한다.
void UUCMSourcedWidget::SelectSourceAction(FName Action)
{
    for (int32 I = 0; I < Elements.Num() && I < SourceVisuals.Num(); I++)
    {
        const auto& E = Elements[I];
        auto V = SourceVisuals[I].Get();
        FLinearColor C = (!Action.IsNone() && E.SourceActionId == Action) ? E.HighlightColor : E.Color;
        if (auto Image = Cast<UImage>(V))
            Image->SetColorAndOpacity(SourceTint(C));
        if (auto Grid = Cast<UCanvasPanel>(V))
            for (auto Child : Grid->GetAllChildren())
                if (auto Image = Cast<UImage>(Child))
                    Image->SetColorAndOpacity(SourceTint(C));
        if (auto Border = Cast<UBorder>(V))
            if (auto Text = Cast<UTextBlock>(Border->GetContent()))
                Text->SetColorAndOpacity(FSlateColor(SourceTint(C)));
    }
}

UUCMWorldWidgetComponent::UUCMWorldWidgetComponent()
{
    SetWidgetSpace(EWidgetSpace::World);
    SetDrawSize(FVector2D(3000, 3000));
    SetPivot(FVector2D(.5, .5));
    SetTwoSided(true);
    SetBlendMode(EWidgetBlendMode::Transparent);
    bReceiveHardwareInput = true;
    SetWindowFocusable(true);
    SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

AUCMTitleFlowPreview::AUCMTitleFlowPreview()
{
    PrimaryActorTick.bCanEverTick = true;
    SetReplicates(false);
    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("PreviewRoot"));
    SetRootComponent(Root);
    TitleCanvas = CreateDefaultSubobject<UUCMWorldWidgetComponent>(TEXT("SourceSplashCanvas"));
    TitleCanvas->SetupAttachment(Root);
    MenuCanvas = CreateDefaultSubobject<UUCMWorldWidgetComponent>(TEXT("SourceMainMenuCanvas"));
    MenuCanvas->SetupAttachment(Root);
}

// 원본 연출 이벤트와 경과 시간을 남기고 화면 갱신 구독자에게 알린다.
void AUCMTitleFlowPreview::Record(const FString& Event)
{
    FString Line = FString::Printf(TEXT("%.6f %s"), GetWorld()->GetTimeSeconds() - StartedAt, *Event);
    PreviewEvents.Add(Line);
    PresentationChanged.Broadcast();
    UE_LOG(LogTemp, Display, TEXT("UCM_TITLEFLOW %s"), *Line);
}

// 설정된 자동 시작 정책에 따라 원본 연출만 준비한다. MW 로그인 승인을 대신하지 않는다.
void AUCMTitleFlowPreview::BeginPlay()
{
    Super::BeginPlay();
    TitleCanvas->SetWidgetClass(TitleWidgetClass);
    MenuCanvas->SetWidgetClass(bExternalMenuPresentation ? nullptr : MenuWidgetClass);
    TitleCanvas->InitWidget();
    MenuCanvas->InitWidget();
    if (auto W = Cast<UUCMSourcedWidget>(TitleCanvas->GetWidget()))
        W->Activated.AddDynamic(this, &AUCMTitleFlowPreview::AcceptPreviewInput);
    if (auto W = Cast<UUCMSourcedWidget>(MenuCanvas->GetWidget()))
        W->Activated.AddDynamic(this, &AUCMTitleFlowPreview::AcceptPreviewInput);
    if (auto W = Cast<UUCMSourcedWidget>(MenuCanvas->GetWidget()))
        W->SourceActionActivated.AddDynamic(this, &AUCMTitleFlowPreview::OnSourceMenuAction);
    if (bEnableSourceFrontendMenus && !SourceMenuEntries.IsEmpty())
    {
        MenuCanvas->SetWidget(nullptr);
        MenuCanvas->SetWidgetClass(nullptr);
        for (const auto& E : SourceMenuEntries)
        {
            auto C = NewObject<UUCMWorldWidgetComponent>(
                this, *FString::Printf(TEXT("SourceMenu_%s"), *E.ActionId.ToString()));
            C->DisableHardwareInput();
            C->SetupAttachment(RootComponent);
            C->RegisterComponent();
            C->SetWorldTransform(E.WorldWidgetTransform);
            C->SetDrawSize(E.CanvasSize);
            if (auto Widget = CreateWidget<UUCMSourcedWidget>(GetWorld(), MenuWidgetClass))
            {
                Widget->Elements.RemoveAll(
                    [&](const auto& Element)
                    {
                        return Element.SourceActionId != E.ActionId;
                    });
                Widget->SourceCanvasSize = E.CanvasSize;
                Widget->SourceActionActivated.AddDynamic(this, &AUCMTitleFlowPreview::OnSourceMenuAction);
                Widget->SourceActionHovered.AddDynamic(this, &AUCMTitleFlowPreview::OnSourceMenuHover);
                if (auto PC = UGameplayStatics::GetPlayerController(this, 0))
                    C->SetOwnerPlayer(PC->GetLocalPlayer());
                C->SetWidget(Widget);
            }
            C->SetVisibility(false);
            SourceMenuCanvases.Add(C);
        }
    }
    if (bStartAutomatically)
        ReplayPreview();
    else
    {
        TitleCanvas->SetVisibility(false);
        MenuCanvas->SetVisibility(false);
    }
}

// 외부 MW 연결부가 메뉴 표시와 입력 허용을 함께 제어할 수 있게 한다.
void AUCMTitleFlowPreview::SetSourceMenuPresentation(bool bVisible, bool bInputEnabled)
{
    for (auto C : SourceMenuCanvases)
        if (C)
        {
            C->SetVisibility(bVisible);
            C->SetCollisionEnabled(bInputEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
        }
}

// 복구 인트로 시퀀스와 실제 바인딩으로 재생을 시작한다.
void AUCMTitleFlowPreview::ReplayPreview()
{
    if (SequencePlayer)
        SequencePlayer->Stop();
    if (SequenceActor)
        SequenceActor->Destroy();
    bCameraTransitionActive = false;
    CameraTransitionTime = 0;
    IgnoredTransitionInputs = 0;
    SelectedSourceMenu = 0;
    SuppressedDuplicateInputs = 0;
    LastSourceMenuRequest = NAME_None;
    LastAcceptedInputFrame = MAX_uint64;
    PreviewStage = 0;
    SequenceFinishedAt = -1;
    GameEntryRequests = 0;
    IgnoredIntroInputs = 0;
    PreviewEvents.Reset();
    StartedAt = GetWorld()->GetTimeSeconds();
    TitleCanvas->SetVisibility(false);
    MenuCanvas->SetVisibility(false);
    for (auto C : SourceMenuCanvases)
        if (C)
            C->SetVisibility(false);
    if (auto PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        PC->bShowMouseCursor = true;
        PC->SetInputMode(FInputModeGameAndUI());
        if (PreviewCamera)
            PC->SetViewTarget(PreviewCamera);
    }
    FMovieSceneSequencePlaybackSettings Settings;
    Settings.bAutoPlay = false;
    Settings.bPauseAtEnd = false;
    Settings.bDisableCameraCuts = false;
    if (IntroSequence)
    {
        ALevelSequenceActor* NewSequenceActor = nullptr;
        SequencePlayer =
            ULevelSequencePlayer::CreateLevelSequencePlayer(this, IntroSequence, Settings, NewSequenceActor);
        SequenceActor = NewSequenceActor;
        if (SequencePlayer)
        {
            SequencePlayer->OnFinished.AddDynamic(this, &AUCMTitleFlowPreview::IntroFinished);
            for (const auto& Binding : SourceBindingOverrides)
            {
                TArray<AActor*> Actors;
                for (TActorIterator<AActor> It(GetWorld()); It; ++It)
                    if (It->GetName() == Binding.ActorName)
                        Actors.Add(*It);
                if (Actors.Num() == 1)
                    NewSequenceActor->SetBinding(FMovieSceneObjectBindingID(UE::MovieScene::FRelativeObjectBindingID(
                                                     Binding.BindingId, MovieSceneSequenceID::Root, 0)),
                                                 Actors, false);
                else
                    Record(TEXT("ERROR_FIDELITY_BINDING_NOT_UNIQUE"));
            }
            if (!bRestoringTitlePose)
            {
                SequencePlayer->Play();
                Record(TEXT("INTRO_PLAY_SOURCE_COMPANY_LOGO_UNSKIPPABLE"));
            }
        }
    }
    else
        Record(TEXT("ERROR_MISSING_INTRO_SEQUENCE"));
}

// 일반 종료는 기존 시간 조건을 유지한 공통 완료 처리로 전달한다.
void AUCMTitleFlowPreview::IntroFinished()
{
    CompletePreviewIntro(false);
}

// 재생 중인 인트로만 스킵하고 정상 종료와 같은 최종 카메라 상태로 연결한다.
bool AUCMTitleFlowPreview::SkipPreviewIntro()
{
    if (PreviewStage != 0 || !IsValid(SequencePlayer) || !SequencePlayer->IsPlaying())
        return false;
    // 시퀀스 바인딩을 유지하며 RestoreTitlePose와 같은 최종 시점을 평가한다.
    SequencePlayer->Pause();
    SequencePlayer->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(
        SequencePlayer->GetEndTime().Time - FFrameTime(1), EUpdatePositionMethod::Jump));
    Record(TEXT("INTRO_SKIPPED_BY_USER"));
    CompletePreviewIntro(true);
    return PreviewStage == 1;
}

// 스킵 여부를 구분하되 공통 최종 시점과 타이틀 표시를 한 경로에서 적용한다.
void AUCMTitleFlowPreview::CompletePreviewIntro(bool bSkipped)
{
    if (PreviewStage != 0)
        return;
    SequenceFinishedAt = GetWorld()->GetTimeSeconds() - StartedAt;
    if (!bSkipped && SequenceFinishedAt + 0.15f < SourceOpenSplashWait)
    {
        Record(TEXT("ERROR_PREMATURE_SEQUENCE_FINISH"));
        return;
    }
    PreviewStage = 1;
    TitleCanvas->SetVisibility(true);
    MenuCanvas->SetVisibility(false);
    for (auto C : SourceMenuCanvases)
        if (C)
            C->SetVisibility(false);
    Record(TEXT("SEQUENCE_FINISHED_TITLE_SHOWN_INPUT_ALLOWED"));
}

// 현재 선택 ID를 실제 원본 메뉴 캔버스들에 반영한다.
void AUCMTitleFlowPreview::RefreshSourceMenu()
{
    if (!bEnableSourceFrontendMenus || !SourceMenuEntries.IsValidIndex(SelectedSourceMenu))
        return;
    if (auto W = Cast<UUCMSourcedWidget>(MenuCanvas->GetWidget()))
        W->SelectSourceAction(SourceMenuEntries[SelectedSourceMenu].ActionId);
    for (auto C : SourceMenuCanvases)
        if (C)
            if (auto Widget = Cast<UUCMSourcedWidget>(C->GetWidget()))
                Widget->SelectSourceAction(SourceMenuEntries[SelectedSourceMenu].ActionId);
}

// 유효한 메뉴 인덱스만 반영하고 선택 변경 알림을 전달한다.
bool AUCMTitleFlowPreview::SetSourceMenuSelection(int32 Index)
{
    if (!SourceMenuEntries.IsValidIndex(Index) || SelectedSourceMenu == Index)
        return false;
    SelectedSourceMenu = Index;
    RefreshSourceMenu();
    OnSourceMenuSelectionChanged();
    return true;
}

// 기존 메뉴 목록 범위 안에서 키보드 선택을 이동한다.
void AUCMTitleFlowPreview::MoveSourceMenu(int32 Direction)
{
    if (!bEnableSourceFrontendMenus || PreviewStage != 2 || SourceMenuEntries.IsEmpty())
        return;
    const int32 Next = (SelectedSourceMenu + Direction + SourceMenuEntries.Num()) % SourceMenuEntries.Num();
    if (SetSourceMenuSelection(Next))
        Record(FString::Printf(TEXT("SOURCE_MENU_SELECTION %s"), *SourceMenuEntries[Next].ActionId.ToString()));
}

// 마우스 호버도 기존 메뉴 선택 상태로 연결해 입력 방식을 섞어 쓸 수 있게 한다.
void AUCMTitleFlowPreview::OnSourceMenuHover(FName Action)
{
    if (PreviewStage != 2 || bSuppressPreviewInput)
        return;
    const int32 Index = SourceMenuEntries.IndexOfByPredicate(
        [Action](const auto& Entry)
        {
            return Entry.ActionId == Action;
        });
    SetSourceMenuSelection(Index);
}

// 실제 표시·입력 단계가 맞을 때만 동작 ID를 선택하고 실행 입력으로 전달한다.
void AUCMTitleFlowPreview::OnSourceMenuAction(FName Action)
{
    if ((!bEnableSourceFrontendMenus && !bExternalMenuPresentation) || PreviewStage != 2)
        return;
    int32 I = SourceMenuEntries.IndexOfByPredicate(
        [&](const auto& E)
        {
            return E.ActionId == Action;
        });
    if (I == INDEX_NONE)
        return;
    SelectedSourceMenu = I;
    RefreshSourceMenu();
    AcceptPreviewInput();
}

// 복구한 역방향 전환이 있으면 재사용하고 기존 타이틀 시점으로 돌아간다.
void AUCMTitleFlowPreview::ReturnToSourceTitle()
{
    if ((!bEnableSourceFrontendMenus && !bExternalMenuPresentation) || PreviewStage != 2)
        return;
    if (bEnableSourceCameraTransition)
    {
        BeginSourceCameraTransition(true);
        return;
    }
    PreviewStage = 1;
    MenuCanvas->SetVisibility(false);
    for (auto C : SourceMenuCanvases)
        if (C)
            C->SetVisibility(false);
    TitleCanvas->SetVisibility(true);
    if (PreviewCamera)
        if (auto PC = UGameplayStatics::GetPlayerController(this, 0))
            PC->SetViewTarget(PreviewCamera);
    Record(TEXT("PREVIEW_RETURN_TO_TITLE_ADAPTER_RUNTIME_EQUIVALENCE_NOT_VERIFIED"));
}

// 인트로·카메라 전환 중 입력을 구분하고 준비된 단계에서만 기존 메뉴 실행을 전달한다.
void AUCMTitleFlowPreview::AcceptPreviewInput()
{
    if (bExternalMenuPresentation && bSuppressPreviewInput)
        return;
    if (bEnableSourceFrontendMenus)
    {
        if (LastAcceptedInputFrame == GFrameCounter)
        {
            SuppressedDuplicateInputs++;
            return;
        }
        LastAcceptedInputFrame = GFrameCounter;
    }
    if (PreviewStage == 0)
    {
        IgnoredIntroInputs++;
        Record(TEXT("INPUT_IGNORED_SOURCE_UNSKIPPABLE"));
        return;
    }
    if (PreviewStage == 3 || PreviewStage == 4)
    {
        IgnoredTransitionInputs++;
        return;
    }
    if (PreviewStage == 1 && bEnableSourceCameraTransition)
    {
        BeginSourceCameraTransition(false);
        return;
    }
    if (PreviewStage == 1)
    {
        PreviewStage = 2;
        TitleCanvas->SetVisibility(false);
        MenuCanvas->SetVisibility(true);
        for (auto C : SourceMenuCanvases)
            if (C)
                C->SetVisibility(true);
        if (MenuCamera)
            if (auto PC = UGameplayStatics::GetPlayerController(this, 0))
                PC->SetViewTarget(MenuCamera);
        RefreshSourceMenu();
        Record(TEXT("PREVIEW_ACCEPT_MAINMENU_SHOWN_AUTH_BRANCH_NOT_IMPLEMENTED"));
        return;
    }
    if (PreviewStage == 2 && bExternalMenuPresentation)
    {
        if (SourceMenuEntries.IsValidIndex(SelectedSourceMenu))
            ExternalMenuAction.Broadcast(SourceMenuEntries[SelectedSourceMenu].ActionId);
        return;
    }
    if (PreviewStage == 2 && bEnableSourceFrontendMenus)
    {
        if (!SourceMenuEntries.IsValidIndex(SelectedSourceMenu))
            return;
        const auto& E = SourceMenuEntries[SelectedSourceMenu];
        LastSourceMenuRequest = E.SourceMethod;
        MenuActionRequested.Broadcast(E.SourceMethod, E.SourceDestination);
        if (E.SourceMethod == TEXT("LocalBattleMode"))
        {
            GameEntryRequests++;
            GameEntryRequested.Broadcast(TEXT("MainMenu.LocalBattleMode"), E.SourceDestination);
        }
        Record(FString::Printf(TEXT("SOURCE_MENU_REQUEST_ONLY %s -> %s NO_DESTINATION_IMPLEMENTED"),
                               *E.SourceMethod.ToString(), *E.SourceDestination.ToString()));
        return;
    }
    if (PreviewStage == 2)
    {
        GameEntryRequests++;
        GameEntryRequested.Broadcast(TEXT("MainMenu.LocalBattleMode"), TEXT("BattleModeSelectMenu"));
        Record(TEXT("GAME_ENTRY_REQUEST_ONLY_NO_MATCH_LOADED"));
    }
}

// 현재 실제 시점을 출발점으로 잡아 복구한 카메라 경로로 부드럽게 연결한다.
void AUCMTitleFlowPreview::BeginSourceCameraTransition(bool bReverse)
{
    auto PC = UGameplayStatics::GetPlayerController(this, 0);
    const auto& Samples = bReverse ? SourceTransitionReverse : SourceTransitionForward;
    if (!PC || !PC->PlayerCameraManager || !MenuCamera || Samples.Num() < 2)
    {
        Record(TEXT("ERROR_SOURCE_TRANSITION_DATA_MISSING"));
        return;
    }
    auto Camera = MenuCamera->FindComponentByClass<UCameraComponent>();
    if (!Camera)
    {
        Record(TEXT("ERROR_SOURCE_TRANSITION_CAMERA_COMPONENT_MISSING"));
        return;
    }
    // 시점을 교체하기 전에 현재 평가된 카메라를 기록한다. 인트로 카메라·시퀀스는 수정하지 않는다.
    TransitionOutgoing =
        FTransform(PC->PlayerCameraManager->GetCameraRotation(), PC->PlayerCameraManager->GetCameraLocation());
    TransitionOutgoingFOV = PC->PlayerCameraManager->GetFOVAngle();
    MenuCamera->SetActorTransform(TransitionOutgoing);
    Camera->SetFieldOfView(TransitionOutgoingFOV);
    PC->SetViewTarget(MenuCamera);
    bCameraTransitionReverse = bReverse;
    bCameraTransitionActive = true;
    CameraTransitionTime = 0;
    PreviewStage = bReverse ? 4 : 3;
    TitleCanvas->SetVisibility(bReverse);
    MenuCanvas->SetVisibility(!bReverse);
    for (auto C : SourceMenuCanvases)
        if (C)
            C->SetVisibility(!bReverse);
    RefreshSourceMenu();
    Record(bReverse ? TEXT("SOURCE_REVERSE_DOLLY_BEGIN_INPUT_LOCKED")
                    : TEXT("SOURCE_FORWARD_DOLLY_BEGIN_INPUT_LOCKED"));
}

// 원본 샘플 시간과 카메라 블렌드를 유지하며 전환을 재생한다.
void AUCMTitleFlowPreview::UpdateSourceCameraTransition(float DeltaTime)
{
    if (!bCameraTransitionActive || !MenuCamera)
        return;
    const auto& Samples = bCameraTransitionReverse ? SourceTransitionReverse : SourceTransitionForward;
    if (Samples.Num() < 2)
        return;
    CameraTransitionTime += DeltaTime;
    int32 I = 0;
    while (I + 1 < Samples.Num() && Samples[I + 1].Time < CameraTransitionTime)
        ++I;
    const auto& A = Samples[I];
    const auto& B = Samples[FMath::Min(I + 1, Samples.Num() - 1)];
    float T = FMath::Clamp((CameraTransitionTime - A.Time) / FMath::Max(B.Time - A.Time, .00001f), 0.f, 1.f);
    FVector P = FMath::Lerp(A.Pose.GetLocation(), B.Pose.GetLocation(), T);
    FQuat Q = FQuat::Slerp(A.Pose.GetRotation(), B.Pose.GetRotation(), T);
    float U = FMath::Clamp(CameraTransitionTime / FMath::Max(SourceBrainBlendSeconds, .00001f), 0.f, 1.f);
    float Weight = U * U * (3 - 2 * U);
    MenuCamera->SetActorLocationAndRotation(FMath::Lerp(TransitionOutgoing.GetLocation(), P, Weight),
                                            FQuat::Slerp(TransitionOutgoing.GetRotation(), Q, Weight));
    // 기존 Preview의 16:9 비율 정책과 인트로 렌즈 키를 유지한다.
    float TargetFOV = FMath::RadiansToDegrees(
        2 * FMath::Atan(FMath::Tan(FMath::DegreesToRadians(SourceTransitionVerticalFOV) * .5f) * 16.f / 9.f));
    if (auto C = MenuCamera->FindComponentByClass<UCameraComponent>())
        C->SetFieldOfView(FMath::Lerp(TransitionOutgoingFOV, TargetFOV, Weight));
    if ((PreviewStage == 3 || PreviewStage == 4) && U >= 1)
    {
        PreviewStage = bCameraTransitionReverse ? 1 : 2;
        Record(bCameraTransitionReverse ? TEXT("SOURCE_REVERSE_BRAIN_BLEND_COMPLETE_TITLE_INPUT_ALLOWED")
                                        : TEXT("SOURCE_FORWARD_BRAIN_BLEND_COMPLETE_MENU_INPUT_ALLOWED"));
    }
    if (CameraTransitionTime >= Samples.Last().Time)
    {
        bCameraTransitionActive = false;
        Record(TEXT("SOURCE_DOLLY_DAMPING_TAIL_COMPLETE"));
    }
}

// 표시 중인 메뉴 표면만 검사해 원본 배경 메시가 버튼 입력을 막지 않게 한다.
void AUCMTitleFlowPreview::UpdateSourceMenuPointer(APlayerController* PC)
{
    if (!PC)
        return;
    const bool bActive = bEnableSourceFrontendMenus && PreviewStage == 2 && !bSuppressPreviewInput;
    if (!bActive)
    {
        PressedSourceMenu.Reset();
        HoveredSourceMenu.Reset();
        bHaveSourceMousePosition = false;
        return;
    }
    FHitResult BestHit;
    float X = 0, Y = 0;
    FVector Start, Direction;
    const bool bHasMouse = PC->GetMousePosition(X, Y);
    if (bHasMouse)
    {
        if (PC->DeprojectScreenPositionToWorld(X, Y, Start, Direction))
        {
            const FVector End = Start + Direction * 100000.f;
            float BestDistance = TNumericLimits<float>::Max();
            // 보이는 메뉴 위젯만 히트 테스트한다. 뒤쪽 원본 표지판이
            // UI를 가리지 않게 하고 다른 월드·화면 위젯의 입력 경로는 유지한다.
            for (const auto& Canvas : SourceMenuCanvases)
                if (Canvas && Canvas->IsVisible() && Canvas->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
                {
                    FHitResult Hit;
                    if (Canvas->LineTraceComponent(Hit, Start, End,
                                                   FCollisionQueryParams(SCENE_QUERY_STAT(UCMMenuPointer), false)) &&
                        Hit.Distance < BestDistance)
                    {
                        BestDistance = Hit.Distance;
                        BestHit = Hit;
                    }
                }
        }
    }
    auto* HitMenu = Cast<UUCMWorldWidgetComponent>(BestHit.GetComponent());
    const int32 HitIndex = SourceMenuCanvases.IndexOfByPredicate(
        [HitMenu](const auto& Canvas)
        {
            return Canvas.Get() == HitMenu;
        });
    if (bHasMouse)
    {
        const FVector2D MousePosition(X, Y);
        if (HitIndex != INDEX_NONE && SourceMenuEntries.IsValidIndex(HitIndex) &&
            (!bHaveSourceMousePosition || !MousePosition.Equals(LastSourceMousePosition) ||
             HoveredSourceMenu.Get() != HitMenu))
            OnSourceMenuHover(SourceMenuEntries[HitIndex].ActionId);
        LastSourceMousePosition = MousePosition;
        bHaveSourceMousePosition = true;
    }
    HoveredSourceMenu = HitMenu;
    if (PC->WasInputKeyJustPressed(EKeys::LeftMouseButton))
    {
        PressedSourceMenu = HitMenu;
        UE_LOG(LogTemp, Display, TEXT("UCM_MENU_POINTER mouse_press hit=%s"), *GetNameSafe(HitMenu));
    }
    else if (PressedSourceMenu.IsValid() &&
             (PC->WasInputKeyJustReleased(EKeys::LeftMouseButton) || !PC->IsInputKeyDown(EKeys::LeftMouseButton)))
    {
        const bool bSameMenu =
            PressedSourceMenu.Get() == HitMenu && HitIndex != INDEX_NONE && SourceMenuEntries.IsValidIndex(HitIndex);
        UE_LOG(LogTemp, Display, TEXT("UCM_MENU_POINTER mouse_release hit=%s same=%d"), *GetNameSafe(HitMenu),
               bSameMenu);
        PressedSourceMenu.Reset();
        if (bSameMenu)
            OnSourceMenuAction(SourceMenuEntries[HitIndex].ActionId);
    }
}

// 인트로·카메라·메뉴의 현재 재생 단계에 맞는 로컬 입력과 표시만 갱신한다.
void AUCMTitleFlowPreview::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    UpdateSourceCameraTransition(DeltaTime);
    UpdateSourceMenuPointer(UGameplayStatics::GetPlayerController(this, 0));
    if (bExternalMenuPresentation && bSuppressPreviewInput)
        return;
    if (bEnableSourceFrontendMenus)
        if (auto PC = UGameplayStatics::GetPlayerController(this, 0))
        {
            if (PC->WasInputKeyJustPressed(EKeys::Escape) ||
                PC->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Right))
            {
                ReturnToSourceTitle();
                return;
            }
            if (PC->WasInputKeyJustPressed(EKeys::W) || PC->WasInputKeyJustPressed(EKeys::Up) ||
                PC->WasInputKeyJustPressed(EKeys::Gamepad_DPad_Up) ||
                PC->WasInputKeyJustPressed(EKeys::Gamepad_LeftStick_Up))
                MoveSourceMenu(-1);
            else if (PC->WasInputKeyJustPressed(EKeys::S) || PC->WasInputKeyJustPressed(EKeys::Down) ||
                     PC->WasInputKeyJustPressed(EKeys::Gamepad_DPad_Down) ||
                     PC->WasInputKeyJustPressed(EKeys::Gamepad_LeftStick_Down))
                MoveSourceMenu(1);
        }
    if (auto PC = UGameplayStatics::GetPlayerController(this, 0))
        if (PC->WasInputKeyJustPressed(EKeys::SpaceBar) || PC->WasInputKeyJustPressed(EKeys::Enter) ||
            PC->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Bottom))
            AcceptPreviewInput();
}

// 현재 시퀀스를 정지하고 연출 액터의 월드 수명을 종료한다.
void AUCMTitleFlowPreview::EndPlay(const EEndPlayReason::Type Reason)
{
    if (SequencePlayer)
        SequencePlayer->Stop();
    Super::EndPlay(Reason);
}

// 원본 연출의 실행 이력과 현재 상태를 진단용 문자열로 반환한다.
FString AUCMTitleFlowPreview::GetPreviewEvidence() const
{
    TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
    O->SetNumberField(TEXT("stage"), PreviewStage);
    O->SetBoolField(TEXT("source_camera_transition_enabled"), bEnableSourceCameraTransition);
    O->SetNumberField(TEXT("camera_transition_time"), CameraTransitionTime);
    O->SetNumberField(TEXT("ignored_transition_inputs"), IgnoredTransitionInputs);
    O->SetBoolField(TEXT("camera_damping_active"), bCameraTransitionActive);
    O->SetBoolField(TEXT("source_frontend_enabled"), bEnableSourceFrontendMenus);
    O->SetNumberField(TEXT("source_world_menu_canvases"), SourceMenuCanvases.Num());
    O->SetNumberField(TEXT("selected_source_menu"), SelectedSourceMenu);
    O->SetNumberField(TEXT("menu_entries"), SourceMenuEntries.Num());
    O->SetNumberField(TEXT("suppressed_duplicate_inputs"), SuppressedDuplicateInputs);
    O->SetStringField(TEXT("last_source_menu_request"), LastSourceMenuRequest.ToString());
    O->SetNumberField(TEXT("elapsed"), GetWorld() ? GetWorld()->GetTimeSeconds() - StartedAt : 0);
    O->SetNumberField(TEXT("sequence_finished_at"), SequenceFinishedAt);
    O->SetNumberField(TEXT("ignored_intro_inputs"), IgnoredIntroInputs);
    O->SetNumberField(TEXT("game_entry_requests"), GameEntryRequests);
    O->SetBoolField(TEXT("title_visible"), TitleCanvas->IsVisible());
    O->SetBoolField(TEXT("menu_visible"), MenuCanvas->IsVisible());
    O->SetBoolField(TEXT("sequence_is_playing"), SequencePlayer && SequencePlayer->IsPlaying());
    O->SetStringField(TEXT("source_company_logo"), SourceCompanyLogo);
    O->SetStringField(TEXT("source_splash"), SourceSplashScreen);
    O->SetStringField(TEXT("source_main_menu"), SourceMainMenu);
    O->SetNumberField(TEXT("app_volume"), FApp::GetVolumeMultiplier());
    O->SetNumberField(TEXT("unfocused_volume"), FApp::GetUnfocusedVolumeMultiplier());
    O->SetNumberField(TEXT("app_delta"), FApp::GetDeltaTime());
    O->SetNumberField(TEXT("real_world_time"), GetWorld()->GetRealTimeSeconds());
    O->SetNumberField(TEXT("time_dilation"), GetWorld()->GetWorldSettings()->GetEffectiveTimeDilation());
    TArray<TSharedPtr<FJsonValue>> Sounds;
    for (TObjectIterator<UAudioComponent> It; It; ++It)
    {
        if (It->GetWorld() != GetWorld())
            continue;
        TSharedRef<FJsonObject> S = MakeShared<FJsonObject>();
        S->SetBoolField(TEXT("playing"), It->IsPlaying());
        S->SetNumberField(TEXT("volume"), It->VolumeMultiplier);
        S->SetStringField(TEXT("sound"), It->Sound ? It->Sound->GetPathName() : TEXT("None"));
        Sounds.Add(MakeShared<FJsonValueObject>(S));
    }
    O->SetArrayField(TEXT("audio_components"), Sounds);
    TArray<TSharedPtr<FJsonValue>> E;
    for (const FString& V : PreviewEvents)
        E.Add(MakeShared<FJsonValueString>(V));
    O->SetArrayField(TEXT("events"), E);
    FString Result;
    auto Writer = TJsonWriterFactory<>::Create(&Result);
    FJsonSerializer::Serialize(O, Writer);
    return Result;
}

// 복구 요소와 현재 위젯 연결 상태를 검증용으로 조회한다.
FString UUCMSourcedWidget::GetSourceWidgetEvidence() const
{
    auto O = MakeShared<FJsonObject>();
    O->SetNumberField(TEXT("source_elements"), Elements.Num());
    O->SetNumberField(TEXT("painted_source_visuals"), SourceVisuals.Num());
    auto Root = WidgetTree ? Cast<UPanelWidget>(WidgetTree->RootWidget) : nullptr;
    O->SetNumberField(TEXT("root_canvas_children"), Root ? Root->GetChildrenCount() : 0);
    O->SetNumberField(TEXT("widget_render_opacity"), GetRenderOpacity());
    int32 AnimationCount = 0;
    for (UClass* C = GetClass(); C; C = C->GetSuperClass())
        if (auto B = Cast<UWidgetBlueprintGeneratedClass>(C))
            AnimationCount += B->Animations.Num();
    O->SetNumberField(TEXT("widget_animation_count"), AnimationCount);
    TArray<TSharedPtr<FJsonValue>> Nodes;
    TArray<UWidget*> All;
    if (WidgetTree)
        WidgetTree->GetAllWidgets(All);
    for (UWidget* W : All)
        if (W->GetName().StartsWith(TEXT("Continue")))
        {
            auto N = MakeShared<FJsonObject>();
            N->SetStringField(TEXT("name"), W->GetName());
            N->SetStringField(TEXT("class"), W->GetClass()->GetName());
            N->SetStringField(TEXT("parent"), W->GetParent() ? W->GetParent()->GetName() : TEXT("None"));
            const auto Size = W->GetCachedGeometry().GetLocalSize();
            const auto Desired = W->GetDesiredSize();
            const auto Position = W->GetCachedGeometry().GetAbsolutePosition();
            N->SetArrayField(TEXT("layout_size"),
                             {MakeShared<FJsonValueNumber>(Size.X), MakeShared<FJsonValueNumber>(Size.Y)});
            N->SetArrayField(TEXT("desired_size"),
                             {MakeShared<FJsonValueNumber>(Desired.X), MakeShared<FJsonValueNumber>(Desired.Y)});
            N->SetArrayField(TEXT("layout_position"),
                             {MakeShared<FJsonValueNumber>(Position.X), MakeShared<FJsonValueNumber>(Position.Y)});
            N->SetNumberField(TEXT("render_opacity"), W->GetRenderOpacity());
            if (auto T = Cast<UTextBlock>(W))
            {
                N->SetStringField(TEXT("text"), T->GetText().ToString());
                N->SetBoolField(TEXT("auto_wrap"), T->GetAutoWrapText());
                N->SetNumberField(TEXT("wrap_at"), T->GetWrapTextAt());
                N->SetStringField(TEXT("color"), T->GetColorAndOpacity().GetSpecifiedColor().ToString());
            }
            if (auto B = Cast<UBorder>(W))
            {
                auto P = B->GetPadding();
                N->SetArrayField(TEXT("padding"),
                                 {MakeShared<FJsonValueNumber>(P.Left), MakeShared<FJsonValueNumber>(P.Top),
                                  MakeShared<FJsonValueNumber>(P.Right), MakeShared<FJsonValueNumber>(P.Bottom)});
                N->SetStringField(TEXT("brush_color"), B->GetBrushColor().ToString());
            }
            Nodes.Add(MakeShared<FJsonValueObject>(N));
        }
    O->SetArrayField(TEXT("continue_prompt_nodes"), Nodes);
    O->SetStringField(TEXT("layout_units"), TEXT("UMG layout/canvas units, not physical screen pixels"));
    FString S;
    auto Writer = TJsonWriterFactory<>::Create(&S);
    FJsonSerializer::Serialize(O, Writer);
    return S;
}

// 원본 시퀀스의 최종 자세와 타이틀 시점을 복원한다.
void AUCMTitleFlowPreview::RestoreTitlePose()
{
    TGuardValue<bool> Guard(bRestoringTitlePose, true);
    ReplayPreview();
    if (SequencePlayer)
    {
        SequencePlayer->Pause();
        SequencePlayer->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(
            SequencePlayer->GetEndTime().Time - FFrameTime(1), EUpdatePositionMethod::Jump));
    }
    StartedAt = GetWorld()->GetTimeSeconds() - SourceOpenSplashWait;
    IntroFinished();
}
