#include "UI/MWTrackSelectionWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"

void UMWTrackSelectionWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (!WidgetTree->RootWidget)
    {
        UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
        WidgetTree->RootWidget = Canvas;
        UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
        Panel->SetBrushColor(FLinearColor(0.025f, 0.035f, 0.055f, 0.92f));
        Panel->SetPadding(FMargin(18));
        UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Panel);
        CanvasSlot->SetPosition(FVector2D(24, 24));
        CanvasSlot->SetAutoSize(true);
        UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
        Panel->SetContent(Box);
        UTextBlock* Heading = WidgetTree->ConstructWidget<UTextBlock>();
        Heading->SetText(FText::FromString(TEXT("트랙을 선택하세요")));
        FSlateFontInfo Font = Heading->GetFont();
        Font.Size = 26;
        Heading->SetFont(Font);
        Box->AddChildToVerticalBox(Heading)->SetPadding(FMargin(0, 0, 0, 12));
        PlayersText = WidgetTree->ConstructWidget<UTextBlock>();
        Box->AddChildToVerticalBox(PlayersText)->SetPadding(FMargin(0, 0, 0, 12));
        StatusText = WidgetTree->ConstructWidget<UTextBlock>();
        StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.85f, 1.f)));
        StatusText->SetAutoWrapText(true);
        StatusText->SetWrapTextAt(440.f);
        Box->AddChildToVerticalBox(StatusText);
    }
    SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UMWTrackSelectionWidget::SetSelectionText(const FText& Players, const FText& Status)
{
    if (PlayersText) PlayersText->SetText(Players);
    if (StatusText) StatusText->SetText(Status);
}
