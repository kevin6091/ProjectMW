#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Construction/MWTrackSelectionTypes.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "MWTrackPreviewPreload.generated.h"

struct FStreamableHandle;
class UMaterialInterface;

// 로비에서 카탈로그를 준비하고 경기 입장 시 서버가 뽑은 후보를 먼저 읽는다.
UCLASS()
class UMWTrackPreviewPreload : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void PlayerControllerChanged(APlayerController* NewController) override;
	virtual void Deinitialize() override;

	bool PrepareCandidates(const TArray<FMWTrackOffer>& Offers);
	void ReleaseCandidates();

private:
	TSharedPtr<FStreamableHandle> CatalogLoadHandle;
	TSharedPtr<FStreamableHandle> CandidateLoadHandle;
	TArray<FSoftObjectPath> CandidatePaths;
	TSet<TWeakObjectPtr<UMaterialInterface>> RequestedMaterials;
};
