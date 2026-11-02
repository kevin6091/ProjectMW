#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Gameplay/Construction/MWTrackSelectionTypes.h"
#include "MWPlayerConstructionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMWPlayerConstructionChanged);

// PlayerState에 기본 서브오브젝트로 한 번 생성한다.
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
	void ResetSelectionForServer(int32 SessionId, int32 Revision);
	void ApplySelectionForServer(const FMWPlayerTrackSelection& NewSelection);
	bool FindCachedReply(int32 SessionId, int32 RequestId, FMWTrackSelectionReply& OutReply) const;
	bool CacheReplyForServer(const FMWTrackSelectionReply& Reply);
	int32 GetCachedReplyCount() const { return ReplyCache.Num(); }
	bool ConsumeHoverBudgetForServer();

	// 초기 시험용 상한이다. 실측 후 설정 데이터로 이동할 수 있다.
	static constexpr int32 MaxCachedReplies = 32;

private:
	UPROPERTY(ReplicatedUsing=OnRep_Selection)
	FMWPlayerTrackSelection Selection;

	UFUNCTION()
	void OnRep_Selection();

	int32 CachedSessionId = 0;
	TMap<int32, FMWTrackSelectionReply> ReplyCache;
	double HoverTokens = 20.0;
	double LastHoverBudgetTime = 0.0;
};
