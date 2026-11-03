#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MWTrackSelectionWidget.generated.h"

class UTextBlock;
class UCanvasPanel;

// 선택 현황을 표시하는 로컬 패널. 서버 상태 해석과 문자열 구성은 Controller가 담당한다.
UCLASS()
class MW_API UMWTrackSelectionWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    virtual void NativeOnInitialized() override;
    void SetSelectionText(const FText& Players, const FText& Status);
    // 현재 선택과 일치하는 원격 커서만 읽어 월드 위치를 보간하고 화면에 투영한다.
    void UpdateRemoteCursors(int32 SessionId, int32 Revision, float DeltaTime);
private:
    UPROPERTY(Transient) TObjectPtr<UTextBlock> PlayersText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
    UPROPERTY(Transient) TObjectPtr<UCanvasPanel> CursorCanvas;
    // 참가자 번호별 로컬 표시다. PlayerState를 강하게 참조하지 않고 퇴장 시 Widget만 제거한다.
    UPROPERTY(Transient) TMap<int32, TObjectPtr<UTextBlock>> RemoteCursorLabels;
    TMap<int32, FVector> SmoothedCursorPositions;
};
