#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Gameplay/Items/MWItemTypes.h"
#include "MWItemDefinitionData.generated.h"

class AActor;
class ULevelSequence;
class USoundBase;
class UStaticMesh;
class UTexture2D;

USTRUCT(BlueprintType)
struct FMWItemSpeedBoostData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.01", Units = "s"))
	float Duration = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
	float SpeedMultiplier = 1.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
	float AccelerationMultiplier = 1.5f;
};

USTRUCT(BlueprintType)
struct FMWItemAreaHitData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.01", Units = "s"))
	float ImpactDelay = .10f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.01", Units = "s"))
	float UseDuration = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1", Units = "cm"))
	float Radius = 220.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1", Units = "cm"))
	float HalfHeight = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1", Units = "cm/s"))
	float KnockbackSpeed = 1100.f;

	// 맞은 차량의 입력 제한 시간과 같은 시간 동안 누적할 회전량이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0", UIMax = "3", Units = "s"))
	float ControlLockDuration = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0", UIMax = "720", Units = "deg"))
	float SpinDegrees = 360.f;
};

USTRUCT(BlueprintType)
struct FMWItemPresentationData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftClassPtr<AActor> VisualClass;

	// 차량 충돌체 상단을 기준으로 한 외형 오프셋, 회전, 스케일이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FTransform MountOffset = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<ULevelSequence> Attach;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<ULevelSequence> Use;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<ULevelSequence> Loop;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<ULevelSequence> Detach;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<USoundBase> UseSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<USoundBase> ImpactSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UStaticMesh> ExhaustMesh;
};

// MWAssetData의 Asset.Item.<ItemId> 항목으로 등록하는 아이템 정적 정의다.
UCLASS(BlueprintType)
class MW_API UMWItemDefinitionData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	EMWItemId ItemId = EMWItemId::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD")
	FVector2D IconContentCenter = FVector2D(.5, .5);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect",
		meta = (EditCondition = "ItemId == EMWItemId::Booster", EditConditionHides))
	FMWItemSpeedBoostData SpeedBoost;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect",
		meta = (EditCondition = "ItemId == EMWItemId::Hammer", EditConditionHides))
	FMWItemAreaHitData AreaHit;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	FMWItemPresentationData Presentation;

	// 엔진 에셋 관리자의 MWAssetData 인덱스를 조회한다. 주행 Tick에서는 보관한 정의를 사용한다.
	UFUNCTION(BlueprintPure, Category = "MW|Items")
	static UMWItemDefinitionData* FindById(EMWItemId Id);

	bool HasValidDefinition() const;
};
