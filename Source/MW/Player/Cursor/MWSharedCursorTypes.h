#pragma once

#include "CoreMinimal.h"
#include "MWSharedCursorTypes.generated.h"

// 좌표의 의미는 화면이 정한다. 공통 전송 계층은 UI나 트랙을 참조하지 않는다.
UENUM(BlueprintType)
enum class EMWCursorSpace : uint8
{
	NormalizedWidget,
	World
};

namespace MWCursorContexts
{
	inline const FName VehicleSelection(TEXT("VehicleSelection"));
}

// 서버가 연 스트림의 최신 표시 상태. 차량 선택 결과/게임 판정과 독립적이다.
USTRUCT(BlueprintType)
struct MW_API FMWSharedCursorState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FName Context;
	UPROPERTY(BlueprintReadOnly) int32 StreamId = 0;
	UPROPERTY(BlueprintReadOnly) EMWCursorSpace Space = EMWCursorSpace::NormalizedWidget;
	// NormalizedWidget: XY=[0,1], Z=0. World: 월드 cm. UI 정밀도를 보존한다.
	UPROPERTY(BlueprintReadOnly) FVector Position = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) bool bVisible = false;

	bool IsOpen() const { return StreamId > 0 && !Context.IsNone(); }
	bool IsValidPosition(const FVector& Value) const;
};
