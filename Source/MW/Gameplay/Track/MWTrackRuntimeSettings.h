#pragma once

#include "Engine/DeveloperSettings.h"
#include "MWTrackRuntimeSettings.generated.h"

class AActor;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "MW Track Runtime"))
class MW_API UMWTrackRuntimeSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    // 모든 맵에서 사용하는 시작·종료 BP다. 두 클래스는 후보 추첨에서도 제외한다.
    UPROPERTY(Config, EditAnywhere, Category = "Tracks")
    TSoftClassPtr<AActor> StartTrackClass;

    UPROPERTY(Config, EditAnywhere, Category = "Tracks")
    TSoftClassPtr<AActor> FinishTrackClass;

    // 현재 시작·종료 BP를 바꿔도 코스 전용 조각이 일반 후보에 섞이지 않게 한다.
    UPROPERTY(Config, EditAnywhere, Category = "Tracks")
    TArray<TSoftClassPtr<AActor>> CourseOnlyTracks;

    // 현재 테마의 평면 바닥과 서버 건설 하한이 같은 월드 높이를 사용한다.
    UPROPERTY(Config, EditAnywhere, Category = "Placement", meta = (Units = "cm"))
    double TrackPlacementFloorZ = 0.0;

    // 열린 끝에 확정 조각의 어느 방향도 들어갈 수 없을 때만 제공한다.
    UPROPERTY(Config, EditAnywhere, Category = "Placement")
    TSoftClassPtr<AActor> BlockedPlacementFallbackClass;
};
