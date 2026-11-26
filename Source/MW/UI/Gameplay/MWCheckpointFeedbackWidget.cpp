#include "UI/Gameplay/MWCheckpointFeedbackWidget.h"

#include "Components/TextBlock.h"

void UMWCheckpointFeedbackWidget::NativeConstruct()
{
    Super::NativeConstruct();
    Clear();
}

void UMWCheckpointFeedbackWidget::Present(int32 CheckpointNumber, int32 EarnedPoints)
{
    if (!CheckpointLabel || !AwardLabel || HoldSeconds + ExitSeconds <= 0.f)
    {
        return;
    }
    CheckpointLabel->SetText(FText::Format(CheckpointFormat, FText::AsNumber(CheckpointNumber)));
    AwardLabel->SetText(FText::Format(AwardFormat, FText::AsNumber(EarnedPoints)));
    AwardLabel->SetVisibility(EarnedPoints > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    Elapsed = 0.f;
    SetRenderOpacity(1.f);
    SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UMWCheckpointFeedbackWidget::Clear()
{
    SetVisibility(ESlateVisibility::Collapsed);
}

void UMWCheckpointFeedbackWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    if (GetVisibility() == ESlateVisibility::Collapsed)
    {
        return;
    }
    Elapsed += DeltaTime;
    if (Elapsed >= HoldSeconds + ExitSeconds)
    {
        Clear();
    }
    else if (Elapsed > HoldSeconds)
    {
        SetRenderOpacity(1.f - FMath::SmoothStep(0.f, ExitSeconds, Elapsed - HoldSeconds));
    }
}
