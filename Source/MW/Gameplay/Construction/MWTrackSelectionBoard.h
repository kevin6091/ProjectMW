#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Gameplay/Construction/MWTrackSelectionTypes.h"
#include "MWTrackSelectionBoard.generated.h"

class UCameraComponent;
class AMWPlayerController;
class AMWTrackOfferPreview;

// 레벨에 배치하는 로컬 표시용 선택판. 서버의 후보 데이터를 받아 Preview를 재구성한다.
UCLASS()
class MW_API AMWTrackSelectionBoard : public AActor
{
    GENERATED_BODY()
public:
    AMWTrackSelectionBoard();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    // 후보 생성에 실패하면 부분적으로 만들어진 Preview도 모두 정리한다.
    bool BuildOffers(const FMWTrackSelectionSession& Session, FText& OutError);
    void ClearOffers();
    // 자기 Hover는 즉시 표시하고, 다른 사람은 PlayerState에 복제된 상태로 표시한다.
    void RefreshMarkers(AMWPlayerController* Viewer, const FMWTrackSelectionSession& Session, int32 LocalHover);
protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Selection")
    TObjectPtr<UCameraComponent> Camera;
    // Offer.BoardSlotIndex가 이 배열의 위치를 지정한다.
    UPROPERTY(VisibleAnywhere, Category="Selection")
    TArray<TObjectPtr<USceneComponent>> Anchors;
private:
    // 후보 번호 → 로컬 Preview. 코스에 설치된 트랙 목록과는 별개다.
    UPROPERTY(Transient)
    TMap<int32, TObjectPtr<AMWTrackOfferPreview>> Offers;
};
