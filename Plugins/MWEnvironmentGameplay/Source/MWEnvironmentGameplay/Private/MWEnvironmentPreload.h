#pragma once

#include "Subsystems/GameInstanceSubsystem.h"
#include "MWEnvironmentPreload.generated.h"

class UMWEnvironmentTheme;
struct FStreamableHandle;

// 로비 월드가 닫혀도 각 로컬 게임 인스턴스가 선택한 테마 하나를 경기 생성기까지 전달한다.
UCLASS(Config = Game)
class UMWEnvironmentPreload : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    void SelectTheme(TSoftObjectPtr<UMWEnvironmentTheme> Theme, bool bTraveling);
    void ClearSelection();
    virtual void Deinitialize() override;

private:
    // 경기 진입 직후에는 하늘과 바닥의 밉을 유지하고 이후에는 일반 스트리밍에 맡긴다.
    UPROPERTY(Config, EditDefaultsOnly, Category = "Environment", meta = (ClampMin = "0"))
    float EntryTextureSeconds = 8.f;

    TSoftObjectPtr<UMWEnvironmentTheme> SelectedTheme;
    TSharedPtr<FStreamableHandle> LoadHandle;
    bool bDepartureStarted = false;

    void WarmEntryTextures(TSoftObjectPtr<UMWEnvironmentTheme> Theme);
};
