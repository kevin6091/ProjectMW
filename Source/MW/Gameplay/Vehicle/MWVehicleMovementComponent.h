

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

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintPure, Category="MW|Drive")
	bool IsDriftControlActive() const;

	virtual float GetMaxSpeed() const override;
	virtual void OnTeleported() override;

	UFUNCTION(BlueprintPure, Category="MW|Drive")
	bool IsGrounded() const { return bGrounded; }

	void ResetDriveInput();

protected:
	virtual void BeginPlay() override;

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
