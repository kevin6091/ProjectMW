#include "Data/MWVehicleDefinition.h"

#include "Data/MWInputData.h"
#include "GameFramework/Pawn.h"

bool UMWVehicleDefinition::ValidateDefinition(TArray<FText>& OutErrors) const
{
	const int32 InitialErrors = OutErrors.Num();
	if (VehicleId.IsNone())
	{
		OutErrors.Add(FText::FromString(TEXT("VehicleId is required.")));
	}
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
		DrivingInputData->ValidateInputData(OutErrors);
	}
	return OutErrors.Num() == InitialErrors;
}
