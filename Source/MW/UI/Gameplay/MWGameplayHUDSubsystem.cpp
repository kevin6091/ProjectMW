// 기존 경기 상태와 아이템 이벤트를 표시하는 HUD다. 서버 결과를 자체 타이머로 확정하지 않는다.

#include "UI/Gameplay/MWGameplayHUDSubsystem.h"

#include "Brushes/SlateImageBrush.h"
#include "UI/Gameplay/MWArrivalScreen.h"
#include "Components/NativeWidgetHost.h"
#include "Components/BoxComponent.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Blueprint/UserWidget.h"
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
#include "UI/Gameplay/MWMatchResultsWidget.h"
#include "UI/Gameplay/MWRaceHUDWidget.h"
#include "UObject/Package.h"
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

class UMWGameplayHUDSubsystem::SMWGameplayHUD : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SMWGameplayHUD)
    {
    }
    SLATE_ARGUMENT(APlayerController*, Controller)
    SLATE_ARGUMENT(FMWPIEHUDPreview, PIEPreview)
    SLATE_ATTRIBUTE(EMWItemId, HeldItem)
    SLATE_ATTRIBUTE(bool, MatchResultsActive)
    SLATE_ARGUMENT(TSharedPtr<SWidget>, RaceHUD)
    SLATE_ARGUMENT(UUserWidget*, Panels)
    SLATE_END_ARGS()

    // 기존 경기 상태를 읽는 표지판·대기판·아이템 슬롯의 Slate 구조와 설정 자원을 준비한다.
    void Construct(const FArguments& Args)
    {
        Controller = Args._Controller;
        PIEPreview = Args._PIEPreview;
        HeldItem = Args._HeldItem;
        MatchResultsActive = Args._MatchResultsActive;
        const auto* Settings = GetDefault<UMWGameplayHUDSubsystem>();
        InitializeLayout(*Settings);
        RecoveryLayout = Settings->CheckpointRecoveryLayout;
        RecoveryCountdownBrush = FSlateRoundedBoxBrush(RecoveryLayout.CountdownBackground, RecoveryLayout.CountdownCornerRadius);
        RecoveryBadgeBrush = FSlateRoundedBoxBrush(FLinearColor::White, RecoveryLayout.BadgeCornerRadius);
        UUserWidget* Panels = Args._Panels;
        UNativeWidgetHost* IconHost = nullptr;
        if (Panels)
        {
            WaitingBoard = Panels->GetWidgetFromName(TEXT("WaitingBoard"));
            SelectSign = Panels->GetWidgetFromName(TEXT("SelectSign"));
            BuildSign = Panels->GetWidgetFromName(TEXT("BuildSign"));
            ItemSlot = Cast<USizeBox>(Panels->GetWidgetFromName(TEXT("ItemSlot")));
            RecoveryPanel = Panels->GetWidgetFromName(TEXT("RecoveryPanel"));
            RecoveryBadge = Cast<UBorder>(Panels->GetWidgetFromName(TEXT("RecoveryBadge")));
            RecoveryCountdown = Cast<UTextBlock>(Panels->GetWidgetFromName(TEXT("RecoveryCountdown")));
            RecoveryFlash = Panels->GetWidgetFromName(TEXT("RecoveryFlash"));
            // WBP가 배치하고, 기존 Slate 모델이 갱신하는 명단 그리드와 아이콘은 호스트에 연결한다.
            auto* RosterHost = Cast<UNativeWidgetHost>(Panels->GetWidgetFromName(TEXT("RosterHost")));
            if (RosterHost) RosterHost->SetContent(SAssignNew(Roster, SGridPanel));
            IconHost = Cast<UNativeWidgetHost>(Panels->GetWidgetFromName(TEXT("ItemIconHost")));
            if (ItemSlot.IsValid()) ItemDesignSize = ItemSlot->GetWidthOverride();

            const auto LogMissingBinding = [Panels](bool bBound, const TCHAR* Binding)
            {
                if (!bBound)
                    UE_LOG(LogMWInGameUI, Warning, TEXT("경기 화면 %s의 %s 바인딩이 없거나 타입이 다릅니다."),
                        *Panels->GetClass()->GetPathName(), Binding);
            };
            LogMissingBinding(WaitingBoard.IsValid(), TEXT("WaitingBoard"));
            LogMissingBinding(RosterHost != nullptr, TEXT("RosterHost (NativeWidgetHost)"));
            LogMissingBinding(SelectSign.IsValid(), TEXT("SelectSign"));
            LogMissingBinding(BuildSign.IsValid(), TEXT("BuildSign"));
            LogMissingBinding(ItemSlot.IsValid(), TEXT("ItemSlot (SizeBox)"));
            LogMissingBinding(IconHost != nullptr, TEXT("ItemIconHost (NativeWidgetHost)"));
        }
        // 안내 시간은 화면 생성 때 한 번 읽고, 경기 단계가 바뀔 때 기존 시계를 다시 시작한다.
        ConstructionPromptSeconds = FMath::IsFinite(Settings->ConstructionPromptSeconds)
                                        ? FMath::Max(0.f, Settings->ConstructionPromptSeconds) : 0.f;
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
        if (IconHost)
        {
            IconHost->SetContent(SNew(SOverlay)
                + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
                [SNew(SImage).Image_Lambda([this] { return GetItemSlotBrush(); })
                    .RenderTransformPivot(FVector2D(.5,.5))
                    .RenderTransform_Lambda([this]() -> TOptional<FSlateRenderTransform>
                    {
                        const FVector2D Offset = RouletteStep == INDEX_NONE ? BoosterIconOffset : FVector2D::ZeroVector;
                        return FSlateRenderTransform(ItemIconScale, Offset * ItemIconScale);
                    })
                    .ColorAndOpacity_Lambda([this] { return FSlateColor(FLinearColor(1,1,1,ItemIconOpacity)); })
                    .Visibility_Lambda([this] { return GetItemSlotBrush() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })]
                + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
                [SNew(STextBlock).Text(NSLOCTEXT("MWGameplayUI","BoosterItem","부스터")).Font(Art.Font(24.f))
                    .ColorAndOpacity_Lambda([this] { return FSlateColor(FLinearColor(.72f,.75f,.80f,ItemIconOpacity)); })
                    .Visibility_Lambda([this] { return !GetItemSlotBrush() && DisplayedItem == EMWItemId::Booster
                        ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })]);
        }
        ChildSlot[SNew(SOverlay)
            + SOverlay::Slot()[MWGameplayUI::FitDesignCanvas(Panels ? Panels->TakeWidget() : SNullWidget::NullWidget)]
            + SOverlay::Slot()[MWGameplayUI::FitDesignCanvas(Args._RaceHUD.ToSharedRef(), HAlign_Right, VAlign_Bottom)]
            + SOverlay::Slot()[MWGameplayUI::FitDesignCanvas(
                SNew(SOverlay)
                + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(RecoveryLayout.CountdownInset)
                [SNew(SHorizontalBox)
                    .Visibility_Lambda([this] { return !HasRecoveryPanel() && GetRecoveringVehicle()
                        ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
                    + SHorizontalBox::Slot().AutoWidth().Padding(RecoveryLayout.BadgeGap)
                    [SNew(SBorder).BorderImage(&RecoveryBadgeBrush).Padding(RecoveryLayout.BadgePadding)
                        .BorderBackgroundColor_Lambda([this] { return GetPlayerTint(Controller.IsValid() ? Controller->PlayerState : nullptr); })
                        [SNew(STextBlock).Font(Art.Font(RecoveryLayout.FontSize)).Text(NSLOCTEXT("MWGameplayUI", "RecoveryBadge", "복귀"))]]
                    + SHorizontalBox::Slot().AutoWidth()
                    [SNew(SBorder).BorderImage(&RecoveryCountdownBrush).Padding(RecoveryLayout.CountdownPadding)
                        [SNew(STextBlock).Font(Art.Font(RecoveryLayout.FontSize)).ColorAndOpacity(RecoveryLayout.CountdownText)
                            .Text_Lambda([this] { return RecoveryCountdownText(); })]]])]
            + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
            [SNew(SBorder).Padding(28).BorderBackgroundColor(FLinearColor(0.025f, 0.035f, 0.05f, 0.94f))
                .Visibility_Lambda([this] { return !PIEPreview.IsEnabled() && !ResultsText().IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
                [SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 20)).Text_Lambda([this] { return ResultsText(); })]]];
    }

    FText ResultsText() const
    {
        if (MatchResultsActive.Get()) return FText::GetEmpty();
        const auto* PC = Controller.Get();
        const auto* GS = PC ? PC->GetWorld()->GetGameState<AMWGameplayGameState>() : nullptr;
        if (!GS) return FText::GetEmpty();
        const auto Progress = GS->GetProgress();
        const bool bFinal = Progress.Phase == EMWRoomPhase::MatchResults;
        if ((!bFinal && Progress.Phase != EMWRoomPhase::RoundResults) || Progress.ResultsEpoch != Progress.RoundEpoch) return FText::GetEmpty();
        FString Text = bFinal ? TEXT("최종 경기 결과\n\n") : FString::Printf(TEXT("라운드 %d 결과\n\n"), Progress.RoundIndex);
        for (const auto& Entry : Progress.Results)
        {
            FString Name = FString::Printf(TEXT("플레이어 %d"), Entry.MemberId);
            for (const APlayerState* Base : GS->PlayerArray)
            {
                const auto* Player = Cast<AMWPlayerState>(Base);
                if (Player && Player->GetLobbyMemberId() == Entry.MemberId && !Player->GetPlayerName().IsEmpty()) { Name = Player->GetPlayerName(); break; }
            }
            const FString Rank = !Entry.bRaceParticipant && !bFinal ? TEXT("관전") : FString::Printf(TEXT("%d위"), bFinal ? Entry.FinalRank : Entry.RoundRank);
            Text += FString::Printf(TEXT("%s  %s  ·  +%d점  ·  누적 %d점%s\n"), *Rank, *Name,
                Entry.CheckpointScore + Entry.FinishScore, Entry.TotalScore, Entry.RaceStatus == EMWRaceStatus::Eliminated ? TEXT("  (탈락)") : TEXT(""));
        }
        if (!bFinal) Text += FString::Printf(TEXT("\n다음 진행까지 %.0f초"), FMath::CeilToDouble(GS->GetPhaseSecondsRemaining()));
        return FText::FromString(Text);
    }

    // 표시 단계·룰렛 시간·차량 닉네임 정보를 갱신한다. 서버 경기 단계를 자체 변경하지 않는다.
    virtual void Tick(const FGeometry& Geometry, double CurrentTime, float) override
    {
        const auto* PC = Controller.Get();
        const auto* GS = PC ? PC->GetWorld()->GetGameState<AMWGameplayGameState>() : nullptr;
        const auto* Vehicle = PC ? PC->GetPawn<AMWVehiclePawn>() : nullptr;
        RefreshRecoveryPanel();
        const bool bItemPhase = PIEPreview.bItemSlot || (GS && GS->GetRoomPhase() == EMWRoomPhase::Racing);
        const bool bShowItemSlot = bItemPhase && Vehicle && Vehicle->GetItemComponent();
        // 각 패널의 바인딩만 검사한다. 안내판 한 개가 없어도 아이템 슬롯은 기존 동작을 유지한다.
        if (ItemSlot.IsValid())
        {
            ItemSlot->SetVisibility(bShowItemSlot ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
            if (bShowItemSlot)
            {
                const FVector2f ViewportSize(Geometry.GetLocalSize());
                const float CanvasScale = FMath::Min(ViewportSize.X / MWGameplayUI::DesignWidth,
                                                     ViewportSize.Y / MWGameplayUI::DesignHeight) *
                                          Geometry.GetAccumulatedLayoutTransform().GetScale();
                if (CanvasScale > UE_SMALL_NUMBER)
                {
                    // 화면이 작으면 슬롯을 최소 픽셀 크기까지 키우되 WBP의 기본 크기보다 작게 줄이지 않는다.
                    const float SlotSize = FMath::Clamp(ItemLayout.MinimumPixels / CanvasScale, ItemDesignSize,
                                                       FMath::Max(ItemDesignSize, MWGameplayUI::DesignHeight * ItemLayout.MaximumHeightFraction));
                    ItemSlot->SetWidthOverride(SlotSize);
                    ItemSlot->SetHeightOverride(SlotSize);
                }
            }
            UpdateItemSlot(bShowItemSlot ? Vehicle : nullptr, CurrentTime);
        }
        if (PIEPreview.IsEnabled() || !GS)
            return;

        const bool bHasRoom = GS->GetRoomSettings().MapThemeId.IsValid();
        const auto* GameplayPC = Cast<AMWGameplayPlayerController>(PC);
        const EMWRoomPhase Phase = GameplayPC ? GameplayPC->GetConstructionPresentationPhase() : GS->GetRoomPhase();
        const bool bCovered = PC->GetGameInstance()->GetSubsystem<UMWArrivalScreen>()->IsCovered();
        // 가림막 뒤에서 안내의 표시 시간이 소진되지 않도록 화면을 공개할 때부터 시간을 잰다.
        if (bCovered || !LastPhase.IsSet() || Phase != LastPhase.GetValue())
        {
            LastPhase = Phase;
            PhaseStarted = CurrentTime;
        }

        const bool bShowWaiting = bHasRoom && Phase == EMWRoomPhase::PreparingRace && !PC->GetPawn();
        if (WaitingBoard.IsValid())
        {
            WaitingBoard->SetVisibility(bShowWaiting && Roster ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
            if (bShowWaiting && Roster && CurrentTime >= NextRosterRefresh)
            {
                RefreshRoster(*GS);
                NextRosterRefresh = CurrentTime + .25;
            }
        }

        if (SelectSign.IsValid())
            SelectSign->SetVisibility(bHasRoom && Phase == EMWRoomPhase::PreparingConstruction &&
                                              CurrentTime - PhaseStarted < ConstructionPromptSeconds
                                          ? ESlateVisibility::HitTestInvisible
                                          : ESlateVisibility::Collapsed);
        if (BuildSign.IsValid())
            BuildSign->SetVisibility(bHasRoom && Phase == EMWRoomPhase::TrackPlacement &&
                                             CurrentTime - PhaseStarted < ConstructionPromptSeconds
                                         ? ESlateVisibility::HitTestInvisible
                                         : ESlateVisibility::Collapsed);
    }

    EMWWaitingBoardReadiness GetWaitingBoardReadiness() const
    {
        if (!WaitingBoard.IsValid() || !Roster) return EMWWaitingBoardReadiness::Unavailable;
        return WaitingBoard->IsVisible() && bRosterBuilt && !DisplayedCells.IsEmpty()
            ? EMWWaitingBoardReadiness::Ready : EMWWaitingBoardReadiness::Pending;
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
        if (PIEPreview.IsEnabled() || !GS || !PC->GetPawn() || !FSlateApplication::IsInitialized())
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
        if (const auto* Recovering = GetRecoveringVehicle())
        {
            DrawRecoverySlot(*PC, *Recovering, Geometry, PixelToOverlay, DrawElements, TopLayer + 1);
        }
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
    const AMWVehiclePawn* GetRecoveringVehicle() const
    {
        const auto* PC = Controller.Get();
        const auto* Vehicle = PC ? PC->GetPawn<AMWVehiclePawn>() : nullptr;
        const auto* State = Vehicle ? Vehicle->GetWorld()->GetGameState<AMWGameplayGameState>() : nullptr;
        return State && State->GetRoomPhase() == EMWRoomPhase::Racing && Vehicle->GetCheckpointRecoveryRemaining() > 0.f
            ? Vehicle : nullptr;
    }

    bool HasRecoveryPanel() const
    {
        return RecoveryPanel.IsValid() && RecoveryBadge.IsValid() && RecoveryCountdown.IsValid();
    }

    void RefreshRecoveryPanel()
    {
        if (!HasRecoveryPanel()) return;
        const auto* Vehicle = GetRecoveringVehicle();
        RecoveryPanel->SetVisibility(Vehicle ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        if (Vehicle)
        {
            RecoveryBadge->SetBrushColor(GetPlayerTint(Vehicle->GetPlayerState()));
            RecoveryCountdown->SetText(RecoveryCountdownText());
        }
        if (RecoveryFlash.IsValid())
        {
            const float Elapsed = Vehicle
                ? FMath::Max(0.f, RecoveryLayout.RingTravelSeconds - Vehicle->GetCheckpointRecoveryRemaining())
                : RecoveryLayout.FlashSeconds;
            const float Alpha = RecoveryLayout.FlashSeconds > UE_SMALL_NUMBER
                ? 1.f - FMath::Clamp(Elapsed / RecoveryLayout.FlashSeconds, 0.f, 1.f) : 0.f;
            RecoveryFlash->SetRenderOpacity(Alpha);
            RecoveryFlash->SetVisibility(Alpha > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        }
    }

    FText RecoveryCountdownText() const
    {
        const auto* Vehicle = GetRecoveringVehicle();
        if (!Vehicle) return FText::GetEmpty();
        FNumberFormattingOptions Decimal;
        Decimal.MinimumFractionalDigits = 1;
        Decimal.MaximumFractionalDigits = 1;
        return FText::Format(NSLOCTEXT("MWGameplayUI", "CheckpointRecoveryCountdown", "재출발 {0}초"),
            FText::AsNumber(Vehicle->GetCheckpointRecoveryRemaining(), &Decimal));
    }

    void DrawRecoverySlot(const APlayerController& PC, const AMWVehiclePawn& Vehicle, const FGeometry& Geometry,
        const FVector2D& PixelToOverlay, FSlateWindowElementList& DrawElements, int32 Layer) const
    {
        const auto* Box = Cast<UBoxComponent>(Vehicle.GetRootComponent());
        if (!Box) return;
        const FVector Extent = Box->GetUnscaledBoxExtent();
        const FVector Corners[] = {{-Extent.X, -Extent.Y, -Extent.Z}, {Extent.X, -Extent.Y, -Extent.Z},
            {Extent.X, Extent.Y, -Extent.Z}, {-Extent.X, Extent.Y, -Extent.Z}};
        TArray<FVector2f> Line;
        Line.Reserve(UE_ARRAY_COUNT(Corners) + 1);
        FVector2f SlotCenter = FVector2f::ZeroVector;
        for (const FVector& Corner : Corners)
        {
            FVector2D ScreenPoint;
            if (!PC.ProjectWorldLocationToScreen(Box->GetComponentTransform().TransformPosition(Corner), ScreenPoint)) return;
            const FVector2f Projected(ScreenPoint * PixelToOverlay);
            Line.Add(Projected);
            SlotCenter += Projected;
        }
        SlotCenter /= UE_ARRAY_COUNT(Corners);
        // TArray에 자기 원소를 참조로 전달할 수 없으므로 닫는 점을 복사한다.
        const FVector2f FirstPoint = Line[0];
        Line.Add(FirstPoint);
        const float Remaining = Vehicle.GetCheckpointRecoveryRemaining();
        const float Travel = FMath::Clamp(Remaining / FMath::Max(RecoveryLayout.RingTravelSeconds, .01f), 0.f, 1.f);
        const float Scale = Geometry.GetLocalSize().Y / 1080.f;
        const float Radius = Scale * FMath::Lerp(RecoveryLayout.RingRadii.X, RecoveryLayout.RingRadii.Y, Travel);
        FLinearColor Tint = GetPlayerTint(PC.PlayerState);
        Tint.A *= Travel;
        TArray<FVector2f> Ring;
        constexpr int32 RingSegments = 32;
        Ring.Reserve(RingSegments + 1);
        for (int32 Segment = 0; Segment <= RingSegments; ++Segment)
        {
            const float Angle = 2.f * PI * Segment / RingSegments;
            Ring.Add(SlotCenter + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
        }
        const float Thickness = FMath::Max(1.f, RecoveryLayout.SlotThickness * Scale);
        FSlateDrawElement::MakeLines(DrawElements, Layer, Geometry.ToPaintGeometry(), MoveTemp(Ring),
            ESlateDrawEffect::None, Tint, true, Thickness);
        const float BreakAlpha = RecoveryLayout.SlotBreakSeconds > UE_SMALL_NUMBER
            ? 1.f - FMath::Clamp(Remaining / RecoveryLayout.SlotBreakSeconds, 0.f, 1.f) : 0.f;
        if (BreakAlpha <= 0.f)
        {
            FSlateDrawElement::MakeLines(DrawElements, Layer, Geometry.ToPaintGeometry(), MoveTemp(Line),
                ESlateDrawEffect::None, RecoveryLayout.SlotColor, true, Thickness);
            return;
        }
        // 투영된 슬롯의 각 변이 중앙에서 떨어지고 짧아지며 사라진다.
        FLinearColor FragmentTint = RecoveryLayout.SlotColor;
        FragmentTint.A *= 1.f - BreakAlpha;
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(Corners); ++Index)
        {
            const FVector2f Center = (Line[Index] + Line[Index + 1]) * .5f;
            const FVector2f Offset = (Center - SlotCenter).GetSafeNormal() *
                (BreakAlpha * RecoveryLayout.SlotBreakDistance * Scale);
            TArray<FVector2f> Fragment{
                FMath::Lerp(Line[Index], Center, BreakAlpha) + Offset,
                FMath::Lerp(Line[Index + 1], Center, BreakAlpha) + Offset};
            FSlateDrawElement::MakeLines(DrawElements, Layer, Geometry.ToPaintGeometry(), MoveTemp(Fragment),
                ESlateDrawEffect::None, FragmentTint, true, Thickness);
        }
    }

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
        if (!Positive(ItemLayout.MinimumPixels) || !Positive(ItemLayout.IconSize) ||
            !Positive(ItemLayout.MaximumHeightFraction) || ItemLayout.MaximumHeightFraction > 1.f)
            ItemLayout = FMWItemSlotLayout();
        bool bValidBoard = ValidMargin(BoardLayout.CellPadding);
        for (double Value : {double(BoardLayout.HeaderHeight), double(BoardLayout.RowHeight),
                             double(BoardLayout.HeaderFontSize), double(BoardLayout.RowFontSize),
                             BoardLayout.ColumnWidths.X, BoardLayout.ColumnWidths.Y,
                             BoardLayout.ColumnWidths.Z, BoardLayout.ColumnWidths.W})
            bValidBoard &= Positive(Value);
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
    FMWCheckpointRecoveryLayout RecoveryLayout;
    FSlateRoundedBoxBrush RecoveryCountdownBrush{FLinearColor::Transparent};
    FSlateRoundedBoxBrush RecoveryBadgeBrush{FLinearColor::Transparent};
    TWeakObjectPtr<APlayerController> Controller;
    TWeakObjectPtr<UWidget> WaitingBoard;
    TSharedPtr<SGridPanel> Roster;
    TWeakObjectPtr<UWidget> SelectSign;
    TWeakObjectPtr<UWidget> BuildSign;
    TWeakObjectPtr<USizeBox> ItemSlot;
    TWeakObjectPtr<UWidget> RecoveryPanel;
    TWeakObjectPtr<UBorder> RecoveryBadge;
    TWeakObjectPtr<UTextBlock> RecoveryCountdown;
    TWeakObjectPtr<UWidget> RecoveryFlash;
    float ItemDesignSize = 0.f;
    TAttribute<EMWItemId> HeldItem;
    TAttribute<bool> MatchResultsActive;
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
    float ConstructionPromptSeconds = 0.f;
    double NextRosterRefresh = 0.0;
    bool bRosterBuilt = false;
    FMWPIEHUDPreview PIEPreview;
    mutable bool bLoggedNameplate = false;
};

// 일반 게임과 PIE 월드만 대상으로 해 에디터 미리보기에서 런타임 기능이 켜지지 않게 한다.
bool UMWGameplayHUDSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{
    return Type == EWorldType::Game || Type == EWorldType::PIE;
}

EMWWaitingBoardReadiness UMWGameplayHUDSubsystem::GetWaitingBoardReadiness(const APlayerController& PC) const
{
    // 아직 HUD를 만들지 않은 상태와 필수 대기판 부품이 없는 상태를 구분한다.
    if (!Overlay || ItemController.Get() != &PC || RoomPhase != EMWRoomPhase::PreparingRace)
        return EMWWaitingBoardReadiness::Pending;
    return StaticCastSharedPtr<SMWGameplayHUD>(Overlay)->GetWaitingBoardReadiness();
}

void UMWGameplayHUDSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    if (InWorld.WorldType == EWorldType::PIE)
    {
        // PIE 인스턴스 번호를 제거한 패키지 이름만 비교하므로 맵을 다시 로드하지 않는다.
        const FName PackageName(*UWorld::RemovePIEPrefix(InWorld.GetOutermost()->GetName()));
        const auto MatchesWorld = [PackageName](const TSoftObjectPtr<UWorld>& Map)
        {
            return Map.ToSoftObjectPath().GetLongPackageFName() == PackageName;
        };
        PIEPreview.bSpeedometer = PIESpeedometerMaps.ContainsByPredicate(MatchesWorld);
        PIEPreview.bItemSlot = PIEItemSlotMaps.ContainsByPredicate(MatchesWorld);
    }
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
    const bool bCanAttach = GameState || PIEPreview.IsEnabled();
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
    if (Overlay || (!ObservedGameState.IsValid() && !PIEPreview.IsEnabled()))
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
    if (!RaceHUDWidget)
    {
        UE_LOG(LogMWInGameUI, Error, TEXT("경기 HUD WBP를 만들 수 없습니다. RaceHUDWidgetClass 설정을 확인하세요."));
    }

    const TWeakObjectPtr<UMWGameplayHUDSubsystem> WeakThis(this);
    UClass* PanelsClass = Settings->MatchPanelsClass.LoadSynchronous();
    MatchPanels = PanelsClass ? CreateWidget<UUserWidget>(PC, PanelsClass) : nullptr;
    if (!MatchPanels)
        UE_LOG(LogMWInGameUI, Error, TEXT("경기 화면 WBP를 로드할 수 없습니다. MatchPanelsClass=%s"),
            *Settings->MatchPanelsClass.ToSoftObjectPath().ToString());
    Overlay = SNew(SMWGameplayHUD).Panels(MatchPanels)
                  .Controller(PC)
                  .Visibility(EVisibility::HitTestInvisible)
                  .PIEPreview(PIEPreview)
                  .MatchResultsActive_Lambda([WeakThis]
                  {
                      const auto* HUD = WeakThis.Get();
                      return HUD && IsValid(HUD->MatchResultsWidget);
                  })
                  .RaceHUD(RaceHUDWidget ? RaceHUDWidget->TakeWidget() : SNullWidget::NullWidget)
                  .HeldItem_Lambda(
                      [WeakThis]
                      {
                          const UMWGameplayHUDSubsystem* HUD = WeakThis.Get();
                          return HUD ? HUD->HeldItem : EMWItemId::None;
                      });
    Viewport = GameViewport;
    GameViewport->AddViewportWidgetContent(Overlay.ToSharedRef(), 20);
    RefreshMatchResults();
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

    RemoveMatchResults();
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
        RemoveMatchResults();
        RoomPhase.Reset();
        return;
    }

    RoomPhase = GameState->GetRoomPhase();
    UE_LOG(LogMWInGameUI, Verbose, TEXT("[MWRaceHUD] 경기 단계 갱신. World=%s Phase=%s"),
           *GetNameSafe(GetWorld()), *UEnum::GetValueAsString(RoomPhase.GetValue()));
    RefreshMatchResults();
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

bool UMWGameplayHUDSubsystem::RestoreMatchResultsInput(APlayerController& Controller)
{
    return MatchResultsWidget && MatchResultsWidget->RestoreInput(Controller);
}

void UMWGameplayHUDSubsystem::RefreshMatchResults()
{
    const auto* State = ObservedGameState.Get();
    if (!State || (State->GetRoomPhase() != EMWRoomPhase::RoundResults && State->GetRoomPhase() != EMWRoomPhase::MatchResults))
    {
        RemoveMatchResults();
        return;
    }

    APlayerController* Controller = ItemController.Get();
    const auto& Progress = State->GetProgressRef();
    // 같은 라운드에서 집계를 끝까지 본 창은 최종 결과에서 합산을 반복하지 않는다.
    const bool bRoundTallySeen = Progress.Phase == EMWRoomPhase::MatchResults
        && AttemptedResultPhase == EMWRoomPhase::RoundResults && AttemptedMatchResultsEpoch == Progress.ResultsEpoch
        && MatchResultsWidget && MatchResultsWidget->HasPresentedRoundTally();
    if (AttemptedResultPhase.IsSet() && AttemptedResultPhase != Progress.Phase)
    {
        RemoveMatchResults();
    }
    if (PIEPreview.IsEnabled() || !Controller || Progress.Results.IsEmpty()
        || Progress.ResultsEpoch != Progress.RoundEpoch || AttemptedMatchResultsEpoch == Progress.ResultsEpoch)
    {
        return;
    }

    // 동일 결과가 재복제돼도 화면 전환과 로컬 포커스를 다시 시작하지 않는다.
    AttemptedResultPhase = Progress.Phase;
    AttemptedMatchResultsEpoch = Progress.ResultsEpoch;
    UClass* WidgetClass = GetDefault<UMWGameplayHUDSubsystem>()->MatchResultsWidgetClass.LoadSynchronous();
    auto* Widget = WidgetClass ? CreateWidget<UMWMatchResultsWidget>(Controller, WidgetClass) : nullptr;
    if (Widget && Widget->Present(*State, !bRoundTallySeen))
    {
        MatchResultsWidget = Widget;
    }
    else
    {
        UE_LOG(LogMWInGameUI, Warning, TEXT("경기 결과 WBP를 표시하지 못해 기존 결과 요약을 유지합니다."));
    }
}

void UMWGameplayHUDSubsystem::RemoveMatchResults()
{
    AttemptedMatchResultsEpoch = INDEX_NONE;
    AttemptedResultPhase.Reset();
    if (MatchResultsWidget)
    {
        auto* Widget = MatchResultsWidget.Get();
        MatchResultsWidget = nullptr;
        Widget->Dismiss();
    }
}

// 실제 구독했던 Controller·ItemComponent와 Viewport에서 연결을 해제한다.
void UMWGameplayHUDSubsystem::RemoveOverlay()
{
    RemoveMatchResults();
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
    // Slate 속성은 HUD 모델을 참조하므로 UMG 호스트가 보관하는 콘텐츠를 먼저 분리한다.
    if (MatchPanels)
    {
        for (FName Name : {FName(TEXT("RosterHost")), FName(TEXT("ItemIconHost"))})
            if (auto* Host = Cast<UNativeWidgetHost>(MatchPanels->GetWidgetFromName(Name))) Host->SetContent(SNullWidget::NullWidget);
    }
    MatchPanels = nullptr;
    Overlay.Reset();
    RaceHUDWidget = nullptr;
    Viewport.Reset();
}
