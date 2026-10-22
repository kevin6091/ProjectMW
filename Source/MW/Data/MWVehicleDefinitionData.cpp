#include "Data/MWVehicleDefinitionData.h"

#include "Data/MWInputData.h"
#include "GameFramework/Pawn.h"

bool UMWVehicleDefinitionData::ValidateDefinition(TArray<FText>& OutErrors) const
{
	// 다른 검사에서 모은 오류는 남기고, 이번 차량 정의에서 새 오류가 생겼는지 확인.
	const int32 InitialErrors = OutErrors.Num();
	if (VehicleId.IsNone())
	{
		OutErrors.Add(FText::FromString(TEXT("VehicleId is required.")));
	}
	// 직접 만들 수 없는 추상 클래스, 더 이상 쓰지 않는 클래스, 새 버전으로 바뀐 예전 클래스는 거부.
	if (!PawnClass || PawnClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		OutErrors.Add(FText::FromString(TEXT("PawnClass must be a concrete, current Pawn class.")));
	}
	if (!DrivingInputData)
	{
		OutErrors.Add(FText::FromString(TEXT("DrivingInputData is required.")));
	}
	else
	{
		// 입력 설정의 오류도 함께 모아서 차량 정의의 검사 결과에 포함.
		DrivingInputData->ValidateInputData(OutErrors);
	}
	return OutErrors.Num() == InitialErrors;
}
