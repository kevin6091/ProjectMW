#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Gameplay/Track/MWTrackRuntimeData.h"
#include "MWTrackRuntimeComponent.generated.h"

class AMWVehiclePawn;
class UPrimitiveComponent;
class UStaticMeshComponent;
class UAudioComponent;
class UBoxComponent;
class UChildActorComponent;

UCLASS()
class UMWTrackRuntimeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMWTrackRuntimeComponent();
	void Configure(const FMWTrackPieceRuntimeData& Data);
	void SetPreview(bool bInPreview);
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	struct FMovedPart
	{
		TWeakObjectPtr<USceneComponent> Component;
		FTransform Rest;
	};

	struct FMotion
	{
		FMWTrackMotionData Data;
		TArray<FMovedPart> Parts;
	};

	struct FRiderTrigger
	{
		const FMWTrackShapeData* Shape = nullptr;
		TSet<TWeakObjectPtr<AMWVehiclePawn>> Riders;
	};

	struct FTrafficSlot
	{
		TWeakObjectPtr<UChildActorComponent> Visual;
		TWeakObjectPtr<UBoxComponent> Hitbox;
		FVector BoundsCenter;
		FRotator VisualCorrection;
		int32 LaneIndex = 0;
		double OffsetSeconds = 0;
		bool bActive = false;
	};

	struct FTrafficLaneState
	{
		double Phase = 0;
		double DriveSeconds = 0;
	};

	UFUNCTION()
	void OnTrigger(UPrimitiveComponent* Overlapped, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);

	UFUNCTION()
	void OnRiderEndOverlap(UPrimitiveComponent* Overlapped, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex);

	UPrimitiveComponent* CreateShape(const FMWTrackShapeData& Shape);
	void UpdateTrain(double Time);
	void SetTrainActive(bool bActive);
	void ConfigureTraffic();
	void UpdateTraffic(double Time);
	void SetTrafficActive(FTrafficSlot& Slot, bool bActive);
	void SetTrainBell(bool bActive);
	void UpdateLift(double Time, float DeltaTime);
	void ApplyMotions(double Time, float GateAlpha);
	void ApplyRollers();
	double GetRaceTime() const;

	const FMWTrackPieceRuntimeData* Piece = nullptr;
	TMap<UPrimitiveComponent*, const FMWTrackShapeData*> Triggers;
	TMap<UPrimitiveComponent*, FRiderTrigger> RiderTriggers;
	TArray<TWeakObjectPtr<UPrimitiveComponent>> RoadCollision;
	TArray<FMotion> Motions;
	TArray<TWeakObjectPtr<UStaticMeshComponent>> Train;
	TWeakObjectPtr<UPrimitiveComponent> LiftRide;
	TWeakObjectPtr<UPrimitiveComponent> TrainHitbox;
	TWeakObjectPtr<UAudioComponent> Bell;
	TArray<FTrafficSlot> Traffic;
	TArray<FTrafficLaneState> TrafficLaneStates;
	bool bTrainActive = false;
	bool bBellActive = false;
	FVector TrainCenter = FVector::ZeroVector;
	double LiftActiveAt = -1;
	float LiftHeight = 0;
	bool bPreview = false;
};
