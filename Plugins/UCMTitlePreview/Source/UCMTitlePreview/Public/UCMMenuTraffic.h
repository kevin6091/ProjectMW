#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UCMMenuTraffic.generated.h"

class UCurveFloat;
class ULevelSequence;
class ULevelSequencePlayer;
class ALevelSequenceActor;

USTRUCT(BlueprintType)
struct FUCMTrafficEvent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Time = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName Gate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOpen = false;
};

USTRUCT(BlueprintType)
struct FUCMTrafficVehicle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AActor> VehicleClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<ULevelSequence> Sequence;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGuid OwnerBinding;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Weight = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SpawnTime = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Duration = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FUCMTrafficEvent> Events;
};

USTRUCT(BlueprintType)
struct FUCMTrafficGatePart
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString SourceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FTransform ParentRelativeToCrossing;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FTransform LocalRest;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Direction = 1;

	UPROPERTY(Transient)
	TObjectPtr<AActor> Target;

	FTransform OriginalWorld;
	uint8 OriginalMobility = 0;
};

USTRUCT(BlueprintType)
struct FUCMTrafficGate
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Lane = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName Gate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UCurveFloat> Curve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float OpenSpeed = .2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CloseSpeed = .2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FUCMTrafficGatePart> Parts;

	// 같은 차단기를 사용하는 차량들의 열기 요청 수를 합산한다.
	int32 OpenCount = 0;
	float Progress = 0;
};

USTRUCT()
struct FUCMTrafficPlayback
{
	GENERATED_BODY()

	// 재사용 풀의 객체는 소유자가 종료될 때 함께 해제한다.
	UPROPERTY()
	TObjectPtr<AActor> Vehicle;

	UPROPERTY()
	TObjectPtr<ALevelSequenceActor> SequenceActor;

	UPROPERTY()
	TObjectPtr<ULevelSequencePlayer> Player;

	int32 Preset = INDEX_NONE;
	int32 Lane = INDEX_NONE;
	int32 NextEvent = 0;
	bool bActive = false;
};

/** 원본 메뉴의 배경 교통 연출 전용이다. 주행 차량·충돌·네트워크 요청·입력·카메라를 담당하지 않는다. */
UCLASS(Blueprintable)
class UCMTITLEPREVIEW_API AUCMMenuTraffic : public AActor
{
	GENERATED_BODY()

public:
	AUCMMenuTraffic();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Original Traffic")
	TArray<FUCMTrafficVehicle> Vehicles;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Original Traffic")
	TArray<FTransform> Lanes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Original Traffic")
	TArray<FUCMTrafficGate> Gates;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Original Traffic")
	int32 PoolLimit = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Original Traffic")
	float SpawnDelayMin = 40;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Original Traffic")
	float SpawnDelayMax = 40;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Original Traffic")
	FString CrossingSourceId = TEXT("1033");

	UFUNCTION(BlueprintCallable, Category="Original Traffic")
	bool StartTraffic();

	UFUNCTION(BlueprintPure, Category="Original Traffic")
	int32 GetActiveVehicleCount() const;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<AActor> Crossing;

	UPROPERTY(Transient)
	TArray<FUCMTrafficPlayback> Pool;

	TArray<double> NextSpawn;
	bool bStarted = false;
	AActor* FindSourceActor(const FString& Id) const;
	bool SpawnTraffic(int32 Lane);
	void ApplyEvents(FUCMTrafficPlayback& Playback, float Time);
	void ReleasePlayback(FUCMTrafficPlayback& Playback);
	void StopTraffic();
};
