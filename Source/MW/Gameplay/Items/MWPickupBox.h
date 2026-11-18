#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MWPickupBox.generated.h"

class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;

// 획득 성공 여부는 서버 ItemComponent에 맡기고 충돌·표시·재등장만 관리한다.
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

private:

    // 서버에서만 실제 차량의 지급 가능 여부를 검사하고 성공 시 상자를 재등장 대기로 바꾼다.
    UFUNCTION()
    void HandleOverlap(UPrimitiveComponent* Overlapped, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
                       int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    // 기존 재등장 상태에 맞춰 충돌과 외형 표시를 함께 갱신한다.
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

    UPROPERTY(ReplicatedUsing = UpdateAvailability)
    bool bAvailable = true;
};
