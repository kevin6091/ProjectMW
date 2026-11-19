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
    if (Surface)
        Surface->SetContent(SNew(SMWGameplaySign).Label(Label).AnimateLoading(bLoading)
            .FaceColor(FaceColor).EdgeColor(EdgeColor).EdgeThickness(EdgeThickness).FontSize(FontSize));
}

void UMWGameplaySignWidget::ReleaseSlateResources(bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    Surface.Reset();
}
