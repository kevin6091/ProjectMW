#include "Data/MWAssetData.h"

const FMWAssetEntry* UMWAssetData::FindEntry(const FGameplayTag AssetId) const
{
	if (!AssetId.IsValid()) return nullptr;
	const FMWAssetEntry* Match = nullptr;
	for (const FMWAssetEntry& Entry : Entries)
	{
		if (Entry.AssetId == AssetId)
		{
			if (Match) return nullptr; // Ambiguous authoring must not silently choose an entry.
			Match = &Entry;
		}
	}
	return Match;
}

TArray<FGameplayTag> UMWAssetData::GetAssetIdsByLabel(const FGameplayTag Label) const
{
	TArray<FGameplayTag> Result;
	if (Label.IsValid())
	{
		for (const FMWAssetEntry& Entry : Entries)
		{
			if (Entry.Labels.HasTagExact(Label)) Result.AddUnique(Entry.AssetId);
		}
	}
	return Result;
}

bool UMWAssetData::ValidateCatalog(TArray<FText>& OutErrors) const
{
	const int32 InitialErrors = OutErrors.Num();
	TSet<FGameplayTag> SeenIds;
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const FMWAssetEntry& Entry = Entries[Index];
		if (!Entry.AssetId.IsValid() || SeenIds.Contains(Entry.AssetId))
		{
			OutErrors.Add(FText::FromString(FString::Printf(TEXT("Entry[%d]: missing or duplicate AssetId '%s'."),
				Index, *Entry.AssetId.ToString())));
		}
		SeenIds.Add(Entry.AssetId);
		if (Entry.Asset.IsNull() || !Entry.Asset.ToSoftObjectPath().IsValid())
		{
			OutErrors.Add(FText::FromString(FString::Printf(TEXT("Entry[%d] '%s': missing or invalid asset path."),
				Index, *Entry.AssetId.ToString())));
		}
	}
	return OutErrors.Num() == InitialErrors;
}
