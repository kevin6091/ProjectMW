#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Items/MWItemComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "MWGameplayHUDSubsystem.generated.h"

class APawn;
class APlayerController;
class SWidget;
class UGameViewportClient;
class UFont;
class UMaterialInterface;
class UTexture2D;
class UMWVehicleSelectionWidget;

UCLASS(Config=Game)
class MW_API UMWGameplayHUDSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	UPROPERTY(Config) TSoftObjectPtr<UFont> DisplayFont;
	UPROPERTY(Config) TSoftObjectPtr<UTexture2D> BoardLogo;
	UPROPERTY(Config) TSoftObjectPtr<UTexture2D> BoardBolt;
	UPROPERTY(Config) TSoftClassPtr<UMWVehicleSelectionWidget> VehiclePalette;
	UPROPERTY(Config) TSoftObjectPtr<UTexture2D> BoosterIcon;
	UPROPERTY(Config) TArray<TSoftObjectPtr<UTexture2D>> ItemRouletteIcons;
	UPROPERTY(Config) TSoftObjectPtr<UMaterialInterface> ItemIconMaterial;
	UPROPERTY(Config) FVector2D BoosterIconContentCenter = FVector2D(.5, .5);

	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

protected:
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	virtual void OnWorldEndPlay(UWorld& World) override;
	virtual void Deinitialize() override;

private:
	void RemoveOverlay();

	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	UFUNCTION()
	void HandleHeldItemChanged(EMWItemId ItemId);

	TWeakObjectPtr<APlayerController> ItemController;
	TWeakObjectPtr<UMWItemComponent> ObservedItems;
	EMWItemId HeldItem = EMWItemId::None;

	TSharedPtr<SWidget> Overlay;
	TWeakObjectPtr<UGameViewportClient> Viewport;
};
