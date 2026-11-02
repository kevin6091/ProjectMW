#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Gameplay/Construction/MWTrackSelectionTypes.h"
#include "MWTrackSelectionBoard.generated.h"

class UCameraComponent;
class AMWPlayerController;
class AMWTrackOfferPreview;

UCLASS()
class MW_API AMWTrackSelectionBoard : public AActor
{
    GENERATED_BODY()
public:
    AMWTrackSelectionBoard();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    bool BuildOffers(const FMWTrackSelectionSession& Session, FText& OutError);
    void ClearOffers();
    void RefreshMarkers(AMWPlayerController* Viewer, const FMWTrackSelectionSession& Session, int32 LocalHover);
protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Selection")
    TObjectPtr<UCameraComponent> Camera;
    UPROPERTY(VisibleAnywhere, Category="Selection")
    TArray<TObjectPtr<USceneComponent>> Anchors;
private:
    UPROPERTY(Transient)
    TMap<int32, TObjectPtr<AMWTrackOfferPreview>> Offers;
};
