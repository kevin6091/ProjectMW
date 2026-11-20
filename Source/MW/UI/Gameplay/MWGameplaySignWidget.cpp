#include "UI/Gameplay/MWGameplaySignWidget.h"
#include "UI/Gameplay/MWGameplaySign.h"
#include "Widgets/Layout/SBox.h"

TSharedRef<SWidget> UMWGameplaySignWidget::RebuildWidget()
{
    return SAssignNew(Surface, SBox);
}

void UMWGameplaySignWidget::SynchronizeProperties()
{
    Super::SynchronizeProperties();
    // UMG가 속성을 다시 적용할 때도 같은 Slate 호스트에서 최신 Designer 값으로 다시 그린다.
    if (Surface)
        Surface->SetContent(SNew(SMWGameplaySign).Label(Label).AnimateLoading(bLoading)
            .FaceColor(FaceColor).EdgeColor(EdgeColor).EdgeThickness(EdgeThickness).FontSize(FontSize));
}

void UMWGameplaySignWidget::ReleaseSlateResources(bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    Surface.Reset();
}
