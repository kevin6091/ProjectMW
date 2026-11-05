#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "Gameplay/Vehicle/MWVehicleMovementComponent.h"
#include "Data/MWVehicleDriveData.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UnrealType.h"
#include <limits>

namespace MWVehicleTests
{
// 테스트용 접근은 리플렉션으로 한정하고 Runtime에 설정 변경용 API를 추가하지 않는다.
FMWVehicleDriveConfig& Config(UMWVehicleDriveData* Data)
{
	FStructProperty* Property = FindFProperty<FStructProperty>(Data->GetClass(), TEXT("Config"));
	check(Property);
	return *Property->ContainerPtrToValuePtr<FMWVehicleDriveConfig>(Data);
}

struct FScene
{
	UWorld* World = nullptr;
	AMWVehiclePawn* Vehicle = nullptr;
	UMWVehicleMovementComponent* Movement = nullptr;
	APlayerController* Controller = nullptr;
	UBoxComponent* Floor = nullptr;

	FScene()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		Floor = AddBox(FVector(0, 0, -50), FVector(30000, 30000, 50));
		Vehicle = World->SpawnActor<AMWVehiclePawn>(FVector(0, 0, 31), FRotator::ZeroRotator);
		Movement = CastChecked<UMWVehicleMovementComponent>(Vehicle->GetMovementComponent());
		UMWVehicleDriveData* Data = NewObject<UMWVehicleDriveData>(Vehicle);
		FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Movement->GetClass(), TEXT("DriveData"));
		check(Property);
		Property->SetObjectPropertyValue_InContainer(Movement, Data);
		Controller = World->SpawnActor<APlayerController>();
		// 키보드 입력과 구분해 Pawn 제어 입력 API와 실제 충돌 월드를 검증한다.
		Vehicle->PossessedBy(Controller);
		Vehicle->DispatchBeginPlay();
	}

	~FScene()
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	}

	UBoxComponent* AddBox(FVector Location, FVector Extent, FRotator Rotation = FRotator::ZeroRotator)
	{
		AActor* Actor = World->SpawnActor<AActor>();
		UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
		Actor->SetRootComponent(Box);
		Box->SetBoxExtent(Extent);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Box->SetCollisionObjectType(ECC_WorldStatic);
		Box->SetCollisionResponseToAllChannels(ECR_Block);
		Box->RegisterComponent();
		Actor->SetActorLocationAndRotation(Location, Rotation);
		return Box;
	}

	void Reset(FVector Location = FVector(0, 0, 31), FRotator Rotation = FRotator::ZeroRotator)
	{
		Vehicle->TeleportTo(Location, Rotation, false, true);
		Movement->StopMovementImmediately();
		Movement->ResetDriveInput();
	}

	void Input(float Throttle, float Reverse = 0, float Steering = 0, bool bDrift = false)
	{
		FMWVehicleDriveInput Value;
		Value.Throttle = Throttle;
		Value.Reverse = Reverse;
		Value.Steering = Steering;
		Value.bDriftRequested = bDrift;
		Vehicle->SubmitDriveInput(Value);
	}

	void Step(float Seconds, float FrameTime = 1.f / 60.f)
	{
		const int32 Frames = FMath::RoundToInt(Seconds / FrameTime);
		for (int32 Index = 0; Index < Frames; ++Index)
		{
			Movement->TickComponent(FrameTime, LEVELTICK_All, nullptr);
		}
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMWDriveConfigTest, "MW.Vehicle.ConfigValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMWDriveConfigTest::RunTest(const FString& Parameters)
{
	UMWVehicleDriveData* Data = NewObject<UMWVehicleDriveData>();
	TArray<FText> Errors;
	TestTrue(TEXT("Default config is valid"), Data->ValidateConfig(Errors));
	const FMWVehicleDriveConfig Default = MWVehicleTests::Config(Data);
	auto Reject = [this, Data, &Default](const TCHAR* Label, auto Change)
	{
		MWVehicleTests::Config(Data) = Default;
		Change(MWVehicleTests::Config(Data));
		TArray<FText> InvalidErrors;
		TestFalse(Label, Data->ValidateConfig(InvalidErrors));
		TestTrue(TEXT("Invalid config explains why"), !InvalidErrors.IsEmpty());
	};
	Reject(TEXT("Negative acceleration"), [](auto& C) { C.DriveAcceleration = -1; });
	Reject(TEXT("NaN speed"), [](auto& C) { C.MaxForwardSpeed = std::numeric_limits<float>::quiet_NaN(); });
	Reject(TEXT("Invalid angle"), [](auto& C) { C.MaxGroundAngle = 90; });
	Reject(TEXT("No ground probe margin"), [](auto& C) { C.GroundProbeDistance = C.GroundClearance; });
	Reject(TEXT("Zero fall speed"), [](auto& C) { C.MaxFallSpeed = 0; });
	Reject(TEXT("Negative gravity scale"), [](auto& C) { C.GravityScale = -1; });
	Reject(TEXT("Drift multiplier outside range"), [](auto& C) { C.DriftFrictionMultiplier = 2; });
	Reject(TEXT("Negative detach speed"), [](auto& C) { C.GroundDetachSpeed = -1; });
	Reject(TEXT("Zero vehicle mass"), [](auto& C) { C.VehicleMass = 0; });
	Reject(TEXT("Momentum retention outside range"), [](auto& C) { C.CollisionMomentumRetention = 2; });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMWDriveControlsTest, "MW.Vehicle.LocalControls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMWDriveControlsTest::RunTest(const FString& Parameters)
{
	MWVehicleTests::FScene Scene;
	Scene.Input(0, 0, 1);
	Scene.Step(0.5f);
	TestTrue(TEXT("Stationary steering does not rotate"), Scene.Vehicle->GetActorRotation().IsNearlyZero());
	TestTrue(TEXT("Floor detected"), Scene.Movement->IsGrounded());
	TestTrue(TEXT("Ground clearance maintained"), FMath::IsNearlyEqual(Scene.Vehicle->GetActorLocation().Z, 31.f, 0.3f));
	Scene.Input(1);
	Scene.Step(2.f);
	TestTrue(TEXT("Forward speed limit"), Scene.Movement->Velocity.X > 1750 && Scene.Movement->Velocity.X <= 1801);
	Scene.Input(0);
	Scene.Step(0.5f);
	TestTrue(TEXT("Coasting retains momentum and slows down"), Scene.Movement->Velocity.X > 1500 && Scene.Movement->Velocity.X < 1790);
	Scene.Input(1, 1);
	Scene.Step(1.f);
	TestTrue(TEXT("Simultaneous throttle and reverse brakes to rest"), Scene.Movement->Velocity.IsNearlyZero(0.1f));
	Scene.Input(0, 1);
	Scene.Step(1.f);
	TestTrue(TEXT("Reverse speed limit"), Scene.Movement->Velocity.X < -550 && Scene.Movement->Velocity.X >= -601);
	Scene.Input(1);
	Scene.Step(1.f / 60.f);
	TestTrue(TEXT("Changing direction brakes first"), Scene.Movement->Velocity.X < 0 && Scene.Movement->Velocity.X > -600);
	Scene.Reset();
	Scene.Movement->Velocity = FVector(800, 0, 0);
	Scene.Input(0, 0, 1);
	Scene.Step(0.2f);
	TestTrue(TEXT("Coasting steering works without throttle"), Scene.Vehicle->GetActorRotation().Yaw > 5);
	Scene.Reset();
	Scene.Movement->Velocity = FVector(800, -300, 0);
	Scene.Input(0, 0, 1);
	Scene.Step(0.1f);
	const float NormalSlip = FMath::Abs(FVector::DotProduct(Scene.Movement->Velocity, Scene.Vehicle->GetActorRightVector()));
	Scene.Reset();
	Scene.Movement->Velocity = FVector(800, -300, 0);
	Scene.Input(0, 0, 1, true);
	Scene.Step(0.1f);
	TestTrue(TEXT("Drift conditions activate"), Scene.Movement->IsDriftControlActive());
	const float DriftSlip = FMath::Abs(FVector::DotProduct(Scene.Movement->Velocity, Scene.Vehicle->GetActorRightVector()));
	AddInfo(FString::Printf(TEXT("Countersteering lateral speed: normal=%.2f drift=%.2f cm/s"), NormalSlip, DriftSlip));
	TestTrue(TEXT("Drifting retains more lateral slip"), DriftSlip > NormalSlip);
	Scene.Controller->SetIgnoreMoveInput(true);
	const float OldYaw = Scene.Vehicle->GetActorRotation().Yaw;
	Scene.Step(0.1f);
	TestFalse(TEXT("Input lock disables drift"), Scene.Movement->IsDriftControlActive());
	TestTrue(TEXT("Input lock disables steering"), FMath::IsNearlyEqual(OldYaw, Scene.Vehicle->GetActorRotation().Yaw, 0.01f));
	Scene.Controller->ResetIgnoreMoveInput();
	Scene.Reset();
	Scene.Input(std::numeric_limits<float>::quiet_NaN(), 0, std::numeric_limits<float>::infinity());
	Scene.Step(0.2f);
	TestTrue(TEXT("Non-finite inputs do not move the vehicle"), Scene.Movement->Velocity.IsNearlyZero());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMWDriveCollisionTest, "MW.Vehicle.CollisionAndGround",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMWDriveCollisionTest::RunTest(const FString& Parameters)
{
	MWVehicleTests::FScene Scene;
	Scene.Reset(FVector(0, 0, 500));
	Scene.Step(0.2f);
	TestTrue(TEXT("Gravity accelerates falling"), Scene.Movement->Velocity.Z < -100);
	Scene.Step(2.f);
	TestTrue(TEXT("Lands on floor"), Scene.Movement->IsGrounded());
	TestTrue(TEXT("Landing clears downward speed"), FMath::Abs(Scene.Movement->Velocity.Z) < 1);
	TestTrue(TEXT("Landing retains clearance"), FMath::IsNearlyEqual(Scene.Vehicle->GetActorLocation().Z, 31.f, 0.5f));
	Scene.Movement->Velocity = FVector(0, 0, 600);
	Scene.Step(0.1f);
	TestFalse(TEXT("Upward impulse releases ground"), Scene.Movement->IsGrounded());
	TestTrue(TEXT("Upward impulse is not erased"), Scene.Vehicle->GetActorLocation().Z > 60 && Scene.Movement->Velocity.Z > 400);
	Scene.Reset(FVector(0, 0, 10000));
	Scene.Movement->Velocity.Z = -5000;
	Scene.Step(1.f / 60.f);
	TestTrue(TEXT("Terminal fall speed"), Scene.Movement->Velocity.Z >= -4001);
	Scene.Reset();
	UBoxComponent* Wall = Scene.AddBox(FVector(400, 0, 200), FVector(20, 1000, 200));
	Scene.Movement->Velocity = FVector(900, 500, 0);
	Scene.Step(0.5f);
	TestTrue(TEXT("Wall blocks forward translation"), Scene.Vehicle->GetActorLocation().X <= 280.5f);
	TestTrue(TEXT("Oblique collision slides along wall"), Scene.Vehicle->GetActorLocation().Y > 100);
	Wall->GetOwner()->Destroy();
	Scene.Floor->SetBoxExtent(FVector(500, 500, 50));
	Scene.Reset();
	Scene.Input(1);
	Scene.Step(2.f);
	TestFalse(TEXT("Driving past the edge releases ground"), Scene.Movement->IsGrounded());
	TestTrue(TEXT("Vehicle falls after leaving the track"), Scene.Vehicle->GetActorLocation().Z < -50);
	Scene.Floor->SetBoxExtent(FVector(30000, 30000, 50));
	Scene.Reset();
	Wall = Scene.AddBox(FVector(120.5f, 0, 200), FVector(20, 1000, 200));
	Scene.Movement->Velocity = FVector(1000, 0, 0);
	Scene.Input(0, 0, 1);
	Scene.Step(1.f / 60.f);
	TestTrue(TEXT("Rotation into a nearby wall is rejected"), FMath::Abs(Scene.Vehicle->GetActorRotation().Yaw) < 0.1f);
	Wall->GetOwner()->Destroy();
	Scene.Floor->GetOwner()->SetActorRotation(FRotator(20, 0, 0));
	Scene.Reset(FVector(0, 0, 300));
	Scene.Step(2.f);
	TestTrue(TEXT("Slope supports vehicle"), Scene.Movement->IsGrounded());
	const float StartHeight = Scene.Vehicle->GetActorLocation().Z;
	Scene.Input(1);
	Scene.Step(0.5f);
	TestTrue(TEXT("Slope travel changes height"), FMath::Abs(Scene.Vehicle->GetActorLocation().Z - StartHeight) > 20);
	TestTrue(TEXT("Remains grounded on slope"), Scene.Movement->IsGrounded());
	Scene.Floor->GetOwner()->SetActorRotation(FRotator(70, 0, 0));
	Scene.Reset(FVector(0, 0, 500));
	Scene.Step(1.f);
	TestFalse(TEXT("Steep surface is not drivable ground"), Scene.Movement->IsGrounded());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMWDriveWallGroundContactTest, "MW.Vehicle.WallGroundContact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMWDriveWallGroundContactTest::RunTest(const FString& Parameters)
{
	MWVehicleTests::FScene Scene;
	constexpr float StepTime = 1.f / 120.f;
	constexpr double Yaw = -15.0;
	const FVector Start(0, 0, 31);
	Scene.Reset(Start, FRotator(0, Yaw, 0));
	const double YawRadians = FMath::DegreesToRadians(FMath::Abs(Yaw));
	const double BoxDepth = 100.0 * FMath::Sin(YawRadians) + 50.0 * FMath::Cos(YawRadians);
	Scene.AddBox(FVector(0, -BoxDepth - 20.0 + 0.2, 200), FVector(1000, 20, 200));

	Scene.Step(StepTime, StepTime);
	TestTrue(TEXT("Side contact does not hide the floor"), Scene.Movement->IsGrounded());
	TestTrue(TEXT("Side contact leaves the ground clearance intact"),
		FMath::IsNearlyEqual(Scene.Vehicle->GetActorLocation().Z, Start.Z, 0.02));

	Scene.Input(0, 1);
	Scene.Step(0.5f);
	TestTrue(TEXT("Reverse can pull the vehicle away from the wall"), Scene.Vehicle->GetActorLocation().Y > 15.0);

	Scene.Reset(Start, FRotator(0, Yaw, 0));
	Scene.Floor->GetOwner()->Destroy();
	Scene.Step(StepTime, StepTime);
	TestFalse(TEXT("A wall without a floor does not count as ground"), Scene.Movement->IsGrounded());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMWDriveVehicleContactTest, "MW.Vehicle.VehicleContact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMWDriveVehicleContactTest::RunTest(const FString& Parameters)
{
	MWVehicleTests::FScene Scene;
	AMWVehiclePawn* OtherPawn = Scene.World->SpawnActor<AMWVehiclePawn>(FVector(202, 0, 31), FRotator::ZeroRotator);
	UMWVehicleMovementComponent* Other = CastChecked<UMWVehicleMovementComponent>(OtherPawn->GetMovementComponent());
	FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Other->GetClass(), TEXT("DriveData"));
	check(Property);
	Property->SetObjectPropertyValue_InContainer(Other, NewObject<UMWVehicleDriveData>(OtherPawn));
	OtherPawn->DispatchBeginPlay();

	Scene.Movement->Velocity = FVector(900, 0, 0);
	Scene.Step(1.f / 120.f, 1.f / 120.f);
	AddInfo(FString::Printf(TEXT("Vehicle contact speed: striker %.2f, target %.2f cm/s"),
		Scene.Movement->Velocity.X, Other->Velocity.X));
	TestTrue(TEXT("A rear impact pushes the target without launching it"), Other->Velocity.X > 100 && Other->Velocity.X < 300);
	TestTrue(TEXT("Contact leaves vehicles separating"), Other->Velocity.X > Scene.Movement->Velocity.X);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMWDriveFrameRateTest, "MW.Vehicle.FrameRateAndReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMWDriveFrameRateTest::RunTest(const FString& Parameters)
{
	MWVehicleTests::FScene Scene;
	Scene.Input(1, 0, 0.5f);
	Scene.Step(1.f, 1.f / 30.f);
	const FVector Location30 = Scene.Vehicle->GetActorLocation();
	const FVector Velocity30 = Scene.Movement->Velocity;
	Scene.Reset();
	Scene.Input(1, 0, 0.5f);
	Scene.Step(1.f, 1.f / 120.f);
	TestTrue(TEXT("30/120 FPS positions agree"), Scene.Vehicle->GetActorLocation().Equals(Location30, 1.f));
	TestTrue(TEXT("30/120 FPS velocities agree"), Scene.Movement->Velocity.Equals(Velocity30, 1.f));
	Scene.Reset();
	Scene.Step(0.2f);
	TestTrue(TEXT("Teleport clears stored input"), Scene.Movement->Velocity.IsNearlyZero());
	Scene.Input(1);
	Scene.Vehicle->UnPossessed();
	Scene.Step(0.2f);
	TestTrue(TEXT("Unpossess clears stored input"), Scene.Movement->Velocity.IsNearlyZero());
	return true;
}

#endif
