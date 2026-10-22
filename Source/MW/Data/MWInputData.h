#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "MWInputData.generated.h"

class UInputAction;
class UInputMappingContext;

USTRUCT(BlueprintType)
struct MW_API FMWInputAction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (Categories = "Input.Action"))
	FGameplayTag InputTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> InputAction;
};

/** One input mode's definitions. The owning local controller manages mapping/binding lifetimes. */
UCLASS(BlueprintType)
class MW_API UMWInputData : public UDataAsset
{
	GENERATED_BODY()

public:
	const UInputMappingContext* GetMappingContext() const { return InputMappingContext; }
	const UInputAction* FindInputActionByTag(FGameplayTag InputTag) const;
	bool ValidateInputData(TArray<FText>& OutErrors) const;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> InputMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true", TitleProperty = "InputTag"))
	TArray<FMWInputAction> InputActions;
};
