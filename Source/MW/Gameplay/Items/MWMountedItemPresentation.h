#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MWMountedItemPresentation.generated.h"

class ALevelSequenceActor;
class AMWVehiclePawn;
class ULevelSequence;
class UMWItemDefinitionData;
class USoundBase;

// 각 창에서 복제된 아이템의 외형을 장착하고 시퀀스만 재생한다.
UCLASS(NotBlueprintable, Transient)
class AMWMountedItemPresentation : public AActor
{
    GENERATED_BODY()

public:
    AMWMountedItemPresentation();

    static AMWMountedItemPresentation* Equip(AMWVehiclePawn& Vehicle, const UMWItemDefinitionData& Definition, bool bAnimateAttach);
    static double GetUseDuration(const UMWItemDefinitionData& Definition, bool bIncludeDetach);
    void PlayUse(bool bShouldDetach);

private:
    enum class EStage : uint8 { Attach, Held, Use, WindDown, Detach };

    bool Start(AMWVehiclePawn& Vehicle, const UMWItemDefinitionData& Definition, bool bAnimateAttach);
    bool PlaySequence(ULevelSequence& Sequence, int32 LoopCount = 0, float PlayRate = 1.f);
    void FinishAttach();
    void PlayImpactSound();
    void ReleaseSequence();
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(Transient) TObjectPtr<AActor> Visual;
    UPROPERTY(Transient) TObjectPtr<ALevelSequenceActor> SequenceActor;
    UPROPERTY(Transient) TObjectPtr<ULevelSequence> UseSequence;
    UPROPERTY(Transient) TObjectPtr<ULevelSequence> WindDownSequence;
    UPROPERTY(Transient) TObjectPtr<ULevelSequence> DetachSequence;
    UPROPERTY(Transient) TObjectPtr<USoundBase> UseSound;
    UPROPERTY(Transient) TObjectPtr<USoundBase> ImpactSound;
    EStage Stage = EStage::Attach;
    int32 UseLoopCount = 0;
    float UsePlayRate = 1.f;
    float ImpactDelay = 0.f;
    bool bImpactSoundPlayed = false;
    bool bDetachAfterUse = true;
};
