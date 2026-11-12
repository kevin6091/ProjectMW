#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "UObject/ObjectKey.h"
#include "MWVehicleDriveInput.h"
#include "MWVehicleNetworkTypes.generated.h"

USTRUCT()
struct FMWVehicleMoveCommand
{
	GENERATED_BODY()

	UPROPERTY()
	uint32 MoveId = 0;

	UPROPERTY()
	FMWVehicleDriveInput Input;
};

USTRUCT()
struct FMWVehicleDriveSnapshot
{
	GENERATED_BODY()

	// 조종자 변경과 텔레포트 전후의 입력 이력을 구분한다.
	UPROPERTY()
	uint32 StreamId = 0;

	UPROPERTY()
	uint32 Revision = 0;

	UPROPERTY()
	uint32 LastProcessedMoveId = 0;

	UPROPERTY()
	FVector_NetQuantize10 Location = FVector::ZeroVector;

	UPROPERTY()
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY()
	FVector_NetQuantize10 Velocity = FVector::ZeroVector;

	UPROPERTY()
	bool bGrounded = false;

	UPROPERTY()
	float ImpactYawRate = 0.f;

	UPROPERTY()
	double ServerTimeSeconds = 0.0;
};

struct FMWVehicleContactTuning
{
	float Mass;
	float Restitution;
	float MomentumRetention;
	float MinBounceSpeed;
};

// 로컬 이력에만 저장한다. 보정 재실행에서 지금 보이는 상대 위치를 다시 읽지 않는다.
struct FMWVehicleContactSample
{
	FObjectKey Vehicle;
	uint32 StreamId;
	uint32 Revision;
	FVector Location;
	FQuat Rotation;
	FVector Velocity;
	FVector Extent;
	FMWVehicleContactTuning Tuning;
};

struct FMWVehiclePredictedImpact
{
	int32 SampleIndex = INDEX_NONE;
	double Fraction = 0.0;
	FVector OtherVelocityDelta = FVector::ZeroVector;
};
