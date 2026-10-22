#include "System/MWAssetManager.h"

#include "Data/MWAssetData.h"
#include "Engine/Engine.h"
#include "Engine/StreamableManager.h"
#include "MWGameplayTags.h"
#include "UObject/StrongObjectPtr.h"

// 에셋 목록과 파일 불러오기에서 생긴 문제를 따로 찾을 수 있도록 로그 분류를 만듭니다.
DEFINE_LOG_CATEGORY_STATIC(LogMWAssets, Log, All);

UMWAssetManager& UMWAssetManager::Get()
{
	// 임의로 새 관리자를 만들지 않고, 엔진 설정으로 생성된 관리자를 사용합니다.
	UMWAssetManager* Manager = Cast<UMWAssetManager>(UAssetManager::GetIfInitialized());
	checkf(Manager, TEXT("Configure AssetManagerClassName=/Script/MW.MWAssetManager before requesting MW assets."));
	return *Manager;
}

void UMWAssetManager::StartInitialLoading()
{
	Super::StartInitialLoading();
	// 아직 실제 에셋을 제작하지 않은 초기 상태도 허용합니다. 이때는 사용할 목록이 없는 상태로 둡니다.
	if (StartupAssetData.IsNull())
	{
		UE_LOG(LogMWAssets, Display, TEXT("MW AssetManager ready; StartupAssetData is not configured yet."));
		return;
	}

	// 원본 목록부터 불러와 검사한 뒤에만, 미리 불러올 에셋을 준비합니다.
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
	// 새 목록의 검사가 끝나기 전에는 현재 목록이나 보관 중인 에셋을 건드리지 않습니다.
	if (!::IsValid(Catalog))
	{
		OutErrors.Add(FText::FromString(TEXT("Asset catalog is null or could not be loaded.")));
		return false;
	}
	if (!Catalog->ValidateCatalog(OutErrors)) return false;

	// 원본 데이터와 별개로 조회에 쓸 경로와 분류를 먼저 완성한 뒤, 한 번에 교체합니다.
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
	// 이 구간에서는 파일 불러오기나 그에 따른 외부 호출 없이 새 목록을 적용합니다.
	// 적용 후 에디터에서 원본 목록을 수정했다면 InitializeCatalog를 다시 호출해야 합니다.
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
	// 불러오기가 끝났을 때도 같은 목록을 쓰고 있는지 확인하기 위해 현재 번호를 기억합니다.
	const uint64 LoadGeneration = CatalogGeneration;
	if (!Path.IsValid())
	{
		UE_LOG(LogMWAssets, Warning, TEXT("Asset ID '%s' is not in the initialized catalog."), *AssetId.ToString());
		return nullptr;
	}
	// 이미 메모리에 있는 객체를 먼저 사용하고, 없을 때만 파일에서 불러옵니다.
	UObject* Asset = Path.ResolveObject();
	if (!Asset) Asset = GetStreamableManager().LoadSynchronous(Path, false);
	// 파일을 불러오는 중 객체 초기화 코드가 목록을 교체할 수 있습니다.
	// 시작할 때와 목록 번호가 다르면 이전 목록의 결과를 새 보관 목록에 넣지 않습니다.
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
	// 관리자가 참조를 유지해서, 사용 중인 객체가 언리얼의 메모리 정리로 사라지지 않게 합니다.
	LoadedAssets.Add(AssetId, Asset);
	return Asset;
}

bool UMWAssetManager::LoadAssetsByLabelSynchronous(const FGameplayTag Label)
{
	check(IsInGameThread());
	if (!HasCatalog() || !Label.IsValid()) return false;
	// 파일 불러오기 중 객체 초기화 함수가 호출되어 원본 조회 목록이 교체될 수 있습니다.
	// 교체 전 목록의 주소를 계속 쓰지 않도록, 필요한 ID들을 별도 배열에 복사합니다.
	const TArray<FGameplayTag> Ids = LabelAssetIds.FindRef(Label);
	const uint64 LoadGeneration = CatalogGeneration;
	// 전체가 성공할 때까지 결과를 임시로 모읍니다. 임시 객체도 참조를 유지해 메모리 정리에서 보호합니다.
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
			// 하나라도 실패하면 이번 묶음은 기존 보관 목록에 추가하지 않습니다.
			return false;
		}
		Pending.Emplace(Id, TStrongObjectPtr<UObject>(Asset));
	}
	// 모든 파일의 성공을 확인한 뒤에만 관리자 보관 목록에 반영합니다.
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
	// 관리자의 참조만 지웁니다. 다른 코드나 엔진이 사용 중인 객체는 남아 있을 수 있습니다.
	LoadedAssets.Reset();
}
