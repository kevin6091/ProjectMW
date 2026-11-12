

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PawnMovementComponent.h"
#include "MWVehicleNetworkTypes.h"
#include "Data/MWVehicleDriveData.h"
#include "MWVehicleMovementComponent.generated.h"

class AMWVehiclePawn;
class UMWTrackRuntimeComponent;

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

	// Pilot ABP와 바퀴 표시는 기존 입력을 조회만 한다. 서버 예측 상태를 변경하지 않는다.
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
	void SetTrackLiftBase(AActor* Base);
	void ClearTrackLiftBase(const AActor* Base);

	UFUNCTION(BlueprintPure, Category="MW|Drive")
	bool IsTrackBoostActive() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, Category="MW|Drive")
	TObjectPtr<UMWVehicleDriveData> DriveData;

private:
	friend class AMWVehiclePawn;

	void SimulateDriveStep(float DeltaTime, TArrayView<const FMWVehicleContactSample> PredictedContacts = {},
		FMWVehiclePredictedImpact* OutImpact = nullptr);
	void CaptureDriveSnapshot(FMWVehicleDriveSnapshot& OutSnapshot) const;
	void ApplyDriveSnapshot(const FMWVehicleDriveSnapshot& Snapshot);
	FMWVehicleContactTuning GetContactTuning() const;

	UPROPERTY(Transient)
	FMWVehicleDriveConfig DriveConfig;

	FMWVehicleDriveInput DriveInput;

	bool bDriveConfigReady = false;
	float TrackMinimumThrottle = 0.f;
	double TrackBoostEndTime = -1.0;
	FVector TrackSurfaceVelocity = FVector::ZeroVector;
	double TrackSurfaceEndTime = -1.0;
	// 발판의 비소유 참조와 선행 틱 수명만 관리한다. Kevin의 스냅샷 계약에는 추가하지 않는다.
	TWeakObjectPtr<UMWTrackRuntimeComponent> TrackLiftBase;

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
	float ImpactYawRate = 0.f;

	void UpdateDriveVelocity(float DeltaTime);
	void UpdateSteering(float DeltaTime, TArrayView<const FMWVehicleContactSample> PredictedContacts);
	void UpdateGroundState();
	void UpdateAirVelocity(float DeltaTime);

	void MoveVehicle(float DeltaTime, TArrayView<const FMWVehicleContactSample> PredictedContacts, FMWVehiclePredictedImpact* OutImpact);

	bool TryResolveVehicleContact(const FHitResult& Hit);

	void AdjustGroundHeight();
};
