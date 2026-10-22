#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MWVehicleDefinition.generated.h"

class APawn;
class UMWInputData;

/**
 * 카잔의 캐릭터 정의를 MW의 차량에 맞게 바꾼 설정 데이터입니다.
 * 생성할 차량과 조작 입력만 미리 설정합니다.
 * 점수, 효과, 현재 이동 상태, 능력 시스템(GAS), 인공지능은 여기서 관리하지 않습니다.
 * 모든 차량이 함께 쓰는 주행 값은 선택한 이동 방식에서 관리하며, 외형별로 따로 두지 않습니다.
 */
UCLASS(BlueprintType)
class MW_API UMWVehicleDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	FName GetVehicleId() const { return VehicleId; }
	const FText& GetDisplayName() const { return DisplayName; }
	TSubclassOf<APawn> GetPawnClass() const { return PawnClass; }
	const UMWInputData* GetDrivingInputData() const { return DrivingInputData; }
	bool ValidateDefinition(TArray<FText>& OutErrors) const;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle", meta = (AllowPrivateAccess = "true"))
	FName VehicleId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle", meta = (AllowPrivateAccess = "true"))
	FText DisplayName;

	// 서버는 미리 승인된 설정 데이터에서 차량 클래스를 고릅니다.
	// 클라이언트가 보낸 클래스는 사용하지 않습니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle", meta = (AllowPrivateAccess = "true", AllowAbstract = "false"))
	TSubclassOf<APawn> PawnClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMWInputData> DrivingInputData;
};
