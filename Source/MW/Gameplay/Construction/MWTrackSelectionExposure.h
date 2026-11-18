#pragma once

#include "Camera/CameraModifier.h"
#include "GameplayTagContainer.h"
#include "Math/Interval.h"
#include "MWTrackSelectionExposure.generated.h"

USTRUCT()
struct FMWTrackSelectionExposureRange
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTag Theme;

	UPROPERTY()
	FFloatInterval EV100;
};

// 선택판이 쓰는 노출 범위를 카메라의 후처리 레이어로 적용하고, 시점이 바뀌면 가중치를 낮춘다.
UCLASS(Config = Game)
class UMWTrackSelectionExposure : public UCameraModifier
{
	GENERATED_BODY()

public:
	virtual bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;

private:
	UPROPERTY(Config)
	TArray<FMWTrackSelectionExposureRange> ThemeRanges;

	UPROPERTY(Config)
	float ReleaseSeconds = 0.f;

	FGameplayTag ActiveTheme;
	FFloatInterval ActiveRange;
	float Weight = 0.f;
	bool bHasRange = false;
};
