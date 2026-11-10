#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MWPickupBox.generated.h"

class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class MW_API AMWPickupBox : public AActor
{
	GENERATED_BODY()

public:
	AMWPickupBox();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* Overlapped, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void UpdateAvailability();

	UPROPERTY(VisibleAnywhere, Category="MW|Items")
	TObjectPtr<UBoxComponent> Trigger;

	UPROPERTY(VisibleAnywhere, Category="MW|Items")
	TObjectPtr<USceneComponent> PresentationRoot;

	UPROPERTY(VisibleAnywhere, Category="MW|Items")
	TObjectPtr<UStaticMeshComponent> Visual;

	UPROPERTY(VisibleAnywhere, Category="MW|Items")
	TObjectPtr<UStaticMeshComponent> LidVisual;

	UPROPERTY(VisibleAnywhere, Category="MW|Items")
	TObjectPtr<UStaticMeshComponent> HandleVisual;

	UPROPERTY(ReplicatedUsing=UpdateAvailability)
	bool bAvailable = true;
};
