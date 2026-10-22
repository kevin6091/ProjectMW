#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MWVehicleDefinition.generated.h"

class APawn;
class UMWInputData;

/**
 * MW counterpart of Khazan's CharacterDefinition.
 * Authored spawn/input settings only; no scores, effects, movement state, GAS or AI.
 * Shared vehicle tuning belongs to the chosen movement solution, not individual skins.
 */
UCLASS(BlueprintType)
class MW_API UMWVehicleDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	FName GetVehicleId() const { return VehicleId; }
	const FText& GetDisplayName() const { return DisplayName; }
	TSubclassOf<APawn> GetPawnClass() const { return PawnClass; }
	const UMWInputData* GetDrivingInputData() const { return DrivingInputData; }
	bool ValidateDefinition(TArray<FText>& OutErrors) const;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle", meta = (AllowPrivateAccess = "true"))
	FName VehicleId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle", meta = (AllowPrivateAccess = "true"))
	FText DisplayName;

	// Server selects this definition from approved data; never accept a client-provided class.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle", meta = (AllowPrivateAccess = "true", AllowAbstract = "false"))
	TSubclassOf<APawn> PawnClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMWInputData> DrivingInputData;
};
