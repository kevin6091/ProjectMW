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

	UFUNCTION(BlueprintCallable, Category = "UCM Preview")
	FString GetFidelityEvidence() const;

	UFUNCTION(BlueprintCallable, Category = "UCM Preview")
	void ResetFidelityRuntime();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	double StartedAt = 0;
	float LastSequenceTime = -1;
	int32 Resets = 0;

	void ApplyMotion(float Seconds);
};
