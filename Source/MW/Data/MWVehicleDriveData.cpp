


#include "Data/MWVehicleDriveData.h"

bool UMWVehicleDriveData::ValidateConfig(TArray<FText>& OutErrors) const
{
	const int32 InitialErrors = OutErrors.Num();
	
	const auto RequireRange = [&OutErrors](const TCHAR* Name, float Value, float Min, float Max)
	{
		if (!FMath::IsFinite(Value) || Value < Min || Value > Max)
		{
			OutErrors.Add(FText::FromString(FString::Printf(TEXT("%s has an invalid value."), Name)));
		}
	};
	
	const float Largest = TNumericLimits<float>::Max();
	
	RequireRange(TEXT("CoastDeceleration"), Config.CoastDeceleration, 0.f, Largest);
	RequireRange(TEXT("FullBrakeDeceleration"), Config.FullBrakeDeceleration, 0.f, Largest);
	RequireRange(TEXT("DirectionChangeSpeedThreshold"), Config.DirectionChangeSpeedThreshold, 0.f, Largest);
	RequireRange(TEXT("MaxReverseSpeed"), Config.MaxReverseSpeed, 0.f, Largest);
	RequireRange(TEXT("TurnRateDegPerSecond"), Config.TurnRateDegPerSecond, 0.f, Largest);
	RequireRange(TEXT("FullSteeringSpeed"), Config.FullSteeringSpeed, 0.f, Largest);
	RequireRange(TEXT("MinSteeringSpeed"), Config.MinSteeringSpeed, 0.f, Largest);
	RequireRange(TEXT("MinDriftSpeed"), Config.MinDriftSpeed, 0.f, Largest);
	RequireRange(TEXT("MinDriftSteering"), Config.MinDriftSteering, 0.f, Largest);
	RequireRange(TEXT("DriftFrictionMultiplier"), Config.DriftFrictionMultiplier, 0.f, Largest);
	RequireRange(TEXT("DriftTurnRateMultiplier"), Config.DriftTurnRateMultiplier, 0.f, Largest);
	
	if (Config.FullBrakeDeceleration < Config.CoastDeceleration)
	{
		OutErrors.Add(FText::FromString(TEXT("FullBrakeDeceleration must be >= CoastDeceleration.")));
	}

	if (Config.FullSteeringSpeed <= Config.MinSteeringSpeed)
	{
		OutErrors.Add(FText::FromString(TEXT("FullSteeringSpeed must be > MinSteeringSpeed.")));
	}

	return OutErrors.Num() == InitialErrors;
}

