#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Gameplay/Construction/MWTrackSnap.h"
#include "MWTrackPlacementViewComponent.generated.h"

class AMWGameplayPlayerController;
class UEnhancedInputComponent;
class UMaterialInterface;
class UMWInputData;

// 소유 Controller에서만 입력을 받고, 현재 화면의 미리보기 Actor를 관리한다.
UCLASS()
class UMWTrackPlacementViewComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	void Update(float DeltaSeconds, UEnhancedInputComponent* Input);
	void ResetView();
	void SetDisplayMaterial(UMaterialInterface* Material);
	void HandlePlacementReply(int32 ReplySessionId, EMWTrackPlacementResult Result);

private:
	struct FGhost
	{
		TWeakObjectPtr<AActor> Actor;
		FMWTrackPortFrames Ports;
		bool bSnapped = false;
	};

	FGhost* GetOrCreateGhost(int32 PlayerId, const FMWPlayerTrackPlacement& Player, bool bLocal);
	void BindInput(UEnhancedInputComponent* Input);
	void ClearInput();
	void HandleTurn();
	void HandleAccept();
	bool UpdatePointer(const AMWGameplayPlayerController& PC, const FMWTrackPlacementSession& Session);
	void UpdateLocalGhost(AMWGameplayPlayerController& PC, const FMWTrackPlacementSession& Session, FGhost& Ghost);
	void SetGhostSnapColor(FGhost& Ghost, bool bSnapped) const;

	TMap<int32, FGhost> Ghosts;
	TWeakObjectPtr<UMaterialInterface> DisplayMaterial;
	TWeakObjectPtr<UEnhancedInputComponent> BoundInput;
	UPROPERTY(Transient) TObjectPtr<UMWInputData> InputData;
	TArray<uint32> InputBindings;
	FMWTrackPlacementIntent Intent;
	int32 SessionId = 0;
	int32 DisplayedRevision = 0;
	int32 PreviewSequence = 0;
	double NextPreviewTime = 0.0;
	bool bTriedLoadingInput = false;
	bool bCanPlace = false;
	bool bWaitingForReply = false;
	bool bAccepted = false;
	EMWTrackPlacementResult LastResult = EMWTrackPlacementResult::Accepted;
};
