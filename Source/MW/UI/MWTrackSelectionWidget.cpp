#include "UI/MWTrackSelectionWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Gameplay/Construction/MWPlayerConstructionComponent.h"
#include "System/MWGameState.h"
#include "System/MWPlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

void UMWTrackSelectionWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    // 첫 화면 구성 전에 네이티브 WidgetTree를 만든다. 별도 Widget BP 없이 사용할 수 있다.
    if (!WidgetTree->RootWidget)
    {
        UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
        WidgetTree->RootWidget = Canvas;
        CursorCanvas = Canvas;
        // 화면 밖으로 투영된 배지가 다른 화면 영역까지 그려지지 않도록 한다.
        Canvas->SetClipping(EWidgetClipping::ClipToBounds);
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

void UMWTrackSelectionWidget::UpdateRemoteCursors(int32 SessionId, int32 Revision, float DeltaTime)
{
    APlayerController* PC = GetOwningPlayer();
    const AMWGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMWGameState>() : nullptr;
    if (!CursorCanvas || !PC || !GS) return;
    const APlayerState* OwnPS = PC->GetPlayerState<APlayerState>();
    TSet<int32> PresentPlayers;
    const FLinearColor Colors[] = {
        FLinearColor(0.2f, 0.85f, 1.f), FLinearColor(1.f, 0.55f, 0.2f),
        FLinearColor(0.5f, 1.f, 0.35f), FLinearColor(1.f, 0.4f, 0.8f)
    };

    for (const APlayerState* BasePS : GS->PlayerArray)
    {
        const AMWPlayerState* PS = Cast<AMWPlayerState>(BasePS);
        if (!PS || PS == OwnPS || PS->GetLobbyMemberId() <= 0) continue;
        const int32 Id = PS->GetLobbyMemberId();
        PresentPlayers.Add(Id);
        UTextBlock* Label = RemoteCursorLabels.FindRef(Id);
        const UMWPlayerConstructionComponent* Personal = PS->GetPlayerConstructionComponent();
        const FMWPlayerConstructionCursor RemoteCursor = Personal ? Personal->GetCursor() : FMWPlayerConstructionCursor();
        if (!RemoteCursor.bIsVisible || RemoteCursor.SelectionSessionId != SessionId || RemoteCursor.OffersRevision != Revision)
        {
            if (Label) Label->SetVisibility(ESlateVisibility::Collapsed);
            SmoothedCursorPositions.Remove(Id);
            continue;
        }

        if (!Label)
        {
            // 아이콘 에셋 없이 '+'를 커서 위치 표식으로 사용하고 참가자 번호·이름을 붙인다.
            Label = WidgetTree->ConstructWidget<UTextBlock>();
            FSlateFontInfo Font = Label->GetFont();
            Font.Size = 20;
            Label->SetFont(Font);
            Label->SetColorAndOpacity(FSlateColor(Colors[(Id - 1) % UE_ARRAY_COUNT(Colors)]));
            Label->SetShadowColorAndOpacity(FLinearColor::Black);
            Label->SetShadowOffset(FVector2D(1, 1));
            UCanvasPanelSlot* CursorSlot = CursorCanvas->AddChildToCanvas(Label);
            CursorSlot->SetAutoSize(true);
            CursorSlot->SetZOrder(100);
            RemoteCursorLabels.Add(Id, Label);
        }

        // 20Hz의 수신 위치를 프레임마다 보간한다. 처음 보이거나 다시 나타날 때는 목표 위치에서 시작한다.
        FVector* Previous = SmoothedCursorPositions.Find(Id);
        const FVector Position = Previous
            ? FMath::VInterpTo(*Previous, FVector(RemoteCursor.WorldPosition), DeltaTime, 18.f)
            : FVector(RemoteCursor.WorldPosition);
        SmoothedCursorPositions.Add(Id, Position);
        FVector2D ScreenPosition;
        // DPI/품질 스케일이 반영된 Widget 좌표를 사용한다. AddToViewport의 전체 화면 기준이다.
        if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Position, ScreenPosition, false))
        {
            Label->SetVisibility(ESlateVisibility::Collapsed);
            continue;
        }
        if (UCanvasPanelSlot* CursorSlot = Cast<UCanvasPanelSlot>(Label->Slot))
            CursorSlot->SetPosition(ScreenPosition);
        Label->SetText(FText::FromString(FString::Printf(TEXT("+ P%d %s"), Id, *PS->GetPlayerName())));
        Label->SetVisibility(ESlateVisibility::HitTestInvisible);
    }

    // 퇴장한 사람의 마지막 커서와 Widget을 남기지 않는다. 자기 커서는 기본 마우스 커서가 표시한다.
    for (auto It = RemoteCursorLabels.CreateIterator(); It; ++It)
    {
        if (PresentPlayers.Contains(It.Key())) continue;
        if (It.Value()) It.Value()->RemoveFromParent();
        SmoothedCursorPositions.Remove(It.Key());
        It.RemoveCurrent();
    }
}
