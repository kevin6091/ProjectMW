#pragma once

#include "Containers/StaticArray.h"
#include "Gameplay/Construction/MWTrackPlacementTypes.h"

using FMWTrackPortFrames = TStaticArray<FTransform, 2>;

namespace MWTrackSnap
{
	inline constexpr double EnterDistance = 150.0;
	inline constexpr double LeaveDistance = 220.0;
	inline constexpr double BuildRadius = 4500.0;

	// BP의 ConnectionPoint1/2를 읽고 Actor 기준의 바깥 방향 프레임으로 변환한다.
	bool ReadPorts(const AActor& Track, FMWTrackPortFrames& OutPorts);
	FTransform MakeRawTransform(const FMWTrackPortFrames& Ports, const FMWTrackPlacementSession& Session,
		const FMWTrackPlacementIntent& Intent);
	bool IsInBuildArea(const FMWTrackPlacementIntent& Intent, const FMWTrackPlacementSession& Session);
	void FindCandidate(const FMWTrackPortFrames& Ports, const FMWTrackPlacementSession& Session, FMWTrackPlacementIntent& Intent);
	bool Resolve(const FMWTrackPortFrames& Ports, const FMWTrackPlacementSession& Session,
		const FMWTrackPlacementIntent& Intent, FTransform& OutTransform);
}
