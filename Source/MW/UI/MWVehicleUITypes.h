#pragma once

#include "CoreMinimal.h"
#include "MWVehicleUITypes.generated.h"

class UTexture2D;

// Presentation values only. This is not a replicated player or vehicle definition.
USTRUCT(BlueprintType)
struct FMWVehiclePlayerPresentation
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText PlayerName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText VehicleName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText StatusText;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText MarkerText;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UTexture2D> Thumbnail;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor PlayerTint;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bOccupied = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bLocal = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bConfirmed = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bHoverPreview = false;
};

// Indices address art-only PreviewCards and presentation slots, never gameplay VehicleIds.
USTRUCT(BlueprintType)
struct FMWVehiclePreviewParticipant
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 SlotIndex = INDEX_NONE;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 PreviewCardIndex = INDEX_NONE;
	// Art-only pointer target while no vehicle has been selected.
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 PreviewPointerCardIndex = INDEX_NONE;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bConfirmed = false;
};

USTRUCT(BlueprintType)
struct FMWVehiclePreviewScenario
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Label;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Capacity = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 LocalSlotIndex = INDEX_NONE;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FMWVehiclePreviewParticipant> Participants;
};
