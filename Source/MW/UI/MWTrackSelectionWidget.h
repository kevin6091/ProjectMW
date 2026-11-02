#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MWTrackSelectionWidget.generated.h"

class UTextBlock;

// 선택 현황을 표시하는 로컬 패널. 서버 상태 해석과 문자열 구성은 Controller가 담당한다.
UCLASS()
class MW_API UMWTrackSelectionWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    virtual void NativeOnInitialized() override;
    void SetSelectionText(const FText& Players, const FText& Status);
private:
    UPROPERTY(Transient) TObjectPtr<UTextBlock> PlayersText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
};
