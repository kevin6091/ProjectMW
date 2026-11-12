#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UCMTitleFidelityDirector.generated.h"

class UNiagaraComponent;

USTRUCT(BlueprintType)
struct FUCMSourcePropMotion
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    TObjectPtr<AActor> Target;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    FString SourceRef;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    FTransform StaticWorld;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    FVector AxisWorld = FVector::UpVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    FVector PivotWorld = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    float InitialDegrees = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    float SpeedDegrees = 0;
};

// 원본 자료로 확인한 배경 소품 회전·파티클·교통의 로컬 재생 수명만 관리한다.
UCLASS(Blueprintable)
class UCMTITLEPREVIEW_API AUCMTitleFidelityDirector : public AActor
{
    GENERATED_BODY()

public:
    AUCMTitleFidelityDirector();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MW Integration")
    bool bStartAutomatically = true;

    UPROPERTY(EditAnywhere, Category = "UCM Source")
    TSoftClassPtr<AActor> MenuTrafficClass;

    UPROPERTY(Transient)
    TObjectPtr<AActor> MenuTraffic;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    TArray<FUCMSourcePropMotion> Motions;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    TArray<TObjectPtr<UNiagaraComponent>> SourceParticles;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    TArray<float> SourceSimulationSpeeds;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    TArray<TObjectPtr<USceneComponent>> BrandingRenderers;

    // 저장된 BP의 속성 연결을 유지하므로 기존 이름을 보존한다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Preview")
    bool ShowOriginalBranding = true;

    // 연출의 현재 재생 상태를 검증용 문자열로 반환한다.
    UFUNCTION(BlueprintCallable, Category = "UCM Preview")
    FString GetFidelityEvidence() const;

    // 메뉴 재진입 시 원본 기준 자세와 교통 재생 상태를 다시 준비한다.
    UFUNCTION(BlueprintCallable, Category = "UCM Preview")
    void ResetFidelityRuntime();

protected:
    // 원본 부품 기준 자세와 로컬 교통 재생을 이 월드에서 준비한다.
    virtual void BeginPlay() override;

    // 이 연출의 경과 시간으로 배경 움직임을 갱신한다. 경기 규칙이나 차량 입력은 수정하지 않는다.
    virtual void Tick(float DeltaSeconds) override;

    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
    double StartedAt = 0;

    float LastSequenceTime = -1;

    int32 Resets = 0;

    // 복구한 원본 시간·축·주기로 타이틀 배경 부품의 움직임을 재생한다.
    void ApplyMotion(float Seconds);
};
