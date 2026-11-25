#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "UObject/ObjectKey.h"
#include "MWVehicleDriveInput.h"
#include "MWVehicleNetworkTypes.generated.h"

inline constexpr float MWVehicleImpactYawDamping = 8.f;

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

	// 서버의 주행 허용과 해당 경기 세대도 위치·스트림과 함께 전달한다.
	UPROPERTY()
	bool bDriveAllowed = false;

	// 완주 후 충돌 해제와 다음 준비 위치에서의 복원을 이동 상태와 함께 전달한다.
	UPROPERTY()
	bool bFinishCollisionDisabled = false;

	UPROPERTY()
	int32 RoundEpoch = 0;

	UPROPERTY()
	FVector_NetQuantize10 Location = FVector::ZeroVector;

	UPROPERTY()
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY()
	FVector_NetQuantize10 Velocity = FVector::ZeroVector;

	UPROPERTY()
	bool bGrounded = false;

	// 보정 재실행의 이전 접촉면도 위치·속도와 같은 스냅샷으로 복원한다.
	UPROPERTY()
	FVector_NetQuantizeNormal GroundNormal = FVector::UpVector;

	UPROPERTY()
	float ImpactYawRate = 0.f;

	// 위치 보정과 함께 복원하는 바퀴 지지 높이의 속도(cm/s)다.
	UPROPERTY()
	float SuspensionSpeed = 0.f;

	UPROPERTY()
	double ServerTimeSeconds = 0.0;

	// 스트림·ACK·위치·속도가 서로 다른 전송 시점의 값으로 섞이지 않도록 한 묶음으로 복제한다.
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
};

template<>
struct TStructOpsTypeTraits<FMWVehicleDriveSnapshot> : TStructOpsTypeTraitsBase2<FMWVehicleDriveSnapshot>
{
	enum { WithNetSerializer = true };
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
	// 새 서버 자료로 상대 위치가 바뀐 양도 연속 Sweep에 포함한다. 충격 계산에는 Velocity만 쓴다.
	FVector CorrectionDelta = FVector::ZeroVector;
	float ImpactYawRate = 0.f;
	FVector Extent;
	FMWVehicleContactTuning Tuning;
	// 불변 원본을 남겨 보정 재실행 때 상대 상태를 같은 예측 시각으로 다시 평가한다.
	double SourceTime = 0.0;
	FVector SourceLocation = FVector::ZeroVector;
	FVector SourceVelocity = FVector::ZeroVector;
	FQuat SourceRotation = FQuat::Identity;
};

struct FMWVehiclePredictedImpact
{
	int32 SampleIndex = INDEX_NONE;
	double Fraction = 0.0;
	FVector OtherVelocityDelta = FVector::ZeroVector;
	FVector Normal = FVector::ZeroVector;
	float OtherYawRateDelta = 0.f;
};
