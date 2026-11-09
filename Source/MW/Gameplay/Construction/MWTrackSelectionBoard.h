#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Gameplay/Construction/MWTrackSelectionTypes.h"
#include "MWTrackSelectionBoard.generated.h"

class UCameraComponent;
class UBoxComponent;
class UTextRenderComponent;
class UMaterialInterface;
class AMWGameplayPlayerController;

// 실제 트랙 BP를 로컬 월드에 전시한다. 설치된 코스 Actor와는 별도 수명이다.
UCLASS()
class MW_API AMWTrackSelectionBoard : public AActor
{
	GENERATED_BODY()
public:
	AMWTrackSelectionBoard();
	virtual void Tick(float DeltaSeconds) override;
	virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	int32 FindHoveredOffer() const;
	bool GetCursorPosition(FVector& OutPosition) const;
	FVector GetBuildOrigin() const;
	double GetLayoutScale() const { return LayoutScale; }
	UMaterialInterface* GetDisplayMaterial() const { return DisplayMaterial; }

protected:
	virtual void BeginPlay() override;
	UPROPERTY(EditAnywhere, Category="Selection", meta=(ClampMin="1.0"))
	double LayoutScale = 1.0;

	UPROPERTY(EditAnywhere, Category="Selection")
	bool bUseTopDownPlacementCamera = false;

	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<USceneComponent>> Anchors;
	UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UBoxComponent>> HitAreas;
	UPROPERTY(EditAnywhere, Category="Selection") TObjectPtr<UMaterialInterface> DisplayMaterial;

private:
	bool BuildOffers(const FMWTrackSelectionSession& Session);
	void ClearOffers();
	void RefreshMarkers(const FMWTrackSelectionSession& Session);
	UPROPERTY(Transient) TMap<int32, TObjectPtr<AActor>> Offers;
	UPROPERTY(Transient) TMap<int32, TObjectPtr<UTextRenderComponent>> Labels;
	TWeakObjectPtr<AMWGameplayPlayerController> Viewer;
	int32 SessionId = 0;
	int32 Revision = 0;
	int32 LocalHover = INDEX_NONE;
	int32 SentHover = INDEX_NONE;
	double LastHoverTime = -1;
	double LastCursorTime = -1;
};
