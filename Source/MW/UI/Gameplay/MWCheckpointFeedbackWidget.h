#pragma once

#include "Blueprint/UserWidget.h"
#include "MWCheckpointFeedbackWidget.generated.h"

class UTextBlock;

// 체크포인트의 번호와 점수만 받아 표시한다. 판면과 시간은 WBP에서 편집한다.
UCLASS(Abstract)
class MW_API UMWCheckpointFeedbackWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void Present(int32 CheckpointNumber, int32 EarnedPoints);
    void Clear();

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> CheckpointLabel;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> AwardLabel;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Checkpoint")
    FText CheckpointFormat;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Checkpoint")
    FText AwardFormat;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Checkpoint", meta = (ClampMin = "0", Units = "s"))
    float HoldSeconds = 0.f;

    UPROPERTY(EditDefaultsOnly, Category = "MW|Checkpoint", meta = (ClampMin = "0", Units = "s"))
    float ExitSeconds = 0.f;

private:
    float Elapsed = 0.f;
};
