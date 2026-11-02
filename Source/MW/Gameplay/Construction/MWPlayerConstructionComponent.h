#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Gameplay/Construction/MWTrackSelectionTypes.h"
#include "MWPlayerConstructionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMWPlayerConstructionChanged);

// PlayerState에 기본 서브오브젝트로 한 번 생성한다. 다른 플레이어의 화면도 이 상태를 읽는다.
UCLASS(BlueprintType, ClassGroup=(MW))
class MW_API UMWPlayerConstructionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMWPlayerConstructionComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="MW|Construction")
	FMWPlayerTrackSelection GetSelection() const { return Selection; }

	UPROPERTY(BlueprintAssignable, Category="MW|Construction")
	FMWPlayerConstructionChanged OnSelectionChanged;

	// 일반 서버 C++ API다. 클라이언트 호출용 RPC나 Blueprint setter가 아니다.
	// 새 선택의 ID·버전을 적용하면서 이전 응답 캐시와 Hover 예산도 함께 초기화한다.
	void ResetSelectionForServer(int32 SessionId, int32 Revision);
	// 값이 실제로 바뀔 때만 복제 갱신을 요청하고 Listen Host에 변경을 알린다.
	void ApplySelectionForServer(const FMWPlayerTrackSelection& NewSelection);
	// 중복 요청 판정은 현재 선택 세션과 RequestId를 기준으로 한다.
	bool FindCachedReply(int32 SessionId, int32 RequestId, FMWTrackSelectionReply& OutReply) const;
	bool CacheReplyForServer(const FMWTrackSelectionReply& Reply);
	int32 GetCachedReplyCount() const { return ReplyCache.Num(); }
	bool ConsumeHoverBudgetForServer();

	// 초기 시험용 상한이다. 실측 후 설정 데이터로 이동할 수 있다.
	static constexpr int32 MaxCachedReplies = 32;

private:
	// Hover와 확정 상태는 소유자뿐 아니라 다른 참가자에게도 공유한다.
	UPROPERTY(ReplicatedUsing=OnRep_Selection)
	FMWPlayerTrackSelection Selection;

	UFUNCTION()
	void OnRep_Selection();

	// 응답 캐시와 요청 예산은 서버 전용이다. UI용 선택 상태와 함께 복제하지 않는다.
	int32 CachedSessionId = 0;
	TMap<int32, FMWTrackSelectionReply> ReplyCache;
	// 최대 20개를 보관하고 초당 20개를 회복하는 토큰 방식의 Hover 처리 예산.
	double HoverTokens = 20.0;
	double LastHoverBudgetTime = 0.0;
};
