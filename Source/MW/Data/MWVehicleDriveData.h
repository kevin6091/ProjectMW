

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MWVehicleDriveData.generated.h"

/**
 *
 */
USTRUCT(BlueprintType)
struct MW_API FMWVehicleDriveConfig
{
	GENERATED_BODY()

	// 전진 최고속도. 단위: cm/s
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MaxForwardSpeed = 1800.f;

	// 추진 가속도. 단위: cm/s²
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Acceleration", meta = (ClampMin = "0.0"))
	float DriveAcceleration = 1600.f;

	// 접지 중 횡방향 미끄러짐의 초당 감쇠 강도 (1/s)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grip", meta = (ClampMin = "0.0"))
	float LateralGrip = 1.5f;

	// 관성 감속
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Braking", meta = (AllowPrivateAccess = "true"))
	float CoastDeceleration = 120.f;

	// 최대 브레이크 감속
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Braking", meta = (AllowPrivateAccess = "true"))
	float FullBrakeDeceleration = 2600.f;

	// 방향 전환 감속 임계값
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reverse", meta = (AllowPrivateAccess = "true"))
	float DirectionChangeSpeedThreshold = 30.f;

	// 최대 후진 속도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reverse", meta = (AllowPrivateAccess = "true"))
	float MaxReverseSpeed = 600.f;

	// 회전 속도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steering", meta = (AllowPrivateAccess = "true"))
	float TurnRateDegPerSecond = 100.f;

	// 최대 조향 속도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steering", meta = (AllowPrivateAccess = "true"))
	float FullSteeringSpeed = 500.f;

	// 최소 조향 속도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steering", meta = (AllowPrivateAccess = "true"))
	float MinSteeringSpeed = 20.f;

	// 최소 드리프트 속도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drift", meta = (AllowPrivateAccess = "true"))
	float MinDriftSpeed = 200.f;

	// 최소 드리프트 조향
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drift", meta = (AllowPrivateAccess = "true"))
	float MinDriftSteering = 0.5f;

	// 드리프트 마찰 계수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drift", meta = (AllowPrivateAccess = "true"))
	float DriftFrictionMultiplier = 0.2f;

	// 드리프트 회전 속도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drift", meta = (AllowPrivateAccess = "true"))
	float DriftTurnRateMultiplier = 1.25f;

	// 현재 충돌체 위치에서 아래 방향으로 Sweep할 거리. 단위: cm
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground", meta = (ClampMin = "0.0", Units = "cm"))
	float GroundProbeDistance = 5.f;

	// 주행 가능한 바닥의 최대 경사각. 단위: 도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground", meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float MaxGroundAngle = 45.f;

	// 바닥과 유지할 작은 여유 간격. 단위: cm
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground", meta = (ClampMin = "0.0", Units = "cm"))
	float GroundClearance = 1.f;

	// 바닥 법선 방향으로 이 속도를 넘어서면 접지를 해제한다. 단위: cm/s
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground", meta = (ClampMin = "0.0", Units = "cm/s"))
	float GroundDetachSpeed = 10.f;
	
	// 차체가 노면 방향으로 정렬되는 최대 회전 속도. 단위: deg/s
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground", meta = (ClampMin = "0.0"))
	float GroundAlignSpeed = 180.f;

	// 엔진에서 가져온 중력에 적용할 배율
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air", meta = (ClampMin = "0.0"))
	float GravityScale = 1.f;

	// 최대 낙하 속도. 단위: cm/s
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air", meta = (ClampMin = "1.0", Units = "cm/s"))
	float MaxFallSpeed = 4000.f;

	// 차량 간 충돌에 사용하는 질량
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collision", meta = (ClampMin = "1.0", Units = "kg"))
	float VehicleMass = 1000.f;

	// 반발 계수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collision", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CollisionRestitution = 0.1f;

	// 반발이 적용되는 최소 접근 속도. 단위: cm/s
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collision", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MinBounceSpeed = 100.f;

	// 차량끼리 부딪힌 뒤 접촉 법선 방향의 공통 운동량을 유지하는 비율
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collision", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CollisionMomentumRetention = 0.35f;

	// 접지 중 벽에 접근한 속도에서 반대 방향으로 되돌릴 비율이다.
	// 차량 간 충돌과 별도로 조정하며 차체를 띄우는 힘은 추가하지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collision", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WallRestitution = 0.7f;

	// 벽 반발을 적용할 최소 접근 속도. 저속 접촉에서는 안쪽 속도만 제거한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collision", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MinWallBounceSpeed = 150.f;
	
	// 다운포스의 기준 속도. 단위: cm/s
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Downforce", meta = (ClampMin = "1.0", Units = "cm/s"))
	float DownforceReferenceSpeed = 1800.f;

	// 기준 속도에서 발생하는 다운포스 가속도.
	// 질량으로 나눈 힘을 사용한다. 단위: cm/s²
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Downforce", meta = (ClampMin = "0.0"))
	float DownforceAtReferenceSpeed = 600.f;

	// 다운포스 가속도의 상한. 단위: cm/s²
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Downforce", meta = (ClampMin = "0.0"))
	float MaxDownforceAcceleration = 1800.f;

	// 진행 방향으로 노면 변화를 조사할 거리. 단위: cm
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground", meta = (ClampMin = "1.0", Units = "cm"))
	float GroundCurvatureProbeDistance = 500.f;

	// 곡률 검사 지점에서 위아래로 조사할 거리. 단위: cm
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground", meta = (ClampMin = "1.0", Units = "cm"))
	float GroundCurvatureProbeHeight = 100.f;

	// 접지를 유지 중일 때 허용할 작은 하중 계산 오차.
	// 단위: cm/s²
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground", meta = (ClampMin = "0.0"))
	float GroundSupportTolerance = 20.f;

	// 타이어가 전달할 수 있는 힘의 한계를 결정하는 계수.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grip", meta = (ClampMin = "0.0"))
	float TireFrictionCoefficient = 2.f;
	
	// 충돌 Box 크기와 독립적인 자세 추정 범위. 차체 전방 기준 앞뒤 반거리다.
	// 좁은 충돌체에서도 여러 도로 면을 함께 조사한다. 단위: cm
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground|Orientation", meta = (ClampMin = "1.0", Units = "cm"))
	float GroundPlaneProbeHalfLength = 100.f;

	// 자세 추정에 사용할 좌우 반거리. 실제 접지 여부는 기존 Box Sweep으로 판단한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground|Orientation", meta = (ClampMin = "1.0", Units = "cm"))
	float GroundPlaneProbeHalfWidth = 40.f;

	// 각 조사 지점에서 위아래로 검사할 거리. 단위: cm
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground|Orientation", meta = (ClampMin = "1.0", Units = "cm"))
	float GroundPlaneProbeHalfHeight = 15.f;

	// 실제 접촉 법선과 조사된 면 사이의 최대 허용 각도.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground|Orientation", meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float GroundPlaneMaxNormalAngle = 15.f;

	// 실제 접촉 평면과 조사점 사이의 최대 높이 차이. 단위: cm
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground|Orientation", meta = (ClampMin = "0.0", Units = "cm"))
	float GroundPlaneMaxHeightError = 8.f;

	// 조사점들이 계산한 평면에서 벗어날 수 있는 거리. 단위: cm
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground|Orientation", meta = (ClampMin = "0.0", Units = "cm"))
	float GroundPlaneMaxFitError = 0.5f;
	
	// 목표 간격 주변에서 보정을 생략할 범위. 단위: cm
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground|Height", meta = (ClampMin = "0.0", Units = "cm"))
	float GroundHeightTolerance = 0.2f;

	// 일반적인 높이 보정의 최대 이동 속도. 단위: cm/s
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground|Height", meta = (ClampMin = "1.0", Units = "cm/s"))
	float GroundHeightCorrectionSpeed = 40.f;

	// 유지하려는 최소 안전 간격. 단위: cm
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground|Height", meta = (ClampMin = "0.0", Units = "cm"))
	float GroundMinClearance = 0.2f;

	// 접지 검사 거리의 끝까지 간격이 벌어지지 않도록 남길 여유.
	// 단위: cm
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground|Height", meta = (ClampMin = "0.01", Units = "cm"))
	float GroundProbeSafetyMargin = 1.f;
	
	// 차량 힘 작용점
	// CollisionRoot 로컬 X 기준. 앞쪽은 양수, 뒤쪽은 음수다.
	UPROPERTY(EditDefaultsOnly, Category = "MW|Drive|Geometry", meta = (Units = "cm"))
	float FrontForceOffsetCm = 50.f;

	UPROPERTY(EditDefaultsOnly, Category = "MW|Drive|Geometry", meta = (Units = "cm"))
	float RearForceOffsetCm = -50.f;
	
	// 저속에서 조향 입력이 최대일 때 허용하는 앞바퀴 각도다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steering", meta = (ClampMin = "0.0", ClampMax = "89.0", Units = "deg"))
	float LowSpeedMaxSteerAngleDegrees = 30.f;

	// 고속에서는 같은 입력에 앞바퀴가 덜 꺾이도록 최대 각도를 낮춘다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steering", meta = (ClampMin = "0.0", ClampMax = "89.0", Units = "deg"))
	float HighSpeedMaxSteerAngleDegrees = 12.f;

	// 이 노면 접선 속력까지는 저속 최대 조향각을 사용한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steering", meta = (ClampMin = "0.0", Units = "cm/s"))
	float SteerAngleBlendStartSpeed = 500.f;

	// 이 속력부터는 고속 최대 조향각을 사용한다. 두 속력 사이에서는 각도를 보간한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steering", meta = (ClampMin = "0.0", Units = "cm/s"))
	float SteerAngleBlendEndSpeed = 1800.f;

	// 앞쪽 미끄러짐 각도 1 rad당 요청할 횡가속도. 단위는 cm/s²/rad다.
	// 실제 횡가속도는 앞쪽에 배정된 타이어 접지 한계 안에서 적용한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grip", meta = (ClampMin = "0.0"))
	float FrontCorneringStiffness = 4500.f;

	// 뒤쪽 미끄러짐 각도 1 rad당 요청할 횡가속도. 단위는 cm/s²/rad다.
	// 앞쪽 값과 독립적으로 조정해 코너에서 뒤가 얼마나 따라오는지 맞춘다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grip", meta = (ClampMin = "0.0"))
	float RearCorneringStiffness = 4500.f;

	// 미끄러짐 각도를 구할 때 타이어 종방향 속력에 적용하는 하한이다.
	// 정지 부근에서 작은 횡속도가 큰 미끄러짐 각도로 해석되는 것을 줄인다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grip", meta = (ClampMin = "1.0", Units = "cm/s"))
	float SlipReferenceSpeed = 200.f;

	// 차체의 수직축 회전 관성을 질량으로 나눈 값. 단위는 cm²다.
	// 값이 클수록 같은 타이어 힘이 만들어 내는 차체 각가속도가 작아진다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steering", meta = (ClampMin = "1.0"))
	float YawInertiaPerMass = 2500.f;
};

UCLASS()
class MW_API UMWVehicleDriveData : public UDataAsset
{
	GENERATED_BODY()

public:
	const FMWVehicleDriveConfig& GetConfig() const
	{
		return Config;
	}

	bool ValidateConfig(TArray<FText>& OutErrors) const;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drive", meta = (AllowPrivateAccess = "true"))
	FMWVehicleDriveConfig Config;
};
