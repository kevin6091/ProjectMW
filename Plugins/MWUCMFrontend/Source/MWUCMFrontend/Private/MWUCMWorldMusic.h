#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UI/MWMenuTypes.h"
#include "MWUCMWorldMusic.generated.h"

class AGameStateBase;
class UAudioComponent;
class USoundWave;
struct FStreamableHandle;

USTRUCT()
struct FMWUCMThemeMusic
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    FName MapThemeId;

    UPROPERTY(EditAnywhere)
    TSoftObjectPtr<USoundWave> Track;
};

// Local frontend and race audio ends with its world.
UCLASS(Config=Game)
class UMWUCMWorldMusic : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void OnWorldBeginPlay(UWorld& World) override;
    virtual void OnWorldEndPlay(UWorld& World) override;

    void SetMainMenuVisible(bool bVisible);
    void PlayMenuHighlight() const;
    void PlayMenuConfirm() const;

protected:
    virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;

private:
    UPROPERTY(Config)
    TSoftObjectPtr<USoundWave> MenuTrack;

    UPROPERTY(Config)
    TSoftObjectPtr<USoundWave> MenuHighlight;

    UPROPERTY(Config)
    TSoftObjectPtr<USoundWave> MenuConfirm;

    UPROPERTY(Config)
    TArray<FMWUCMThemeMusic> ThemeTracks;

    UPROPERTY(Config)
    float MusicVolume = 0.35f;

    UPROPERTY(Config)
    float MenuEffectVolume = 0.65f;

    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> PlayingMusic;

    TWeakObjectPtr<AGameStateBase> BoundState;
    FTimerHandle PlayerReadinessTimer;
    FSoftObjectPath RequestedTrack;
    TSharedPtr<FStreamableHandle> PendingLoad;
    TSharedPtr<FStreamableHandle> MenuEffectsLoad;
    bool bMainMenuVisible = false;

    void PlayMenuEffect(USoundWave* Sound) const;

    void BindGameState(AGameStateBase* State);
    void HandleGameStateSet(AGameStateBase* State);
    TSoftObjectPtr<USoundWave> GetDesiredTrack() const;
    void StartLoadedTrack(FSoftObjectPath Track);
    void StopMusic(bool bFade);

    UFUNCTION()
    void RefreshMusic();

    UFUNCTION()
    void HandleSessionChanged(EMWSessionState PreviousState, EMWSessionState NewState);
};
