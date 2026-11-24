#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Gameplay/Construction/MWTrackSnap.h"
#include "Gameplay/Construction/MWTrackSelectionTypes.h"
#include "MWTrackConstructionComponent.generated.h"

class APlayerController;
class AMWPlayerState;
class UMWPlayerConstructionComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMWTrackConstructionSessionChanged);

DECLARE_MULTICAST_DELEGATE_OneParam(FMWAllSelectionsConfirmed, int32);

DECLARE_MULTICAST_DELEGATE_TwoParams(FMWPlacementFinished, int32, int32);

DECLARE_MULTICAST_DELEGATE_TwoParams(FMWConstructionFailed, int32, const FString&);

UCLASS(BlueprintType, ClassGroup=(MW))
class MW_API UMWTrackConstructionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMWTrackConstructionComponent();
	
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	UFUNCTION(BlueprintPure, Category = "MW|Construction")
	FMWTrackSelectionSession GetSelectionSession() const { return SelectionSession; }
	
	UPROPERTY(BlueprintAssignable, Category = "MW|Construction")
	FMWTrackConstructionSessionChanged OnSelectionSessionChanged;
	
	// 서버의 외부 진행 코드가 호출하는 일반 C++ API.
	bool BeginSelection(
		const TArray<AMWPlayerState*>& SelectionPlayers, 
		FText& OutError);
	void EndSelection();
	// Hover와 커서 갱신에는 호출되지 않는 서버 진행 이벤트다.
	FMWAllSelectionsConfirmed OnAllSelectionsConfirmed;
	FMWPlacementFinished OnPlacementFinished;
	FMWConstructionFailed OnConstructionFailed;
	// 호스트 설치와 원격 복제에서 로컬 표현이 같은 확정 목록을 읽는다.
	FSimpleMulticastDelegate OnPlacementSessionChanged;
	
	// 소유 PlayerController의 서버 RPC에서만 요청자를 넘긴다.
	void UpdateHover(APlayerController* Requester, int32 SessionId, int32 Revision, int32 OfferId);
	void UpdateCursor(APlayerController* Requester, int32 SessionId, int32 Revision,
		int32 Sequence, bool bVisible, const FVector& WorldPosition);
	FMWTrackSelectionReply ConfirmSelection(APlayerController* Requester, int32 SessionId, int32 Revision, int32 OfferId, int32 RequestId);

	UFUNCTION(BlueprintPure, Category = "MW|Construction")
	const FMWTrackPlacementSession& GetPlacementSession() const { return PlacementSession; }

	// BuildFrame은 레벨의 출발 그리드이며 +X가 주행 방향이다.
	bool BeginPlacement(const FTransform& BuildFrame, double LayoutScale, FText& OutError);
	void CancelPlacement();
	void UpdatePlacementPreview(APlayerController* Requester, int32 SessionId, int32 Sequence,
		const FMWTrackPlacementIntent& Intent);
	EMWTrackPlacementResult PlaceTrack(APlayerController* Requester, int32 SessionId, int32 CourseRevision,
		const FMWTrackPlacementIntent& Intent);
	void RemovePlacementPlayer(AMWPlayerState* Player);
	bool IsAllSelectionsConfirmed() const;
	bool IsCourseReadyForRace(int32 SessionId, int32 CourseRevision) const;
	
private:
	struct FPreparedTrack
	{
		TWeakObjectPtr<AActor> Actor;
		FMWTrackPortFrames Ports;
		TArray<FVector> GroundPoints;
		bool bEnableCollision = false;
		bool bEnableTick = false;
		int32 LastPreviewSequence = 0;
		double NextPreviewTime = 0.0;

		bool SetTransform(const FTransform& Transform) const;
		void Destroy() const;
	};

	UPROPERTY(ReplicatedUsing = OnRep_PlacementSession)
	FMWTrackPlacementSession PlacementSession;
	TMap<TWeakObjectPtr<AMWPlayerState>, FPreparedTrack> PendingTracks;
	FPreparedTrack PendingEndCap;
	// 종료 캡 직전 도로의 열린 끝. 다음 라운드에 종료 캡만 제거하고 여기부터 확장한다.
	FMWOpenTrackEnd NextRoundEnd;
	bool bCommittingPlacement = false;

	UFUNCTION()
	void OnRep_PlacementSession();

	bool PrepareTrack(const TSoftClassPtr<AActor>& TrackClass, const FVector& Origin, bool bSinglePort,
		FPreparedTrack& OutTrack, FText& OutError);
	void PublishTrack(const FPreparedTrack& Prepared, const FTransform& Transform, FMWTrackEndId ConnectedTo = {}, int32 PortIndex = INDEX_NONE);
	FPreparedTrack* FindPendingTrack(APlayerController* Requester, int32 SessionId);
	EMWTrackPlacementResult RecoverBlockedTrack(AMWPlayerState& Player, FPreparedTrack& Prepared);
	bool PositionEndCap(const FMWOpenTrackEnd& Tail, FTransform& OutTransform) const;
	void FinishPlacement(const FMWOpenTrackEnd& Tail, const FTransform& EndTransform);

	UPROPERTY(ReplicatedUsing=OnRep_SelectionSession)
	FMWTrackSelectionSession SelectionSession;

	UFUNCTION()
	void OnRep_SelectionSession();

	bool HasServerAuthority() const;
	bool ValidateRequest(APlayerController* Requester,
		int32 SessionId, int32 Revision, int32 OfferId, bool bAllowNone,
		UMWPlayerConstructionComponent*& OutPlayer,
		EMWTrackSelectionRejectReason& OutReason) const;

	int32 NextSelectionSessionId = 0;
	bool bPreparingSelection = false;
	TSet<TWeakObjectPtr<AMWPlayerState>> Participants;
};
