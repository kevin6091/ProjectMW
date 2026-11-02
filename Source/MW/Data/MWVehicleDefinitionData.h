#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MWVehicleDefinitionData.generated.h"

class APawn;
class UMWInputData;
class UMWVehicleDriveData;

// 생성할 차량과 조작 입력만 미리 설정합니다.
// 모든 차량이 함께 쓰는 주행 값은 선택한 이동 방식에서 관리하며, 외형별로 따로 두지 않음.

UCLASS(BlueprintType)
class MW_API UMWVehicleDefinitionData : public UDataAsset
{
	GENERATED_BODY()

public:
	// 화면에 보이는 이름과 별개로, 같은 차량을 계속 구분하는 데 쓰는 ID를 반환.
	FName GetVehicleId() const { return VehicleId; }

	// 차량 선택 화면 등에 표시할 이름을 반환. 이름이 비어 있어도 정의 검사는 통과할 수 있음.
	const FText& GetDisplayName() const { return DisplayName; }

	// 필수 차량 설정과 연결된 입력 설정을 검사. 오류를 OutErrors에 추가하고, 새 오류가 없으면 true를 반환.
	bool ValidateDefinition(TArray<FText>& OutErrors) const;

private:
	// 표시 이름을 바꿔도 같은 차량으로 찾을 수 있게 하는 필수 ID.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle", meta = (AllowPrivateAccess = "true"))
	FName VehicleId;

	// 플레이어에게 보여 줄 이름. 차량 생성과 입력 설정에는 영향을 주지 않는 선택 항목.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle", meta = (AllowPrivateAccess = "true"))
	FText DisplayName;

	// 차량의 가속, 조향 같은 주행 입력을 연결하는 필수 설정.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle", meta = (AllowPrivateAccess = "true"))
	UMWVehicleDriveData* DriveConfig;
};
