#pragma once

#include "CoreMinimal.h"

class AActor;
class UMaterialInterface;

namespace MWTrackPreview
{
	// 목적지에 실제로 표시하는 메시의 등록·렌더 상태와 각 슬롯의 재질을 확인한다.
	bool CollectVisibleMaterials(const AActor* Track, TArray<UMaterialInterface*>& OutMaterials);

	// 전시 액터를 스폰하지 않고 BP의 Wireframes 하위에서 실제 슬롯 재질을 수집한다.
	void CollectWireframeMaterials(UClass* TrackClass, TSet<UMaterialInterface*>& OutMaterials);

	// 전시용 Actor에서만 Wireframes 하위 메시를 표시하고 보이는 메시의 월드 범위를 반환한다.
	FBox ShowWireframes(AActor& Track);
	// 선택판·배치 고스트는 확정 트랙으로 승격되지 않고 폐기된다.
	bool IsDisplayOnly(const AActor& Track);

	void HideWireframes(AActor& Track);
	void SetWireframeColor(AActor& Track, const FLinearColor& Color);
}
