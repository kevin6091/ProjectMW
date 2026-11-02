#include "Data/MWVehicleDefinitionData.h"

#include "Data/MWVehicleDriveData.h"
#include "GameFramework/Pawn.h"

bool UMWVehicleDefinitionData::ValidateDefinition(TArray<FText>& OutErrors) const
{
	// 다른 검사에서 모은 오류는 남기고, 이번 차량 정의에서 새 오류가 생겼는지 확인.
	const int32 InitialErrors = OutErrors.Num();
	if (VehicleId.IsNone())
	{
		OutErrors.Add(FText::FromString(TEXT("VehicleId is required.")));
	}
	
	if (!DriveConfig || !DriveConfig->ValidateConfig(OutErrors))
	{
		OutErrors.Add(FText::FromString(TEXT("DriveConfig has invalid values.")));
	}
	
	return OutErrors.Num() == InitialErrors;
}
