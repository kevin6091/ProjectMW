#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UI/Common/MWMenuTypes.h"
#include "MWUCMWorldMusic.generated.h"

class AGameStateBase;
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

// 월드의 경기 상태와 메뉴 상태를 관찰해 곡을 선택한다. 실제 재생은 레벨 이동에도 유지된다.
UCLASS(Config = Game)
class UMWUCMWorldMusic : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& World) override;
	virtual void OnWorldEndPlay(UWorld& World) override;

	void SetMainMenuVisible(bool bVisible);
	void SetFrontendMusicActive(bool bActive);
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

	// 월드가 소유한 상태는 관찰만 한다. 종료 시 구독과 준비 확인 타이머를 함께 해제한다.
	TWeakObjectPtr<AGameStateBase> BoundState;
	FTimerHandle PlayerReadinessTimer;
	TSharedPtr<FStreamableHandle> MenuEffectsLoad;
	bool bMainMenuVisible = false;
	bool bFrontendMusicActive = false;

	void PlayMenuEffect(USoundWave* Sound) const;

	void BindGameState(AGameStateBase* State);
	void HandleGameStateSet(AGameStateBase* State);
	TSoftObjectPtr<USoundWave> GetDesiredTrack() const;

	UFUNCTION()
	void RefreshMusic();

	UFUNCTION()
	void HandleSessionChanged(EMWSessionState PreviousState, EMWSessionState NewState);
};
