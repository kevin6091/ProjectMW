#pragma once

#include "CoreMinimal.h"

class AActor;

namespace MWTrackPreview
{
	// 전시용 Actor에서만 Wireframes 하위 메시를 표시하고 보이는 메시의 월드 범위를 반환한다.
	FBox ShowWireframes(AActor& Track);
	void HideWireframes(AActor& Track);
	void SetWireframeColor(AActor& Track, const FLinearColor& Color);
}
