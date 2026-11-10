#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Track/MWTrackRuntimeData.h"
#include "Subsystems/WorldSubsystem.h"
#include "MWTrackRuntimeSubsystem.generated.h"

class AMWVehiclePawn;
class UMWTrackRuntimeComponent;

USTRUCT(BlueprintType)
struct FMWTrackRaceProgress
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="MW|Race") int32 Checkpoints = 0;
	UPROPERTY(BlueprintReadOnly, Category="MW|Race") bool bFinished = false;
	UPROPERTY(BlueprintReadOnly, Category="MW|Race") float ElapsedSeconds = 0;
};

UCLASS()
class MW_API UMWTrackRuntimeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UMWTrackRuntimeSubsystem();
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category="MW|Race")
	FMWTrackRaceProgress GetTrackRaceProgress(AMWVehiclePawn* Vehicle) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="MW|Race")
	bool RespawnAtTrackCheckpoint(AMWVehiclePawn* Vehicle);

	void ProcessRaceTrigger(AMWVehiclePawn& Vehicle, AActor& Track,
		EMWTrackTrigger Trigger, const TArray<FTransform>& RespawnPoints);
	void SetTrackPreview(AActor& Actor, bool bPreview);

protected:
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

private:
	struct FRacer
	{
		FMWTrackRaceProgress Progress;
		TSet<TWeakObjectPtr<AActor>> Passed;
		TArray<FTransform> RespawnTransforms;
		double StartTime = -1;
	};

	void AttachTrack(AActor* Actor);
	void OnActorSpawned(AActor* Actor);

	UPROPERTY() TObjectPtr<UMWTrackRuntimeData> Catalog;
	TMap<FName, const FMWTrackPieceRuntimeData*> Pieces;
	TMap<TWeakObjectPtr<AMWVehiclePawn>, FRacer> Racers;
	FDelegateHandle ActorSpawnHandle;
};
