#include "System/MWAssetManager.h"

#include "Data/MWAssetData.h"
#include "Engine/Engine.h"
#include "Engine/StreamableManager.h"
#include "MWGameplayTags.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogMWAssets, Log, All);

UMWAssetManager& UMWAssetManager::Get()
{
	UMWAssetManager* Manager = Cast<UMWAssetManager>(UAssetManager::GetIfInitialized());
	checkf(Manager, TEXT("Configure AssetManagerClassName=/Script/MW.MWAssetManager before requesting MW assets."));
	return *Manager;
}

void UMWAssetManager::StartInitialLoading()
{
	Super::StartInitialLoading();
	if (StartupAssetData.IsNull())
	{
		UE_LOG(LogMWAssets, Display, TEXT("MW AssetManager ready; StartupAssetData is not configured yet."));
		return;
	}

	UMWAssetData* Catalog = StartupAssetData.LoadSynchronous();
	TArray<FText> Errors;
	if (!InitializeCatalog(Catalog, Errors))
	{
		for (const FText& Error : Errors) UE_LOG(LogMWAssets, Error, TEXT("%s"), *Error.ToString());
		return;
	}
	if (!LoadAssetsByLabelSynchronous(MWGameplayTags::AssetLabel_Preload))
	{
		UE_LOG(LogMWAssets, Error, TEXT("MW startup preload failed for %s."), *Catalog->GetPathName());
	}
}

bool UMWAssetManager::InitializeCatalog(UMWAssetData* Catalog, TArray<FText>& OutErrors)
{
	check(IsInGameThread());
	if (!::IsValid(Catalog))
	{
		OutErrors.Add(FText::FromString(TEXT("Asset catalog is null or could not be loaded.")));
		return false;
	}
	if (!Catalog->ValidateCatalog(OutErrors)) return false;

	TMap<FGameplayTag, FSoftObjectPath> NewPaths;
	TMap<FGameplayTag, TArray<FGameplayTag>> NewLabels;
	for (const FMWAssetEntry& Entry : Catalog->GetEntries())
	{
		NewPaths.Add(Entry.AssetId, Entry.Asset.ToSoftObjectPath());
		for (const FGameplayTag Label : Entry.Labels)
		{
			NewLabels.FindOrAdd(Label).Add(Entry.AssetId);
		}
	}
	// No loading/callbacks occur during the commit. Editor edits require explicit reinitialization.
	LoadedAssets.Reset();
	AssetData = Catalog;
	AssetPaths = MoveTemp(NewPaths);
	LabelAssetIds = MoveTemp(NewLabels);
	++CatalogGeneration;
	return true;
}

FSoftObjectPath UMWAssetManager::FindAssetPath(const FGameplayTag AssetId) const
{
	check(IsInGameThread());
	const FSoftObjectPath* Path = AssetPaths.Find(AssetId);
	return Path ? *Path : FSoftObjectPath();
}

UObject* UMWAssetManager::FindLoadedAsset(const FGameplayTag AssetId) const
{
	check(IsInGameThread());
	const TObjectPtr<UObject>* Asset = LoadedAssets.Find(AssetId);
	return Asset ? Asset->Get() : nullptr;
}

UObject* UMWAssetManager::LoadAssetSynchronous(const FGameplayTag AssetId)
{
	check(IsInGameThread());
	if (UObject* Existing = FindLoadedAsset(AssetId)) return Existing;
	const FSoftObjectPath Path = FindAssetPath(AssetId);
	const uint64 LoadGeneration = CatalogGeneration;
	if (!Path.IsValid())
	{
		UE_LOG(LogMWAssets, Warning, TEXT("Asset ID '%s' is not in the initialized catalog."), *AssetId.ToString());
		return nullptr;
	}
	UObject* Asset = Path.ResolveObject();
	if (!Asset) Asset = GetStreamableManager().LoadSynchronous(Path, false);
	if (LoadGeneration != CatalogGeneration)
	{
		UE_LOG(LogMWAssets, Warning, TEXT("Catalog changed while loading '%s'; result was not cached."), *AssetId.ToString());
		return nullptr;
	}
	if (!Asset)
	{
		UE_LOG(LogMWAssets, Error, TEXT("Failed to load '%s' from '%s'."), *AssetId.ToString(), *Path.ToString());
		return nullptr;
	}
	LoadedAssets.Add(AssetId, Asset);
	return Asset;
}

bool UMWAssetManager::LoadAssetsByLabelSynchronous(const FGameplayTag Label)
{
	check(IsInGameThread());
	if (!HasCatalog() || !Label.IsValid()) return false;
	// Loading can invoke PostLoad callbacks. Keep values, not pointers into replaceable catalog maps.
	const TArray<FGameplayTag> Ids = LabelAssetIds.FindRef(Label);
	const uint64 LoadGeneration = CatalogGeneration;
	TArray<TPair<FGameplayTag, TStrongObjectPtr<UObject>>> Pending;
	for (const FGameplayTag Id : Ids)
	{
		if (FindLoadedAsset(Id)) continue;
		const FSoftObjectPath Path = FindAssetPath(Id);
		UObject* Asset = Path.ResolveObject();
		if (!Asset) Asset = GetStreamableManager().LoadSynchronous(Path, false);
		if (LoadGeneration != CatalogGeneration)
		{
			UE_LOG(LogMWAssets, Warning, TEXT("Catalog changed while loading label '%s'; pending results were discarded."), *Label.ToString());
			return false;
		}
		if (!Asset)
		{
			UE_LOG(LogMWAssets, Error, TEXT("Label '%s': failed to load '%s' from '%s'."),
				*Label.ToString(), *Id.ToString(), *Path.ToString());
			return false; // Preserve cache state if any member fails.
		}
		Pending.Emplace(Id, TStrongObjectPtr<UObject>(Asset));
	}
	for (const auto& Entry : Pending) LoadedAssets.Add(Entry.Key, Entry.Value.Get());
	return true;
}

void UMWAssetManager::ReleaseAsset(const FGameplayTag AssetId)
{
	check(IsInGameThread());
	LoadedAssets.Remove(AssetId);
}

void UMWAssetManager::ReleaseAllAssets()
{
	check(IsInGameThread());
	LoadedAssets.Reset(); // Drop only our references; consumers/engine may still retain these objects.
}
