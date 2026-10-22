#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "GameplayTagContainer.h"
#include "MWAssetManager.generated.h"

class UMWAssetData;

/**
 * 에셋을 ID로 찾고, 필요할 때 파일을 불러와 메모리에 보관하는 관리자입니다.
 * 서버와 각 클라이언트가 자기 관리자를 사용하며, 이 관리자의 상태가 서로 복제되지는 않습니다.
 * 모든 함수는 게임 스레드에서 호출합니다. 실행 중에는 엔진이 만든 같은 관리자를 사용합니다.
 */
UCLASS(Config = Game)
class MW_API UMWAssetManager : public UAssetManager
{
	GENERATED_BODY()

public:
	/** 엔진 설정에 따라 만들어진 관리자를 반환합니다. 초기화나 설정이 잘못됐으면 오류로 중단합니다. */
	static UMWAssetManager& Get();

	/** 엔진 시작 때 설정된 에셋 목록을 적용하고, 미리 불러올 분류의 에셋을 준비합니다. */
	virtual void StartInitialLoading() override;

	/**
	 * 목록을 검사한 뒤 ID, 경로, 분류를 복사해서 조회에 사용합니다.
	 * 실패하면 기존 목록과 보관 중인 에셋을 유지하고, 오류를 OutErrors 뒤에 추가합니다.
	 * 성공하면 기존 에셋 보관 목록을 비웁니다.
	 * 나중에 원본 목록을 편집하면 이 함수를 다시 호출해야 반영됩니다.
	 */
	bool InitializeCatalog(UMWAssetData* Catalog, TArray<FText>& OutErrors);

	/** 검사를 통과한 에셋 목록이 있는지 확인합니다. 모든 에셋을 불러왔다는 뜻은 아닙니다. */
	bool HasCatalog() const { return AssetData != nullptr; }

	/** 등록된 파일 경로만 읽습니다. ID가 없으면 빈 경로를 반환하며, 파일은 불러오지 않습니다. */
	FSoftObjectPath FindAssetPath(FGameplayTag AssetId) const;

	/** 이 관리자가 보관 중인 에셋만 찾습니다. 없으면 nullptr를 반환하며, 새로 불러오지 않습니다. */
	UObject* FindLoadedAsset(FGameplayTag AssetId) const;

	/**
	 * 에셋을 불러올 때까지 기다린 뒤 보관합니다. 이미 보관 중이면 같은 객체를 반환합니다.
	 * ID가 없거나, 불러오기에 실패하거나, 도중에 목록이 바뀌면 nullptr를 반환합니다.
	 * 게임 시작 같은 준비 단계에서 사용하고, 주행 중 매 프레임 호출하지 않습니다.
	 */
	UObject* LoadAssetSynchronous(FGameplayTag AssetId);

	/**
	 * 분류 태그가 정확히 같은 에셋을 모두 불러온 뒤 한꺼번에 보관 목록에 추가합니다.
	 * 하나라도 실패하면 이번 묶음은 추가하지 않습니다. 목록이나 태그가 잘못됐으면 false를 반환합니다.
	 * 조건에 맞는 에셋이 하나도 없는 경우는 성공으로 처리합니다.
	 */
	bool LoadAssetsByLabelSynchronous(FGameplayTag Label);

	/**
	 * 해당 ID로 보관하던 참조만 지웁니다. 다른 ID나 다른 코드가 사용 중인 객체를 강제로 지우지는 않습니다.
	 */
	void ReleaseAsset(FGameplayTag AssetId);

	/** 관리자가 보관하던 참조를 모두 지웁니다. 실제 메모리 정리 시점은 언리얼이 결정합니다. */
	void ReleaseAllAssets();

	/** 보관 중인 에셋을 요청한 타입으로 읽습니다. 에셋이 없거나 타입이 다르면 nullptr를 반환합니다. */
	template<typename AssetType>
	AssetType* FindLoadedAsset(FGameplayTag AssetId) const
	{
		return Cast<AssetType>(FindLoadedAsset(AssetId));
	}

	/** 에셋을 불러온 뒤 요청한 타입인지 확인합니다. 다른 타입이면 nullptr를 반환합니다. */
	template<typename AssetType>
	AssetType* LoadAssetSynchronous(FGameplayTag AssetId)
	{
		return Cast<AssetType>(LoadAssetSynchronous(AssetId));
	}

private:
	// 게임 시작 때 불러올 원본 에셋 목록입니다. 초기 제작 중에는 비워 둘 수 있습니다.
	// 실제 게임 코드가 목록을 사용하기 전에는 제작한 MWAssetData의 경로를 설정해야 합니다.
	UPROPERTY(Config, EditAnywhere, Category = "Assets")
	TSoftObjectPtr<UMWAssetData> StartupAssetData;

	// 현재 적용한 원본 목록을 메모리에 유지합니다. 실제 조회에는 아래에 복사해 둔 경로와 분류를 사용합니다.
	UPROPERTY(Transient)
	TObjectPtr<UMWAssetData> AssetData;

	// 불러온 에셋을 ID별로 보관합니다. 파일명이 같아도 서로 다른 ID와 전체 경로로 구분합니다.
	// 같은 에셋에 여러 ID를 붙였다면, 한 ID의 보관을 해제해도 다른 ID의 참조는 남습니다.
	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UObject>> LoadedAssets;

	// 초기화할 때 복사한 ID별 전체 파일 경로입니다. 원본 목록을 편집해도 자동으로 바뀌지는 않습니다.
	TMap<FGameplayTag, FSoftObjectPath> AssetPaths;

	// 분류 태그별로 어떤 에셋 ID가 속해 있는지 모아 둔 조회 목록입니다.
	TMap<FGameplayTag, TArray<FGameplayTag>> LabelAssetIds;

	// 목록을 교체할 때마다 늘리는 번호입니다. 불러오기 도중 목록이 바뀌었는지 확인할 때 사용합니다.
	uint64 CatalogGeneration = 0;
};
