

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PawnMovementComponent.h"
#include "MWVehicleNetworkTypes.h"
#include "MWVehicleDriveInput.h"
#include "Data/MWVehicleDriveData.h"
#include "MWVehicleMovementComponent.generated.h"

class AMWVehiclePawn;
class UMWItemComponent;
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

	// 파일럿·핸들·바퀴 표시에서 입력을 조회만 한다. 주행과 서버 상태는 변경하지 않는다.
	UFUNCTION(BlueprintPure, Category="MW|Drive")
	float GetSteeringInput() const { return DriveInput.Steering; }

	float GetUnboostedForwardSpeed() const
	{
		return bDriveConfigReady ? FMath::Max(0.f, DriveConfig.MaxForwardSpeed) : 0.f;
	}

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintPure, Category="MW|Drive")
	bool IsDriftControlActive() const;

	virtual float GetMaxSpeed() const override;
	virtual void OnTeleported() override;
	virtual void StopMovementImmediately() override;

	UFUNCTION(BlueprintPure, Category="MW|Drive")
	bool IsGrounded() const { return bGrounded; }

	void ResetDriveInput();

	// 기존 트랙 효과의 요청만 보관한다. main의 Snapshot과 접지 판정은 유지한다.
	void ApplyTrackBoost(float MinimumThrottle, float Duration);
	void ApplyTrackSurfaceVelocity(const FVector& VelocityCmPerSecond, float Duration);

	// 발판을 먼저 갱신하도록 약한 참조와 틱 선행 관계를 함께 관리한다.
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
	FTransform LimitDrivePresentation(const FTransform& Desired, float DeltaTime,
		TArrayView<const FMWVehicleContactSample> Contacts) const;
	FTransform ConstrainContactPresentation(const FTransform& LocalPose, const FTransform& OtherPose,
		const FVector& OtherExtent, const FVector& Normal, bool bShowContact) const;

	UPROPERTY(Transient)
	FMWVehicleDriveConfig DriveConfig;

	FMWVehicleDriveInput DriveInput;

	bool bDriveConfigReady = false;

	float TrackMinimumThrottle = 0.f;
	double TrackBoostEndTime = -1.0;
	FVector TrackSurfaceVelocity = FVector::ZeroVector;
	double TrackSurfaceEndTime = -1.0;
	TWeakObjectPtr<UMWTrackRuntimeComponent> TrackLiftBase;

	// 현재 호출에서만 쓰는 Pawn 소유 참조다. 활성 상태·배율을 읽으며 효과 확정과 복제는 변경하지 않는다.
	const UMWItemComponent* GetActiveItemBooster() const;

	void ResolveDriveControl(float& OutThrottle, float& OutBrake) const;

	FVector GetGroundProbeUp() const;
	// 현재 회전에서 노면에 정렬된 목표 회전을 계산한다.
	// 실제 충돌체를 움직이거나 회전시키지는 않는다.
	FQuat CalculateGroundAlignRotation(const FQuat& CurrentRotation, float DeltaTime) const;
	// 최근 바닥 검사에서 접지 조건을 만족했는지
	bool bGrounded = false;
	// 실제 접촉면의 법선.
	// 접지 조건, 속도 투영, 접지 하중 계산에 사용한다.
	FVector GroundNormal = FVector::UpVector;
	// 여러 지점의 높이로 구한 차체 정렬 방향.
	// 각 이동 단계에서 다시 계산하며 이전 값을 보간해서 누적하지 않는다.
	FVector GroundOrientationNormal = FVector::UpVector;
	// 이번 이동 단계에서 적용할 회전
	FQuat TargetRotation = FQuat::Identity;
	// 차체의 아래 방향에서 지지면까지의 이동 거리.
	float GroundDistance = 0.f;
	float ImpactYawRate = 0.f;
	// 접지 검사에서 다시 계산하는 값. 단위: cm/s²
	double GroundNormalLoadAcceleration = 0.0;

	// 기준 회전에 노면 법선 축의 조향을 더한다.
	// 실제 충돌체나 TargetRotation은 변경하지 않는다.
	FQuat CalculateSteeringRotation(const FQuat& BaseRotation, float DeltaTime) const;

	// 회전 전후에 노면과의 간격을 유지하기 위한 위치 보정량.
	// 현재 차량의 Box 충돌체를 기준으로 계산한다.
	FVector CalculateRotationGroundOffset(const FQuat& FromRotation, const FQuat& ToRotation) const;

	// 해당 위치와 회전에 실제 차체 충돌이 발생하는지 검사한다.
	bool IsVehiclePoseBlocked(const FVector& Location, const FQuat& Rotation, float ShapeInflation = 0.f,
		TArrayView<const FMWVehicleContactSample> PredictedContacts = {}) const;

	void UpdateDriveVelocity(float DeltaTime);
	void UpdateAirVelocity(float DeltaTime);
	// 외부 충격에 의한 속도 변화량을 적용한다. 단위: cm/s
	void ApplyExternalVelocityChange(const FVector& DeltaVelocity);
	// 지지면의 안쪽 속도를 제거하고, 접지 중 벽에는 노면과 평행한 반발을 적용한다.
	// 차량 간 충돌과 공중 접촉에는 벽 반발 설정을 적용하지 않는다.
	void ResolveContactVelocity(const FHitResult& Hit);

	// 기하학적으로 유효한 도로 후보를 찾는다.
	// 접지 상태나 속도는 변경하지 않는다.
	// 박스 스윕을 보완할 때는 확인한 평면의 법선도 반환한다.
	bool FindGroundCandidate(FHitResult& OutHit, const FVector& PreviousNormal, bool bWasGrounded,
		FVector& OutOrientationNormal) const;
	void UpdateGroundState(FVector* OutOrientationNormal = nullptr);
	// 예측 접촉을 포함해 노면 정렬과 조향을 적용하고, 실제 충돌체를 움직였는지 반환한다.
	bool UpdateGroundRotation(float DeltaTime, TArrayView<const FMWVehicleContactSample> PredictedContacts);
	
	// 이전 노면의 접선 속도를 새 노면으로 회전시킨다.
	// 외력·점프에 의한 분리 속도와 접지 여부는 호출부에서 판단한다.
	static bool TryTransportGroundTangentVelocity(
		const FVector& InTangentVelocity,
		const FVector& FromNormal,
		const FVector& ToNormal,
		FVector& OutTangentVelocity);

	// 차체 아래의 접촉을 조사해 정렬 평면을 계산한다.
	// 기존 접촉이 없으면 검증된 평면 위의 접촉으로 지지 후보도 복원한다.
	bool TryCalculateGroundPlane(FHitResult& InOutContact, const FVector& ReferenceNormal, FVector& OutNormal) const;

	// 다운포스 계산
	// 한 번의 접지 평가에서만 사용하는 진단 값이다. 예측/복제 상태에는 포함하지 않는다.
	struct FGroundSupportDiagnostics
	{
		bool bEvaluated = false;
		bool bSameTrackAhead = false;
		// 실제 접지 검사에서 박스 스윕 대신 노면 평면을 사용했는지 기록한다.
		bool bUsedGroundPlane = false;
		const TCHAR* CurvatureResult = TEXT("NotEvaluated");
		double TangentSpeed = 0.0;
		double RequiredAcceleration = 0.0;
		double GravityNormalAcceleration = 0.0;
		double DownforceAcceleration = 0.0;
		double NormalLoadAcceleration = 0.0;
		double CrestSupportAcceleration = 0.0;
		double SampleDistance = 0.0;
		FVector AheadNormal = FVector::ZeroVector;
	};

	double CalculateDownforceAcceleration(double TangentSpeed) const;
	double EstimateRequiredNormalAcceleration(const FHitResult& Contact, const FVector& TangentVelocity, bool bWasGrounded,
		FGroundSupportDiagnostics& Diagnostics) const;
	double CalculateGroundNormalLoadAcceleration(const FHitResult& Contact, const FVector& TangentVelocity, bool bWasGrounded,
		FGroundSupportDiagnostics& Diagnostics) const;
	void LogGroundSupport(const TCHAR* Decision, bool bWasGrounded, const FHitResult* Candidate,
		const FGroundSupportDiagnostics& Diagnostics) const;

	void MoveVehicle(float DeltaTime, TArrayView<const FMWVehicleContactSample> PredictedContacts, FMWVehiclePredictedImpact* OutImpact);
	bool MoveWithWorldCollision(const FVector& Delta, float DeltaTime, const FVector& SurfaceVelocity);

	bool TryResolveVehicleContact(const FHitResult& Hit);

	void AdjustGroundHeight(float DeltaTime);
};
