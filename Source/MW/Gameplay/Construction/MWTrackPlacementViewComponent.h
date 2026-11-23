#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Gameplay/Construction/MWTrackSnap.h"
#include "MWTrackPlacementViewComponent.generated.h"

class AMWGameplayPlayerController;
class UEnhancedInputComponent;
class UMWInputData;
class USceneComponent;

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
		bool bRotateAroundCenter = false;
	};

	struct FPointerSample
	{
		float X = 0.0f;
		int32 ViewportWidth = 0;
	};

	struct FHeldSway
	{
		TWeakObjectPtr<USceneComponent> Wireframes;
		FTransform RestPose;
		FVector Center;
		FVector Axis;
		TOptional<FPointerSample> PreviousPointer;
		float AngleDegrees = 0.0f;
	};

	struct FGhost
	{
		TOptional<FGhostSnapBlend> SnapBlend;
		TOptional<FHeldSway> HeldSway;
		TOptional<FTransform> PreviousSnapTarget;
		TWeakObjectPtr<AActor> Actor;
		FMWTrackPortFrames Ports;
		bool bSnapped = false;
		bool bLastRemoteReversed = false;
		bool bLastLocalReversed = false;
	};

	FGhost* GetOrCreateGhost(int32 PlayerId, const FMWPlayerTrackPlacement& Player, bool bLocal);
	void BindInput(UEnhancedInputComponent* Input);
	void ClearInput();
	void HandleTurn();
	void HandleAccept();
	bool UpdateLocalPreview(AMWGameplayPlayerController& PC, const FMWTrackPlacementSession& Session, FGhost& Ghost,
		float DeltaSeconds, TOptional<FPointerSample>& OutPointer);
	bool ResolveGhostFromPointer(const AMWGameplayPlayerController& PC, const FMWTrackPlacementSession& Session,
		const FGhost& Ghost, FTransform& OutTarget, bool& bOutSnapped, FPointerSample* OutPointer = nullptr);
	FTransform BlendGhostPose(FGhost& Ghost, const FTransform& Target, bool bSnapped, float DeltaSeconds,
		double FreeMoveAlpha, bool bTurned = false) const;
	void PulseSnapContact(const FGhost& Ghost, const FTransform& Target) const;
	void UpdateHeldSway(FGhost& Ghost, double ScreenDelta, float DeltaSeconds) const;
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
