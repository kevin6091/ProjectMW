#pragma once

#include "Gameplay/Construction/MWTrackSnap.h"

class MWTrackPlacementClearance
{
public:
	static TArray<FVector> ReadLocalPoints(const AActor& Track, TConstArrayView<FTransform> Ports);
	static bool FitsFloor(TConstArrayView<FVector> LocalPoints, const FTransform& Transform, double FloorZ);
	static bool FitsWithFinish(TConstArrayView<FVector> LocalPoints, const FMWTrackPlacementGround& Ground,
		const FTransform& Transform, const FTransform& FreePort);
	static bool FitsEitherPort(TConstArrayView<FVector> LocalPoints, const FMWTrackPlacementGround& Ground,
		const FMWTrackPortFrames& Ports, const FTransform& OpenEnd);

private:
	static void AddCorners(const FBox& Box, const FTransform& Local, TArray<FVector>& OutPoints);
};
