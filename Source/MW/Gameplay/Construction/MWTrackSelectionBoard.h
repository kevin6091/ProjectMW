#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Gameplay/Construction/MWTrackSelectionTypes.h"
#include "MWTrackSelectionBoard.generated.h"

class UCameraComponent;
class UBoxComponent;
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
	// 복구 요청은 전시 액터만 비워 같은 후보를 다시 만들게 한다. 서버의 선택 세션은 유지한다.
	void RetryArrivalOffers() { ClearOffers(); }
	void CollectArrivalMaterials(const FMWTrackSelectionSession& Session, TArray<UMaterialInterface*>& Materials, FString& Pending) const;
	int32 FindHoveredOffer() const;
	bool GetCursorPosition(FVector& OutPosition) const;
	FVector GetBuildOrigin() const;
	double GetLayoutScale() const { return LayoutScale; }

protected:
	virtual void BeginPlay() override;
	UPROPERTY(EditAnywhere, Category="Selection", meta=(ClampMin="1.0"))
	double LayoutScale = 1.0;

	// 레벨에 저장된 값을 유지하기 위해 기존 프로퍼티 이름을 쓴다.
	UPROPERTY(EditAnywhere, Category="Selection", meta=(DisplayName="Frame Course During Placement"))
	bool bUseTopDownPlacementCamera = true;

	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
	// 레벨에 저장된 이전 3칸 배열이 새 생성자에서 만든 6칸을 덮어쓰지 않게 한다.
	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> SlotAnchors;
	UPROPERTY(Transient) TArray<TObjectPtr<UBoxComponent>> SlotHitAreas;

private:
	static constexpr double RowSpacing = 1260.0;
	static constexpr double ColumnSpacing = 1120.0;
	static constexpr double BoardHeight = 80.0;
	static const FVector SlotExtent;
	static const FVector SelectionViewDirection;

	void FrameBounds(FMinimalViewInfo& View, const FBox& Bounds, double Aspect, double ScreenFraction);

	bool BuildOffers(const FMWTrackSelectionSession& Session);
	void ClearOffers();
	void RefreshMarkers(const FMWTrackSelectionSession& Session);
	UPROPERTY(Transient) TMap<int32, TObjectPtr<AActor>> Offers;
	TWeakObjectPtr<AMWGameplayPlayerController> Viewer;
	int32 SessionId = 0;
	int32 Revision = 0;
	int32 LocalHover = INDEX_NONE;
	int32 SentHover = INDEX_NONE;
	double LastHoverTime = -1;
	double LastCursorTime = -1;
};
