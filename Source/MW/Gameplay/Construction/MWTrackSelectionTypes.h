#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MWTrackSelectionTypes.generated.h"

// 이번 선택판에 제시한 후보 한 칸
USTRUCT(BlueprintType)
struct MW_API FMWTrackOffer
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 OfferId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly)
	FGameplayTag DefinitionAssetId;

	UPROPERTY(BlueprintReadOnly)
	int32 BoardSlotIndex = INDEX_NONE;
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

	UPROPERTY(BlueprintReadOnly)
	TArray<FMWTrackOffer> Offers;
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
	TooManyRequests
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
