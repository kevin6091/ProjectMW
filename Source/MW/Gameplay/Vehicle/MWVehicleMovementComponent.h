

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

	FVector GetGroundProbeUp() const;
	// 현재 회전에서 노면에 정렬된 목표 회전을 계산한다.
	// 실제 충돌체를 움직이거나 회전시키지는 않는다.
	FQuat CalculateGroundAlignRotation(const FQuat& CurrentRotation, float DeltaTime) const;
	// 최근 바닥 검사에서 접지 조건을 만족했는지
	bool bGrounded = false;
	// 바닥 표면에서 바깥쪽으로 향하는 수직 방향
	FVector GroundNormal = FVector::UpVector;
	// 이번 이동 단계에서 적용할 회전
	FQuat TargetRotation = FQuat::Identity;
	// 아래 방향 Sweep으로 구한 접촉까지의 이동 거리
	float GroundDistance = 0.f;
	
	// 기준 회전에 노면 법선 축의 조향을 더한다.
	// 실제 충돌체나 TargetRotation은 변경하지 않는다.
	FQuat CalculateSteeringRotation(const FQuat& BaseRotation, float DeltaTime) const;
	
	// 회전 전후에 노면과의 간격을 유지하기 위한 위치 보정량.
	// 현재 차량의 Box 충돌체를 기준으로 계산한다.
	FVector CalculateRotationGroundOffset(const FQuat& FromRotation, const FQuat& ToRotation) const;
	
	// 해당 위치와 회전에 실제 차체 충돌이 발생하는지 검사한다.
	bool IsVehiclePoseBlocked(const FVector& Location, const FQuat& Rotation, float ShapeInflation = 0.f) const;

	void UpdateDriveVelocity(float DeltaTime);
	void UpdateAirVelocity(float DeltaTime);
	// 외부 충격에 의한 속도 변화량을 적용한다. 단위: cm/s
	void ApplyExternalVelocityChange(const FVector& DeltaVelocity);
	// 충돌면 안쪽 속도를 제거하고,
	// 지지면 보정과 장애물 접촉을 구분한다.
	void ResolveContactVelocity(const FHitResult& Hit);
	
	// 기하학적으로 유효한 도로 후보를 찾는다.
	// 접지 상태나 속도는 변경하지 않는다.
	// 반환값이 true일 때만 OutHit을 사용한다.
	bool FindGroundCandidate(FHitResult& OutHit) const;
	void UpdateGroundState();
	// 노면 정렬과 조향을 충돌·간격 보정과 함께 적용한다.
	void UpdateGroundRotation(float DeltaTime);
	
	// 이전 노면의 접선 속도를 새 노면으로 회전시킨다.
	// 외력·점프에 의한 분리 속도와 접지 여부는 호출부에서 판단한다.
	static bool TryTransportGroundTangentVelocity(
		const FVector& InTangentVelocity,
		const FVector& FromNormal,
		const FVector& ToNormal,
		FVector& OutTangentVelocity);
	
	// 다운포스 계산
	// 한 번의 접지 평가에서만 사용하는 진단 값이다. 예측/복제 상태에는 포함하지 않는다.
	struct FGroundSupportDiagnostics
	{
		bool bEvaluated = false;
		const TCHAR* CurvatureResult = TEXT("NotEvaluated");
		double TangentSpeed = 0.0;
		double RequiredAcceleration = 0.0;
		double GravityNormalAcceleration = 0.0;
		double DownforceAcceleration = 0.0;
		double NormalLoadAcceleration = 0.0;
		double SampleDistance = 0.0;
		FVector AheadNormal = FVector::ZeroVector;
	};

	double CalculateDownforceAcceleration(double TangentSpeed) const;
	double EstimateRequiredNormalAcceleration(const FHitResult& Contact, const FVector& TangentVelocity,
		FGroundSupportDiagnostics& Diagnostics) const;
	double CalculateGroundNormalLoadAcceleration(const FHitResult& Contact, const FVector& TangentVelocity,
		FGroundSupportDiagnostics& Diagnostics) const;
	void LogGroundSupport(const TCHAR* Decision, bool bWasGrounded, const FHitResult* Candidate,
		const FGroundSupportDiagnostics& Diagnostics) const;

	// 접지 검사에서 다시 계산하는 값. 단위: cm/s²
	double GroundNormalLoadAcceleration = 0.0;
	
	void MoveVehicle(float DeltaTime);

	bool TryResolveVehicleContact(const FHitResult& Hit);

	void AdjustGroundHeight();
};
