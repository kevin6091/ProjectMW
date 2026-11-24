#pragma once

#include "Gameplay/Construction/MWTrackSnap.h"

namespace MWTrackPlacementClearance
{
	TArray<FVector> ReadLocalPoints(const AActor& Track, TConstArrayView<FTransform> Ports);
	bool FitsFloor(TConstArrayView<FVector> LocalPoints, const FTransform& Transform, double FloorZ);
	bool FitsWithFinish(TConstArrayView<FVector> LocalPoints, const FMWTrackPlacementGround& Ground,
		const FTransform& Transform, const FTransform& FreePort);
	bool FitsEitherPort(TConstArrayView<FVector> LocalPoints, const FMWTrackPlacementGround& Ground,
		const FMWTrackPortFrames& Ports, const FTransform& OpenEnd);
}
