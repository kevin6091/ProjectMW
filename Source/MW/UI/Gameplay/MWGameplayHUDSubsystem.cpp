// 기존 경기 상태와 아이템 이벤트를 표시하는 HUD다. 서버 결과를 자체 타이머로 확정하지 않는다.

#include "UI/Gameplay/MWGameplayHUDSubsystem.h"

#include "Brushes/SlateImageBrush.h"
#include "Components/SceneComponent.h"
#include "Data/MWVehicleDefinitionData.h"
#include "Engine/GameViewportClient.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/Paths.h"
#include "Player/MWGameplayPlayerController.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Fonts/FontMeasure.h"
#include "Styling/CoreStyle.h"
#include "System/GameState/MWGameplayGameState.h"
#include "System/MWAssetManager.h"
#include "System/MWPlayerState.h"
#include "UI/Gameplay/MWGameplaySign.h"
#include "UI/Gameplay/MWRaceHUDWidget.h"
#include "Widgets/SNullWidget.h"
#include "UI/Common/MWParticipantPalette.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

DEFINE_LOG_CATEGORY_STATIC(LogMWInGameUI, Log, All);

namespace
{
    // 명시된 아이템 픽업 개발 맵의 PIE에서만 슬롯 단독 표시를 허용한다.
    bool IsItemPickupTestPIE(const UWorld& World)
    {
        return World.WorldType == EWorldType::PIE && UWorld::RemovePIEPrefix(World.GetOutermost()->GetName()) ==
                                                         TEXT("/Game/Developers/user/L_Dev_Drive_ItemPickup");
    }


    class SMWItemSlotFrame : public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(SMWItemSlotFrame)
        {
        }
        SLATE_ARGUMENT(float, Inset)
        SLATE_DEFAULT_SLOT(FArguments, Content)
        SLATE_END_ARGS()

        // 아이템 내부 콘텐츠와 벡터 유리 외곽을 같은 슬롯 크기에 연결한다.
        void Construct(const FArguments& Args)
        {
            ChildSlot.Padding(Args._Inset)[Args._Content.Widget];
        }

        // SVG 테두리와 그림자를 현재 Geometry로 그려 확대 시 외곽이 깨지는 것을 줄인다.
        virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
                              FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style,
                              bool bParentEnabled) const override
        {
            const FVector2f Size(Geometry.GetLocalSize());
            // 벡터 유리 면 아래의 그림자까지 그려 크기가 달라져도 하단이 잘리지 않게 한다.
            const FVector2f ArtSize(Size.X, Size.Y * 176.f / 168.f);
            FSlateDrawElement::MakeBox(Elements, Layer, Geometry.ToPaintGeometry(ArtSize, FSlateLayoutTransform()),
                                       &GlassBrush, ESlateDrawEffect::NoPixelSnapping, Style.GetColorAndOpacityTint());
            return SCompoundWidget::OnPaint(Args, Geometry, CullingRect, Elements, Layer + 1, Style, bParentEnabled);
        }

    private:
        FSlateVectorImageBrush GlassBrush{FPaths::ProjectContentDir() / TEXT("UI/Common/Slate/ItemSlotGlass.svg"),
                                          FVector2D(168.f, 176.f)};
    };

    class SMWGameplayHUD : public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(SMWGameplayHUD)
        {
        }
        SLATE_ARGUMENT(APlayerController*, Controller)
        SLATE_ARGUMENT(bool, bItemPickupTest)
        SLATE_ATTRIBUTE(EMWItemId, HeldItem)
        SLATE_ARGUMENT(TSharedPtr<SWidget>, RaceHUD)
        SLATE_END_ARGS()

        // 기존 경기 상태를 읽는 표지판·대기판·아이템 슬롯의 Slate 구조와 설정 자원을 준비한다.
        void Construct(const FArguments& Args)
        {
            Controller = Args._Controller;
            bItemPickupTest = Args._bItemPickupTest;
            HeldItem = Args._HeldItem;
            const auto* Settings = GetDefault<UMWGameplayHUDSubsystem>();
            InitializeLayout(*Settings);
            InitializeItemTiming(*Settings);
            BoosterTexture.Reset(Settings->BoosterIcon.LoadSynchronous());
            if (BoosterTexture)
            {
                const FVector2D TextureSize(BoosterTexture->GetSizeX(), BoosterTexture->GetSizeY());
                const FVector2D IconSize = TextureSize * (ItemLayout.IconSize / FMath::Max(1.0, TextureSize.GetMax()));
                BoosterIconOffset = (FVector2D(.5, .5) - Settings->BoosterIconContentCenter) * IconSize;
                UObject* IconResource = BoosterTexture.Get();
                if (UMaterialInterface* Material = Settings->ItemIconMaterial.LoadSynchronous())
                {
                    BoosterIconMaterial.Reset(UMaterialInstanceDynamic::Create(Material, GetTransientPackage()));
                    if (BoosterIconMaterial)
                    {
                        BoosterIconMaterial->SetTextureParameterValue(TEXT("ItemIcon"), BoosterTexture.Get());
                        IconResource = BoosterIconMaterial.Get();
                    }
                }
                BoosterBrush = FSlateImageBrush(IconResource, IconSize);
            }
            for (const TSoftObjectPtr<UTexture2D>& Icon : Settings->ItemRouletteIcons)
            {
                if (UTexture2D* Texture = Icon.LoadSynchronous())
                {
                    const FVector2D TextureSize(Texture->GetSizeX(), Texture->GetSizeY());
                    const FVector2D IconSize = TextureSize * (ItemLayout.IconSize / FMath::Max(1.0, TextureSize.GetMax()));
                    RouletteTextures.Emplace(Texture);
                    RouletteBrushes.Emplace(FSlateImageBrush(Texture, IconSize));
                }
            }
            ForceVolatile(true);
            const auto* Brush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));

            const TSharedRef<SWidget> CenteredHUD = MWGameplayUI::FitDesignCanvas(
                SNew(SOverlay) +
                SOverlay::Slot()
                    [SAssignNew(WaitingBoard, SOverlay).Visibility(EVisibility::Collapsed) +
                     SOverlay::Slot().HAlign(HAlign_Center)[SNew(SBox).WidthOverride(
                         BoardLayout.PoleWidth)[SNew(SImage).Image(Art.PoleBrush())]] +
                     SOverlay::Slot()
                         .HAlign(HAlign_Center)
                         .VAlign(VAlign_Top)
                         .Padding(BoardLayout.LogoMargin)[SNew(SBox).WidthOverride(BoardLayout.LogoSize.X).HeightOverride(
                             BoardLayout.LogoSize.Y)
                             [SNew(SScaleBox)
                                  .Stretch(EStretch::ScaleToFit)
                                  .HAlign(HAlign_Center)
                                  .VAlign(VAlign_Center)[SNew(SImage).Image(Art.LogoBrush())]]] +
                     SOverlay::Slot()
                         .HAlign(HAlign_Center)
                         .VAlign(VAlign_Center)
                         .Padding(BoardLayout.PanelMargin)
                             [SNew(SMWGameplayPanel)
                                  .Size(BoardLayout.PanelSize)
                                  .Fill(FLinearColor::FromSRGBColor(FColor(92, 145, 211)))
                                  .Border(FLinearColor::FromSRGBColor(FColor(126, 174, 227)))
                                  .BorderWidth(BoardLayout.BorderWidth)[SNew(SVerticalBox) +
                                                    SVerticalBox::Slot().AutoHeight()
                                                        [SNew(SBox)
                                                             .HeightOverride(BoardLayout.TitleHeight)
                                                             .HAlign(HAlign_Center)
                                                             .VAlign(VAlign_Center)
                                                                 [SNew(STextBlock)
                                                                      .Text(NSLOCTEXT("MWGameplayUI",
                                                                                      "RaceWaitingTitle", "경기 준비"))
                                                                      .Font(Art.Font(BoardLayout.TitleFontSize))
                                                                      .ColorAndOpacity(FLinearColor::White)]] +
                                                    SVerticalBox::Slot().AutoHeight()[SNew(SBox).HeightOverride(
                                                        BoardLayout.DividerHeight)[SNew(SBorder).BorderImage(Brush).BorderBackgroundColor(
                                                        FLinearColor(.11f, .19f, .32f))]] +
                                                    SVerticalBox::Slot().AutoHeight().Padding(
                                                        BoardLayout.RosterPadding)[SAssignNew(Roster, SGridPanel)]]]] +
                SOverlay::Slot()
                    .HAlign(HAlign_Center)
                    .VAlign(VAlign_Center)[SAssignNew(SelectSign, SMWGameplaySign)
                                               .Visibility(EVisibility::Collapsed)
                                               .Label(NSLOCTEXT("MWGameplayUI", "SelectTrack",
                                                                "트랙 부속품을\n고르세요"))] +
                SOverlay::Slot()
                    .HAlign(HAlign_Center)
                    .VAlign(VAlign_Center)[SAssignNew(BuildSign, SMWGameplaySign)
                                               .Visibility(EVisibility::Collapsed)
                                               .Label(NSLOCTEXT("MWGameplayUI", "BuildTrack", "트랙을\n만드세요"))]);

            ChildSlot
                [SNew(SOverlay) + SOverlay::Slot()[CenteredHUD] +
                 SOverlay::Slot()
                 [
                     MWGameplayUI::FitDesignCanvas(Args._RaceHUD.ToSharedRef(), HAlign_Right, VAlign_Bottom)
                 ] +
                 SOverlay::Slot()[MWGameplayUI::FitDesignCanvas(
                     SNew(SOverlay) +
                         SOverlay::Slot()
                             .HAlign(HAlign_Left)
                             .VAlign(VAlign_Bottom)
                             .Padding(ItemLayout.Margin)
                                 [SAssignNew(ItemSlot, SBox)
                                      .WidthOverride(ItemLayout.Size)
                                      .HeightOverride(ItemLayout.Size)
                                      .Visibility(EVisibility::Collapsed)
                                          [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
                                               [SNew(SBox)
                                                    .WidthOverride(ItemLayout.Size)
                                                    .HeightOverride(ItemLayout.Size)
                                                        [SNew(SMWItemSlotFrame)
                                                             .Inset(ItemLayout.FrameInset)
                                                             [SNew(SOverlay) +
                                                              SOverlay::Slot()
                                                                  .HAlign(HAlign_Center)
                                                                  .VAlign(VAlign_Center)
                                                                      [SNew(SImage)
                                                                           .Image_Lambda(
                                                                               [this]
                                                                               {
                                                                                   return GetItemSlotBrush();
                                                                               })
                                                                           .RenderTransformPivot(FVector2D(.5, .5))
                                                                           .RenderTransform_Lambda(
                                                                               [this]()
                                                                                   -> TOptional<FSlateRenderTransform>
                                                                               {
                                                                                   const FVector2D Offset =
                                                                                       RouletteStep == INDEX_NONE
                                                                                           ? BoosterIconOffset
                                                                                           : FVector2D::ZeroVector;
                                                                                   return FSlateRenderTransform(
                                                                                       ItemIconScale,
                                                                                       Offset * ItemIconScale);
                                                                               })
                                                                           .ColorAndOpacity_Lambda(
                                                                               [this]
                                                                               {
                                                                                   return FSlateColor(FLinearColor(
                                                                                       1.f, 1.f, 1.f, ItemIconOpacity));
                                                                               })
                                                                           .Visibility_Lambda(
                                                                               [this]
                                                                               {
                                                                                   return GetItemSlotBrush()
                                                                                              ? EVisibility::
                                                                                                    HitTestInvisible
                                                                                              : EVisibility::Collapsed;
                                                                               })] +
                                                              SOverlay::Slot()
                                                                  .HAlign(HAlign_Center)
                                                                  .VAlign(VAlign_Center)
                                                                      [SNew(STextBlock)
                                                                           .Font(Art.Font(24.f))
                                                                           .ColorAndOpacity_Lambda(
                                                                               [this]
                                                                               {
                                                                                   return FSlateColor(
                                                                                       FLinearColor(.72f, .75f, .80f,
                                                                                                    ItemIconOpacity));
                                                                               })
                                                                           .Visibility_Lambda(
                                                                               [this]
                                                                               {
                                                                                   return !GetItemSlotBrush() &&
                                                                                                  DisplayedItem ==
                                                                                                      EMWItemId::Booster
                                                                                              ? EVisibility::
                                                                                                    HitTestInvisible
                                                                                              : EVisibility::Collapsed;
                                                                               })
                                                                           .Text(NSLOCTEXT("MWGameplayUI",
                                                                                           "BoosterItem",
                                                                                           "부스터"))]]]]]],
                     HAlign_Left, VAlign_Bottom)]];
        }

        // 표시 단계·룰렛 시간·차량 닉네임 정보를 갱신한다. 서버 경기 단계를 자체 변경하지 않는다.
        virtual void Tick(const FGeometry& Geometry, double CurrentTime, float) override
        {
            const auto* PC = Controller.Get();
            const auto* GS = PC ? PC->GetWorld()->GetGameState<AMWGameplayGameState>() : nullptr;
            const auto* Vehicle = PC ? PC->GetPawn<AMWVehiclePawn>() : nullptr;
            const bool bItemPhase = bItemPickupTest || (GS && GS->GetRoomPhase() == EMWRoomPhase::Racing);
            const bool bShowItemSlot = bItemPhase && Vehicle && Vehicle->GetItemComponent();
            ItemSlot->SetVisibility(bShowItemSlot ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
            if (bShowItemSlot)
            {
                const FVector2f ViewportSize(Geometry.GetLocalSize());
                const float CanvasScale = FMath::Min(ViewportSize.X / MWGameplayUI::DesignWidth,
                                                     ViewportSize.Y / MWGameplayUI::DesignHeight) *
                                          Geometry.GetAccumulatedLayoutTransform().GetScale();
                if (CanvasScale > UE_SMALL_NUMBER)
                {
                    const float SlotSize = FMath::Clamp(ItemLayout.MinimumPixels / CanvasScale, ItemLayout.Size,
                                                       MWGameplayUI::DesignHeight * ItemLayout.MaximumHeightFraction);
                    ItemSlot->SetWidthOverride(SlotSize);
                    ItemSlot->SetHeightOverride(SlotSize);
                }
            }
            UpdateItemSlot(bShowItemSlot ? Vehicle : nullptr, CurrentTime);
            if (bItemPickupTest || !GS)
                return;

            const bool bHasRoom = GS->GetRoomSettings().MapThemeId.IsValid();
            const EMWRoomPhase Phase = GS->GetRoomPhase();
            if (!LastPhase.IsSet() || Phase != LastPhase.GetValue())
            {
                LastPhase = Phase;
                PhaseStarted = CurrentTime;
            }

            const bool bShowWaiting = bHasRoom && Phase == EMWRoomPhase::PreparingRace && !PC->GetPawn();
            WaitingBoard->SetVisibility(bShowWaiting ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
            if (bShowWaiting && CurrentTime >= NextRosterRefresh)
            {
                RefreshRoster(*GS);
                NextRosterRefresh = CurrentTime + .25;
            }

            SelectSign->SetVisibility(bHasRoom && Phase == EMWRoomPhase::PreparingConstruction
                                          ? EVisibility::HitTestInvisible
                                          : EVisibility::Collapsed);
            BuildSign->SetVisibility(bHasRoom && Phase == EMWRoomPhase::TrackPlacement &&
                                             CurrentTime - PhaseStarted < 2.0
                                         ? EVisibility::HitTestInvisible
                                         : EVisibility::Collapsed);
        }

        // 현재 카메라에 투영한 차량 이름과 선택 커서를 표시하고 화면 밖 대상은 제외한다.
        virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
                              FSlateWindowElementList& DrawElements, int32 LayerId, const FWidgetStyle& Style,
                              bool bParentEnabled) const override
        {
            const int32 TopLayer =
                SCompoundWidget::OnPaint(Args, Geometry, CullingRect, DrawElements, LayerId, Style, bParentEnabled);
            APlayerController* PC = Controller.Get();
            const auto* GS = PC ? PC->GetWorld()->GetGameState<AMWGameplayGameState>() : nullptr;
            if (bItemPickupTest || !GS || !PC->GetPawn() || !FSlateApplication::IsInitialized())
                return TopLayer;

            const EMWRoomPhase Phase = GS->GetRoomPhase();
            if (Phase != EMWRoomPhase::PreparingRace && Phase != EMWRoomPhase::Countdown &&
                Phase != EMWRoomPhase::Racing)
                return TopLayer;

            int32 ViewportWidth = 0;
            int32 ViewportHeight = 0;
            PC->GetViewportSize(ViewportWidth, ViewportHeight);
            if (ViewportWidth <= 0 || ViewportHeight <= 0)
                return TopLayer;
            const FVector2D PixelToOverlay = Geometry.GetLocalSize() / FVector2D(ViewportWidth, ViewportHeight);
            const auto FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
            const double NameScale = FMath::Min(Geometry.GetLocalSize().X / 1920.0, Geometry.GetLocalSize().Y / 1080.0);
            FSlateFontInfo NameFont = Art.Font(FMath::Max(11.0, 30.0 * NameScale));
            NameFont.OutlineSettings.OutlineSize = FMath::Max(1, FMath::RoundToInt(3.0 * NameScale));
            NameFont.OutlineSettings.OutlineColor = FLinearColor(.035f, .025f, .02f);
            for (const APlayerState* State : GS->PlayerArray)
            {
                const auto* Vehicle = State ? State->GetPawn<AMWVehiclePawn>() : nullptr;
                if (!IsValid(Vehicle) || !Vehicle->GetRootComponent() || State->GetPlayerName().IsEmpty())
                    continue;

                const FBoxSphereBounds& Bounds = Vehicle->GetRootComponent()->Bounds;
                // 차량 루트의 충돌 경계 위에 이름을 두고 화면 좌표로 투영해 카메라 거리 변화에 대응한다.
                const FVector NameAnchor = Bounds.Origin + FVector::UpVector * (Bounds.BoxExtent.Z + 18.f);
                FVector2D Position;
                if (!PC->ProjectWorldLocationToScreen(NameAnchor, Position))
                {
                    continue;
                }

                const FText Name = FText::AsCultureInvariant(State->GetPlayerName().ToUpper());
                const FVector2D Size = FontMeasure->Measure(Name, NameFont);
                Position = Position * PixelToOverlay - FVector2D(Size.X * .5, Size.Y + 8.0 * NameScale);
                if (!FSlateRect::DoRectanglesIntersect(FSlateRect(FVector2D::ZeroVector, Geometry.GetLocalSize()),
                                                       FSlateRect(Position, Position + Size)))
                    continue;

                FSlateDrawElement::MakeText(DrawElements, TopLayer + 1,
                                            Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Position)), Name,
                                            NameFont, ESlateDrawEffect::None, GetPlayerTint(State));
                if (!bLoggedNameplate)
                {
                    UE_LOG(LogMWInGameUI, Verbose, TEXT("Rendered vehicle nameplate in %s"),
                           *GetNameSafe(PC->GetWorld()));
                    bLoggedNameplate = true;
                }
            }
            return TopLayer + 1;
        }

    private:
        // 크기 설정은 생성 시 한 번 검증한다. 잘못된 입력은 기존 배치로 복구해 NaN·역전된 Clamp를 막는다.
        void InitializeLayout(const UMWGameplayHUDSubsystem& Settings)
        {
            ItemLayout = Settings.ItemSlotLayout;
            BoardLayout = Settings.WaitingBoardLayout;
            const auto Positive = [](double Value) { return FMath::IsFinite(Value) && Value > 0.0; };
            const auto ValidMargin = [](const FMargin& Margin)
            {
                return FMath::IsFinite(Margin.Left) && FMath::IsFinite(Margin.Top) &&
                       FMath::IsFinite(Margin.Right) && FMath::IsFinite(Margin.Bottom);
            };
            if (!Positive(ItemLayout.Size) || !Positive(ItemLayout.MinimumPixels) ||
                !Positive(ItemLayout.IconSize) || !Positive(ItemLayout.MaximumHeightFraction) ||
                ItemLayout.MaximumHeightFraction > 1.f ||
                ItemLayout.Size > MWGameplayUI::DesignHeight * ItemLayout.MaximumHeightFraction ||
                !FMath::IsFinite(ItemLayout.FrameInset) || ItemLayout.FrameInset < 0.f ||
                ItemLayout.FrameInset * 2.f >= ItemLayout.Size || !ValidMargin(ItemLayout.Margin))
            {
                UE_LOG(LogMWInGameUI, Warning, TEXT("아이템 슬롯 치수가 잘못되어 기존 기본 배치로 복구합니다."));
                ItemLayout = FMWItemSlotLayout();
            }
            bool bValidBoard = ValidMargin(BoardLayout.PanelMargin) && ValidMargin(BoardLayout.LogoMargin) &&
                               ValidMargin(BoardLayout.RosterPadding) && ValidMargin(BoardLayout.CellPadding);
            for (const double Value : {BoardLayout.PanelSize.X, BoardLayout.PanelSize.Y,
                                      BoardLayout.LogoSize.X, BoardLayout.LogoSize.Y,
                                      double(BoardLayout.BorderWidth), double(BoardLayout.PoleWidth),
                                      double(BoardLayout.TitleHeight), double(BoardLayout.TitleFontSize),
                                      double(BoardLayout.DividerHeight), double(BoardLayout.HeaderHeight),
                                      double(BoardLayout.RowHeight), double(BoardLayout.HeaderFontSize),
                                      double(BoardLayout.RowFontSize), BoardLayout.ColumnWidths.X,
                                      BoardLayout.ColumnWidths.Y, BoardLayout.ColumnWidths.Z, BoardLayout.ColumnWidths.W})
            {
                bValidBoard &= Positive(Value);
            }
            if (!bValidBoard)
            {
                UE_LOG(LogMWInGameUI, Warning, TEXT("대기판 치수가 잘못되어 기존 기본 배치로 복구합니다."));
                BoardLayout = FMWWaitingBoardLayout();
            }
        }

        // 설정은 화면 생성 때 한 번 복사한다. 진행 중인 연출의 기준이 중간에 바뀌지 않게 한다.
        void InitializeItemTiming(const UMWGameplayHUDSubsystem& Settings)
        {
            RouletteSwitchTimes = Settings.ItemRouletteSwitchTimes;
            UseDurationSeconds = Settings.ItemUseDuration;
            double PreviousTime = 0.0;
            for (const double SwitchTime : RouletteSwitchTimes)
            {
                if (!FMath::IsFinite(SwitchTime) || SwitchTime <= PreviousTime)
                {
                    UE_LOG(LogMWInGameUI, Warning,
                           TEXT("아이템 룰렛 시각은 유한한 양수·오름차순이어야 합니다. 룰렛 표시를 생략합니다."));
                    RouletteSwitchTimes.Reset();
                    break;
                }
                PreviousTime = SwitchTime;
            }
            if (!FMath::IsFinite(UseDurationSeconds) || UseDurationSeconds < 0.f)
            {
                UE_LOG(LogMWInGameUI, Warning,
                       TEXT("아이템 사용 표시 시간은 유한한 0 이상이어야 합니다. 슬롯을 즉시 비웁니다."));
                UseDurationSeconds = 0.f;
            }
        }

        // 실제 보유 값의 변경으로 획득 룰렛과 사용 축소·페이드를 시작한다.
        void UpdateItemSlot(const AMWVehiclePawn* Vehicle, double CurrentTime)
        {
            const EMWItemId CurrentItem = Vehicle ? HeldItem.Get(EMWItemId::None) : EMWItemId::None;
            if (!Vehicle || ItemSlotVehicle.Get() != Vehicle)
            {
                ItemSlotVehicle = Vehicle;
                DisplayedItem = TargetItem = CurrentItem;
                RouletteStep = INDEX_NONE;
                ItemTransitionStart.Reset();
                ItemIconScale = ItemIconOpacity = 1.f;
                return;
            }

            if (CurrentItem != TargetItem)
            {
                TargetItem = CurrentItem;
                ItemTransitionStart = CurrentTime;
                RouletteStep = INDEX_NONE;
                if (CurrentItem != EMWItemId::None)
                {
                    DisplayedItem = CurrentItem;
                    ItemIconScale = ItemIconOpacity = 1.f;
                    if (RouletteBrushes.Num() < 2 || RouletteSwitchTimes.IsEmpty())
                    {
                        ItemTransitionStart.Reset();
                        return;
                    }
                    UE_LOG(LogMWInGameUI, Verbose, TEXT("Item slot roulette started"));
                }
                else
                {
                    // 룰렛 도중 바로 사용해도 실제 지급 아이템을 기준으로 사용 연출을 마무리한다.
                    UE_LOG(LogMWInGameUI, Verbose, TEXT("Item slot use started"));
                }
            }

            if (!ItemTransitionStart.IsSet())
                return;

            const double Elapsed = CurrentTime - ItemTransitionStart.GetValue();
            if (TargetItem != EMWItemId::None)
            {
                UpdateItemRoulette(Elapsed);
                return;
            }

            const float Progress = UseDurationSeconds > 0.f
                                       ? FMath::Clamp(static_cast<float>(Elapsed / UseDurationSeconds), 0.f, 1.f)
                                       : 1.f;
            ItemIconScale = FMath::InterpEaseOut(1.f, .72f, Progress, 2.f);
            ItemIconOpacity = 1.f - Progress;
            if (Progress >= 1.f)
            {
                DisplayedItem = EMWItemId::None;
                ItemTransitionStart.Reset();
                UE_LOG(LogMWInGameUI, Verbose, TEXT("Item slot use finished"));
            }
        }

        // 시간 간격을 늘리며 표시 아이콘만 돌리고 실제 받은 아이템에 정지한다.
        void UpdateItemRoulette(double Elapsed)
        {
            int32 Step = 0;
            while (Step < RouletteSwitchTimes.Num() && Elapsed >= RouletteSwitchTimes[Step])
            {
                ++Step;
            }
            // 연출이 끝나면 가짜 후보 표시만 해제한다. 아이템 상태의 원본은 기존 아이템 컴포넌트다.
            if (Step == RouletteSwitchTimes.Num())
            {
                RouletteStep = INDEX_NONE;
                ItemTransitionStart.Reset();
                UE_LOG(LogMWInGameUI, Verbose, TEXT("Item slot roulette settled on %s"),
                       *UEnum::GetValueAsString(DisplayedItem));
            }
            else if (Step != RouletteStep)
            {
                RouletteStep = Step;
                UE_LOG(LogMWInGameUI, Verbose, TEXT("Item slot roulette card %d at %.0f ms"), Step + 1,
                       Elapsed * 1000.0);
            }
        }

        const FSlateBrush* GetItemSlotBrush() const
        {
            if (RouletteStep != INDEX_NONE)
            {
                return &RouletteBrushes[RouletteStep % RouletteBrushes.Num()];
            }
            return BoosterTexture && DisplayedItem == EMWItemId::Booster ? &BoosterBrush : nullptr;
        }

        // 공통 글꼴과 색으로 대기판 셀을 추가해 참가자 열 배치를 일치시킨다.
        void AddRosterCell(int32 Column, int32 Row, const FText& Text, FLinearColor Tint)
        {
            Roster->AddSlot(Column, Row)
                .Padding(BoardLayout.CellPadding)[SNew(SBox)
                                   .WidthOverride(BoardLayout.ColumnWidths[Column])
                                   .HeightOverride(Row == 0 ? BoardLayout.HeaderHeight : BoardLayout.RowHeight)
                                       [SNew(SScaleBox)
                                            .Stretch(EStretch::ScaleToFit)
                                            .StretchDirection(EStretchDirection::DownOnly)
                                            .HAlign(HAlign_Left)
                                            .VAlign(VAlign_Center)[SNew(STextBlock)
                                                                       .Text(Text)
                                                                       .ColorAndOpacity(Tint)
                                                                       .Font(Art.Font(Row == 0 ? BoardLayout.HeaderFontSize : BoardLayout.RowFontSize))]]];
        }

        // 기존 GameState의 참가자와 선택 차량·대기 상태를 대기판에 반영한다.
        void RefreshRoster(const AMWGameplayGameState& GS)
        {
            TArray<const AMWPlayerState*> Players;
            for (const APlayerState* State : GS.PlayerArray)
            {
                if (const auto* Player = Cast<AMWPlayerState>(State);
                    Player && !Player->IsInactive() && Player->GetLobbyMemberId() > 0)
                {
                    Players.Add(Player);
                }
            }
            Players.Sort(
                [](const AMWPlayerState& A, const AMWPlayerState& B)
                {
                    return A.GetLobbyMemberId() < B.GetLobbyMemberId();
                });

            TArray<FText> Cells;
            TArray<FLinearColor> VehicleTints;
            for (const AMWPlayerState* Player : Players)
            {
                const auto* Vehicle =
                    UMWAssetManager::Get().FindLoadedAsset<UMWVehicleDefinitionData>(Player->GetVehicleAssetID());
                Cells.Add(FText::AsNumber(VehicleTints.Num() + 1));
                Cells.Add(Player->GetPlayerName().IsEmpty() ? NSLOCTEXT("MWGameplayUI", "UnnamedPlayer", "플레이어")
                                                            : FText::AsCultureInvariant(Player->GetPlayerName()));
                Cells.Add(Vehicle ? Vehicle->GetDisplayName() : FText::GetEmpty());
                Cells.Add(Player->GetPawn() ? NSLOCTEXT("MWGameplayUI", "RaceReady", "준비 완료")
                                            : NSLOCTEXT("MWGameplayUI", "RaceWaiting", "기다리는 중"));
                VehicleTints.Add(GetPlayerTint(Player));
            }
            if (bRosterBuilt && Cells.Num() == DisplayedCells.Num() && VehicleTints == DisplayedVehicleTints)
            {
                bool bChanged = false;
                for (int32 Index = 0; Index < Cells.Num() && !bChanged; ++Index)
                    bChanged = !Cells[Index].EqualTo(DisplayedCells[Index]);
                if (!bChanged)
                    return;
            }

            bRosterBuilt = true;
            DisplayedCells = MoveTemp(Cells);
            DisplayedVehicleTints = MoveTemp(VehicleTints);
            Roster->ClearChildren();
            AddRosterCell(0, 0, NSLOCTEXT("MWGameplayUI", "GridColumn", "위치"), FLinearColor::White);
            AddRosterCell(1, 0, NSLOCTEXT("MWGameplayUI", "DriverColumn", "드라이버"), FLinearColor::White);
            AddRosterCell(2, 0, NSLOCTEXT("MWGameplayUI", "VehicleColumn", "차량"), FLinearColor::White);
            AddRosterCell(3, 0, NSLOCTEXT("MWGameplayUI", "StatusColumn", "상태"), FLinearColor::White);
            constexpr int32 Columns = 4;
            for (int32 Index = 0; Index < DisplayedCells.Num(); ++Index)
            {
                const int32 Column = Index % Columns;
                const int32 Row = Index / Columns;
                AddRosterCell(Column, Row + 1, DisplayedCells[Index],
                              Column == 2 ? DisplayedVehicleTints[Row] : FLinearColor::White);
            }
        }

        // 기존 참가자 식별 기준과 팔레트에서 닉네임·커서 표시 색을 얻는다.
        FLinearColor GetPlayerTint(const APlayerState* State) const
        {
            const auto* Player = Cast<AMWPlayerState>(State);
            return Player ? GetDefault<UMWParticipantPalette>()->GetColorForMember(Player->GetLobbyMemberId())
                          : FLinearColor::White;
        }

        FMWItemSlotLayout ItemLayout;
        FMWWaitingBoardLayout BoardLayout;
        TWeakObjectPtr<APlayerController> Controller;
        TSharedPtr<SOverlay> WaitingBoard;
        TSharedPtr<SGridPanel> Roster;
        TSharedPtr<SMWGameplaySign> SelectSign;
        TSharedPtr<SMWGameplaySign> BuildSign;
        TSharedPtr<SBox> ItemSlot;
        TAttribute<EMWItemId> HeldItem;
        TWeakObjectPtr<const AMWVehiclePawn> ItemSlotVehicle;
        EMWItemId DisplayedItem = EMWItemId::None;
        EMWItemId TargetItem = EMWItemId::None;
        TOptional<double> ItemTransitionStart;
        TArray<double> RouletteSwitchTimes;
        float UseDurationSeconds = 0.f;
        float ItemIconScale = 1.f;
        float ItemIconOpacity = 1.f;
        int32 RouletteStep = INDEX_NONE;
        TArray<TStrongObjectPtr<UTexture2D>> RouletteTextures;
        TArray<FSlateBrush> RouletteBrushes;
        TStrongObjectPtr<UTexture2D> BoosterTexture;
        TStrongObjectPtr<UMaterialInstanceDynamic> BoosterIconMaterial;
        FVector2D BoosterIconOffset = FVector2D::ZeroVector;
        FSlateBrush BoosterBrush;
        TArray<FText> DisplayedCells;
        TArray<FLinearColor> DisplayedVehicleTints;
        FMWGameplayUIArt Art;
        TOptional<EMWRoomPhase> LastPhase;
        double PhaseStarted = 0.0;
        double NextRosterRefresh = 0.0;
        bool bRosterBuilt = false;
        bool bItemPickupTest = false;
        mutable bool bLoggedNameplate = false;
    };
}

// 일반 게임과 PIE 월드만 대상으로 해 에디터 미리보기에서 런타임 기능이 켜지지 않게 한다.
bool UMWGameplayHUDSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{
    return Type == EWorldType::Game || Type == EWorldType::PIE;
}

// 초기 연결 또는 GameState 교체가 필요할 때만 연결용 틱을 허용한다.
bool UMWGameplayHUDSubsystem::IsTickable() const
{
    const UWorld* World = GetWorld();
    if (!World || World->GetNetMode() == NM_DedicatedServer)
    {
        return false;
    }

    const AMWGameplayGameState* GameState = World->GetGameState<AMWGameplayGameState>();
    GameState = IsValid(GameState) ? GameState : nullptr;
    const bool bNeedsStateBinding = ObservedGameState.Get() != GameState || (!GameState && RoomPhase.IsSet());

    // 오버레이 생성 전이라도 원본 상태가 사라졌으면 이전 구독과 캐시를 정리한다.
    const bool bCanAttach = GameState || IsItemPickupTestPIE(*World);
    return bNeedsStateBinding || (!Overlay && bCanAttach);
}

// 로컬 Controller와 Viewport가 준비됐을 때 기존 HUD 오버레이를 한 번 연결한다.
void UMWGameplayHUDSubsystem::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    RefreshGameStateBinding();
    if (Overlay)
    {
        return;
    }

    auto* GameInstance = World->GetGameInstance();
    auto* PC =
        GameInstance ? Cast<AMWGameplayPlayerController>(GameInstance->GetFirstLocalPlayerController(World)) : nullptr;
    auto* GameViewport = GameInstance ? GameInstance->GetGameViewportClient() : nullptr;
    if (!GameViewport || !PC || PC->GetWorld() != World || !PC->GetLocalPlayer() || !PC->HasActorBegunPlay())
        return;

    ItemController = PC;
    PC->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::HandlePossessedPawnChanged);
    HandlePossessedPawnChanged(nullptr, PC->GetPawn());

    // WBP는 월드당 한 번 만들고 UObject 참조를 유지해 Slate 표시 중 수집되지 않게 한다.
    const auto* Settings = GetDefault<UMWGameplayHUDSubsystem>();
    UClass* WidgetClass = Settings->RaceHUDWidgetClass.LoadSynchronous();
    RaceHUDWidget = WidgetClass ? CreateWidget<UMWRaceHUDWidget>(PC, WidgetClass) : nullptr;
    if (RaceHUDWidget)
    {
        RaceHUDWidget->SetItemPickupTest(IsItemPickupTestPIE(*World));
    }
    else
    {
        UE_LOG(LogMWInGameUI, Error, TEXT("경기 HUD WBP를 만들 수 없습니다. RaceHUDWidgetClass 설정을 확인하세요."));
    }

    const TWeakObjectPtr<UMWGameplayHUDSubsystem> WeakThis(this);
    Overlay = SNew(SMWGameplayHUD)
                  .Controller(PC)
                  .Visibility(EVisibility::HitTestInvisible)
                  .bItemPickupTest(IsItemPickupTestPIE(*World))
                  .RaceHUD(RaceHUDWidget ? RaceHUDWidget->TakeWidget() : SNullWidget::NullWidget)
                  .HeldItem_Lambda(
                      [WeakThis]
                      {
                          const UMWGameplayHUDSubsystem* HUD = WeakThis.Get();
                          return HUD ? HUD->HeldItem : EMWItemId::None;
                      });
    Viewport = GameViewport;
    GameViewport->AddViewportWidgetContent(Overlay.ToSharedRef(), 20);
    UE_LOG(LogMWInGameUI, Verbose, TEXT("Attached gameplay HUD in %s to %s"), *GetNameSafe(World), *GetNameSafe(PC));
}

// 초기 조회와 이벤트 구독을 같은 곳에서 처리해 HUD 생성 전에 바뀐 단계도 놓치지 않는다.
void UMWGameplayHUDSubsystem::RefreshGameStateBinding()
{
    AMWGameplayGameState* GameState = GetWorld()->GetGameState<AMWGameplayGameState>();
    GameState = IsValid(GameState) ? GameState : nullptr;
    if (ObservedGameState.Get() == GameState)
    {
        if (!GameState)
        {
            RoomPhase.Reset();
        }
        return;
    }

    if (AMWGameplayGameState* Previous = ObservedGameState.Get())
    {
        Previous->OnGameplayStateChanged.RemoveDynamic(this, &ThisClass::HandleGameplayStateChanged);
    }

    ObservedGameState = GameState;
    if (GameState)
    {
        GameState->OnGameplayStateChanged.AddUniqueDynamic(this, &ThisClass::HandleGameplayStateChanged);
    }

    HandleGameplayStateChanged();
}

void UMWGameplayHUDSubsystem::HandleGameplayStateChanged()
{
    const AMWGameplayGameState* GameState = ObservedGameState.Get();
    if (!GameState || GameState->GetWorld() != GetWorld())
    {
        RoomPhase.Reset();
        return;
    }

    RoomPhase = GameState->GetRoomPhase();
    UE_LOG(LogMWInGameUI, Verbose, TEXT("[MWRaceHUD] 경기 단계 갱신. World=%s Phase=%s"),
           *GetNameSafe(GetWorld()), *UEnum::GetValueAsString(RoomPhase.GetValue()));
}

// 이 기능의 틱 비용을 엔진 통계에서 구분할 수 있도록 식별자를 반환한다.
TStatId UMWGameplayHUDSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UMWGameplayHUDSubsystem, STATGROUP_Tickables);
}

// 월드가 닫힐 때 실제 Viewport 오버레이와 아이템 구독을 먼저 제거한다.
void UMWGameplayHUDSubsystem::OnWorldEndPlay(UWorld& World)
{
    RemoveOverlay();
    Super::OnWorldEndPlay(World);
}

// 월드 종료 경로가 달라도 같은 오버레이·구독 정리를 수행한다.
void UMWGameplayHUDSubsystem::Deinitialize()
{
    RemoveOverlay();
    Super::Deinitialize();
}

// 이전 아이템 구독을 먼저 끊고 새 소유 Pawn의 현재 값과 이벤트를 함께 연결한다.
void UMWGameplayHUDSubsystem::HandlePossessedPawnChanged(APawn*, APawn* NewPawn)
{
    if (UMWItemComponent* OldItems = ObservedItems.Get())
    {
        OldItems->OnHeldItemChanged.RemoveDynamic(this, &ThisClass::HandleHeldItemChanged);
    }

    const auto* Vehicle = Cast<AMWVehiclePawn>(NewPawn);
    UMWItemComponent* Items = Vehicle ? Vehicle->GetItemComponent() : nullptr;
    ObservedItems = Items;
    if (Items)
    {
        Items->OnHeldItemChanged.AddUniqueDynamic(this, &ThisClass::HandleHeldItemChanged);
    }
    HandleHeldItemChanged(Items ? Items->GetCurrentItemId() : EMWItemId::None);
}

// 실제 받은 보유 아이템만 표시 모델에 반영한다. 룰렛이 지급 결과를 다시 정하지 않는다.
void UMWGameplayHUDSubsystem::HandleHeldItemChanged(EMWItemId ItemId)
{
    if (HeldItem != ItemId)
    {
        HeldItem = ItemId;
        UE_LOG(LogMWInGameUI, Verbose, TEXT("Item HUD state: %s"), *UEnum::GetValueAsString(HeldItem));
    }
}

// 실제 구독했던 Controller·ItemComponent와 Viewport에서 연결을 해제한다.
void UMWGameplayHUDSubsystem::RemoveOverlay()
{
    if (AMWGameplayGameState* GameState = ObservedGameState.Get())
    {
        GameState->OnGameplayStateChanged.RemoveDynamic(this, &ThisClass::HandleGameplayStateChanged);
    }
    ObservedGameState.Reset();
    RoomPhase.Reset();

    if (APlayerController* PC = ItemController.Get())
    {
        PC->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePossessedPawnChanged);
    }
    ItemController.Reset();
    HandlePossessedPawnChanged(nullptr, nullptr);
    if (Overlay && Viewport.IsValid())
        Viewport->RemoveViewportWidgetContent(Overlay.ToSharedRef());
    Overlay.Reset();
    RaceHUDWidget = nullptr;
    Viewport.Reset();
}
