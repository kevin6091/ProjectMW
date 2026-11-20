#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Gameplay/Construction/MWTrackSnap.h"
#include "MWTrackPlacementViewComponent.generated.h"

class AMWGameplayPlayerController;
class UEnhancedInputComponent;
class UMWInputData;

// 소유 Controller에서만 입력을 받고, 현재 화면의 미리보기 Actor를 관리한다.
UCLASS()
class UMWTrackPlacementViewComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	void Update(float DeltaSeconds, UEnhancedInputComponent* Input);
	void ResetView();
    // 첫 화면 준비 검사에는 현재 배치 세션에서 그리는 해당 참가자의 고스트만 제공한다.
    AActor* GetArrivalGhost(int32 ExpectedSession, int32 MemberId) const;
	void HandlePlacementReply(int32 ReplySessionId, EMWTrackPlacementResult Result);

private:
	struct FGhostSnapBlend
	{
		FTransform Start;
		FTransform Target;
		double ElapsedSeconds = 0.0;
	};

	struct FGhost
	{
		TOptional<FGhostSnapBlend> SnapBlend;
		TOptional<FTransform> PreviousSnapTarget;
		TWeakObjectPtr<AActor> Actor;
		FMWTrackPortFrames Ports;
		bool bSnapped = false;
	};

	FGhost* GetOrCreateGhost(int32 PlayerId, const FMWPlayerTrackPlacement& Player, bool bLocal);
	void BindInput(UEnhancedInputComponent* Input);
	void ClearInput();
	void HandleTurn();
	void HandleAccept();
	bool UpdateLocalPreview(AMWGameplayPlayerController& PC, const FMWTrackPlacementSession& Session, FGhost& Ghost, float DeltaSeconds);
	bool ResolveGhostFromPointer(const AMWGameplayPlayerController& PC, const FMWTrackPlacementSession& Session,
		const FGhost& Ghost, FTransform& OutTarget, bool& bOutSnapped);
	FTransform BlendGhostPose(FGhost& Ghost, const FTransform& Target, bool bSnapped, float DeltaSeconds, double FreeMoveAlpha) const;
	void SetGhostSnapColor(FGhost& Ghost, bool bSnapped) const;

	TMap<int32, FGhost> Ghosts;
	TWeakObjectPtr<UEnhancedInputComponent> BoundInput;
	UPROPERTY(Transient) TObjectPtr<UMWInputData> InputData;
	TArray<uint32> InputBindings;
	FMWTrackPlacementIntent Intent;
	int32 SessionId = 0;
	int32 PreviewSequence = 0;
	double NextPreviewTime = 0.0;
	bool bTriedLoadingInput = false;
	bool bWaitingForReply = false;
	bool bAccepted = false;
	EMWTrackPlacementResult LastResult = EMWTrackPlacementResult::Accepted;
};
