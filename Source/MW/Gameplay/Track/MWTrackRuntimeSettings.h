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

    // 시작 BP의 ㄷ자 표식 컴포넌트를 참가자 배정 순서대로 연결한다.
    UPROPERTY(Config, EditAnywhere, Category = "Start Grid")
    TArray<FName> StartSlotMarkers;

    // 실제 신호등의 빨간 행(위에서부터)과 초록 행을 바인딩한다.
    UPROPERTY(Config, EditAnywhere, Category = "Start Signal")
    TArray<FName> RedSignalRows;

    UPROPERTY(Config, EditAnywhere, Category = "Start Signal")
    FName GreenSignalRow;

    UPROPERTY(Config, EditAnywhere, Category = "Start Signal")
    FName SignalIntensityParameter;

    UPROPERTY(Config, EditAnywhere, Category = "Start Signal")
    FName SignalColorParameter;

    UPROPERTY(Config, EditAnywhere, Category = "Start Signal", meta = (Units = "s", ClampMin = "0.1"))
    float StartSignalStepSeconds = 1.f;

    UPROPERTY(Config, EditAnywhere, Category = "Start Signal", meta = (ClampMin = "0", ClampMax = "1"))
    float UnlitSignalColorScale = 0.12f;

    // 시작 그리드의 로컬 축으로 카메라와 초점을 지정한다.
    UPROPERTY(Config, EditAnywhere, Category = "Start Camera")
    FVector StartCameraOffset = FVector::ZeroVector;

    UPROPERTY(Config, EditAnywhere, Category = "Start Camera")
    FVector StartCameraFocus = FVector::ZeroVector;

    UPROPERTY(Config, EditAnywhere, Category = "Start Camera", meta = (Units = "s", ClampMin = "0.01"))
    float StartCameraBlendSeconds = 0.45f;

    // 현재 시작·종료 BP를 바꿔도 코스 전용 조각이 일반 후보에 섞이지 않게 한다.
    UPROPERTY(Config, EditAnywhere, Category = "Tracks")
    TArray<TSoftClassPtr<AActor>> CourseOnlyTracks;

    // 현재 테마의 평면 바닥과 서버 건설 하한이 같은 월드 높이를 사용한다.
    UPROPERTY(Config, EditAnywhere, Category = "Placement", meta = (Units = "cm"))
    double TrackPlacementFloorZ = 0.0;

    // 전원에게 공개한 후보 준비 실패 뒤 동시에 들어온 재시도 요청을 한 번만 처리한다.
    UPROPERTY(Config, EditAnywhere, Category = "Placement", meta = (Units = "s", ClampMin = "0.1"))
    float ReselectionRetryCooldown = 1.0f;
};
