#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "MWAssetData.generated.h"

// 사용할 에셋 하나의 구분 태그, 파일 경로, 분류 태그를 묶은 항목입니다.
USTRUCT(BlueprintType)
struct MW_API FMWAssetEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset")
	FGameplayTag AssetId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset")
	TSoftObjectPtr<UObject> Asset;

	// 시작할 때 미리 불러오기처럼, 여러 에셋을 함께 고를 때 쓰는 분류 태그.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset")
	FGameplayTagContainer Labels;
};

// 게임에서 사용할 에셋 목록을 편집하는 데이터.
// 실제 파일 불러오기와 메모리 보관은 MWAssetManager가 담당.
UCLASS(BlueprintType)
class MW_API UMWAssetData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// 등록한 항목 전체를 읽습니다. 배열을 복사하거나 에셋을 불러오지 않음.
	const TArray<FMWAssetEntry>& GetEntries() const { return Entries; }

	// ID가 정확히 같은 항목을 찾습니다. ID가 잘못됐거나, 없거나, 중복되면 nullptr를 반환.
	const FMWAssetEntry* FindEntry(FGameplayTag AssetId) const;

	// 분류 태그가 정확히 같은 항목의 ID를 중복 없이 모읍니다. 상위·하위 태그는 자동으로 포함하지 않음.
	TArray<FGameplayTag> GetAssetIdsByLabel(FGameplayTag Label) const;

	// ID의 누락·중복과 에셋 경로를 검사하고, 발견한 오류를 OutErrors 뒤에 추가.
	// 이번 검사에서 새 오류가 없으면 true를 반환.
	// 실제 파일이 있는지, 정상적으로 불러올 수 있는지는 검사하지 않음.
	bool ValidateCatalog(TArray<FText>& OutErrors) const;

private:
	// 에디터에서 작성하는 원본 목록. 관리자에 적용한 뒤 수정하면 다시 초기화해야 반영.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assets", meta = (AllowPrivateAccess = "true", TitleProperty = "AssetId"))
	TArray<FMWAssetEntry> Entries;
};
