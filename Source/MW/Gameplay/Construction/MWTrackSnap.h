#pragma once

#include "Containers/StaticArray.h"
#include "Gameplay/Construction/MWTrackPlacementTypes.h"

using FMWTrackPortFrames = TStaticArray<FTransform, 2>;

namespace MWTrackSnap
{
	inline constexpr double EnterDistance = 150.0;
	inline constexpr double LeaveDistance = 220.0;
	inline constexpr double ApproachDistance = 550.0;
	inline constexpr double BuildRadius = 4500.0;

	// BP의 ConnectionPoint1/2를 읽고 Actor 기준의 바깥 방향 프레임으로 변환한다.
	bool ReadPorts(const AActor& Track, FMWTrackPortFrames& OutPorts);
	FTransform MakeRawTransform(const FMWTrackPortFrames& Ports, const FMWTrackPlacementSession& Session,
		const FMWTrackPlacementIntent& Intent);
	bool IsInBuildArea(const FMWTrackPlacementIntent& Intent, const FMWTrackPlacementSession& Session);
	// 마우스 광선을 후보별 높이에 투영하고, 선택한 작업 좌표를 설치 요청에 담는다.
	bool UpdateIntentFromRay(const FMWTrackPortFrames& Ports, const FMWTrackPlacementSession& Session,
		const FVector& RayOrigin, const FVector& RayDirection, FMWTrackPlacementIntent& Intent);
	// 표시할 자세를 반환한다. true일 때만 끝점까지 붙은 설치 자세다.
	bool Resolve(const FMWTrackPortFrames& Ports, const FMWTrackPlacementSession& Session,
		const FMWTrackPlacementIntent& Intent, FTransform& OutTransform);
}
