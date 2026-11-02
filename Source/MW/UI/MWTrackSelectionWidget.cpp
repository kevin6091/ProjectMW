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
    // 첫 화면 구성 전에 네이티브 WidgetTree를 만든다. 별도 Widget BP 없이 사용할 수 있다.
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
        // 제목·참가자 현황·요청 상태를 세로로 배치하고, 내용 변경 시 TextBlock만 갱신한다.
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
    // 안내 패널이 커서 입력을 가로채 선택판 조작을 방해하지 않도록 한다.
    SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UMWTrackSelectionWidget::SetSelectionText(const FText& Players, const FText& Status)
{
    if (PlayersText) PlayersText->SetText(Players);
    if (StatusText) StatusText->SetText(Status);
}
