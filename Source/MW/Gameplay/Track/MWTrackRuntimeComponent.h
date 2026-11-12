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
	FVector GetLiftWorldOffset() const;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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
	void ClearRiders();
	double GetRaceTime() const;

	// 설정 값과 그 안의 에셋 참조는 이 컴포넌트의 수명 동안 보관한다.
	UPROPERTY(Transient)
	// 설정 배열 원소를 가리키는 포인터 대신 값을 보관해 카탈로그 조회 수명과 분리한다.
	FMWTrackPieceRuntimeData Piece;
	bool bConfigured = false;
	TMap<TWeakObjectPtr<UPrimitiveComponent>, const FMWTrackShapeData*> Triggers;
	TMap<TWeakObjectPtr<UPrimitiveComponent>, FRiderTrigger> RiderTriggers;
	TArray<TWeakObjectPtr<UActorComponent>> GeneratedComponents;
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
	// 작동 시작은 서버가 결정하고 각 기기는 복제된 서버 시각으로 같은 높이를 계산한다.
	UPROPERTY(Replicated)
	double LiftActiveAt = -1;
	float LiftHeight = 0;
	bool bPreview = false;
};
