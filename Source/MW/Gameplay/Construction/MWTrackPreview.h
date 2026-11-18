#pragma once

#include "CoreMinimal.h"

class AActor;

namespace MWTrackPreview
{
	// 전시용 Actor에서만 Wireframes 하위 메시를 표시하고 보이는 메시의 월드 범위를 반환한다.
	FBox ShowWireframes(AActor& Track);
	// 선택판·배치 고스트는 확정 트랙으로 승격되지 않고 폐기된다.
	bool IsDisplayOnly(const AActor& Track);

	void HideWireframes(AActor& Track);
	void SetWireframeColor(AActor& Track, const FLinearColor& Color);
}
