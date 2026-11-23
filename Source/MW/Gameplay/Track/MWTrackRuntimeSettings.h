#pragma once

#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"
#include "MWTrackRuntimeSettings.generated.h"

class AActor;
class UMWTrackRuntimeData;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "MW Track Runtime"))
class UMWTrackRuntimeSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    // 후보 목록과 설치 후 주행 설정의 공용 카탈로그다.
    UPROPERTY(Config, EditAnywhere, Category = "Tracks")
    TSoftObjectPtr<UMWTrackRuntimeData> RuntimeCatalog;

    // 카탈로그의 클래스 이름에 대응하는 BP 패키지가 있는 공용 폴더다.
    UPROPERTY(Config, EditAnywhere, Category = "Tracks", meta = (LongPackageName))
    FDirectoryPath TrackBlueprintDirectory;

    // 실제 코스 양 끝은 GameMode가 선택한다. 이 목록은 무작위 후보 추첨에서만 제외한다.
    UPROPERTY(Config, EditAnywhere, Category = "Tracks")
    TArray<TSoftClassPtr<AActor>> CourseOnlyTracks;
};
