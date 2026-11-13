


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
	RequireRange(TEXT("MaxForwardSpeed"), Config.MaxForwardSpeed, 0.f, Largest);
	RequireRange(TEXT("DriveAcceleration"), Config.DriveAcceleration, 0.f, Largest);
	RequireRange(TEXT("LateralGrip"), Config.LateralGrip, 0.f, Largest);

	RequireRange(TEXT("CoastDeceleration"), Config.CoastDeceleration, 0.f, Largest);
	RequireRange(TEXT("FullBrakeDeceleration"), Config.FullBrakeDeceleration, 0.f, Largest);
	RequireRange(TEXT("DirectionChangeSpeedThreshold"), Config.DirectionChangeSpeedThreshold, 0.f, Largest);
	RequireRange(TEXT("MaxReverseSpeed"), Config.MaxReverseSpeed, 0.f, Largest);
	RequireRange(TEXT("TurnRateDegPerSecond"), Config.TurnRateDegPerSecond, 0.f, Largest);
	RequireRange(TEXT("FullSteeringSpeed"), Config.FullSteeringSpeed, 0.f, Largest);
	RequireRange(TEXT("MinSteeringSpeed"), Config.MinSteeringSpeed, 0.f, Largest);
	RequireRange(TEXT("MinDriftSpeed"), Config.MinDriftSpeed, 0.f, Largest);
	RequireRange(TEXT("MinDriftSteering"), Config.MinDriftSteering, 0.f, 1.f);
	RequireRange(TEXT("DriftFrictionMultiplier"), Config.DriftFrictionMultiplier, 0.f, 1.f);
	RequireRange(TEXT("DriftTurnRateMultiplier"), Config.DriftTurnRateMultiplier, 0.f, Largest);
	RequireRange(TEXT("GroundClearance"), Config.GroundClearance, 0.f, Largest);
	RequireRange(TEXT("GroundProbeDistance"), Config.GroundProbeDistance, 0.f, Largest);
	RequireRange(TEXT("MaxGroundAngle"), Config.MaxGroundAngle, 0.f, 89.f);
	RequireRange(TEXT("GroundDetachSpeed"), Config.GroundDetachSpeed, 0.f, Largest);
	RequireRange(TEXT("GroundAlignSpeed"), Config.GroundAlignSpeed, 0.f, Largest);
	RequireRange(TEXT("GravityScale"), Config.GravityScale, 0.f, Largest);
	RequireRange(TEXT("MaxFallSpeed"), Config.MaxFallSpeed, 1.f, Largest);
	RequireRange(TEXT("VehicleMass"), Config.VehicleMass, 1.f, Largest);
	RequireRange(TEXT("CollisionRestitution"), Config.CollisionRestitution, 0.f, 1.f);
	RequireRange(TEXT("MinBounceSpeed"), Config.MinBounceSpeed, 0.f, Largest);
	RequireRange(TEXT("CollisionMomentumRetention"), Config.CollisionMomentumRetention, 0.f, 1.f);
	RequireRange(TEXT("DownforceReferenceSpeed"), Config.DownforceReferenceSpeed, 1.f, Largest);
	RequireRange(TEXT("DownforceAtReferenceSpeed"), Config.DownforceAtReferenceSpeed, 0.f, Largest);
	RequireRange(TEXT("MaxDownforceAcceleration"), Config.MaxDownforceAcceleration, 0.f, Largest);
	RequireRange(TEXT("GroundCurvatureProbeDistance"), Config.GroundCurvatureProbeDistance, 1.f, Largest);
	RequireRange(TEXT("GroundCurvatureProbeHeight"), Config.GroundCurvatureProbeHeight, 1.f, Largest);
	RequireRange(TEXT("GroundSupportTolerance"), Config.GroundSupportTolerance, 0.f, Largest);
	RequireRange(TEXT("TireFrictionCoefficient"), Config.TireFrictionCoefficient, 0.f, Largest);
	
	if (Config.GroundProbeDistance <= Config.GroundClearance)
	{
		OutErrors.Add(FText::FromString(TEXT("GroundProbeDistance must be > GroundClearance.")));
	}

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

