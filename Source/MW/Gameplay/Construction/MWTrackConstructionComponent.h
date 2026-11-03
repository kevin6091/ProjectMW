#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Gameplay/Construction/MWTrackSelectionTypes.h"
#include "MWTrackConstructionComponent.generated.h"

class APlayerController;
class AMWPlayerState;
class UMWPlayerConstructionComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMWTrackConstructionSessionChanged);

// GameState에 상주하는 선택 기능의 서버 진입점. 공용 후보·참가자·승인 규칙을 관리한다.
// 실제 코스 배치 Transform이나 전체 경기 진행 상태를 저장하지 않는다.
UCLASS(BlueprintType, ClassGroup=(MW))
class MW_API UMWTrackConstructionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMWTrackConstructionComponent();
	
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
	UFUNCTION(BlueprintPure, Category = "MW|Construction")
	FMWTrackSelectionSession GetSelectionSession() const { return SelectionSession; }
	
	// 각 컴퓨터 내부의 변경 알림이다. 네트워크 전달은 SelectionSession 속성 복제가 담당한다.
	UPROPERTY(BlueprintAssignable, Category = "MW|Construction")
	FMWTrackConstructionSessionChanged OnSelectionSessionChanged;
	
	// 서버의 외부 진행 코드가 호출하는 일반 C++ API.
	bool BeginSelection(const TArray<FGameplayTag>& DefinitionIds, FText& OutError);
	// 접수만 닫고 후보와 확정 결과는 남긴다. 다음 단계로의 이동은 호출자가 결정한다.
	void EndSelection();
	
	// 소유 PlayerController의 서버 RPC에서만 요청자를 넘긴다.
	void UpdateHover(APlayerController* Requester, int32 SessionId, int32 Revision, int32 OfferId);
	// 커서는 후보 밖에서도 표시하며, 선택 확정 뒤에도 접수가 열린 동안 계속 공유할 수 있다.
	void UpdateCursor(APlayerController* Requester, int32 SessionId, int32 Revision,
		int32 Sequence, bool bVisible, const FVector& WorldPosition);
	FMWTrackSelectionReply ConfirmSelection(APlayerController* Requester, int32 SessionId, int32 Revision, int32 OfferId, int32 RequestId);
	
private:
	// 모든 클라이언트가 같은 후보를 로컬 화면에 재구성하는 데 필요한 공개 데이터.
	UPROPERTY(ReplicatedUsing=OnRep_SelectionSession)
	FMWTrackSelectionSession SelectionSession;

	UFUNCTION()
	void OnRep_SelectionSession();

	bool HasServerAuthority() const;
	// 요청의 공통 검사. bAllowNone은 Hover 해제·커서 공유처럼 특정 후보가 필요 없는 경우에 허용한다.
	bool ValidateRequest(APlayerController* Requester,
		int32 SessionId, int32 Revision, int32 OfferId, bool bAllowNone,
		UMWPlayerConstructionComponent*& OutPlayer,
		EMWTrackSelectionRejectReason& OutReason) const;

	// 아래 값들은 서버 내부 관리용이며 복제하지 않는다.
	int32 NextSelectionSessionId = 0;
	bool bPreparingSelection = false;
	// 시작 시점의 참가자 목록을 고정한다. Weak 참조로 퇴장한 PlayerState의 수명을 늘리지 않는다.
	TSet<TWeakObjectPtr<AMWPlayerState>> Participants;
};
