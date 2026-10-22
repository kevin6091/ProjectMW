#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "MWInputData.generated.h"

class UInputAction;
class UInputMappingContext;

/** 가속, 조향 같은 입력 기능의 태그와 그 기능에 쓸 입력 액션을 연결한 항목. */
USTRUCT(BlueprintType)
struct MW_API FMWInputAction
{
	GENERATED_BODY()

	// 키 자체가 아니라 가속, 아이템 사용처럼 입력의 목적을 구분하는 태그.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (Categories = "Input.Action"))
	FGameplayTag InputTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> InputAction;
};

// 주행 또는 건설처럼 한 가지 입력 모드에 필요한 설정을 모은 데이터.
// 이 데이터는 키 입력을 직접 처리하지 않음.
// 플레이어의 로컬 컨트롤러가 키 연결과 입력 처리 함수를 등록하고, 모드 종료 때 해제.
UCLASS(BlueprintType)
class MW_API UMWInputData : public UDataAsset
{
	GENERATED_BODY()

public:
	const UInputMappingContext* GetMappingContext() const { return InputMappingContext; }

	// 태그가 정확히 같은 입력 액션을 찾음. 태그나 액션이 없거나 태그가 중복되면 nullptr를 반환.
	const UInputAction* FindInputActionByTag(FGameplayTag InputTag) const;

	// 키 연결 설정과 입력 액션의 누락, 태그의 누락·중복을 검사.
	// 오류는 OutErrors 뒤에 추가.
	// 이번 검사에서 새 오류가 없으면 true를 반환.
	bool ValidateInputData(TArray<FText>& OutErrors) const;

private:
	// 이 입력 모드에서 사용할 키와 입력 액션의 연결 설정.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> InputMappingContext;

	// 각 입력 기능을 태그로 찾기 위한 목록. 같은 태그는 한 번만 등록.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true", TitleProperty = "InputTag"))
	TArray<FMWInputAction> InputActions;
};
