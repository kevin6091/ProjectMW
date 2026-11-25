#if WITH_DEV_AUTOMATION_TESTS

#include "Gameplay/Construction/MWTrackSnap.h"
#include "Gameplay/Construction/MWTrackPlacementClearance.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMWTrackPreviewMatchesSnapTest, "MW.Construction.PreviewMatchesSnap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMWTrackPreviewMatchesSnapTest::RunTest(const FString& Parameters)
{
	const FMWTrackPortFrames Ports = {
		FTransform(FRotator(0.0, 180.0, 0.0), FVector(-350.0, 0.0, 20.0)),
		FTransform(FRotator(0.0, 45.0, 0.0), FVector(350.0, 180.0, 100.0))
	};
	FMWTrackPlacementSession Session;
	const FMWOpenTrackEnd End = {{1, 0}, FTransform(FVector(300.0, 600.0, 180.0))};
	Session.OpenEnds.Add(End);

	// 첫 포트가 정렬될 때의 조각 중심을 기준으로 비스듬한 카메라 광선을 만든다.
	const FVector TargetLocation = End.WorldFrame.GetLocation();
	const FVector ReferenceCenter = TargetLocation + (Ports[1].GetLocation() - Ports[0].GetLocation()) * 0.5;
	const FVector RayDirection = FVector(0.25, -0.4, -1.0).GetSafeNormal();
	FMWTrackPlacementIntent Intent;
	FTransform Display;
	const auto AimAt = [&](const FVector& Offset)
	{
		return MWTrackSnap::UpdateIntentFromRay(Ports, Session, ReferenceCenter + Offset - RayDirection * 1000.0,
			RayDirection, Intent);
	};

	const FVector OutsideSnap(175.0, 60.0, 0.0);
	for (const bool bReversed : {false, true})
	{
		Intent.bReversed = bReversed;
		const int32 ExpectedPort = bReversed ? 1 : 0;
		TestTrue(TEXT("A point just outside snap still has an aligned preview"), AimAt(OutsideSnap));
		TestEqual(TEXT("Reversing switches which end of the curve connects"), Intent.SourcePort, ExpectedPort);
		TestFalse(TEXT("The preview does not snap outside the enter radius"), MWTrackSnap::Resolve(Ports, Session, Intent, Display));
		const FTransform PreviewPort = Ports[ExpectedPort] * Display;
		TestTrue(TEXT("The visible port offset matches the distance used for snap"),
			PreviewPort.GetLocation().Equals(TargetLocation + OutsideSnap, 0.01));
		TestTrue(TEXT("The preview port still faces the target"),
			FVector::DotProduct(PreviewPort.GetUnitAxis(EAxis::X), End.WorldFrame.GetUnitAxis(EAxis::X)) < -0.999);
	}

	TestTrue(TEXT("The reversed preview can enter the snap radius"), AimAt(FVector(140.0, 0.0, 0.0)));
	TestTrue(TEXT("The reversed preview snaps"), MWTrackSnap::Resolve(Ports, Session, Intent, Display));
	TestTrue(TEXT("The snapped port sits exactly at the target"),
		Display.TransformPosition(Ports[1].GetLocation()).Equals(TargetLocation, 0.01));
	Intent.bReversed = false;
	TestTrue(TEXT("Turning preserves the snap in the hold radius"), AimAt(OutsideSnap));
	TestTrue(TEXT("The other end is still snapped to the same target"), MWTrackSnap::Resolve(Ports, Session, Intent, Display));
	TestTrue(TEXT("The other port sits exactly at the target"),
		Display.TransformPosition(Ports[0].GetLocation()).Equals(TargetLocation, 0.01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMWTrackPlacementClearanceTest, "MW.Construction.PlacementClearance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMWTrackPlacementClearanceTest::RunTest(const FString& Parameters)
{
	const TArray<FVector> RoadPoints{{-100.0, 0.0, 40.0}, {100.0, 0.0, 40.0}};
	const FTransform LowPivot(FVector(0.0, 0.0, -20.0));
	TestTrue(TEXT("A pivot below the floor does not reject a road that clears it"),
		MWTrackPlacementClearance::FitsFloor(RoadPoints, LowPivot, 0.0));
	TestFalse(TEXT("The same pose is blocked by a raised floor"),
		MWTrackPlacementClearance::FitsFloor(RoadPoints, LowPivot, 30.0));
	TestTrue(TEXT("A point touching the floor is allowed"),
		MWTrackPlacementClearance::FitsFloor(RoadPoints, FTransform(FVector(0.0, 0.0, -40.0)), 0.0));

	const FTransform Tilted(FRotator(45.0, 0.0, 0.0), FVector(0.0, 0.0, 20.0));
	TestFalse(TEXT("Low road geometry is rejected even when the pivot is above ground"),
		MWTrackPlacementClearance::FitsFloor(RoadPoints, Tilted, 0.0));

	FMWTrackPlacementGround Ground;
	Ground.FinishLocalPoints.Add(FVector(100.0, 0.0, -20.0));
	TestFalse(TEXT("The visible track fits but its required finish does not"),
		MWTrackPlacementClearance::FitsWithFinish(RoadPoints, Ground, LowPivot, FTransform::Identity));
	TestTrue(TEXT("A normal high exit can still reserve the finish"),
		MWTrackPlacementClearance::FitsWithFinish(RoadPoints, Ground, LowPivot, FTransform(FVector(0.0, 0.0, 100.0))));

	const FMWTrackPortFrames Rise = {
		FTransform(FQuat(FVector::UpVector, UE_PI), FVector(-100.0, 0.0, 0.0)),
		FTransform(FVector(100.0, 0.0, 80.0))
	};
	const TArray<FVector> RisePoints{Rise[0].GetLocation(), Rise[1].GetLocation()};
	const FTransform End(FVector(0.0, 0.0, 25.0));
	TestFalse(TEXT("The descending direction would put the lower port below the floor"),
		MWTrackPlacementClearance::FitsWithFinish(RisePoints, Ground,
			MWTrackSnap::AlignPorts(Rise[1], End), Rise[0]));
	TestTrue(TEXT("Reversing a blocked ramp still leaves a valid direction"),
		MWTrackPlacementClearance::FitsEitherPort(RisePoints, Ground, Rise, End));
	TArray<FVector> DeepUnderside = RisePoints;
	DeepUnderside.Add(FVector(0.0, 0.0, -100.0));
	TestFalse(TEXT("Both blocked directions are eligible for recovery"),
		MWTrackPlacementClearance::FitsEitherPort(DeepUnderside, Ground, Rise, End));
	return true;
}

#endif
