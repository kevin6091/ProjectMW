#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MWTrackSelectionTypes.generated.h"

// 선택 기능의 데이터 계약. 전체 경기 Phase나 라운드 진행 상태는 외부 진행 코드가 관리한다.
// 이번 선택판에 제시한 후보 한 칸. 같은 트랙 정의도 서로 다른 후보 번호로 제시할 수 있다.
// 한 유형의 서로 다른 트랙은 각자 고유 DefinitionAssetId로 구분한다.
USTRUCT(BlueprintType)
struct MW_API FMWTrackOffer
{
	GENERATED_BODY()

	// 이번 선택 안에서 식별하는 후보 번호. 개별 트랙 정의를 식별하는 에셋 ID와 구분한다.
	UPROPERTY(BlueprintReadOnly)
	int32 OfferId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly)
	FGameplayTag DefinitionAssetId; // AssetManager에서 트랙 정의를 찾는 태그.

	UPROPERTY(BlueprintReadOnly)
	int32 BoardSlotIndex = INDEX_NONE; // 선택판 Anchor 배열의 인덱스.
};

// GameState 컴포넌트가 복제하는 공용 정보. 개인 상태도 동일한 세션·버전인지 확인한 뒤 사용한다.
USTRUCT(BlueprintType)
struct MW_API FMWTrackSelectionSession
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 SelectionSessionId = 0; // 선택 시작마다 새로 발급한다. 온라인 방 세션 ID와는 별개다.

	UPROPERTY(BlueprintReadOnly)
	int32 OffersRevision = 0; // 후보 구성의 버전. 현재 구현은 새 선택마다 1로 시작한다.

	UPROPERTY(BlueprintReadOnly)
	bool bIsOpen = false; // 새 Hover·확정 요청을 접수하는지 나타낸다.

	UPROPERTY(BlueprintReadOnly)
	TArray<FMWTrackOffer> Offers;
};

// PlayerState 컴포넌트가 다른 플레이어에게도 복제하는 개인 선택 상태.
USTRUCT(BlueprintType)
struct MW_API FMWPlayerTrackSelection
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 SelectionSessionId = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 OffersRevision = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 HoveredOfferId = INDEX_NONE; // INDEX_NONE이면 어느 후보도 가리키지 않는 상태다.

	UPROPERTY(BlueprintReadOnly)
	int32 SelectedOfferId = INDEX_NONE; // 서버가 확정한 후보. 선택 접수가 끝나도 보존한다.

	UPROPERTY(BlueprintReadOnly)
	bool bConfirmed = false; // 확정 뒤에는 다른 후보로 변경할 수 없다.
};

// 서버 판정 결과. Controller가 사용자에게 보여줄 안내 문구로 변환한다.
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

// 개인 상태 복제와 별도로, 요청한 소유자에게만 RPC로 돌려주는 처리 결과.
// 세션·버전·요청·후보를 함께 돌려줘 늦게 도착한 이전 응답을 구분한다.
USTRUCT(BlueprintType)
struct MW_API FMWTrackSelectionReply
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 SelectionSessionId = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 OffersRevision = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 RequestId = 0; // 같은 요청을 다시 받으면 서버가 저장된 응답을 반환한다.

	UPROPERTY(BlueprintReadOnly)
	int32 OfferId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly)
	bool bAccepted = false;

	UPROPERTY(BlueprintReadOnly)
	EMWTrackSelectionRejectReason Reason = EMWTrackSelectionRejectReason::None;
};
