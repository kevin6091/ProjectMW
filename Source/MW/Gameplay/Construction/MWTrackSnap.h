#pragma once

#include "Containers/StaticArray.h"
#include "Gameplay/Construction/MWTrackPlacementTypes.h"

using FMWTrackPortFrames = TStaticArray<FTransform, 2>;

class MWTrackSnap
{
public:
	inline static constexpr double EnterDistance = 150.0;
	inline static constexpr double LeaveDistance = 220.0;
	inline static constexpr double ApproachDistance = 550.0;
	inline static constexpr double BuildRadius = 4500.0;

	// BP의 ConnectionPoint1/2를 읽고 Actor 기준의 바깥 방향 프레임으로 변환한다.
	static MW_API bool ReadPorts(const AActor& Track, FMWTrackPortFrames& OutPorts);
	static bool ReadFirstPort(const AActor& Track, FTransform& OutPort);
	static bool ReadStartGrid(const AActor& Track, FTransform& OutFrame);
	static FTransform AlignPorts(const FTransform& Source, const FTransform& Target);
	static FTransform MakeRawTransform(const FMWTrackPortFrames& Ports, const FMWTrackPlacementSession& Session,
		const FMWTrackPlacementIntent& Intent);
	static bool IsInBuildArea(const FMWTrackPlacementIntent& Intent, const FMWTrackPlacementSession& Session);
	// 마우스 광선을 후보별 높이에 투영하고, 선택한 작업 좌표를 설치 요청에 담는다.
	static bool UpdateIntentFromRay(const FMWTrackPortFrames& Ports, const FMWTrackPlacementSession& Session,
		const FVector& RayOrigin, const FVector& RayDirection, FMWTrackPlacementIntent& Intent);
	// 표시할 자세를 반환한다. true일 때만 끝점까지 붙은 설치 자세다.
	static bool Resolve(const FMWTrackPortFrames& Ports, const FMWTrackPlacementSession& Session,
		const FMWTrackPlacementIntent& Intent, FTransform& OutTransform);

private:
	static FTransform ReverseFrame(const FTransform& Frame);
	static FVector GetPortCenter(const FMWTrackPortFrames& Ports);
	static bool ReadLocalFrame(const AActor& Track, FName Name, FTransform& OutFrame);
	static bool ReadPort(const AActor& Track, FName Name, FTransform& OutPort);
};
