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
		const TArray<FGameplayTag>& DefinitionIds,
		const TArray<AMWPlayerState*>& SelectionPlayers, 
		FText& OutError);
	void EndSelection();
	// Hover와 커서 갱신에는 호출되지 않는 서버 진행 이벤트다.
	FSimpleMulticastDelegate OnAllSelectionsConfirmed;
	FSimpleMulticastDelegate OnPlacementFinished;
	
	// 소유 PlayerController의 서버 RPC에서만 요청자를 넘긴다.
	void UpdateHover(APlayerController* Requester, int32 SessionId, int32 Revision, int32 OfferId);
	void UpdateCursor(APlayerController* Requester, int32 SessionId, int32 Revision,
		int32 Sequence, bool bVisible, const FVector& WorldPosition);
	FMWTrackSelectionReply ConfirmSelection(APlayerController* Requester, int32 SessionId, int32 Revision, int32 OfferId, int32 RequestId);

	UFUNCTION(BlueprintPure, Category = "MW|Construction")
	const FMWTrackPlacementSession& GetPlacementSession() const { return PlacementSession; }

	// BuildFrame은 레벨의 출발 그리드이며 +X가 주행 방향이다.
	bool BeginPlacement(const FMWTrackCourseEnds& CourseEnds, const FTransform& BuildFrame, double LayoutScale, FText& OutError);
	void CancelPlacement();
	void UpdatePlacementPreview(APlayerController* Requester, int32 SessionId, int32 Sequence,
		const FMWTrackPlacementIntent& Intent);
	EMWTrackPlacementResult PlaceTrack(APlayerController* Requester, int32 SessionId, int32 CourseRevision,
		const FMWTrackPlacementIntent& Intent);
	void RemovePlacementPlayer(AMWPlayerState* Player);
	
private:
	struct FPreparedTrack
	{
		TWeakObjectPtr<AActor> Actor;
		FGameplayTag DefinitionId;
		FMWTrackPortFrames Ports;
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
	bool bCommittingPlacement = false;

	UFUNCTION()
	void OnRep_PlacementSession();

	bool PrepareTrack(FGameplayTag DefinitionId, const FVector& Origin, bool bSinglePort, FPreparedTrack& OutTrack, FText& OutError);
	void PublishTrack(const FPreparedTrack& Prepared, const FTransform& Transform, FMWTrackEndId ConnectedTo = {}, int32 PortIndex = INDEX_NONE);
	FPreparedTrack* FindPendingTrack(APlayerController* Requester, int32 SessionId);
	bool PositionEndCap(const FMWOpenTrackEnd& Tail, FTransform& OutTransform) const;
	void FinishPlacement(const FMWOpenTrackEnd& Tail, const FTransform& EndTransform);

	UPROPERTY(ReplicatedUsing=OnRep_SelectionSession)
	FMWTrackSelectionSession SelectionSession;

	UFUNCTION()
	void OnRep_SelectionSession();

	bool HasServerAuthority() const;
	bool AreAllSelectionsConfirmed() const;
	bool ValidateRequest(APlayerController* Requester,
		int32 SessionId, int32 Revision, int32 OfferId, bool bAllowNone,
		UMWPlayerConstructionComponent*& OutPlayer,
		EMWTrackSelectionRejectReason& OutReason) const;

	int32 NextSelectionSessionId = 0;
	bool bPreparingSelection = false;
	TSet<TWeakObjectPtr<AMWPlayerState>> Participants;
};
