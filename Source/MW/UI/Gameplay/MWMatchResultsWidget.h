#pragma once

#include "Blueprint/UserWidget.h"
#include "Engine/TimerHandle.h"
#include "System/MWRoomTypes.h"
#include "MWMatchResultsWidget.generated.h"

class ACameraActor;
class AMWGameplayGameState;
class UBorder;
class UHorizontalBox;
class UImage;
class UNiagaraComponent;
class UNiagaraSystem;
class UProgressBar;
class UTextBlock;
class UTexture2D;
class UVerticalBox;
class UWidgetSwitcher;

UENUM(BlueprintType)
enum class EMWMatchResultsScreen : uint8
{
    Inactive,
    FinishShot,
    Tally,
    RoundPositions,
    Winner,
    Standings
};

// 집계 열과 순위 카드는 동일한 참가자 데이터를 각 WBP의 바인딩으로 표시한다.
UCLASS(Abstract)
class MW_API UMWMatchResultRowWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetResult(const FMWPlayerRoundState& Result, const FText& PlayerName, int32 Rank, UTexture2D* Portrait);
    void SetTallyScore(int32 Score, int32 Target);
    void ShowRank(int32 Rank);
    void SetPodiumEntry(float MedalAlpha, float PlateAlpha, float NameAlpha, float PortraitAlpha,
        const FVector2D& PlateStart, const FVector2D& MedalStart);

private:
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> MemberAccent;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> CardBackground;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> RankLabel;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> RankSuffix;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> NameLabel;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> TotalScoreLabel;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> ScoreBar;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> VehiclePortrait;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> TallyScoreMarker;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> TallyVehicleBadge;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidgetSwitcher> RankMedals;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> PodiumMedal;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Tally", meta = (ClampMin = "1", ClampMax = "10"))
    int32 ScoreDigits = 1;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Tally", meta = (ClampMin = "0", ClampMax = "1"))
    float MarkerMinimumFill = 0.f;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Winner")
    FVector2D PortraitEntryOffset = FVector2D::ZeroVector;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Winner", meta = (ClampMin = "0.1", ClampMax = "1"))
    float InitialNameScale = 1.f;

    TOptional<int32> ShownScore;
};

// 경기 단계는 서버가 소유한다. 이 위젯은 각 창의 점수판과 결승선 연출을 관리한다.
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
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;

private:
    // 이전 버전의 WBP를 사용 중인 작업 공간에서도 종료된 화면과 입력을 숨긴다.
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> ResultsPanel;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> ContinueButton;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> TallyPanel;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TallyHeading;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UHorizontalBox> TallyColumns;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> PodiumPanel;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UVerticalBox> PodiumCards;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> WinnerCardHost;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> RunnerUpCardHost;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> ThirdPlaceCardHost;
    UPROPERTY(Transient) TArray<TObjectPtr<UMWMatchResultRowWidget>> PodiumRevealOrder;

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

    // 가려졌을 때만 순서대로 평가하는 결승 소켓 기준 대체 시점(cm)이다.
    UPROPERTY(EditDefaultsOnly, Category = "MW|Finish Shot")
    TArray<FVector> AlternateCameraOffsets;

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

    UPROPERTY(EditDefaultsOnly, Category = "MW|Tally")
    FText RoundIntroFormat;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Tally", meta = (Units = "s", ClampMin = "0"))
    float RoundIntroSeconds = 0.f;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Tally", meta = (ClampMin = "0.05", ClampMax = "1"))
    float AwardMotionFraction = 0.6f;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Tally", meta = (Units = "s", ClampMin = "0"))
    float FinishStageSeconds = 0.f;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Tally", meta = (Units = "s", ClampMin = "0"))
    float BonusStageSeconds = 0.f;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Tally", meta = (Units = "s", ClampMin = "0"))
    float StageSeconds = 0.f;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Tally", meta = (Units = "s", ClampMin = "0"))
    float MaxTallySeconds = 0.f;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Tally", meta = (Units = "s", ClampMin = "0"))
    float RoundPositionsHoldSeconds = 0.f;

    // 각 카드의 진입에 배분할 시간이다. 1위 카드가 자리를 잡은 뒤의 정지 시간은 별도다.
    UPROPERTY(EditDefaultsOnly, Category = "MW|Winner", meta = (Units = "s", ClampMin = "0"))
    float WinnerSeconds = 0.f;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Winner", meta = (Units = "s", ClampMin = "0"))
    float FirstPlaceHoldSeconds = 0.f;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Winner")
    FVector2D CardEntryOffset = FVector2D::ZeroVector;

    // 금·은·동 메달의 시작 높이 차이를 뷰포트 높이 비율로 편집한다.
    UPROPERTY(EditDefaultsOnly, Category = "MW|Winner", meta = (ClampMin = "-1", ClampMax = "1"))
    FVector MedalEntryHeightFractions = FVector::ZeroVector;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Winner", meta = (ClampMin = "0", ClampMax = "0.95"))
    float DetailsEntryFraction = 0.35f;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Winner", meta = (ClampMin = "0", ClampMax = "0.95"))
    float PlateEntryFraction = 0.f;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Winner", meta = (ClampMin = "0", ClampMax = "0.95"))
    float PortraitEntryFraction = 0.35f;

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
        float Duration = 0.f;
        int32 LargestAward = 0;
    };

    TArray<FMWPlayerRoundState> Results;
    TArray<FScoreStage> ScoreStages;
    TArray<int32> DisplayedScores;
    bool bFinalResults = false;
    int32 BarScaleScore = 0;
    int32 RoundNumber = 0;
    int32 CurrentStage = 0;
    float StageElapsed = 0.f;
    float RevealElapsed = 0.f;
    double RoundEndServerTime = 0.0;

    bool BeginFinishShot(const AMWGameplayGameState& State);
    void PlayCelebration();
    void PrepareTally();
    void BeginTally();
    void ShowTallyStage();
    void EndTally();
    void BeginPodium();
    void UpdatePodium(float DeltaTime);
    void ShowStandings();
    void ClearPresentation();
};
