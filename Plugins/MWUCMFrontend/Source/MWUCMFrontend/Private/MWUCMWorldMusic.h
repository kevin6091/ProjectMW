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
    // 현재 연결 상태와 GameState 알림을 구독해 로컬 음악 선택을 시작한다.
    virtual void OnWorldBeginPlay(UWorld& World) override;

    // 이 월드의 준비 타이머와 상태 구독만 정리하고 지속 음악은 게임 인스턴스에 남긴다.
    virtual void OnWorldEndPlay(UWorld& World) override;

    // 메뉴가 실제 보이는지 기록해 선택 효과음이 다른 화면에서 나지 않게 한다.
    void SetMainMenuVisible(bool bVisible);

    // 생성·검색 창에서도 같은 프런트엔드 음악이 이어지도록 메뉴 가시성과 분리한다.
    void SetFrontendMusicActive(bool bActive);

    // 현재 메인 메뉴의 선택 이동 효과음을 공통 표시 조건으로 재생한다.
    void PlayMenuHighlight() const;

    // 현재 메인 메뉴의 선택 확정 효과음을 공통 표시 조건으로 재생한다.
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

    // 메인 메뉴가 실제 표시되는 로컬 화면에서만 설정된 효과음을 재생한다.
    void PlayMenuEffect(USoundWave* Sound) const;

    // 이전 상태 구독을 끊은 뒤 실제 현재 로비·경기 GameState에 연결한다.
    void BindGameState(AGameStateBase* State);

    // 늦게 준비된 GameState도 구독해 초기 음악 선택을 놓치지 않는다.
    void HandleGameStateSet(AGameStateBase* State);

    // 기존 경기 단계와 테마로 필요한 곡을 고른다. 게임 진행 상태를 직접 변경하지 않는다.
    TSoftObjectPtr<USoundWave> GetDesiredTrack() const;

    // 로컬 표시 준비와 곡 설정을 확인한 뒤 게임 인스턴스의 지속 음악 서비스에 요청한다.
    UFUNCTION()
    void RefreshMusic();

    // 기존 연결 상태 알림을 받아 현재 화면에 맞는 음악만 다시 평가한다.
    UFUNCTION()
    void HandleSessionChanged(EMWSessionState PreviousState, EMWSessionState NewState);
};
