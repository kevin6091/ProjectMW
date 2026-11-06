#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MWRemoteCursorOverlay.generated.h"

class UCanvasPanel;
class UScaleBox;

// 표시 화면이 공급하는 모양의 크기와 포인터 끝 위치. 특정 커서/에셋을 가정하지 않는다.
struct FMWCursorVisualLayout
{
	FVector2D DesignSize = FVector2D::ZeroVector;
	FVector2D DisplaySize = FVector2D::ZeroVector;
	FVector2D Hotspot = FVector2D::ZeroVector;
};

// 화면 위 표시와 보간만 담당한다. 네트워크·차량·트랙 상태를 조회하지 않는다.
UCLASS()
class MW_API UMWRemoteCursorOverlay : public UUserWidget
{
	GENERATED_BODY()

public:
	void BeginCursorFrame();
	UUserWidget* FindCursorVisual(int32 PlayerId) const;
	void PresentCursor(int32 PlayerId, int32 StreamId, FVector2D Target, UUserWidget* Visual,
		const FMWCursorVisualLayout& Layout, float DeltaTime);
	void EndCursorFrame();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> CursorCanvas;
	UPROPERTY(Transient) TMap<int32, TObjectPtr<UScaleBox>> CursorHosts;
	struct FDisplayState
	{
		int32 StreamId = 0;
		FVector2D Position = FVector2D::ZeroVector;
	};
	TMap<int32, FDisplayState> DisplayStates;
	TSet<int32> SeenPlayers;
};
