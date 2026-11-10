

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PawnMovementComponent.h"
#include "MWVehicleDriveInput.h"
#include "Data/MWVehicleDriveData.h"
#include "MWVehicleMovementComponent.generated.h"

class AMWVehiclePawn;
struct FMWVehicleDriveSnapshot;

/**
 *
 */
UCLASS()
class MW_API UMWVehicleMovementComponent : public UPawnMovementComponent
{
	GENERATED_BODY()

public:
	UMWVehicleMovementComponent();

	void SetDriveInput(const FMWVehicleDriveInput& NewInput);

	UFUNCTION(BlueprintPure, Category="MW|Drive")
	float GetSteeringInput() const { return DriveInput.Steering; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintPure, Category="MW|Drive")
	bool IsDriftControlActive() const;

	virtual float GetMaxSpeed() const override;
	float GetUnboostedForwardSpeed() const
	{
		return bDriveConfigReady ? FMath::Max(0.f, DriveConfig.MaxForwardSpeed) : 0.f;
	}
	virtual void OnTeleported() override;

	UFUNCTION(BlueprintPure, Category="MW|Drive")
	bool IsGrounded() const { return bGrounded; }

	void ResetDriveInput();
	void ApplyTrackBoost(float MinimumThrottle, float Duration);
	void ApplyTrackSurfaceVelocity(const FVector& VelocityCmPerSecond, float Duration);

	UFUNCTION(BlueprintPure, Category="MW|Drive")
	bool IsTrackBoostActive() const;

protected:
	virtual void BeginPlay() override;
	virtual bool OverlapTest(const FVector& Location, const FQuat& Rotation, ECollisionChannel Channel,
		const FCollisionShape& Shape, const AActor* IgnoreActor) const override;

	UPROPERTY(EditDefaultsOnly, Category="MW|Drive")
	TObjectPtr<UMWVehicleDriveData> DriveData;

private:
	friend class AMWVehiclePawn;

	void SimulateDriveStep(float DeltaTime);
	void CaptureDriveSnapshot(FMWVehicleDriveSnapshot& OutSnapshot) const;
	void ApplyDriveSnapshot(const FMWVehicleDriveSnapshot& Snapshot);

	UPROPERTY(Transient)
	FMWVehicleDriveConfig DriveConfig;

	FMWVehicleDriveInput DriveInput;

	bool bDriveConfigReady = false;
	float TrackMinimumThrottle = 0.f;
	double TrackBoostEndTime = -1.0;
	FVector TrackSurfaceVelocity = FVector::ZeroVector;
	double TrackSurfaceEndTime = -1.0;

	bool HasItemBooster() const;
	void ResolveDriveControl(float& OutThrottle, float& OutBrake) const;

	// 최근 바닥 검사에서 접지 조건을 만족했는지
	bool bGrounded = false;
	// 바닥 표면에서 바깥쪽으로 향하는 수직 방향
	FVector GroundNormal = FVector::UpVector;
	// 이번 이동 단계에서 적용할 회전
	FQuat TargetRotation = FQuat::Identity;
	// 아래 방향 Sweep으로 구한 접촉까지의 이동 거리
	float GroundDistance = 0.f;

	void UpdateDriveVelocity(float DeltaTime);
	void UpdateSteering(float DeltaTime);
	void UpdateGroundState();
	void UpdateAirVelocity(float DeltaTime);

	void MoveVehicle(float DeltaTime);

	bool TryResolveVehicleContact(const FHitResult& Hit);

	void AdjustGroundHeight();
};
