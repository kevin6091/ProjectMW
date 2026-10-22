#include "Data/MWAssetData.h"

// 원본 에셋 목록을 검색하고 설정 오류를 검사. 여기서는 에셋 파일을 불러오지 않음.

const FMWAssetEntry* UMWAssetData::FindEntry(const FGameplayTag AssetId) const
{
	if (!AssetId.IsValid()) return nullptr;
	const FMWAssetEntry* Match = nullptr;
	for (const FMWAssetEntry& Entry : Entries)
	{
		if (Entry.AssetId == AssetId)
		{
			// 같은 ID가 여러 개면 임의로 하나를 골라 사용하지 않습니다.
			if (Match) return nullptr;
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
			// 이름이 비슷한 상위·하위 태그까지 같은 분류로 보지 않고, 지정한 태그만 비교.
			if (Entry.Labels.HasTagExact(Label)) Result.AddUnique(Entry.AssetId);
		}
	}
	return Result;
}

bool UMWAssetData::ValidateCatalog(TArray<FText>& OutErrors) const
{
	// 호출자가 이미 모아 둔 오류는 유지하고, 이번 검사에서 새 오류가 늘었는지 확인.
	const int32 InitialErrors = OutErrors.Num();
	// 같은 에셋에 서로 다른 ID를 붙이는 것은 허용하지만, 하나의 ID를 여러 항목에 쓰지는 못함.
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
		// 실제 파일을 열지 않고, 참조가 비어 있거나 경로 형식이 잘못됐는지만 확인.
		if (Entry.Asset.IsNull() || !Entry.Asset.ToSoftObjectPath().IsValid())
		{
			OutErrors.Add(FText::FromString(FString::Printf(TEXT("Entry[%d] '%s': missing or invalid asset path."),
				Index, *Entry.AssetId.ToString())));
		}
	}
	return OutErrors.Num() == InitialErrors;
}
