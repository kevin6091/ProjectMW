#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
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
	
	// 다음 이동 단계에서 이전 접지면으로 사용할 법선.
	UPROPERTY()
	FVector_NetQuantizeNormal GroundNormal = FVector::UpVector;
};
