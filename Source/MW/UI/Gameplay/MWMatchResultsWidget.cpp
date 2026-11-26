#include "UI/Gameplay/MWMatchResultsWidget.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/Construction/MWTrackConstructionComponent.h"
#include "Gameplay/Track/MWTrackRuntimeSettings.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "System/GameState/MWGameplayGameState.h"
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
    if (ScoreBar) ScoreBar->SetFillColorAndOpacity(Color);
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
    if (RankLabel)
    {
        RankLabel->SetVisibility(Rank > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
        if (Rank > 0) RankLabel->SetText(FText::Format(LOCTEXT("Rank", "{0}위"), FText::AsNumber(Rank)));
        if (UWidgetLayoutLibrary::SlotAsCanvasSlot(RankLabel))
        {
            NameLabel->SetVisibility(Rank > 0 ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
        }
    }
}

void UMWMatchResultRowWidget::SetPodiumDetailsOpacity(float Opacity)
{
    NameLabel->SetRenderOpacity(Opacity);
    TotalScoreLabel->SetRenderOpacity(Opacity);
    if (VehiclePortrait) VehiclePortrait->SetRenderOpacity(Opacity);
}

void UMWMatchResultRowWidget::SetTallyScore(int32 Score, int32 Target)
{
    if (ShownScore.IsSet() && ShownScore.GetValue() == Score)
    {
        return;
    }
    ShownScore = Score;
    FNumberFormattingOptions ScoreFormat;
    ScoreFormat.SetUseGrouping(ScoreDigits <= 1).SetMinimumIntegralDigits(FMath::Clamp(ScoreDigits, 1, 10));
    TotalScoreLabel->SetText(FText::AsNumber(Score, &ScoreFormat));
    if (ScoreBar)
    {
        const float Fraction = Target > 0 ? FMath::Clamp(static_cast<float>(Score) / Target, 0.f, 1.f) : 0.f;
        ScoreBar->SetPercent(Fraction);
        // 차량 배지와 점수는 한 슬롯에서 움직인다. 기존 WBP에서는 점수 글자 슬롯을 사용한다.
        UWidget* ScoreMarker = TallyScoreMarker ? TallyScoreMarker.Get() : TotalScoreLabel.Get();
        for (UWidget* Marker : {static_cast<UWidget*>(NameLabel.Get()), static_cast<UWidget*>(RankLabel.Get()), ScoreMarker})
        {
            if (auto* MarkerSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(Marker))
            {
                FAnchors Anchors = MarkerSlot->GetAnchors();
                Anchors.Minimum.Y = Anchors.Maximum.Y = 1.f - Fraction;
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
        || (bFinalResults && (!PodiumPanel || !PodiumCards || !PodiumCardClass))
        || (!bFinalResults && Progress.Phase != EMWRoomPhase::RoundResults) || Progress.Results.IsEmpty()
        || Progress.RoundEpoch <= 0 || Progress.ResultsEpoch != Progress.RoundEpoch)
    {
        return false;
    }

    TMap<int32, const AMWPlayerState*> Players;
    for (const APlayerState* Base : State.PlayerArray)
    {
        if (const auto* Player = Cast<AMWPlayerState>(Base))
        {
            Players.Add(Player->GetLobbyMemberId(), Player);
        }
    }

    Results = Progress.Results;
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
        // 이전 버전의 WBP는 1위 전용 슬롯 없이도 전체 순위를 보여 준다.
        if (WinnerCardHost) WinnerCardHost->SetVisibility(ESlateVisibility::Collapsed);
        for (const FMWPlayerRoundState& Result : Progress.Results)
        {
            if (Result.FinalRank < 1 || Result.FinalRank > 3) continue;
            auto* PodiumCard = CreateEntry(PodiumCardClass, Result);
            if (!PodiumCard) return false;
            if (WinnerCardHost && Result.FinalRank == 1)
            {
                WinnerCardHost->SetVisibility(ESlateVisibility::HitTestInvisible);
                WinnerCardHost->SetContent(PodiumCard);
            }
            else
            {
                PodiumCards->AddChildToVerticalBox(PodiumCard);
            }
            PodiumRevealOrder.Add(PodiumCard);
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
    if (PodiumPanel) PodiumPanel->SetVisibility(ESlateVisibility::Collapsed);
    if (ContinueButton) ContinueButton->SetVisibility(ESlateVisibility::Collapsed);
    if (!AddToPlayerScreen())
    {
        return false;
    }
    // 라운드 집계도 결승을 배경으로 즉시 시작한다. 카메라 단독 연출은 매치 종료 때만 기다린다.
    if (BeginFinishShot(State) && bFinalResults)
    {
        Screen = EMWMatchResultsScreen::FinishShot;
        RestoreInput(*Controller);
    }
    else
    {
        BeginTally();
        RestoreInput(*Controller);
    }
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
    const FVector CameraLocation = FinishFrame.TransformPositionNoScale(CameraOffset);
    const FVector Direction = FinishFrame.TransformPositionNoScale(LookAtOffset) - CameraLocation;
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
    const float Blend = FMath::IsFinite(CameraBlendSeconds) ? FMath::Clamp(CameraBlendSeconds, 0.f, ShotSeconds) : 0.f;
    Controller->SetViewTargetWithBlend(FinishCamera, Blend, VTBlend_EaseInOut, 0.f, true);

    if (bFinalResults)
    {
        GetWorld()->GetTimerManager().SetTimer(PresentationTimer, this, &ThisClass::BeginTally, ShotSeconds, false);
    }
    UE_LOG(LogMWMatchResults, Log, TEXT("World=%s Screen=FinishShot Piece=%d Epoch=%d Camera=%s"),
        *GetWorld()->GetOutermost()->GetName(), Finish.PieceId, Progress.ResultsEpoch, *CameraLocation.ToCompactString());
    return true;
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

    const auto AddStage = [this](const FText& Label, float Seconds, auto GetAward, bool bKeepEmpty = false)
    {
        FScoreStage Stage;
        Stage.Label = Label;
        Stage.Duration = FMath::IsFinite(Seconds) && Seconds > 0.f ? Seconds : StageSeconds;
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
    AddStage(FinishStageText, FinishStageSeconds, [](const FMWPlayerRoundState& Result) { return Result.FinishScore - Result.FirstFinishBonus; });
    AddStage(FirstFinishBonusText, BonusStageSeconds, [](const FMWPlayerRoundState& Result) { return Result.FirstFinishBonus; });
    if (!ScoreStages.IsEmpty() && !RoundIntroFormat.IsEmpty() && FMath::IsFinite(RoundIntroSeconds) && RoundIntroSeconds > 0.f)
    {
        FScoreStage Intro;
        Intro.Label = FText::Format(RoundIntroFormat, FText::AsNumber(RoundNumber));
        Intro.Duration = RoundIntroSeconds;
        Intro.Awards.Init(0, Results.Num());
        ScoreStages.Insert(MoveTemp(Intro), 0);
    }
}

void UMWMatchResultsWidget::BeginTally()
{
    float TimeBudget = MaxTallySeconds;
    if (!bFinalResults)
    {
        const auto* State = GetWorld()->GetGameState<AMWGameplayGameState>();
        const double Now = State ? State->GetServerWorldTimeSeconds() : RoundEndServerTime;
        TimeBudget = FMath::Min(TimeBudget, static_cast<float>(RoundEndServerTime - Now) - RoundPositionsHoldSeconds);
    }
    TallyPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
    if (ScoreStages.IsEmpty() || !FMath::IsFinite(StageSeconds) || StageSeconds <= 0.f
        || !FMath::IsFinite(TimeBudget) || TimeBudget <= 0.f)
    {
        EndTally();
        return;
    }

    Screen = EMWMatchResultsScreen::Tally;
    float RequestedSeconds = 0.f;
    for (const FScoreStage& Stage : ScoreStages) RequestedSeconds += Stage.Duration;
    // 서버 단계가 예약한 종료 시각에 맞추되 항목별 상대 길이와 멈춤은 유지한다.
    const float TimeScale = FMath::Min(1.f, TimeBudget / RequestedSeconds);
    for (FScoreStage& Stage : ScoreStages) Stage.Duration *= TimeScale;
    ShowTallyStage();
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
    if (Screen == EMWMatchResultsScreen::Winner)
    {
        UpdatePodium(DeltaTime);
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
    const float MotionSeconds = Stage.Duration * FMath::Clamp(AwardMotionFraction, 0.05f, 1.f);
    const float Alpha = FMath::Clamp(StageElapsed / MotionSeconds, 0.f, 1.f);
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
        if (FMath::IsFinite(RoundPositionsHoldSeconds) && RoundPositionsHoldSeconds > 0.f)
        {
            GetWorld()->GetTimerManager().SetTimer(PresentationTimer, this, &ThisClass::BeginPodium, RoundPositionsHoldSeconds, false);
            return;
        }
    }
    BeginPodium();
}

void UMWMatchResultsWidget::BeginPodium()
{
    TallyPanel->SetVisibility(ESlateVisibility::Collapsed);
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
    const float HoldSeconds = Count > 1 && FMath::IsFinite(FirstPlaceHoldSeconds)
        ? FMath::Max(FirstPlaceHoldSeconds, 0.f) : 0.f;
    const float TotalSeconds = WinnerSeconds + HoldSeconds;
    RevealElapsed = FMath::Min(RevealElapsed + DeltaTime, TotalSeconds);
    const float CardSeconds = WinnerSeconds / Count;
    // 1위가 자리를 잡은 뒤 혼자 보이는 시간을 확보하고 나머지를 서버 순위대로 연다.
    for (int32 Index = 0; Index < Count; ++Index)
    {
        UMWMatchResultRowWidget* Card = PodiumRevealOrder[Index];
        const float Start = Index == 0 ? 0.f : CardSeconds * Index + HoldSeconds;
        const float Alpha = FMath::SmoothStep(0.f, CardSeconds, RevealElapsed - Start);
        Card->SetRenderOpacity(Alpha);
        Card->SetRenderTranslation(CardEntryOffset * (1.f - Alpha));
        const float DetailsStart = CardSeconds * FMath::Clamp(DetailsEntryFraction, 0.f, .95f);
        Card->SetPodiumDetailsOpacity(FMath::SmoothStep(DetailsStart, CardSeconds, RevealElapsed - Start));
    }
    if (RevealElapsed >= TotalSeconds)
    {
        ShowStandings();
    }
}

void UMWMatchResultsWidget::ShowStandings()
{
    // 마지막 프레임과 즉시 표시 경로에서 모두 카드가 제자리에 남도록 한다.
    for (UMWMatchResultRowWidget* Card : PodiumRevealOrder)
    {
        Card->SetRenderOpacity(1.f);
        Card->SetPodiumDetailsOpacity(1.f);
        Card->SetRenderTranslation(FVector2D::ZeroVector);
    }
    Screen = EMWMatchResultsScreen::Standings;
    UE_LOG(LogMWMatchResults, Log, TEXT("World=%s Screen=Standings"), *GetWorld()->GetOutermost()->GetName());
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
