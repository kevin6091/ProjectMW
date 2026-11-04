#include "Data/MWVehicleDefinitionData.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "MWVehicleDriveData.h"
#include "Data/MWInputData.h"
#include "GameFramework/Pawn.h"

bool UMWVehicleDefinitionData::ValidateDefinition(TArray<FText>& OutErrors) const
{
	// 다른 검사에서 모은 오류는 남기고, 이번 차량 정의에서 새 오류가 생겼는지 확인.
	const int32 InitialErrors = OutErrors.Num();
	
	if (!PawnClass.IsValid() || !DriveData || !DriveData->ValidateConfig(OutErrors))
	{
		OutErrors.Add(FText::FromString(TEXT("DriveData is required.")));
	}
	
	return OutErrors.Num() == InitialErrors;
}
