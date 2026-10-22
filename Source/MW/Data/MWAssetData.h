#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "MWAssetData.generated.h"

USTRUCT(BlueprintType)
struct MW_API FMWAssetEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset")
	FGameplayTag AssetId;

	// Objects only. Class references belong in typed definitions (TSubclassOf/TSoftClassPtr).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset")
	TSoftObjectPtr<UObject> Asset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset")
	FGameplayTagContainer Labels;
};

/** Small authored catalog. Reading it never loads an asset or changes runtime state. */
UCLASS(BlueprintType)
class MW_API UMWAssetData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	const TArray<FMWAssetEntry>& GetEntries() const { return Entries; }
	const FMWAssetEntry* FindEntry(FGameplayTag AssetId) const;
	TArray<FGameplayTag> GetAssetIdsByLabel(FGameplayTag Label) const;
	bool ValidateCatalog(TArray<FText>& OutErrors) const;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assets", meta = (AllowPrivateAccess = "true", TitleProperty = "AssetId"))
	TArray<FMWAssetEntry> Entries;
};
