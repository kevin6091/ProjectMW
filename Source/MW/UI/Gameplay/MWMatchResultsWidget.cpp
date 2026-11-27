#include "UI/Gameplay/MWMatchResultsWidget.h"

#include "Algo/Count.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/WidgetSwitcher.h"
#include "Data/MWVehicleDefinitionData.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/Construction/MWTrackConstructionComponent.h"
#include "Gameplay/Track/MWTrackRuntimeSettings.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "System/GameState/MWGameplayGameState.h"
#include "System/MWAssetManager.h"
#include "System/MWPlayerState.h"
#include "TimerManager.h"
#include "UI/Common/MWParticipantPalette.h"
#include "UI/Gameplay/MWArrivalScreen.h"
#include "UI/VehicleSelection/MWVehicleSelectionWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogMWMatchResults, Log, All);

#define LOCTEXT_NAMESPACE "MWMatchResults"

void UMWMatchResultRowWidget::SetResult(const FMWPlayerRoundState& Result, const FText& PlayerName, int32 Rank, UTexture2D* Portrait)
{
    const FLinearColor Color = GetDefault<UMWParticipantPalette>()->GetColorForMember(Result.MemberId);
    if (MemberAccent) MemberAccent->SetBrushColor(Color);
    if (CardBackground) CardBackground->SetBrushColor(Color);
    if (ScoreBar)
    {
        FLinearColor Fill = FMath::Lerp(Color, FLinearColor::White, TallyFillWhiteMix) * TallyFillExposure;
        Fill.A = Color.A;
        ScoreBar->SetFillColorAndOpacity(Fill);
    }
    if (TallyFillOutline && FMath::IsFinite(TallyOutlineParticipantMix) && TallyOutlineParticipantMix > 0.f)
    {
        // 브러시의 도장색과 알파는 WBP에서 가져오고 참가자색은 테두리에 한 번만 반영한다.
        FSlateBrush Brush = TallyFillOutline->GetBrush();
        const FLinearColor Base = Brush.OutlineSettings.Color.GetSpecifiedColor();
        FLinearColor Outline = FMath::Lerp(Base, Color, FMath::Clamp(TallyOutlineParticipantMix, 0.f, 1.f));
        Outline.A = Base.A;
        Brush.OutlineSettings.Color = FSlateColor(Outline);
        TallyFillOutline->SetBrush(Brush);
    }
    if (RankLabel)
    {
        RankLabel->SetText(Rank > 0 ? FText::Format(LOCTEXT("Rank", "{0}위"), FText::AsNumber(Rank))
            : LOCTEXT("Spectating", "관전"));
    }
    if (RankMedals)
    {
        // 카드 WBP는 1위부터 메달 이미지를 순서대로 보관한다.
        const bool bHasMedal = Rank > 0 && Rank <= RankMedals->GetChildrenCount();
        if (bHasMedal)
        {
            RankMedals->SetActiveWidgetIndex(Rank - 1);
        }
        RankMedals->SetVisibility(bHasMedal ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
        if (MemberAccent)
        {
            MemberAccent->SetVisibility(bHasMedal ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
        }
    }
    if (VehiclePortrait)
    {
        VehiclePortrait->SetBrushFromTexture(Portrait, true);
        VehiclePortrait->SetVisibility(Portrait ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    }
    if (TallyVehicleBadge)
    {
        TallyVehicleBadge->SetVisibility(Portrait ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
    NameLabel->SetText(PlayerName);
    TotalScoreLabel->SetText(FText::AsNumber(Result.TotalScore));
}

void UMWMatchResultRowWidget::ShowRank(int32 Rank)
{
    if (RankSuffix)
    {
        RankSuffix->SetVisibility(Rank > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    }
    if (RankLabel)
    {
        RankLabel->SetVisibility(Rank > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
        if (Rank > 0)
        {
            const FText Number = FText::AsNumber(Rank);
            RankLabel->SetText(RankSuffix ? Number : FText::Format(LOCTEXT("Rank", "{0}위"), Number));
        }
        if (TallyRankGroup || UWidgetLayoutLibrary::SlotAsCanvasSlot(RankLabel))
        {
            NameLabel->SetVisibility(Rank > 0 ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
        }
    }
}

void UMWMatchResultRowWidget::SetPodiumEntry(float MedalAlpha, float PlateAlpha, float NameAlpha, float PortraitAlpha,
    const FVector2D& PlateStart, FVector2D MedalStart, const FGeometry* ViewGeometry)
{
    // 새 WBP는 메달과 판을 같은 부모의 별도 슬롯에 둔다. 기존 카드는 전체 단위로 진입한다.
    if (PodiumMedal && CardBackground)
    {
        if (ViewGeometry)
        {
            const FGeometry& MedalGeometry = PodiumMedal->GetCachedGeometry();
            const float LayoutScale = MedalGeometry.GetAccumulatedLayoutTransform().GetScale();
            const double Width = MedalGeometry.GetLocalSize().X;
            if (LayoutScale > UE_SMALL_NUMBER && Width > UE_SMALL_NUMBER
                && ViewGeometry->GetLocalSize().X > UE_SMALL_NUMBER)
            {
                // 판 전체 폭 대신 메달 자신의 오른쪽 끝을 화면 왼쪽에 둬 과도한 진입 거리를 없앤다.
                // 현재 애니메이션의 이동값을 되먹이지 않도록 렌더 변환 전 레이아웃 경계를 사용한다.
                const double Right = MedalGeometry.GetLayoutBoundingRect().Right;
                const double Left = ViewGeometry->GetLayoutBoundingRect().Left;
                const double ExtraWidth = Width * FMath::Max(0., PodiumMedal->GetRenderTransform().Scale.X - 1.);
                MedalStart.X = FMath::Min(0., (Left - Right) / LayoutScale - ExtraWidth);
            }
            else
            {
                // 첫 레이아웃 전 임시 오프셋이 보이면 다음 프레임에 메달이 왼쪽으로 튄다.
                MedalAlpha = 0.f;
            }
        }
        PodiumMedal->SetRenderOpacity(MedalAlpha);
        PodiumMedal->SetRenderTranslation(MedalStart * (1.f - MedalAlpha));
        CardBackground->SetRenderOpacity(PlateAlpha);
        CardBackground->SetRenderTranslation(PlateStart * (1.f - PlateAlpha));
    }
    else
    {
        SetRenderOpacity(PlateAlpha);
        SetRenderTranslation(PlateStart * (1.f - PlateAlpha));
    }
    NameLabel->SetRenderOpacity(NameAlpha);
    NameLabel->SetRenderScale(FVector2D(FMath::Lerp(InitialNameScale, 1.f, NameAlpha)));
    TotalScoreLabel->SetRenderOpacity(NameAlpha);
    if (VehiclePortrait)
    {
        VehiclePortrait->SetRenderOpacity(bVehiclePortraitSuppressed ? 0.f : PortraitAlpha);
        VehiclePortrait->SetRenderTranslation(PortraitEntryOffset * (1.f - PortraitAlpha));
    }
}

void UMWMatchResultRowWidget::SetVehiclePortraitSuppressed(bool bSuppressed)
{
    bVehiclePortraitSuppressed = bSuppressed;
    if (bSuppressed && VehiclePortrait) VehiclePortrait->SetRenderOpacity(0.f);
}

void UMWMatchResultRowWidget::SetTallyScore(int32 Score, int32 Target)
{
    const bool bSameScore = ShownScore.IsSet() && ShownScore.GetValue() == Score;
    if (bSameScore && ShownScaleScore == Target)
    {
        return;
    }
    ShownScore = Score;
    ShownScaleScore = Target;
    if (!bSameScore)
    {
        FNumberFormattingOptions ScoreFormat;
        ScoreFormat.SetUseGrouping(ScoreDigits <= 1).SetMinimumIntegralDigits(FMath::Clamp(ScoreDigits, 1, 10));
        TotalScoreLabel->SetText(FText::AsNumber(Score, &ScoreFormat));
    }
    if (ScoreBar)
    {
        const float Fraction = Target > 0 ? FMath::Clamp(static_cast<float>(Score) / Target, 0.f, 1.f) : 0.f;
        // 바탕 위에서 점수에 비례해 자라도록 하고, 색 막대와 정보행에 같은 경계를 적용한다.
        const float Fill = FMath::Lerp(FMath::Clamp(TallyBaseFraction, 0.f, 1.f), 1.f, Fraction);
        ScoreBar->SetPercent(Fill);
        const float MarkerAnchor = 1.f - Fill;
        if (auto* OutlineSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(TallyFillOutline))
        {
            // 테두리의 여백과 굵기는 WBP에서 편집하고, 위쪽 경계만 채움 진행률과 함께 움직인다.
            FAnchors Anchors = OutlineSlot->GetAnchors();
            Anchors.Minimum.Y = MarkerAnchor;
            OutlineSlot->SetAnchors(Anchors);
            TallyFillOutline->SetVisibility(Fill > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
        }
        UWidget* NameMarker = TallyNameMarker ? TallyNameMarker.Get() : NameLabel.Get();
        // 차량 배지와 점수는 한 슬롯에서 움직인다. 기존 WBP에서는 점수 글자 슬롯을 사용한다.
        UWidget* ScoreMarker = TallyScoreMarker ? TallyScoreMarker.Get() : TotalScoreLabel.Get();
        // 새 점수판은 숫자와 접미사를 한 슬롯에 두어 높이와 간격을 같이 유지한다.
        UWidget* RankMarker = TallyRankGroup ? TallyRankGroup.Get() : RankLabel.Get();
        UWidget* SuffixMarker = TallyRankGroup ? nullptr : RankSuffix.Get();
        for (UWidget* Marker : {NameMarker, RankMarker, SuffixMarker, ScoreMarker})
        {
            if (auto* MarkerSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(Marker))
            {
                FAnchors Anchors = MarkerSlot->GetAnchors();
                Anchors.Minimum.Y = Anchors.Maximum.Y = MarkerAnchor;
                MarkerSlot->SetAnchors(Anchors);
            }
        }
    }
}

void UMWMatchResultsWidget::NativeDestruct()
{
    ClearPresentation();
    Super::NativeDestruct();
}

bool UMWMatchResultsWidget::Present(const AMWGameplayGameState& State, bool bPlayTally)
{
    APlayerController* Controller = GetOwningPlayer();
    const FMWGameplayProgress& Progress = State.GetProgressRef();
    bFinalResults = Progress.Phase == EMWRoomPhase::MatchResults;
    if (!Controller || !Controller->IsLocalController() || Controller->GetWorld() != State.GetWorld()
        || !TallyPanel || !TallyColumns || !TallyHeading || !TallyColumnClass
        || (bFinalResults && (!PodiumPanel || (!PodiumCards && !(WinnerCardHost && RunnerUpCardHost && ThirdPlaceCardHost))
            || !PodiumCardClass))
        || (!bFinalResults && Progress.Phase != EMWRoomPhase::RoundResults) || Progress.Results.IsEmpty()
        || Progress.RoundEpoch <= 0 || Progress.ResultsEpoch != Progress.RoundEpoch)
    {
        return false;
    }

    TMap<int32, AMWPlayerState*> Players;
    for (APlayerState* Base : State.PlayerArray)
    {
        if (auto* Player = Cast<AMWPlayerState>(Base))
        {
            Players.Add(Player->GetLobbyMemberId(), Player);
        }
    }

    Results = Progress.Results;
    if (TallyColumnExtent)
    {
        const float* Width = TallyWidthByPlayerCount.Find(Results.Num());
        if (Width && FMath::IsFinite(*Width) && *Width > 0.f)
        {
            TallyColumnExtent->SetWidthOverride(*Width);
        }
    }
    RoundNumber = Progress.RoundIndex;
    BarScaleScore = Progress.TargetScore;
    RoundEndServerTime = Progress.PhaseEndServerTime;
    const auto CreateEntry = [&](TSubclassOf<UMWMatchResultRowWidget> EntryClass, const FMWPlayerRoundState& Result)
    {
        auto* Entry = CreateWidget<UMWMatchResultRowWidget>(Controller, EntryClass);
        if (Entry)
        {
            const AMWPlayerState* Player = Players.FindRef(Result.MemberId);
            const FText Name = Player && !Player->GetPlayerName().IsEmpty() ? FText::AsCultureInvariant(Player->GetPlayerName())
                : FText::Format(LOCTEXT("UnknownPlayer", "플레이어 {0}"), FText::AsNumber(Result.MemberId));
            UTexture2D* Portrait = Player
                ? GetDefault<UMWVehicleSelectionWidget>()->DefaultVehicleThumbnails.FindRef(Player->GetVehicleAssetID()).LoadSynchronous()
                : nullptr;
            Entry->SetResult(Result, Name, bFinalResults ? Result.FinalRank : Result.RoundRank, Portrait);
        }
        return Entry;
    };
    // 집계는 참가자 슬롯 순을 유지하고 순위 카드는 서버가 확정한 순서를 사용한다.
    Results.Sort([](const FMWPlayerRoundState& A, const FMWPlayerRoundState& B) { return A.MemberId < B.MemberId; });
    for (const FMWPlayerRoundState& Result : Results)
    {
        auto* Column = CreateEntry(TallyColumnClass, Result);
        if (!Column) return false;
        Column->ShowRank(0);
        auto* ColumnSlot = TallyColumns->AddChildToHorizontalBox(Column);
        ColumnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        BarScaleScore = FMath::Max(BarScaleScore, Result.TotalScore);
    }
    if (bFinalResults)
    {
        if (const auto* Winner = Progress.Results.FindByPredicate([](const FMWPlayerRoundState& Result) { return Result.FinalRank == 1; }))
        {
            WinnerPlayer = Players.FindRef(Winner->MemberId);
            if (const AMWPlayerState* Player = WinnerPlayer.Get(); Player && WinnerVehicleShot.IsValid())
            {
                PrepareWinnerVehicleClass(*Player);
            }
        }
        // 순위별 슬롯은 WBP가 배치한다. 기존 에셋에서는 하위 카드를 수직 목록에 추가한다.
        UBorder* Hosts[] = {WinnerCardHost.Get(), RunnerUpCardHost.Get(), ThirdPlaceCardHost.Get()};
        UMWMatchResultRowWidget* RankedCards[UE_ARRAY_COUNT(Hosts)] = {};
        for (UBorder* Host : Hosts)
        {
            if (Host) Host->SetVisibility(ESlateVisibility::Collapsed);
        }
        for (const FMWPlayerRoundState& Result : Progress.Results)
        {
            if (Result.FinalRank < 1 || Result.FinalRank > 3) continue;
            auto* PodiumCard = CreateEntry(PodiumCardClass, Result);
            if (!PodiumCard) return false;
            if (UBorder* Host = Hosts[Result.FinalRank - 1])
            {
                Host->SetVisibility(ESlateVisibility::HitTestInvisible);
                Host->SetContent(PodiumCard);
            }
            else
            {
                PodiumCards->AddChildToVerticalBox(PodiumCard);
            }
            RankedCards[Result.FinalRank - 1] = PodiumCard;
        }
        for (UMWMatchResultRowWidget* Card : RankedCards)
        {
            if (Card) PodiumRevealOrder.Add(Card);
        }
    }
    for (const FMWPlayerRoundState& Result : Progress.Results)
    {
        UE_LOG(LogMWMatchResults, Log, TEXT("World=%s Epoch=%d Member=%d Rank=%d Total=%d Checkpoints=%d CheckpointScore=%d FinishScore=%d"),
            *GetWorld()->GetOutermost()->GetName(), Progress.ResultsEpoch, Result.MemberId, Result.FinalRank,
            Result.TotalScore, Result.CheckpointsPassed, Result.CheckpointScore, Result.FinishScore);
    }
    if (bPlayTally)
    {
        PrepareTally();
    }

    if (ResultsPanel) ResultsPanel->SetVisibility(ESlateVisibility::Collapsed);
    TallyPanel->SetVisibility(ESlateVisibility::Collapsed);
    if (PodiumPanel)
    {
        PodiumPanelOpacity = PodiumPanel->GetRenderOpacity();
        PodiumPanel->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (ContinueButton) ContinueButton->SetVisibility(ESlateVisibility::Collapsed);
    if (!AddToPlayerScreen())
    {
        return false;
    }
    // 라운드 집계도 결승을 배경으로 즉시 시작한다. 카메라 단독 연출은 매치 종료 때만 기다린다.
    if (BeginFinishShot(State) && bFinalResults && ScoreStages.IsEmpty())
    {
        BeginPodiumApproach();
    }
    else
    {
        BeginTally(State);
    }
    RestoreInput(*Controller);
    return true;
}

bool UMWMatchResultsWidget::BeginFinishShot(const AMWGameplayGameState& State)
{
    const auto& Course = State.GetConstructionComponent()->GetPlacementSession();
    const auto& Progress = State.GetProgressRef();
    if (!FMath::IsFinite(ShotSeconds) || ShotSeconds <= 0.f || Course.PlacedTracks.IsEmpty()
        || Course.SessionId != Progress.CourseSessionId || Course.CourseRevision != Progress.CourseRevision)
    {
        return false;
    }

    const FMWPlacedTrack& Finish = Course.PlacedTracks.Last();
    const auto& FinishClass = GetDefault<UMWTrackRuntimeSettings>()->FinishTrackClass;
    if (FinishClass.IsNull() || Finish.TrackClass != FinishClass)
    {
        return false;
    }

    // 설치가 쓰는 결승 포트를 복제된 코스 Transform에 합성한다. 결승 Actor 포인터의 도착을 기다리지 않는다.
    FinishFrame = Course.Ground.FinishPort * Finish.WorldTransform;
    const FVector LookAt = FinishFrame.TransformPositionNoScale(LookAtOffset);
    FVector CameraLocation = FinishFrame.TransformPositionNoScale(CameraOffset);
    int32 ViewIndex = 0;
    const bool bHasMotion = bFinalResults && PodiumCameraKeys.Num() >= 2;
    PodiumCameraTravelScale = FVector::ZeroVector;
    if ((!AlternateCameraOffsets.IsEmpty() || bHasMotion) && !LookAt.ContainsNaN() && !CameraLocation.ContainsNaN())
    {
        // 가시성 충돌이 없는 트랙 장식도 시야를 가릴 수 있어 렌더 메시 경계까지 검사한다.
        TArray<FBox> TrackBounds;
        TArray<FBox> FinishApproachBounds;
        for (const FMWPlacedTrack& Piece : Course.PlacedTracks)
        {
            if (Piece.PieceId == Finish.PieceId || !IsValid(Piece.Actor.Get())) continue;
            auto& Bounds = bHasMotion && Piece.PieceId == Finish.ConnectedTo.PieceId
                ? FinishApproachBounds : TrackBounds;
            TInlineComponentArray<UStaticMeshComponent*> Meshes(Piece.Actor.Get(), true);
            for (const UStaticMeshComponent* Mesh : Meshes)
            {
                if (Mesh->IsVisible() && Mesh->GetStaticMesh()) Bounds.Add(Mesh->Bounds.GetBox());
            }
        }
        FCollisionQueryParams Query(SCENE_QUERY_STAT(MWFinishShotVisibility), true);
        if (IsValid(Finish.Actor.Get())) Query.AddIgnoredActor(Finish.Actor.Get());
        const auto IsObstructed = [&](const FVector& Location, const FVector& Aim)
        {
            const auto BlocksSight = [&](const FBox& Bounds)
            {
                return FMath::LineBoxIntersection(Bounds, Location, Aim, Aim - Location);
            };
            return FVector::DistSquared(Location, Aim) <= UE_KINDA_SMALL_NUMBER
                || GetWorld()->LineTraceTestByChannel(Location, Aim, ECC_Visibility, Query)
                || TrackBounds.ContainsByPredicate(BlocksSight)
                || FinishApproachBounds.ContainsByPredicate([&](const FBox& Bounds)
                {
                    // 결승에 연결된 도로의 상자가 주시점까지 감싸는 경우다. 실제 가림은 위의 충돌 검사로 확인한다.
                    return Bounds.IsInsideOrOn(Location) || (!Bounds.IsInsideOrOn(Aim) && BlocksSight(Bounds));
                });
        };
        const auto& Keys = PodiumCameraKeys;
        const bool bCanAnimate = bHasMotion && Keys[0].Time == 0.f && Keys.Last().Time == 1.f
            && Keys[0].CameraDelta.IsZero() && Keys[0].AimDelta.IsZero() && Keys[0].FieldOfViewDelta == 0.f;
        const auto FindClearTravelScale = [&](const FVector& Origin) -> FVector
        {
            if (!bCanAnimate)
            {
                return FVector::ZeroVector;
            }

            // 재생 중 시점을 바꾸지 않도록 각 후보의 가시성과 이동 경로를 선택 전에 검사한다.
            constexpr int32 SamplesPerSegment = 6;
            for (const FVector& Scale : {FVector(1.), FVector(.65, 1., .65), FVector(.65),
                FVector(.35, .75, .35), FVector(.35)})
            {
                bool bClear = true;
                FVector Previous = Origin;
                for (int32 Index = 1; bClear && Index < Keys.Num(); ++Index)
                {
                    const auto& Start = Keys[Index - 1];
                    const auto& End = Keys[Index];
                    if (!(Start.Time < End.Time) || End.CameraDelta.ContainsNaN() || End.AimDelta.ContainsNaN()
                        || !FMath::IsFinite(End.FieldOfViewDelta))
                    {
                        return FVector::ZeroVector;
                    }
                    for (int32 Sample = 1; Sample <= SamplesPerSegment; ++Sample)
                    {
                        const float Alpha = static_cast<float>(Sample) / SamplesPerSegment;
                        const FVector Position = Origin + FinishFrame.TransformVectorNoScale(
                            FMath::Lerp(Start.CameraDelta, End.CameraDelta, Alpha) * Scale);
                        const FVector Aim = LookAt + FinishFrame.TransformVectorNoScale(
                            FMath::Lerp(Start.AimDelta, End.AimDelta, Alpha) * Scale);
                        if (IsObstructed(Position, Aim) || GetWorld()->LineTraceTestByChannel(
                            Previous, Position, ECC_Visibility, Query))
                        {
                            bClear = false;
                            break;
                        }
                        Previous = Position;
                    }
                }
                if (bClear)
                {
                    return Scale;
                }
            }
            return FVector::ZeroVector;
        };

        ViewIndex = INDEX_NONE;
        const auto TryCameraOffset = [&](const FVector& Offset, int32 Index)
        {
            const FVector Origin = FinishFrame.TransformPositionNoScale(Offset);
            if (Origin.ContainsNaN() || IsObstructed(Origin, LookAt))
            {
                return false;
            }
            const FVector TravelScale = FindClearTravelScale(Origin);
            if (ViewIndex == INDEX_NONE || !TravelScale.IsZero())
            {
                CameraLocation = Origin;
                ViewIndex = Index;
                PodiumCameraTravelScale = TravelScale;
            }
            // 시상은 검증된 이동 후보가 나올 때까지 검사한다. 정지 화면은 첫 가시 후보를 사용한다.
            return !bCanAnimate || !TravelScale.IsZero();
        };

        bool bFound = TryCameraOffset(CameraOffset, 0);
        if (!bFound && bCanAnimate)
        {
            // 측면으로 보내기 전에 대체 시점의 높이만 사용해 기본 접근 방향을 보존한다.
            for (int32 Index = 0; !bFound && Index < AlternateCameraOffsets.Num(); ++Index)
            {
                const double Height = AlternateCameraOffsets[Index].Z;
                if (Height > CameraOffset.Z)
                {
                    bFound = TryCameraOffset(FVector(CameraOffset.X, CameraOffset.Y, Height),
                        AlternateCameraOffsets.Num() + Index + 1);
                }
            }
        }
        for (int32 Index = 0; !bFound && Index < AlternateCameraOffsets.Num(); ++Index)
        {
            bFound = TryCameraOffset(AlternateCameraOffsets[Index], Index + 1);
        }
        if (ViewIndex == INDEX_NONE && bCanAnimate)
        {
            return false;
        }
    }
    const FVector Direction = LookAt - CameraLocation;
    if (CameraLocation.ContainsNaN() || Direction.ContainsNaN() || Direction.IsNearlyZero())
    {
        return false;
    }

    FActorSpawnParameters Spawn;
    Spawn.ObjectFlags = RF_Transient;
    FinishCamera = GetWorld()->SpawnActor<ACameraActor>(CameraLocation, Direction.Rotation(), Spawn);
    if (!FinishCamera)
    {
        return false;
    }

    APlayerController* Controller = GetOwningPlayer();
    PreviousViewTarget = Controller->GetViewTarget();
    if (FMath::IsFinite(CameraFOV) && CameraFOV > 0.f)
    {
        FinishCamera->GetCameraComponent()->SetFieldOfView(CameraFOV);
    }
    FinishCameraOrigin = CameraLocation;
    FinishCameraBaseFOV = FinishCamera->GetCameraComponent()->FieldOfView;
    const float Blend = !bHasMotion && FMath::IsFinite(CameraBlendSeconds)
        ? FMath::Clamp(CameraBlendSeconds, 0.f, ShotSeconds) : 0.f;
    Controller->SetViewTargetWithBlend(FinishCamera, Blend, VTBlend_EaseInOut, 0.f, true);

    UE_LOG(LogMWMatchResults, Log, TEXT("World=%s Screen=FinishShot Piece=%d Epoch=%d View=%d Camera=%s TravelScale=%.2f SideScale=%.2f"),
        *GetWorld()->GetOutermost()->GetName(), Finish.PieceId, Progress.ResultsEpoch, ViewIndex,
        *CameraLocation.ToCompactString(), PodiumCameraTravelScale.X, PodiumCameraTravelScale.Y);
    return true;
}

void UMWMatchResultsWidget::UpdatePodiumCamera(float SequenceElapsed)
{
    if (!FinishCamera || PodiumCameraTravelScale.IsZero()) return;
    const float Duration = ShotSeconds + GetPodiumDuration();
    const float Time = Duration > 0.f ? FMath::Clamp(SequenceElapsed / Duration, 0.f, 1.f) : 1.f;
    int32 Index = 1;
    while (Index < PodiumCameraKeys.Num() - 1 && Time > PodiumCameraKeys[Index].Time) ++Index;
    const auto& Start = PodiumCameraKeys[Index - 1];
    const auto& End = PodiumCameraKeys[Index];
    const float Alpha = FMath::SmoothStep(Start.Time, End.Time, Time);
    const FVector Location = FinishCameraOrigin + FinishFrame.TransformVectorNoScale(
        FMath::Lerp(Start.CameraDelta, End.CameraDelta, Alpha) * PodiumCameraTravelScale);
    const FVector Aim = FinishFrame.TransformPositionNoScale(LookAtOffset
        + FMath::Lerp(Start.AimDelta, End.AimDelta, Alpha) * PodiumCameraTravelScale);
    FinishCamera->SetActorLocationAndRotation(Location, (Aim - Location).Rotation());
    FinishCamera->GetCameraComponent()->SetFieldOfView(FMath::Clamp(FinishCameraBaseFOV
        + FMath::Lerp(Start.FieldOfViewDelta, End.FieldOfViewDelta, Alpha) * static_cast<float>(PodiumCameraTravelScale.X), 1.f, 179.f));
}

void UMWMatchResultsWidget::PrepareWinnerVehicleClass(const AMWPlayerState& Player)
{
    const auto& Assets = UMWAssetManager::Get();
    const auto Id = Player.GetVehicleAssetID();
    const auto* Definition = Assets.FindLoadedAsset<UMWVehicleDefinitionData>(Id);
    if (!Definition) Definition = Cast<UMWVehicleDefinitionData>(Assets.FindAssetPath(Id).ResolveObject());
    if (!Definition) return;

    WinnerVehicleClass = Definition->GetPawnClass();
    if (WinnerVehicleClass.IsPending())
    {
        // 결승 카메라가 보이는 동안 외형을 준비한다. 시상 공개는 파일 I/O를 기다리지 않는다.
        WinnerVehicleClassLoad = UMWAssetManager::GetStreamableManager().RequestAsyncLoad(WinnerVehicleClass.ToSoftObjectPath());
    }
}

float UMWMatchResultsWidget::GetWinnerVehicleLead() const
{
    return FMath::Clamp(WinnerVehicleShot.LeadSeconds, 0.f, ShotSeconds);
}

float UMWMatchResultsWidget::GetPodiumCardStagger() const
{
    return PodiumRevealOrder.Num() > 1 && FMath::IsFinite(WinnerSeconds) && WinnerSeconds > 0.f
        && FMath::IsFinite(PodiumCardStaggerSeconds) && PodiumCardStaggerSeconds > 0.f
        ? FMath::Min(PodiumCardStaggerSeconds, WinnerSeconds / PodiumRevealOrder.Num()) : 0.f;
}

float UMWMatchResultsWidget::GetPodiumDuration() const
{
    const float Reveal = FMath::IsFinite(WinnerSeconds) ? FMath::Max(WinnerSeconds, 0.f) : 0.f;
    const float Hold = PodiumRevealOrder.Num() > 1 && GetPodiumCardStagger() <= 0.f && FMath::IsFinite(FirstPlaceHoldSeconds)
        ? FMath::Max(FirstPlaceHoldSeconds, 0.f) : 0.f;
    return Reveal + Hold;
}

void UMWMatchResultsWidget::UpdateWinnerVehicle(float SequenceElapsed)
{
    APlayerController* Controller = GetOwningPlayer();
    if (!Controller || !FinishCamera || SequenceElapsed < 0.f || PodiumRevealOrder.IsEmpty()) return;

    if (!bWinnerVehicleSpawnAttempted)
    {
        const auto* Player = WinnerPlayer.Get();
        auto* Vehicle = Player ? Player->GetPawn<AMWVehiclePawn>() : nullptr;
        UClass* VehicleClass = Vehicle ? Vehicle->GetClass() : WinnerVehicleClass.Get();
        if (!Player || !WinnerVehicleShot.IsValid() || !VehicleClass) return;
        bWinnerVehicleSpawnAttempted = true;

        FActorSpawnParameters Spawn;
        Spawn.ObjectFlags = RF_Transient;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Showcase = GetWorld()->SpawnActor<AMWWinnerVehicleShowcase>(Spawn);
        if (Showcase && Showcase->Initialize(*Controller, Vehicle, VehicleClass))
        {
            WinnerShowcase = Showcase;
            UE_LOG(LogMWMatchResults, Log, TEXT("World=%s WinnerVehicle Member=%d Asset=%s Source=%s"),
                *GetWorld()->GetOutermost()->GetName(), Player->GetLobbyMemberId(), *Player->GetVehicleAssetID().ToString(),
                Vehicle ? TEXT("Pawn") : TEXT("ClassDefaults"));
        }
        else if (Showcase)
        {
            Showcase->Destroy();
        }
        if (!WinnerShowcase.IsValid())
        {
            UE_LOG(LogMWMatchResults, Warning, TEXT("World=%s WinnerVehicle fallback Member=%d Class=%s"),
                *GetWorld()->GetOutermost()->GetName(), Player->GetLobbyMemberId(), *GetNameSafe(VehicleClass));
        }
    }

    if (AMWWinnerVehicleShowcase* Showcase = WinnerShowcase.Get())
    {
        const float Total = GetWinnerVehicleLead() + GetPodiumDuration();
        if (Showcase->Present(WinnerVehicleShot, FMath::Clamp(SequenceElapsed / Total, 0.f, 1.f)))
        {
            PodiumRevealOrder[0]->SetVehiclePortraitSuppressed(true);
        }
    }
}

void UMWMatchResultsWidget::PlayCelebration()
{
    // 연출 전용이다. 트랙 충돌이나 차량·점수 상태를 건드리지 않는다.
    if (!FinishCamera || !CelebrationSystem)
    {
        return;
    }
    for (const FVector& Offset : CelebrationOffsets)
    {
        if (auto* Component = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, CelebrationSystem,
            FinishFrame.TransformPositionNoScale(Offset), FinishFrame.Rotator(), CelebrationScale))
        {
            CelebrationComponents.Add(Component);
        }
    }
}

void UMWMatchResultsWidget::PrepareTally()
{
    int32 CheckpointCount = 0;
    for (const FMWPlayerRoundState& Result : Results)
    {
        CheckpointCount = FMath::Max(CheckpointCount, Result.CheckpointAwards.Num());
        DisplayedScores.Add(Result.TotalScore - Result.CheckpointScore - Result.FinishScore);
    }

    const auto AddStage = [this](const FText& Label, float Seconds, auto GetAward,
        bool bKeepEmpty = false, float FinalReadSeconds = 0.f)
    {
        FScoreStage Stage;
        Stage.Label = Label;
        Stage.Duration = FMath::IsFinite(Seconds) && Seconds > 0.f ? Seconds : StageSeconds;
        Stage.MotionSeconds = Stage.Duration * FMath::Clamp(AwardMotionFraction, 0.05f, 1.f);
        if (bFinalResults && FMath::IsFinite(FinalReadSeconds) && FinalReadSeconds > 0.f)
        {
            // 최종 완주와 보너스는 점수 이동이 끝난 뒤에 읽는 시간을 따로 부여한다.
            Stage.Duration = Stage.MotionSeconds + FinalReadSeconds;
        }
        for (const FMWPlayerRoundState& Result : Results)
        {
            const int32 Award = GetAward(Result);
            Stage.Awards.Add(Award);
            Stage.LargestAward = FMath::Max(Stage.LargestAward, Award);
        }
        if (Stage.LargestAward > 0 || bKeepEmpty)
        {
            ScoreStages.Add(MoveTemp(Stage));
        }
    };
    for (int32 Index = 0; Index < CheckpointCount; ++Index)
    {
        AddStage(FText::Format(CheckpointStageFormat, FText::AsNumber(Index + 1)), StageSeconds, [Index](const FMWPlayerRoundState& Result)
        {
            return Result.CheckpointAwards.IsValidIndex(Index) ? Result.CheckpointAwards[Index] : 0;
        }, true);
    }
    AddStage(CheckpointRemainderText, StageSeconds, [](const FMWPlayerRoundState& Result)
    {
        int32 Remainder = Result.CheckpointScore;
        for (const int32 Award : Result.CheckpointAwards)
        {
            Remainder -= Award;
        }
        return Remainder;
    });
    AddStage(FinishStageText, FinishStageSeconds,
        [](const FMWPlayerRoundState& Result) { return Result.FinishScore - Result.FirstFinishBonus; }, false, FinalFinishReadSeconds);
    AddStage(FirstFinishBonusText, BonusStageSeconds,
        [](const FMWPlayerRoundState& Result) { return Result.FirstFinishBonus; }, false, FinalBonusReadSeconds);
    if (!ScoreStages.IsEmpty() && !RoundIntroFormat.IsEmpty() && FMath::IsFinite(RoundIntroSeconds) && RoundIntroSeconds > 0.f)
    {
        FScoreStage Intro;
        Intro.Label = FText::Format(RoundIntroFormat, FText::AsNumber(RoundNumber));
        Intro.Duration = RoundIntroSeconds;
        Intro.MotionSeconds = RoundIntroSeconds;
        Intro.Awards.Init(0, Results.Num());
        ScoreStages.Insert(MoveTemp(Intro), 0);
    }
}

void UMWMatchResultsWidget::BeginTally(const AMWGameplayGameState& State)
{
    TallyPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
    const bool bExtendedFinal = FMath::IsFinite(FinalMaxTallySeconds) && FinalMaxTallySeconds > 0.f;
    const FMWGameplayProgress& Progress = State.GetProgressRef();
    // 마지막 라운드로 보이는 경우 짧은 서버 대기 구간을 집계 대기로 사용한다.
    // 최종 점수와 시상은 실제 MatchResults 복제를 받은 뒤에만 표시한다.
    if (bExtendedFinal && !bFinalResults && !ScoreStages.IsEmpty()
        && (Progress.RaceMode == EMWRaceMode::SuddenDeath || Algo::CountIf(Progress.Results,
            [&Progress](const FMWPlayerRoundState& Result) { return Result.TotalScore >= Progress.TargetScore; }) == 1))
    {
        Screen = EMWMatchResultsScreen::AwaitingFinalResults;
        TallyHeading->SetText(LOCTEXT("PreparingMatchTally", "경기 결과 집계 중"));
        ApplyTallyStartingScores();
        UE_LOG(LogMWMatchResults, Log, TEXT("World=%s Screen=AwaitingFinalResults"), *GetWorld()->GetOutermost()->GetName());
        return;
    }

    float TimeBudget = bFinalResults && bExtendedFinal ? FinalMaxTallySeconds : MaxTallySeconds;
    if (!bFinalResults)
    {
        TimeBudget = FMath::Min(TimeBudget, static_cast<float>(RoundEndServerTime - State.GetServerWorldTimeSeconds())
            - RoundPositionsHoldSeconds);
    }
    if (ScoreStages.IsEmpty() || !FMath::IsFinite(StageSeconds) || StageSeconds <= 0.f
        || !FMath::IsFinite(TimeBudget) || TimeBudget <= 0.f)
    {
        EndTally();
        return;
    }

    Screen = EMWMatchResultsScreen::Tally;
    if (AMWWinnerVehicleShowcase* Showcase = WinnerShowcase.Get()) Showcase->SetActorHiddenInGame(true);
    float RequestedSeconds = 0.f;
    for (const FScoreStage& Stage : ScoreStages) RequestedSeconds += Stage.Duration;
    // 서버 단계가 예약한 종료 시각에 맞추되 항목별 상대 길이와 멈춤은 유지한다.
    const float TimeScale = FMath::Min(1.f, TimeBudget / RequestedSeconds);
    for (FScoreStage& Stage : ScoreStages)
    {
        Stage.Duration *= TimeScale;
        Stage.MotionSeconds *= TimeScale;
    }
    ShowTallyStage();
    ApplyTallyStartingScores();
}

void UMWMatchResultsWidget::ApplyTallyStartingScores()
{
    for (int32 Index = 0; Index < Results.Num(); ++Index)
    {
        CastChecked<UMWMatchResultRowWidget>(TallyColumns->GetChildAt(Index))->SetTallyScore(DisplayedScores[Index], BarScaleScore);
    }
}

void UMWMatchResultsWidget::ShowTallyStage()
{
    const FScoreStage& Stage = ScoreStages[CurrentStage];
    TallyHeading->SetText(Stage.Label);
    UE_LOG(LogMWMatchResults, Log, TEXT("World=%s Screen=Tally Stage=%d/%d Label=%s"),
        *GetWorld()->GetOutermost()->GetName(), CurrentStage + 1, ScoreStages.Num(), *Stage.Label.ToString());
}

void UMWMatchResultsWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    if (Screen == EMWMatchResultsScreen::FinishShot)
    {
        FinishShotElapsed += DeltaTime;
        UpdatePodiumCamera(FinishShotElapsed);
        UpdateWinnerVehicle(FinishShotElapsed - (ShotSeconds - GetWinnerVehicleLead()));
        return;
    }
    if (Screen == EMWMatchResultsScreen::Winner)
    {
        UpdatePodium(DeltaTime);
        return;
    }
    if (Screen == EMWMatchResultsScreen::Standings && WinnerShowcase.IsValid())
    {
        FIntPoint Size = FIntPoint::ZeroValue;
        if (const APlayerController* Controller = GetOwningPlayer()) Controller->GetViewportSize(Size.X, Size.Y);
        if (Size != SettledViewportSize)
        {
            SettledViewportSize = Size;
            UpdateWinnerVehicle(GetWinnerVehicleLead() + GetPodiumDuration());
        }
        return;
    }
    if (Screen != EMWMatchResultsScreen::Tally)
    {
        return;
    }
    StageElapsed += DeltaTime;
    // 프레임이 밀려도 서버가 지급한 구간의 합은 모두 한 번씩 반영한다.
    while (StageElapsed >= ScoreStages[CurrentStage].Duration)
    {
        for (int32 Index = 0; Index < Results.Num(); ++Index)
        {
            DisplayedScores[Index] += ScoreStages[CurrentStage].Awards[Index];
        }
        StageElapsed -= ScoreStages[CurrentStage].Duration;
        if (++CurrentStage == ScoreStages.Num())
        {
            EndTally();
            return;
        }
        ShowTallyStage();
    }
    const FScoreStage& Stage = ScoreStages[CurrentStage];
    const float Alpha = FMath::Clamp(StageElapsed / Stage.MotionSeconds, 0.f, 1.f);
    // 같은 항목의 모든 열을 같은 속도로 올리고, 각 열의 실제 지급분에 닿으면 그 열만 멈춘다.
    const int32 AwardProgress = FMath::RoundToInt(Stage.LargestAward * Alpha);
    for (int32 Index = 0; Index < Results.Num(); ++Index)
    {
        const int32 Display = DisplayedScores[Index] + FMath::Min(Stage.Awards[Index], AwardProgress);
        CastChecked<UMWMatchResultRowWidget>(TallyColumns->GetChildAt(Index))->SetTallyScore(Display, BarScaleScore);
    }
}

void UMWMatchResultsWidget::EndTally()
{
    if (!bFinalResults || !ScoreStages.IsEmpty())
    {
        Screen = EMWMatchResultsScreen::RoundPositions;
        TallyHeading->SetText(RoundPositionsText.IsEmpty() ? LOCTEXT("RoundPositions", "누적 순위") : RoundPositionsText);
        // 열은 참가자 슬롯 순으로 둔다. 누적 점수 동점은 서버의 라운드 순위와 슬롯으로 가른다.
        TArray<const FMWPlayerRoundState*> Ranked;
        for (const FMWPlayerRoundState& Result : Results) Ranked.Add(&Result);
        Ranked.Sort([](const FMWPlayerRoundState& A, const FMWPlayerRoundState& B)
        {
            if (A.TotalScore != B.TotalScore) return A.TotalScore > B.TotalScore;
            if (A.RoundRank != B.RoundRank) return A.RoundRank < B.RoundRank;
            return A.MemberId < B.MemberId;
        });
        for (int32 Index = 0; Index < Results.Num(); ++Index)
        {
            auto* Column = CastChecked<UMWMatchResultRowWidget>(TallyColumns->GetChildAt(Index));
            Column->SetTallyScore(Results[Index].TotalScore, BarScaleScore);
            Column->ShowRank(bFinalResults ? Results[Index].FinalRank : Ranked.Find(&Results[Index]) + 1);
        }
        UE_LOG(LogMWMatchResults, Log, TEXT("World=%s Screen=RoundPositions"), *GetWorld()->GetOutermost()->GetName());
        if (!bFinalResults) return;
        // 확정 라운드 화면을 받지 못한 클라이언트도 집계를 건너뛰지 않고 최종 시상까지 이어 간다.
        const float HoldSeconds = FMath::IsFinite(FinalMaxTallySeconds) && FinalMaxTallySeconds > 0.f
            ? FinalPositionsHoldSeconds : RoundPositionsHoldSeconds;
        if (FMath::IsFinite(HoldSeconds) && HoldSeconds > 0.f)
        {
            GetWorld()->GetTimerManager().SetTimer(PresentationTimer, this, &ThisClass::BeginPodiumApproach,
                HoldSeconds, false);
            return;
        }
        BeginPodiumApproach();
        return;
    }
    BeginPodium();
}

void UMWMatchResultsWidget::BeginPodiumApproach()
{
    if (!FinishCamera)
    {
        BeginPodium();
        return;
    }

    TallyPanel->SetVisibility(ESlateVisibility::Collapsed);
    if (PodiumPanel && PodiumMedalEntrySeconds > 0.f && !PodiumMedalDelaySeconds.IsEmpty())
    {
        // 첫 메달이 제 좌표에서 들어오도록 결승 접근 중 투명하게 배치만 준비한다.
        PodiumPanel->SetRenderOpacity(0.f);
        PodiumPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    Screen = EMWMatchResultsScreen::FinishShot;
    // 결과 복제 순서와 관계없이 판 퇴장부터 카메라·차량·카드가 같은 시간을 쓴다.
    FinishShotElapsed = 0.f;
    UpdatePodiumCamera(0.f);
    UE_LOG(LogMWMatchResults, Log, TEXT("World=%s Screen=PodiumApproach"), *GetWorld()->GetOutermost()->GetName());
    GetWorld()->GetTimerManager().SetTimer(PresentationTimer, this, &ThisClass::BeginPodium, ShotSeconds, false);
}

void UMWMatchResultsWidget::BeginPodium()
{
    TallyPanel->SetVisibility(ESlateVisibility::Collapsed);
    PodiumPanel->SetRenderOpacity(PodiumPanelOpacity);
    PodiumPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    PlayCelebration();
    if (!FMath::IsFinite(WinnerSeconds) || WinnerSeconds <= 0.f || PodiumRevealOrder.IsEmpty())
    {
        ShowStandings();
        return;
    }

    Screen = EMWMatchResultsScreen::Winner;
    UpdatePodium(0.f);
    UE_LOG(LogMWMatchResults, Log, TEXT("World=%s Screen=Winner"), *GetWorld()->GetOutermost()->GetName());
}

void UMWMatchResultsWidget::UpdatePodium(float DeltaTime)
{
    const int32 Count = PodiumRevealOrder.Num();
    const float TotalSeconds = GetPodiumDuration();
    const float HoldSeconds = TotalSeconds - WinnerSeconds;
    RevealElapsed = FMath::Min(RevealElapsed + DeltaTime, TotalSeconds);
    UpdatePodiumCamera(ShotSeconds + RevealElapsed);
    UpdateWinnerVehicle(GetWinnerVehicleLead() + RevealElapsed);
    const float Stagger = GetPodiumCardStagger();
    const float CardSeconds = Stagger > 0.f ? WinnerSeconds - Stagger * (Count - 1) : WinnerSeconds / Count;
    // 이름이 펴지는 중에도 다음 순위의 메달과 판이 들어올 수 있다.
    for (int32 Index = 0; Index < Count; ++Index)
    {
        UMWMatchResultRowWidget* Card = PodiumRevealOrder[Index];
        const float Start = Stagger > 0.f ? Stagger * Index : Index == 0 ? 0.f : CardSeconds * Index + HoldSeconds;
        const float Elapsed = RevealElapsed - Start;
        const auto EntryAlpha = [&](float Fraction)
        {
            return FMath::SmoothStep(CardSeconds * FMath::Clamp(Fraction, 0.f, .95f), CardSeconds, Elapsed);
        };
        float MedalAlpha = EntryAlpha(0.f);
        if (PodiumMedalDelaySeconds.IsValidIndex(Index) && FMath::IsFinite(PodiumMedalEntrySeconds))
        {
            const float Delay = PodiumMedalDelaySeconds[Index];
            const float Duration = FMath::Min(PodiumMedalEntrySeconds, TotalSeconds - Delay);
            if (FMath::IsFinite(Delay) && Delay >= 0.f && Duration > UE_SMALL_NUMBER)
            {
                // 메달은 먼저 들어오고 끝에서 감속한다. 판과 이름은 기존 순위별 시간으로 재생한다.
                const float Progress = FMath::Clamp((RevealElapsed - Delay) / Duration, 0.f, 1.f);
                MedalAlpha = 1.f - FMath::Cube(1.f - Progress);
            }
        }
        const float PlateAlpha = EntryAlpha(PlateEntryFraction);
        FVector2D EntryStart = CardEntryOffset;
        FVector2D MedalStart = CardEntryOffset;
        const FGeometry& CardGeometry = Card->GetCachedGeometry();
        const FGeometry& ViewGeometry = GetCachedGeometry();
        if (RunnerUpCardHost && ThirdPlaceCardHost && CardGeometry.GetLocalSize().X > UE_SMALL_NUMBER
            && ViewGeometry.GetLocalSize().Y > UE_SMALL_NUMBER)
        {
            // 카드 로컬 좌표계에서 화면 왼쪽과 카드 폭을 구해 전체 판이 화면 밖에서 시작하게 한다.
            const FVector2D ViewLeft = CardGeometry.AbsoluteToLocal(ViewGeometry.GetAbsolutePosition());
            EntryStart.X = FMath::Min(EntryStart.X, ViewLeft.X - CardGeometry.GetLocalSize().X);
            const FVector2D ViewBottom = CardGeometry.AbsoluteToLocal(ViewGeometry.LocalToAbsolute(
                FVector2D(0.f, ViewGeometry.GetLocalSize().Y)));
            MedalStart = FVector2D(EntryStart.X, (ViewBottom.Y - ViewLeft.Y) * MedalEntryHeightFractions[Index]);
        }
        Card->SetPodiumEntry(MedalAlpha, PlateAlpha, EntryAlpha(DetailsEntryFraction),
            EntryAlpha(PortraitEntryFraction), EntryStart, MedalStart, &ViewGeometry);
    }
    if (RevealElapsed >= TotalSeconds)
    {
        ShowStandings();
    }
}

void UMWMatchResultsWidget::ShowStandings()
{
    UpdatePodiumCamera(ShotSeconds + GetPodiumDuration());
    UpdateWinnerVehicle(GetWinnerVehicleLead() + GetPodiumDuration());
    // 마지막 프레임과 즉시 표시 경로에서 모두 카드가 제자리에 남도록 한다.
    for (UMWMatchResultRowWidget* Card : PodiumRevealOrder)
    {
        Card->SetPodiumEntry(1.f, 1.f, 1.f, 1.f, FVector2D::ZeroVector, FVector2D::ZeroVector);
    }
    Screen = EMWMatchResultsScreen::Standings;
    UE_LOG(LogMWMatchResults, Log, TEXT("World=%s Screen=Standings WinnerVehicle=%d"),
        *GetWorld()->GetOutermost()->GetName(), WinnerShowcase.IsValid());
}

bool UMWMatchResultsWidget::RestoreInput(APlayerController& Controller)
{
    if (!bFinalResults || Screen == EMWMatchResultsScreen::Inactive || &Controller != GetOwningPlayer())
    {
        return false;
    }
    const auto* Arrival = Controller.GetGameInstance()->GetSubsystem<UMWArrivalScreen>();
    if (Arrival && Arrival->IsCovered())
    {
        return false;
    }
    FInputModeUIOnly Mode;
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    Mode.SetWidgetToFocus(TakeWidget());
    Controller.SetInputMode(Mode);
    Controller.bShowMouseCursor = false;
    return true;
}

void UMWMatchResultsWidget::Dismiss()
{
    ClearPresentation();
    RemoveFromParent();
}

void UMWMatchResultsWidget::ClearPresentation()
{
    Screen = EMWMatchResultsScreen::Inactive;
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(PresentationTimer);
    }
    for (const TWeakObjectPtr<UNiagaraComponent>& Component : CelebrationComponents)
    {
        if (Component.IsValid())
        {
            Component->DestroyComponent();
        }
    }
    CelebrationComponents.Reset();
    if (AMWWinnerVehicleShowcase* Showcase = WinnerShowcase.Get())
    {
        Showcase->Destroy();
    }
    WinnerShowcase.Reset();
    if (WinnerVehicleClassLoad && WinnerVehicleClassLoad->IsLoadingInProgress()) WinnerVehicleClassLoad->CancelHandle();
    WinnerVehicleClassLoad.Reset();
    WinnerVehicleClass.Reset();
    if (IsValid(FinishCamera))
    {
        if (APlayerController* Controller = GetOwningPlayer(); Controller && Controller->GetViewTarget() == FinishCamera)
        {
            AActor* Target = PreviousViewTarget.IsValid() ? PreviousViewTarget.Get() : Controller->GetPawn();
            Controller->SetViewTarget(IsValid(Target) ? Target : Controller);
        }
        FinishCamera->Destroy();
    }
    FinishCamera = nullptr;
    PreviousViewTarget.Reset();
}

#undef LOCTEXT_NAMESPACE
