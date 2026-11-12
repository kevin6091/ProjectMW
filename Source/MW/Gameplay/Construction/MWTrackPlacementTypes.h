#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "GameplayTagContainer.h"
#include "MWTrackPlacementTypes.generated.h"

class AActor;

USTRUCT(BlueprintType)
struct FMWTrackCourseEnds
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Track", meta = (Categories = "Asset.Track"))
	FGameplayTag StartDefinitionId;

	UPROPERTY(EditAnywhere, Category = "Track", meta = (Categories = "Asset.Track"))
	FGameplayTag EndDefinitionId;
};

USTRUCT(BlueprintType)
struct FMWTrackEndId
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 PieceId = 0;
	UPROPERTY(BlueprintReadOnly) int32 PortIndex = INDEX_NONE;

	bool IsValid() const { return PieceId > 0 && (PortIndex == 0 || PortIndex == 1); }
	bool operator==(const FMWTrackEndId& Other) const
	{
		return PieceId == Other.PieceId && PortIndex == Other.PortIndex;
	}
};

USTRUCT(BlueprintType)
struct FMWOpenTrackEnd
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FMWTrackEndId Id;
	// +X는 코스 바깥, +Z는 도로 위쪽이다.
	UPROPERTY(BlueprintReadOnly) FTransform WorldFrame = FTransform::Identity;
};

USTRUCT(BlueprintType)
struct FMWPlacedTrack
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 PieceId = 0;
	UPROPERTY(BlueprintReadOnly) FGameplayTag DefinitionAssetId;
	UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> Actor;
	UPROPERTY(BlueprintReadOnly) FTransform WorldTransform = FTransform::Identity;
	UPROPERTY(BlueprintReadOnly) FMWTrackEndId ConnectedTo;
	UPROPERTY(BlueprintReadOnly) int32 ConnectedPortIndex = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct FMWTrackPlacementSession
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 SessionId = 0;
	UPROPERTY(BlueprintReadOnly) int32 CourseRevision = 0;
	UPROPERTY(BlueprintReadOnly) bool bIsOpen = false;
	UPROPERTY(BlueprintReadOnly) FVector BuildOrigin = FVector::ZeroVector;
	// 기준 트랙을 출발 슬롯 방향으로 돌린 각도를 공용 미리보기의 기본 방향으로 쓴다.
	UPROPERTY(BlueprintReadOnly) double PreviewYaw = 0.0;
	UPROPERTY(BlueprintReadOnly) double LayoutScale = 1.0;
	UPROPERTY(BlueprintReadOnly) FBox CourseBounds = FBox(ForceInit);
	UPROPERTY(BlueprintReadOnly) TArray<FMWOpenTrackEnd> OpenEnds;
	UPROPERTY(BlueprintReadOnly) TArray<FMWPlacedTrack> PlacedTracks;
};

USTRUCT(BlueprintType)
struct FMWPlayerTrackPlacement
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 SessionId = 0;
	UPROPERTY(BlueprintReadOnly) FGameplayTag DefinitionAssetId;
	UPROPERTY(BlueprintReadOnly) FTransform PreviewTransform = FTransform::Identity;
	UPROPERTY(BlueprintReadOnly) bool bSnapped = false;
	UPROPERTY(BlueprintReadOnly) bool bPlaced = false;
};

// 미리보기와 확정 요청에 동일한 입력을 사용한다. 최종 설치 Transform은 서버가 계산한다.
USTRUCT()
struct FMWTrackPlacementIntent
{
	GENERATED_BODY()

	UPROPERTY() FVector_NetQuantize CursorLocation = FVector::ZeroVector;
	UPROPERTY() bool bReversed = false;
	// 근처 끝점의 각도만 따라가는 상태와 실제 스냅 요청을 구분한다.
	UPROPERTY() bool bSnapRequested = false;
	UPROPERTY() FMWTrackEndId Target;
	UPROPERTY() int32 SourcePort = INDEX_NONE;
};

UENUM()
enum class EMWTrackPlacementResult : uint8
{
	Accepted,
	Unavailable,
	CourseChanged,
	NotSnapped
};
