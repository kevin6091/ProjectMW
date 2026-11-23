#pragma once

#include "CoreMinimal.h"

class APlayerController;
class UUserWidget;
class UWidget;
class UWidgetComponent;

namespace MWCursorProjection
{
	// 활성 뷰포트 안의 포인터를 역투영에 사용하는 픽셀 좌표로 읽는다.
	MW_API bool SampleViewportPointer(const APlayerController& PC, FVector2D& OutPosition);
	// 화면 포인터의 광선과 주어진 평면이 만나는 월드 위치를 구한다. 실패하면 표시를 숨길 수 있다.
	MW_API bool SampleWorldPlane(const APlayerController& PC, const FVector& Point, const FVector& Normal, FVector& OutPosition);
	MW_API bool SampleNormalized(const UUserWidget& Widget, const UWidget& ReferenceLayer,
		const UWidgetComponent* Surface, APlayerController& PC, FVector2D& OutPosition);
	// 평면 WidgetComponent의 DrawSize/Pivot/Transform 규약. 임의 카메라 하드코딩 없음.
	MW_API FVector WidgetPointToWorld(const UWidgetComponent& Surface, const FVector2D& DrawPosition);
	MW_API bool ToViewport(const UUserWidget& Widget, const UWidget& ReferenceLayer,
		const UWidgetComponent* Surface, const FVector2D& NormalizedPosition, APlayerController& PC, FVector2D& OutPosition);
}
