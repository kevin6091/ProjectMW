#include "UI/Cursor/MWRemoteCursorOverlay.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"

TSharedRef<SWidget> UMWRemoteCursorOverlay::RebuildWidget()
{
	// NativeConstruct는 Slate 루트가 만들어진 뒤 호출된다. 그 전에 표시 트리를 준비한다.
	if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		CursorCanvas = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = CursorCanvas;
	}
	return Super::RebuildWidget();
}

void UMWRemoteCursorOverlay::NativeConstruct()
{
	Super::NativeConstruct();
	// 전체 오버레이가 입력을 통과시킨다. 로컬 OS/소프트웨어 커서 매핑을 변경하지 않는다.
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UMWRemoteCursorOverlay::BeginCursorFrame()
{
	SeenPlayers.Reset();
}

UUserWidget* UMWRemoteCursorOverlay::FindCursorVisual(int32 PlayerId) const
{
	const UScaleBox* Host = CursorHosts.FindRef(PlayerId);
	const USizeBox* DesignBox = Host ? Cast<USizeBox>(Host->GetContent()) : nullptr;
	return DesignBox ? Cast<UUserWidget>(DesignBox->GetContent()) : nullptr;
}

void UMWRemoteCursorOverlay::PresentCursor(int32 PlayerId, int32 StreamId, FVector2D Target,
	UUserWidget* Visual, const FMWCursorVisualLayout& Layout, float DeltaTime)
{
	if (!CursorCanvas || !IsValid(Visual) || Target.ContainsNaN() || Layout.DesignSize.ContainsNaN()
		|| Layout.DisplaySize.ContainsNaN() || Layout.Hotspot.ContainsNaN()
		|| Layout.DesignSize.X <= 0 || Layout.DesignSize.Y <= 0
		|| Layout.DisplaySize.X <= 0 || Layout.DisplaySize.Y <= 0) return;
	UScaleBox* Host = CursorHosts.FindRef(PlayerId);
	if (!Host)
	{
		Host = WidgetTree->ConstructWidget<UScaleBox>();
		Host->SetStretch(EStretch::ScaleToFit);
		USizeBox* DesignBox = WidgetTree->ConstructWidget<USizeBox>();
		UScaleBoxSlot* ContentSlot = CastChecked<UScaleBoxSlot>(Host->SetContent(DesignBox));
		ContentSlot->SetHorizontalAlignment(HAlign_Center);
		ContentSlot->SetVerticalAlignment(VAlign_Center);
		CursorCanvas->AddChildToCanvas(Host);
		CursorHosts.Add(PlayerId, Host);
	}
	USizeBox* DesignBox = CastChecked<USizeBox>(Host->GetContent());
	DesignBox->SetWidthOverride(Layout.DesignSize.X);
	DesignBox->SetHeightOverride(Layout.DesignSize.Y);
	if (DesignBox->GetContent() != Visual) DesignBox->SetContent(Visual);
	Visual->SetVisibility(ESlateVisibility::HitTestInvisible);
	SeenPlayers.Add(PlayerId);

	FDisplayState* Previous = DisplayStates.Find(PlayerId);
	FDisplayState Next;
	Next.StreamId = StreamId;
	Next.Position = Previous && Previous->StreamId == StreamId
		? FMath::Vector2DInterpTo(Previous->Position, Target, DeltaTime, 18.f) : Target;
	DisplayStates.Add(PlayerId, Next);
	UCanvasPanelSlot* HostSlot = CastChecked<UCanvasPanelSlot>(Host->Slot);
	HostSlot->SetSize(Layout.DisplaySize);
	// ScaleToFit의 여백까지 포함해 시각 커서의 끝을 네트워크 위치에 맞춘다.
	const double Scale = FMath::Min(Layout.DisplaySize.X / Layout.DesignSize.X, Layout.DisplaySize.Y / Layout.DesignSize.Y);
	const FVector2D FittedSize = Layout.DesignSize * Scale;
	const FVector2D PointerOffset = (Layout.DisplaySize - FittedSize) * 0.5 + Layout.Hotspot * FittedSize;
	HostSlot->SetAlignment(PointerOffset / Layout.DisplaySize);
	HostSlot->SetPosition(Next.Position);
}

void UMWRemoteCursorOverlay::EndCursorFrame()
{
	for (auto It = CursorHosts.CreateIterator(); It; ++It)
	{
		if (SeenPlayers.Contains(It.Key())) continue;
		if (It.Value()) It.Value()->RemoveFromParent();
		DisplayStates.Remove(It.Key());
		It.RemoveCurrent();
	}
}
