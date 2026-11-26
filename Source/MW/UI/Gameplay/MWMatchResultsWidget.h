#pragma once

#include "Blueprint/UserWidget.h"
#include "Engine/TimerHandle.h"
#include "System/MWRoomTypes.h"
#include "MWMatchResultsWidget.generated.h"

class ACameraActor;
class AMWGameplayGameState;
class UBorder;
class UButton;
class UHorizontalBox;
class UImage;
class UNiagaraComponent;
class UNiagaraSystem;
class UProgressBar;
class UTextBlock;
class UTexture2D;
class UVerticalBox;

UENUM(BlueprintType)
enum class EMWMatchResultsScreen : uint8
{
    Inactive,
    FinishShot,
    Tally,
    RoundPositions,
    Winner,
    Standings,
    Overview
};

// 집계 열·순위 카드·개요 행은 동일한 참가자 데이터를 각 WBP의 바인딩으로 표시한다.
UCLASS(Abstract)
class MW_API UMWMatchResultRowWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetResult(const FMWPlayerRoundState& Result, const FText& PlayerName, int32 Rank, UTexture2D* Portrait);
    void SetTallyScore(int32 Score, int32 Target);
    void ShowRank(bool bShow);

private:
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> MemberAccent;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> CardBackground;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> RankLabel;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> NameLabel;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> TotalScoreLabel;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> CheckpointLabel;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> FinishLabel;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> ScoreBar;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> VehiclePortrait;

    TOptional<int32> ShownScore;
};

// 경기 단계는 서버가 소유한다. 이 위젯은 각 창의 결승선 연출과 두 결과 화면을 관리한다.
UCLASS(Abstract)
class MW_API UMWMatchResultsWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    bool Present(const AMWGameplayGameState& State, bool bPlayTally);
    bool HasPresentedRoundTally() const { return Screen == EMWMatchResultsScreen::RoundPositions; }
    bool RestoreInput(APlayerController& Controller);
    void Dismiss();

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

private:
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> ResultsPanel;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> MatchContext;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UVerticalBox> ResultRows;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> ContinueButton;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> TallyPanel;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TallyHeading;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UHorizontalBox> TallyColumns;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> PodiumPanel;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UVerticalBox> PodiumCards;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> WinnerCardHost;
    UPROPERTY(Transient) TArray<TObjectPtr<UMWMatchResultRowWidget>> PodiumRevealOrder;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Results")
    TSubclassOf<UMWMatchResultRowWidget> ResultRowClass;
    UPROPERTY(EditDefaultsOnly, Category = "MW|Results")
    TSubclassOf<UMWMatchResultRowWidget> TallyColumnClass;
    UPROPERTY(EditDefaultsOnly, Category = "MW|Results")
    TSubclassOf<UMWMatchResultRowWidget> PodiumCardClass;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Finish Shot", meta = (Units = "s", ClampMin = "0"))
    float ShotSeconds = 0.f;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Finish Shot", meta = (Units = "s", ClampMin = "0"))
    float CameraBlendSeconds = 0.f;

    // 결승 소켓 기준의 카메라 위치와 시선이다. 맵 좌표나 특정 테마에 의존하지 않는다.
    UPROPERTY(EditDefaultsOnly, Category = "MW|Finish Shot", meta = (Units = "cm"))
    FVector CameraOffset = FVector::ZeroVector;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Finish Shot", meta = (Units = "cm"))
    FVector LookAtOffset = FVector::ZeroVector;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Finish Shot", meta = (Units = "deg", ClampMin = "0", ClampMax = "179"))
    float CameraFOV = 0.f;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Finish Shot")
    TObjectPtr<UNiagaraSystem> CelebrationSystem;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Finish Shot")
    TArray<FVector> CelebrationOffsets;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Finish Shot")
    FVector CelebrationScale = FVector::OneVector;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Tally")
    FText RoundPositionsText;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Tally")
    FText CheckpointStageFormat;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Tally")
    FText CheckpointRemainderText;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Tally")
    FText FinishStageText;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Tally")
    FText FirstFinishBonusText;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Tally", meta = (Units = "s", ClampMin = "0"))
    float StageSeconds = 0.f;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Tally", meta = (Units = "s", ClampMin = "0"))
    float MaxTallySeconds = 0.f;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Tally", meta = (Units = "s", ClampMin = "0"))
    float RoundPositionsHoldSeconds = 0.f;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Winner", meta = (Units = "s", ClampMin = "0"))
    float WinnerSeconds = 0.f;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Winner")
    FVector2D CardEntryOffset = FVector2D::ZeroVector;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "MW|Results", meta = (AllowPrivateAccess = "true"))
    EMWMatchResultsScreen Screen = EMWMatchResultsScreen::Inactive;

    UPROPERTY(Transient) TObjectPtr<ACameraActor> FinishCamera;
    TWeakObjectPtr<AActor> PreviousViewTarget;
    TArray<TWeakObjectPtr<UNiagaraComponent>> CelebrationComponents;
    FTransform FinishFrame;
    FTimerHandle PresentationTimer;

    struct FScoreStage
    {
        FText Label;
        TArray<int32> Awards;
    };

    TArray<FMWPlayerRoundState> Results;
    TArray<FScoreStage> ScoreStages;
    TArray<int32> DisplayedScores;
    bool bFinalResults = false;
    int32 BarScaleScore = 0;
    int32 CurrentStage = 0;
    float StageDuration = 0.f;
    float StageElapsed = 0.f;
    float RevealElapsed = 0.f;
    double RoundEndServerTime = 0.0;

    bool BeginFinishShot(const AMWGameplayGameState& State);
    void PlayCelebration();
    void PrepareTally();
    void BeginTally();
    void ShowTallyStage();
    void EndTally();
    void UpdatePodium(float DeltaTime);
    void ShowStandings();
    void ClearPresentation();

    UFUNCTION() void HandleContinue();
};
