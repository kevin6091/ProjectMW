#pragma once

#include "Components/Widget.h"
#include "MWItemGlassWidget.generated.h"

class UMaterialInterface;

// Designer에서 슬롯 유리의 색·그림자 비율과 선택적 UI 재질을 지정한다.
UCLASS(meta=(DisplayName="MW Item Glass"))
class MW_API UMWItemGlassWidget : public UWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category="Appearance") FLinearColor Tint = FLinearColor::White;
    UPROPERTY(EditAnywhere, Category="Appearance") TObjectPtr<UMaterialInterface> Material;
    UPROPERTY(EditAnywhere, Category="Appearance", meta=(ClampMin="1",ClampMax="2"))
    float ShadowHeightRatio = 176.f / 168.f;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void SynchronizeProperties() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
private:
    TSharedPtr<class SImage> Image;
    FSlateBrush MaterialBrush;
    TSharedPtr<FSlateBrush> VectorBrush;
};
