#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MWVehicleSelectionPreviewActor.generated.h"

class APlayerController;
class UWidgetComponent;
class UMWVehicleSelectionWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMWVehiclePreviewClosed);

// PIE-only development host. It does not participate in match progression or create a Pawn.
UCLASS()
class MW_API AMWVehicleSelectionPreviewActor : public AActor
{
	GENERATED_BODY()

public:
	AMWVehicleSelectionPreviewActor();
	UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Preview") TSubclassOf<UMWVehicleSelectionWidget> PreviewWidgetClass;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MW|Vehicle UI|Preview") TObjectPtr<UWidgetComponent> PreviewSurface;
	UPROPERTY(EditAnywhere, Category = "MW|Vehicle UI|Preview") bool bOpenOnBeginPlay = false;
	UPROPERTY(EditInstanceOnly, Category = "MW|Vehicle UI|Preview") TObjectPtr<AActor> PreviewViewTarget;
	UPROPERTY(BlueprintAssignable, Category = "MW|Vehicle UI|Preview") FMWVehiclePreviewClosed OnPreviewClosed;
	UFUNCTION(BlueprintCallable, Category = "MW|Vehicle UI|Preview") bool OpenPreview();
	UFUNCTION(BlueprintCallable, Category = "MW|Vehicle UI|Preview") void ClosePreview();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient) TObjectPtr<UMWVehicleSelectionWidget> PreviewWidget;
	TWeakObjectPtr<APlayerController> PreviewController;
	TWeakObjectPtr<AActor> PreviousViewTarget;
	bool bPreviousCursorVisible = false;
};
