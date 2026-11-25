#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "MWTrackSelectionTypes.generated.h"

class AActor;

namespace MWTrackSelection
{
	inline constexpr int32 Columns = 3;
	inline constexpr int32 Rows = 2;
	inline constexpr int32 OfferCount = Columns * Rows;
}

// 이번 선택판에 제시한 후보 한 칸
USTRUCT(BlueprintType)
struct MW_API FMWTrackOffer
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 OfferId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly)
	TSoftClassPtr<AActor> TrackClass;

	UPROPERTY(BlueprintReadOnly)
	int32 BoardSlotIndex = INDEX_NONE;

	// 서버가 확정한 소유자다. 후보 데이터는 배치 단계에서도 사용하므로 그대로 보관한다.
	UPROPERTY(BlueprintReadOnly)
	int32 ClaimedByMemberId = INDEX_NONE;
};

// 현재 트랙 선택 기능의 공개 정보
USTRUCT(BlueprintType)
struct MW_API FMWTrackSelectionSession
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 SelectionSessionId = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 OffersRevision = 0;

	UPROPERTY(BlueprintReadOnly)
	bool bIsOpen = false;

	// 같은 라운드에서 현재 참가자 모두가 이번 배치를 되돌리고 다시 후보를 고른다.
	UPROPERTY(BlueprintReadOnly)
	bool bIsPlacementReselection = false;

	// 이미 배치한 참가자의 PlayerState가 늦게 복제돼도 다시 선택할 대상을 즉시 구분한다.
	UPROPERTY()
	TArray<int32> ReselectionMemberIds;

	UPROPERTY(BlueprintReadOnly)
	TArray<FMWTrackOffer> Offers;

	// 참가자에게 보여 줄 후보가 모두 준비될 때까지 같은 선택 세션을 유지한다.
	bool IsWaitingForReselectionOffers() const { return bIsOpen && bIsPlacementReselection && Offers.IsEmpty(); }
};

// 플레이어 한 명의 선택 상태
USTRUCT(BlueprintType)
struct MW_API FMWPlayerTrackSelection
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 SelectionSessionId = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 OffersRevision = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 HoveredOfferId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly)
	int32 SelectedOfferId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly)
	bool bConfirmed = false;
};

// 월드 커서 표시는 Hover/확정 상태와 별도로 복제한다.
USTRUCT(BlueprintType)
struct MW_API FMWPlayerConstructionCursor
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 SelectionSessionId = 0;
	UPROPERTY(BlueprintReadOnly) int32 OffersRevision = 0;
	UPROPERTY(BlueprintReadOnly) FVector_NetQuantize WorldPosition = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) bool bIsVisible = false;
};

UENUM(BlueprintType)
enum class EMWTrackSelectionRejectReason : uint8
{
	None,
	Unavailable,
	NotOpen,
	StaleSession,
	StaleOffers,
	NotParticipant,
	InvalidOffer,
	AlreadyConfirmed,
	InvalidRequest,
	RequestConflict,
	TooManyRequests,
	OfferTaken UMETA(DisplayName = "다른 플레이어가 먼저 선택했습니다")
};

USTRUCT(BlueprintType)
struct MW_API FMWTrackSelectionReply
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 SelectionSessionId = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 OffersRevision = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 RequestId = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 OfferId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly)
	bool bAccepted = false;

	UPROPERTY(BlueprintReadOnly)
	EMWTrackSelectionRejectReason Reason = EMWTrackSelectionRejectReason::None;
};
