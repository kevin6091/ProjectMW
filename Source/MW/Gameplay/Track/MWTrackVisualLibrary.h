#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "MWTrackVisualLibrary.generated.h"

class AActor;

UCLASS()
class MW_API UMWTrackVisualLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** 로컬 미리보기 트랙의 표시 모드를 바꾸고 요청한 모드 지원 여부를 반환한다. 전용 와이어프레임이 없으면 일반 메시를 표시하고 false를 반환한다. */
    UFUNCTION(BlueprintCallable, Category = "MW|Track|Visual")
    static bool SetTrackWireframePreview(AActor* TrackVisual, bool bEnableWireframe);
};
