#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MWCoursePickupComponent.generated.h"

class AMWPickupBox;
class APlayerStart;
struct FMWTrackPlacementSession;

// 일반 경기의 서버 GameMode가 이번 라운드 상자의 생성과 제거를 소유한다.
UCLASS()
class UMWCoursePickupComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    void Prepare(const FMWTrackPlacementSession& Course, TConstArrayView<APlayerStart*> PlayerStarts);
    void Clear();

private:
    TArray<TWeakObjectPtr<AMWPickupBox>> Boxes;
};
