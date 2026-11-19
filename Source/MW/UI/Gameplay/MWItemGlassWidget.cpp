#include "UI/Gameplay/MWItemGlassWidget.h"
#include "Brushes/SlateImageBrush.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"
#include "Widgets/Images/SImage.h"

TSharedRef<SWidget> UMWItemGlassWidget::RebuildWidget()
{
    VectorBrush = MakeShared<FSlateVectorImageBrush>(FPaths::ProjectContentDir() / TEXT("UI/Common/Slate/ItemSlotGlass.svg"), FVector2D(168,176));
    return SAssignNew(Image, SImage);
}
void UMWItemGlassWidget::SynchronizeProperties()
{
    Super::SynchronizeProperties();
    if (!Image) return;
    MaterialBrush.SetResourceObject(Material);
    Image->SetImage(Material ? &MaterialBrush : VectorBrush.Get());
    Image->SetColorAndOpacity(Tint);
    // 그림자가 슬롯 하단으로만 늘어나도록 위쪽을 기준점으로 둔다.
    Image->SetRenderTransformPivot(FVector2D::ZeroVector);
    Image->SetRenderTransform(FSlateRenderTransform(FScale2D(1.f, ShadowHeightRatio)));
}
void UMWItemGlassWidget::ReleaseSlateResources(bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    Image.Reset();
    VectorBrush.Reset();
}
