#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "UObject/SoftObjectPtr.h"
#include "MWItemComponent.generated.h"

class ALevelSequenceActor;
class ULevelSequence;
class UStaticMesh;
class UStaticMeshComponent;
class USoundBase;

// Public cosmetic state for observers. Inventory and use validation remain owner-only.
USTRUCT()
struct FMWBoosterPresentation
{
	GENERATED_BODY()

	UPROPERTY()
	bool bHeld = false;

	UPROPERTY()
	double FiringEndsAt = 0.0;
};

UENUM(BlueprintType)
enum class EMWItemId : uint8
{
	None,
	Booster
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMWHeldItemChanged, EMWItemId, ItemId);

UCLASS()
class MW_API UMWItemComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMWItemComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="MW|Items")
	EMWItemId GetCurrentItemId() const { return CurrentItemId; }

	UPROPERTY(BlueprintAssignable, Category="MW|Items")
	FMWHeldItemChanged OnHeldItemChanged;

	bool TryGrantBooster();
	void RequestUseItem();
	void ResetItemState();
	bool IsBoosterActive() const;
	float GetBoosterTimeRemaining() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool IsItemPlayAllowed() const;
	double GetServerTime() const;
	void SetCurrentItem(EMWItemId ItemId);
	void LoadBoosterVisualAssets();
	bool SpawnBoosterVisual();
	void PlayBoosterAnimation(const TSoftObjectPtr<ULevelSequence>& SequenceAsset, bool bLoop = false);
	void StopBoosterAnimation();
	void StartBoosterJet();
	void StopBoosterJet();
	void PlayBoosterFireLoop();
	void RestoreBoosterPose();
	void RemoveBoosterVisual();

	UFUNCTION()
	void OnRep_BoosterPresentation();

	UPROPERTY(ReplicatedUsing=OnRep_BoosterPresentation)
	FMWBoosterPresentation BoosterPresentation;

	UPROPERTY()
	TSubclassOf<AActor> BoosterVisualClass;

	UPROPERTY()
	TSoftObjectPtr<ULevelSequence> BoosterAttach;

	UPROPERTY()
	TSoftObjectPtr<ULevelSequence> BoosterFireStart;

	UPROPERTY()
	TSoftObjectPtr<ULevelSequence> BoosterFireLoop;

	UPROPERTY()
	TSoftObjectPtr<ULevelSequence> BoosterDetach;

	UPROPERTY()
	TObjectPtr<UStaticMesh> BoosterJetMesh;

	UPROPERTY()
	TObjectPtr<USoundBase> BoosterFireSound;

	UPROPERTY(Transient)
	TObjectPtr<AActor> BoosterVisual;

	UPROPERTY(Transient)
	TObjectPtr<ALevelSequenceActor> BoosterSequenceActor;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> BoosterJet;

	FTimerHandle BoosterExpiryTimer;
	FTimerHandle BoosterAnimationTimer;
	double DisplayedFiringEnd = 0.0;
	bool bBoosterDetaching = false;

	UFUNCTION()
	void HandleGameStateChanged();

	UFUNCTION()
	void OnRep_CurrentItem();

	UFUNCTION(Server, Reliable)
	void ServerUseItem(uint32 ExpectedGrantId);

	UPROPERTY(ReplicatedUsing=OnRep_CurrentItem)
	EMWItemId CurrentItemId = EMWItemId::None;

	UPROPERTY(Replicated)
	uint32 GrantId = 0;

	UPROPERTY(Replicated)
	double BoosterEndsAt = 0.0;
};
