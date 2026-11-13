#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MWEnvironmentGameplay.generated.h"

class AMWEnvironmentGenerator;
class AMWGameplayGameState;
class UMWEnvironmentTheme;
class USceneComponent;

USTRUCT()
struct FMWEnvironmentBinding
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    FName MapThemeId;

    UPROPERTY(EditAnywhere)
    TSoftObjectPtr<UMWEnvironmentTheme> Environment;
};

// 프로젝트 연결 계층이다. 기존 복제 상태를 읽어 배경만 표시하며 경기 규칙은 바꾸지 않는다.
UCLASS(Config = Game, DefaultConfig)
class MWENVIRONMENTGAMEPLAY_API UMWEnvironmentGameplay : public UWorldSubsystem
{
    GENERATED_BODY()
public:

    UPROPERTY(Config, EditAnywhere, Category = "Environment")
    bool Enabled = false;

    UPROPERTY(Config, EditAnywhere, Category = "Environment")
    TArray<FMWEnvironmentBinding> Bindings;
    // 기존 방 설정에는 복제되는 경기 시드가 없으므로 로컬 시드 정책을 설정으로 명시한다.
    UPROPERTY(Config, EditAnywhere, Category = "Environment")
    int32 LocalEnvironmentSeed = 42;

    UPROPERTY(Config, EditAnywhere, Category = "Environment")
    bool ReplaceLevelLighting = false;
    // 지정한 개발 배경의 표시만 숨긴다. 충돌과 원본 에셋 데이터는 그대로 둔다.
    UPROPERTY(Config, EditAnywhere, Category = "Environment")
    TArray<FString> HiddenBackdropActorPaths;

    UFUNCTION(BlueprintPure, Category = "Environment")
    AMWEnvironmentGenerator* GetGenerator() const
    {
        return Generator;
    }

    UFUNCTION(BlueprintPure, Category = "Environment")
    FString GetBindingStatus() const
    {
        return Status;
    }

    // 로컬 경기 월드에서 기존 상태를 읽고 늦은 복제·컨트롤러 준비 점검을 등록한다.
    virtual void OnWorldBeginPlay(UWorld& World) override;

    // 준비 점검 타이머와 실제 구독 상태를 해제한 뒤 배경 수명을 닫는다.
    virtual void Deinitialize() override;

protected:
    virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;

private:

    UPROPERTY(Transient)
    TObjectPtr<AMWEnvironmentGenerator> Generator;

    UPROPERTY(Transient)
    TObjectPtr<AMWGameplayGameState> State;

    FTimerHandle ReadinessTimer;

    FName ActiveTheme;

    FString Status = TEXT("Disabled");

    TMap<TWeakObjectPtr<USceneComponent>, bool> LevelLightingVisibility;

    TMap<TWeakObjectPtr<AActor>, bool> LevelBackdropVisibility;

    // 기존 복제 테마·단계와 로컬 컨트롤러를 확인해 완료본 배경 생성기를 연결한다.
    UFUNCTION()
    void RefreshState();

    // 현재 생성기를 정리하고 잠시 숨겼던 레벨 배경·조명의 표시 상태를 복원한다.
    void StopEnvironment();
};
