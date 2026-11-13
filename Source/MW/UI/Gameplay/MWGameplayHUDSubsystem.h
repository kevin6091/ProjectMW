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
class UMWVehicleSelectionWidget;

// 월드에서 로컬 HUD 연결을 준비하고 Pawn 교체·월드 종료 때 실제 구독과 화면을 정리한다.
UCLASS(Config = Game)
class MW_API UMWGameplayHUDSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:

    // 계기판 눈금 상한만 조절한다. 차량의 실제 최고 속도에는 영향을 주지 않는다.
    UPROPERTY(Config, EditAnywhere, Category = "HUD|Speedometer", meta = (ClampMin = "30", ClampMax = "600"))
    float SpeedometerMaxKmh = 120.f;

    UPROPERTY(Config)
    TSoftObjectPtr<UFont> DisplayFont;

    UPROPERTY(Config)
    TSoftObjectPtr<UTexture2D> BoardLogo;

    UPROPERTY(Config)
    TSoftObjectPtr<UTexture2D> BoardBolt;

    UPROPERTY(Config)
    TSoftClassPtr<UMWVehicleSelectionWidget> VehiclePalette;

    UPROPERTY(Config)
    TSoftObjectPtr<UTexture2D> BoosterIcon;

    UPROPERTY(Config)
    TArray<TSoftObjectPtr<UTexture2D>> ItemRouletteIcons;

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
