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
    UPROPERTY(EditAnywhere) FName MapThemeId;
    UPROPERTY(EditAnywhere) TSoftObjectPtr<UMWEnvironmentTheme> Environment;
};

// Project-local adapter. Reads existing replicated state; never changes match rules.
UCLASS(Config=Game, DefaultConfig)
class MWENVIRONMENTGAMEPLAY_API UMWEnvironmentGameplay : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    UPROPERTY(Config, EditAnywhere, Category="Environment") bool Enabled = false;
    UPROPERTY(Config, EditAnywhere, Category="Environment") TArray<FMWEnvironmentBinding> Bindings;
    // There is no replicated match seed in FMWRoomSettings. Deliberately explicit local policy.
    UPROPERTY(Config, EditAnywhere, Category="Environment") int32 LocalEnvironmentSeed = 42;
    UPROPERTY(Config, EditAnywhere, Category="Environment") bool ReplaceLevelLighting = false;
    // Explicit visual-only development backdrops; collision and asset data remain untouched.
    UPROPERTY(Config, EditAnywhere, Category="Environment") TArray<FString> HiddenBackdropActorPaths;
    UFUNCTION(BlueprintPure, Category="Environment") AMWEnvironmentGenerator* GetGenerator() const { return Generator; }
    UFUNCTION(BlueprintPure, Category="Environment") FString GetBindingStatus() const { return Status; }
    virtual void OnWorldBeginPlay(UWorld& World) override;
    virtual void Deinitialize() override;
protected:
    virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;
private:
    UPROPERTY(Transient) TObjectPtr<AMWEnvironmentGenerator> Generator;
    UPROPERTY(Transient) TObjectPtr<AMWGameplayGameState> State;
    FTimerHandle ReadinessTimer;
    FName ActiveTheme;
    FString Status = TEXT("Disabled");
    TMap<TWeakObjectPtr<USceneComponent>, bool> LevelLightingVisibility;
    TMap<TWeakObjectPtr<AActor>, bool> LevelBackdropVisibility;
    UFUNCTION() void RefreshState();
    void StopEnvironment();
};
