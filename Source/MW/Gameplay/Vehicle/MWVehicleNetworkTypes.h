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

	// 스냅샷 시점의 발판 이동량도 보내 수신 월드의 현재 발판 위치를 기준으로 차량 위치를 복원한다.
	UPROPERTY()
	TWeakObjectPtr<AActor> TrackLiftBase;

	UPROPERTY()
	FVector_NetQuantize10 TrackLiftOffset = FVector::ZeroVector;

	UPROPERTY()
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY()
	FVector_NetQuantize10 Velocity = FVector::ZeroVector;

	UPROPERTY()
	bool bGrounded = false;
};
