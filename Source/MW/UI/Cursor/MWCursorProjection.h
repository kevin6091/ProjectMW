#pragma once

#include "CoreMinimal.h"

class APlayerController;
class UUserWidget;
class UWidget;
class UWidgetComponent;

namespace MWCursorProjection
{
	MW_API bool SampleNormalized(const UUserWidget& Widget, const UWidget& ReferenceLayer,
		const UWidgetComponent* Surface, APlayerController& PC, FVector2D& OutPosition);
	// 평면 WidgetComponent의 DrawSize/Pivot/Transform 규약. 임의 카메라 하드코딩 없음.
	MW_API FVector WidgetPointToWorld(const UWidgetComponent& Surface, const FVector2D& DrawPosition);
	MW_API bool ToViewport(const UUserWidget& Widget, const UWidget& ReferenceLayer,
		const UWidgetComponent* Surface, const FVector2D& NormalizedPosition, APlayerController& PC, FVector2D& OutPosition);
}
