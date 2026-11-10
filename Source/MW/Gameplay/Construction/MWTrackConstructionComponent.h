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

	// BuildFrame은 시작 도로의 중앙, +X는 주행 방향이다. 도로 폭과 길이는 원본 BP를 따른다.
	bool BeginPlacement(const FTransform& BuildFrame, double LayoutScale, FText& OutError);
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
		FMWTrackPortFrames Ports;
		bool bEnableCollision = false;
		bool bEnableTick = false;
		int32 LastPreviewSequence = 0;
		double NextPreviewTime = 0.0;
	};

	UPROPERTY(ReplicatedUsing = OnRep_PlacementSession)
	FMWTrackPlacementSession PlacementSession;
	TMap<TWeakObjectPtr<AMWPlayerState>, FPreparedTrack> PendingTracks;
	bool bCommittingPlacement = false;

	UFUNCTION()
	void OnRep_PlacementSession();

	bool PrepareTrack(FGameplayTag DefinitionId, const FVector& Origin, FPreparedTrack& OutTrack, FText& OutError);
	bool PublishTrack(const FPreparedTrack& Prepared, const FTransform& Transform);
	FPreparedTrack* FindPendingTrack(APlayerController* Requester, int32 SessionId);
	void FinishPlacementIfReady();

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
