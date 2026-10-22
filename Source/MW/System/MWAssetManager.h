#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "GameplayTagContainer.h"
#include "MWAssetManager.generated.h"

class UMWAssetData;

/** Process lifetime service, not replicated gameplay state. All methods run on the game thread. */
UCLASS(Config = Game)
class MW_API UMWAssetManager : public UAssetManager
{
	GENERATED_BODY()

public:
	static UMWAssetManager& Get();
	virtual void StartInitialLoading() override;

	// Validates and snapshots the catalog; failure preserves the previous catalog/cache.
	bool InitializeCatalog(UMWAssetData* Catalog, TArray<FText>& OutErrors);
	bool HasCatalog() const { return AssetData != nullptr; }
	FSoftObjectPath FindAssetPath(FGameplayTag AssetId) const;
	UObject* FindLoadedAsset(FGameplayTag AssetId) const;
	UObject* LoadAssetSynchronous(FGameplayTag AssetId);
	bool LoadAssetsByLabelSynchronous(FGameplayTag Label);
	void ReleaseAsset(FGameplayTag AssetId);
	void ReleaseAllAssets();

	template<typename AssetType>
	AssetType* FindLoadedAsset(FGameplayTag AssetId) const
	{
		return Cast<AssetType>(FindLoadedAsset(AssetId));
	}

	template<typename AssetType>
	AssetType* LoadAssetSynchronous(FGameplayTag AssetId)
	{
		return Cast<AssetType>(LoadAssetSynchronous(AssetId));
	}

private:
	// Optional during initial setup. Configure a real catalog before any gameplay consumer uses it.
	UPROPERTY(Config, EditAnywhere, Category = "Assets")
	TSoftObjectPtr<UMWAssetData> StartupAssetData;

	UPROPERTY(Transient)
	TObjectPtr<UMWAssetData> AssetData;

	// Stable IDs distinguish identical basenames; aliases retain independent cache references.
	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UObject>> LoadedAssets;

	TMap<FGameplayTag, FSoftObjectPath> AssetPaths;
	TMap<FGameplayTag, TArray<FGameplayTag>> LabelAssetIds;
	uint64 CatalogGeneration = 0;
};
