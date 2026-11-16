#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Items/MWItemComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "System/MWRoomTypes.h"
#include "MWGameplayHUDSubsystem.generated.h"

class APawn;
class AMWGameplayGameState;
class APlayerController;
class SWidget;
class UGameViewportClient;
class UFont;
class UMaterialInterface;
class UTexture2D;
class UMWRaceHUDWidget;

// 월드에서 로컬 HUD 연결을 준비하고 Pawn 교체·월드 종료 때 실제 구독과 화면을 정리한다.
UCLASS(Config = Game)
class MW_API UMWGameplayHUDSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:

    // 화면 디자인은 지정된 WBP에서 편집하고 기존 Slate HUD에는 그 결과만 연결한다.
    UPROPERTY(Config)
    TSoftClassPtr<UMWRaceHUDWidget> RaceHUDWidgetClass;

    UPROPERTY(Config)
    TSoftObjectPtr<UFont> DisplayFont;

    UPROPERTY(Config)
    TSoftObjectPtr<UTexture2D> BoardLogo;

    UPROPERTY(Config)
    TSoftObjectPtr<UTexture2D> BoardBolt;

    UPROPERTY(Config)
    TSoftObjectPtr<UTexture2D> BoosterIcon;

    UPROPERTY(Config)
    TArray<TSoftObjectPtr<UTexture2D>> ItemRouletteIcons;

    // 획득 후 아이콘을 바꾸는 누적 시각(초)이다. 양수·오름차순이며 마지막 값에서 실제 아이템에 멈춘다.
    // 빈 배열은 룰렛을 생략하고 실제 지급 아이템을 바로 표시한다.
    UPROPERTY(Config, EditAnywhere, Category = "MW|UI|Item Animation", meta = (DisplayName = "룰렛 전환 누적 시각(초)"))
    TArray<double> ItemRouletteSwitchTimes;

    // 사용 시 축소·페이드 시간(초)이다. 0이면 즉시 비우며 서버 아이템 사용 시점에는 관여하지 않는다.
    UPROPERTY(Config, EditAnywhere, Category = "MW|UI|Item Animation", meta = (ClampMin = "0", Units = "s"))
    float ItemUseDuration = .20f;

    UPROPERTY(Config)
    TSoftObjectPtr<UMaterialInterface> ItemIconMaterial;

    UPROPERTY(Config)
    FVector2D BoosterIconContentCenter = FVector2D(.5, .5);

    // 로컬 Controller와 Viewport가 준비됐을 때 기존 HUD 오버레이를 한 번 연결한다.
    virtual void Tick(float DeltaTime) override;

    // 초기 연결 또는 GameState 교체가 필요할 때만 연결용 틱을 허용한다.
    virtual bool IsTickable() const override;

    virtual TStatId GetStatId() const override;

    // HUD 소비자는 관찰 중인 서버 단계만 읽는다. 준비된 GameState가 없으면 값이 없다.
    TOptional<EMWRoomPhase> GetObservedRoomPhase() const
    {
        return RoomPhase;
    }

protected:
    virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

    // 월드가 닫힐 때 실제 Viewport 오버레이와 아이템 구독을 먼저 제거한다.
    virtual void OnWorldEndPlay(UWorld& World) override;

    // 월드 종료 경로가 달라도 같은 오버레이·구독 정리를 수행한다.
    virtual void Deinitialize() override;

private:
    UPROPERTY(Transient)
    TObjectPtr<UMWRaceHUDWidget> RaceHUDWidget;

    // 월드의 실제 GameState가 바뀌면 이전 구독을 해제하고 현재 값을 즉시 읽는다.
    void RefreshGameStateBinding();

    // 호스트의 직접 변경과 클라이언트의 RepNotify가 보내는 기존 이벤트를 함께 받는다.
    UFUNCTION()
    void HandleGameplayStateChanged();

    // 실제 구독했던 Controller·ItemComponent와 Viewport에서 연결을 해제한다.
    void RemoveOverlay();

    // 이전 아이템 구독을 먼저 끊고 새 소유 Pawn의 현재 값과 이벤트를 함께 연결한다.
    UFUNCTION()
    void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

    // 실제 받은 보유 아이템만 표시 모델에 반영한다. 룰렛이 지급 결과를 다시 정하지 않는다.
    UFUNCTION()
    void HandleHeldItemChanged(EMWItemId ItemId);

    TWeakObjectPtr<AMWGameplayGameState> ObservedGameState;

    TOptional<EMWRoomPhase> RoomPhase;

    TWeakObjectPtr<APlayerController> ItemController;

    TWeakObjectPtr<UMWItemComponent> ObservedItems;

    EMWItemId HeldItem = EMWItemId::None;

    TSharedPtr<SWidget> Overlay;

    TWeakObjectPtr<UGameViewportClient> Viewport;
};
