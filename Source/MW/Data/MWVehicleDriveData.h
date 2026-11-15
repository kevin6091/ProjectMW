

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
