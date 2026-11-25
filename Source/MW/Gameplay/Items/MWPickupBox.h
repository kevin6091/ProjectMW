#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MWPickupBox.generated.h"

class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;

// 획득 성공 여부는 서버 ItemComponent에 맡기고 충돌과 외형을 관리한다.
UCLASS()
class MW_API AMWPickupBox : public AActor
{
    GENERATED_BODY()

public:
    AMWPickupBox();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    // 서버 중첩 처리를 등록하고 현재 획득 가능 상태를 충돌·표시에 반영한다.
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    void ApplyDefaultPresentation();
    void RefreshPickupCollision();
    void SetPresentationVisible(bool bVisible);
    void UpdateIdlePresentation();
    void UpdateOpeningPresentation();

    // 서버에서만 실제 차량의 지급 가능 여부를 검사하고 성공 시 상자를 비활성화한다.
    UFUNCTION()
    void HandleOverlap(UPrimitiveComponent* Overlapped, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
                       int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    // 기존 서버 상태를 받은 창에서만 획득 연출을 시작한다.
    UFUNCTION()
    void UpdateAvailability();

    UPROPERTY(VisibleAnywhere, Category = "MW|Items")
    TObjectPtr<UBoxComponent> Trigger;

    UPROPERTY(VisibleAnywhere, Category = "MW|Items")
    TObjectPtr<USceneComponent> PresentationRoot;

    UPROPERTY(VisibleAnywhere, Category = "MW|Items")
    TObjectPtr<UStaticMeshComponent> Visual;

    UPROPERTY(VisibleAnywhere, Category = "MW|Items")
    TObjectPtr<UStaticMeshComponent> LidVisual;

    UPROPERTY(VisibleAnywhere, Category = "MW|Items")
    TObjectPtr<UStaticMeshComponent> HandleVisual;

    // 레벨 인스턴스 또는 Blueprint 기본값에서 시각 연출의 높이와 속도를 편집한다.
    UPROPERTY(EditAnywhere, Category = "MW|Items|Presentation", meta = (ClampMin = "0", Units = "cm"))
    float HoverHeight = 34.f;

    UPROPERTY(EditAnywhere, Category = "MW|Items|Presentation", meta = (ClampMin = "0", Units = "cm"))
    float BobAmplitude = 8.f;

    UPROPERTY(EditAnywhere, Category = "MW|Items|Presentation", meta = (ClampMin = "0", Units = "Hz"))
    float BobFrequency = 0.8f;

    UPROPERTY(EditAnywhere, Category = "MW|Items|Presentation")
    float SpinDegreesPerSecond = 100.f;

    UPROPERTY(EditAnywhere, Category = "MW|Items|Presentation", meta = (Units = "deg"))
    float IdleTilt = 12.f;

    UPROPERTY(EditAnywhere, Category = "MW|Items|Presentation", meta = (ClampMin = "0", Units = "s"))
    float OpeningDuration = 0.22f;

    UPROPERTY(EditAnywhere, Category = "MW|Items|Presentation", meta = (ClampMin = "0", Units = "s"))
    float DisappearDuration = 0.18f;

    UPROPERTY(EditAnywhere, Category = "MW|Items|Presentation", meta = (ClampMin = "0", ClampMax = "180", Units = "deg"))
    float LidOpenAngle = 105.f;

    UPROPERTY(ReplicatedUsing = UpdateAvailability)
    bool bAvailable = true;

    FTransform RestPresentation;
    FTransform RestLid;
    FTransform RestHandle;
    double IdleStartedAt = 0.0;
    double OpeningStartedAt = 0.0;
    bool bPresentationInitialized = false;
};
