#include "UI/Gameplay/MWMatchResultsWidget.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
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
    if (ScoreBar) ScoreBar->SetFillColorAndOpacity(Color);
    if (RankLabel)
    {
        RankLabel->SetText(Rank > 0 ? FText::Format(LOCTEXT("Rank", "{0}위"), FText::AsNumber(Rank))
            : LOCTEXT("Spectating", "관전"));
    }
    if (VehiclePortrait)
    {
        VehiclePortrait->SetBrushFromTexture(Portrait, true);
        VehiclePortrait->SetVisibility(Portrait ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    }
    NameLabel->SetText(PlayerName);
    TotalScoreLabel->SetText(FText::AsNumber(Result.TotalScore));
    if (CheckpointLabel)
        CheckpointLabel->SetText(FText::Format(LOCTEXT("CheckpointResult", "{0}개 · +{1}"),
            FText::AsNumber(Result.CheckpointsPassed), FText::AsNumber(Result.CheckpointScore)));
    if (FinishLabel)
        FinishLabel->SetText(FText::Format(LOCTEXT("FinishBonus", "+{0}"), FText::AsNumber(Result.FinishScore)));
}

void UMWMatchResultRowWidget::ShowRank(bool bShow)
{
    if (RankLabel)
    {
        RankLabel->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    }
}

void UMWMatchResultRowWidget::SetTallyScore(int32 Score, int32 Target)
{
    if (ShownScore.IsSet() && ShownScore.GetValue() == Score)
    {
        return;
    }
    ShownScore = Score;
    TotalScoreLabel->SetText(FText::AsNumber(Score));
    if (ScoreBar)
    {
        ScoreBar->SetPercent(Target > 0 ? FMath::Clamp(static_cast<float>(Score) / Target, 0.f, 1.f) : 0.f);
    }
}

void UMWMatchResultsWidget::NativeConstruct()
{
    Super::NativeConstruct();
    ContinueButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleContinue);
}

void UMWMatchResultsWidget::NativeDestruct()
{
    ContinueButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleContinue);
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
        || (bFinalResults && (!PodiumPanel || !PodiumCards || !ResultRowClass || !PodiumCardClass))
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
    BarScaleScore = Progress.TargetScore;
    RoundEndServerTime = Progress.PhaseEndServerTime;
    const auto CreateEntry = [&](TSubclassOf<UMWMatchResultRowWidget> EntryClass, const FMWPlayerRoundState& Result, bool bPortrait)
    {
        auto* Entry = CreateWidget<UMWMatchResultRowWidget>(Controller, EntryClass);
        if (Entry)
        {
            const AMWPlayerState* Player = Players.FindRef(Result.MemberId);
            const FText Name = Player && !Player->GetPlayerName().IsEmpty() ? FText::AsCultureInvariant(Player->GetPlayerName())
                : FText::Format(LOCTEXT("UnknownPlayer", "플레이어 {0}"), FText::AsNumber(Result.MemberId));
            UTexture2D* Portrait = bPortrait && Player
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
        auto* Column = CreateEntry(TallyColumnClass, Result, false);
        if (!Column) return false;
        Column->ShowRank(false);
        auto* ColumnSlot = TallyColumns->AddChildToHorizontalBox(Column);
        ColumnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        BarScaleScore = FMath::Max(BarScaleScore, Result.TotalScore);
    }
    if (bFinalResults)
    {
        for (const FMWPlayerRoundState& Result : Progress.Results)
        {
            auto* OverviewRow = CreateEntry(ResultRowClass, Result, false);
            auto* PodiumCard = CreateEntry(PodiumCardClass, Result, true);
            if (!OverviewRow || !PodiumCard) return false;
            ResultRows->AddChildToVerticalBox(OverviewRow);
            PodiumCards->AddChildToVerticalBox(PodiumCard);
        }
    }
    for (const FMWPlayerRoundState& Result : Progress.Results)
    {
        UE_LOG(LogMWMatchResults, Log, TEXT("World=%s Epoch=%d Member=%d Rank=%d Total=%d Checkpoints=%d CheckpointScore=%d FinishScore=%d"),
            *GetWorld()->GetOutermost()->GetName(), Progress.ResultsEpoch, Result.MemberId, Result.FinalRank,
            Result.TotalScore, Result.CheckpointsPassed, Result.CheckpointScore, Result.FinishScore);
    }
    MatchContext->SetText(FText::Format(LOCTEXT("MatchContext", "최종 라운드 {0}  ·  목표 {1}점"),
        FText::AsNumber(Progress.RoundIndex), FText::AsNumber(Progress.TargetScore)));
    if (bPlayTally)
    {
        PrepareTally();
    }

    ResultsPanel->SetVisibility(ESlateVisibility::Collapsed);
    TallyPanel->SetVisibility(ESlateVisibility::Collapsed);
    if (PodiumPanel) PodiumPanel->SetVisibility(ESlateVisibility::Collapsed);
    ContinueButton->SetVisibility(ESlateVisibility::Collapsed);
    if (!AddToPlayerScreen())
    {
        return false;
    }
    if (bFinalResults && BeginFinishShot(State))
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

    GetWorld()->GetTimerManager().SetTimer(PresentationTimer, this, &ThisClass::BeginTally, ShotSeconds, false);
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

    const auto AddStage = [this](const FText& Label, auto GetAward)
    {
        FScoreStage Stage;
        Stage.Label = Label;
        bool bHasAward = false;
        for (const FMWPlayerRoundState& Result : Results)
        {
            const int32 Award = GetAward(Result);
            Stage.Awards.Add(Award);
            bHasAward |= Award > 0;
        }
        if (bHasAward)
        {
            ScoreStages.Add(MoveTemp(Stage));
        }
    };
    for (int32 Index = 0; Index < CheckpointCount; ++Index)
    {
        AddStage(FText::Format(CheckpointStageFormat, FText::AsNumber(Index + 1)), [Index](const FMWPlayerRoundState& Result)
        {
            return Result.CheckpointAwards.IsValidIndex(Index) ? Result.CheckpointAwards[Index] : 0;
        });
    }
    AddStage(CheckpointRemainderText, [](const FMWPlayerRoundState& Result)
    {
        int32 Remainder = Result.CheckpointScore;
        for (const int32 Award : Result.CheckpointAwards)
        {
            Remainder -= Award;
        }
        return Remainder;
    });
    AddStage(FinishStageText, [](const FMWPlayerRoundState& Result) { return Result.FinishScore - Result.FirstFinishBonus; });
    AddStage(FirstFinishBonusText, [](const FMWPlayerRoundState& Result) { return Result.FirstFinishBonus; });
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
    if (ScoreStages.IsEmpty() || StageSeconds <= 0.f || TimeBudget <= 0.f)
    {
        EndTally();
        return;
    }

    Screen = EMWMatchResultsScreen::Tally;
    StageDuration = FMath::Min(StageSeconds, TimeBudget / ScoreStages.Num());
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
    while (StageElapsed >= StageDuration)
    {
        for (int32 Index = 0; Index < Results.Num(); ++Index)
        {
            DisplayedScores[Index] += ScoreStages[CurrentStage].Awards[Index];
        }
        StageElapsed -= StageDuration;
        if (++CurrentStage == ScoreStages.Num())
        {
            EndTally();
            return;
        }
        ShowTallyStage();
    }
    const float Alpha = FMath::SmoothStep(0.f, StageDuration, StageElapsed);
    for (int32 Index = 0; Index < Results.Num(); ++Index)
    {
        const int32 Display = DisplayedScores[Index] + FMath::RoundToInt(ScoreStages[CurrentStage].Awards[Index] * Alpha);
        CastChecked<UMWMatchResultRowWidget>(TallyColumns->GetChildAt(Index))->SetTallyScore(Display, BarScaleScore);
    }
}

void UMWMatchResultsWidget::EndTally()
{
    if (!bFinalResults)
    {
        Screen = EMWMatchResultsScreen::RoundPositions;
        TallyHeading->SetText(RoundPositionsText);
        for (int32 Index = 0; Index < Results.Num(); ++Index)
        {
            auto* Column = CastChecked<UMWMatchResultRowWidget>(TallyColumns->GetChildAt(Index));
            Column->SetTallyScore(Results[Index].TotalScore, BarScaleScore);
            Column->ShowRank(true);
        }
        return;
    }

    TallyPanel->SetVisibility(ESlateVisibility::Collapsed);
    PodiumPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    PlayCelebration();
    if (WinnerSeconds <= 0.f)
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
    RevealElapsed = FMath::Min(RevealElapsed + DeltaTime, WinnerSeconds);
    const int32 Count = PodiumCards->GetChildrenCount();
    const float Reveal = RevealElapsed / WinnerSeconds * Count;
    for (int32 Index = 0; Index < Count; ++Index)
    {
        UWidget* Card = PodiumCards->GetChildAt(Index);
        const float Alpha = FMath::SmoothStep(0.f, 1.f, Reveal - Index);
        Card->SetRenderOpacity(Alpha);
        Card->SetRenderTranslation(CardEntryOffset * (1.f - Alpha));
    }
    if (RevealElapsed >= WinnerSeconds)
    {
        ShowStandings();
    }
}

void UMWMatchResultsWidget::ShowStandings()
{
    Screen = EMWMatchResultsScreen::Standings;
    ContinueButton->SetVisibility(ESlateVisibility::Visible);
    if (auto* Controller = GetOwningPlayer())
    {
        RestoreInput(*Controller);
    }
    UE_LOG(LogMWMatchResults, Log, TEXT("World=%s Screen=Standings"), *GetWorld()->GetOutermost()->GetName());
}

FReply UMWMatchResultsWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    // 마우스 버튼과 Slate의 표준 확인 키가 같은 로컬 전환을 사용한다.
    if (Screen == EMWMatchResultsScreen::Standings && !Event.IsRepeat()
        && FSlateApplication::Get().GetNavigationActionFromKey(Event) == EUINavigationAction::Accept)
    {
        HandleContinue();
        return FReply::Handled();
    }
    return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

void UMWMatchResultsWidget::HandleContinue()
{
    if (Screen != EMWMatchResultsScreen::Standings)
    {
        return;
    }
    Screen = EMWMatchResultsScreen::Overview;
    PodiumPanel->SetVisibility(ESlateVisibility::Collapsed);
    ResultsPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    if (auto* Controller = GetOwningPlayer())
    {
        RestoreInput(*Controller);
    }
    UE_LOG(LogMWMatchResults, Log, TEXT("World=%s Screen=Overview"), *GetWorld()->GetOutermost()->GetName());
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
    Mode.SetWidgetToFocus(Screen == EMWMatchResultsScreen::Standings ? ContinueButton->TakeWidget() : TakeWidget());
    Controller.SetInputMode(Mode);
    const bool bContinueAvailable = Screen == EMWMatchResultsScreen::Standings;
    if (bContinueAvailable)
    {
        ContinueButton->SetUserFocus(&Controller);
    }
    Controller.bShowMouseCursor = bContinueAvailable;
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
