#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MWItemComponent.generated.h"

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

private:
	bool IsItemPlayAllowed() const;
	double GetServerTime() const;
	void SetCurrentItem(EMWItemId ItemId);

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
