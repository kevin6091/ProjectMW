#pragma once

#include "Components/Widget.h"
#include "MWGameplaySignWidget.generated.h"

// WBP에서 크기·문구·색과 로딩 애니메이션을 편집하는 공통 벡터 표지판이다.
UCLASS(meta=(DisplayName="MW Gameplay Sign"))
class MW_API UMWGameplaySignWidget : public UWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category="Appearance") FText Label;
    UPROPERTY(EditAnywhere, Category="Appearance") bool bLoading = false;
    UPROPERTY(EditAnywhere, Category="Appearance") FColor FaceColor = FColor(255,162,22);
    UPROPERTY(EditAnywhere, Category="Appearance") FColor EdgeColor = FColor(50,43,31);
    UPROPERTY(EditAnywhere, Category="Appearance", meta=(ClampMin="0", ClampMax=".4"))
    float EdgeThickness = .042f;
    UPROPERTY(EditAnywhere, Category="Appearance", meta=(ClampMin="1"))
    float FontSize = 54.f;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void SynchronizeProperties() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
private:
    TSharedPtr<class SBox> Surface;
};
