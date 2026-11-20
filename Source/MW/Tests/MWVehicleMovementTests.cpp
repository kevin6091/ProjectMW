#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "Gameplay/Vehicle/MWVehicleMovementComponent.h"
#include "Data/MWVehicleDriveData.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "System/MWDriveTestGameMode.h"
#include "System/GameState/MWGameplayGameState.h"
#include "System/MWPlayerState.h"
#include "Engine/NetConnection.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
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

	explicit FScene(const FMWVehicleDriveConfig& InConfig = FMWVehicleDriveConfig())
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		AGameStateBase* TestState = World->SpawnActor<AGameStateBase>();
		TestState->GameModeClass = AMWDriveTestGameMode::StaticClass();
		World->SetGameState(TestState);
		Floor = AddBox(FVector(0, 0, -50), FVector(30000, 30000, 50));
		Vehicle = World->SpawnActor<AMWVehiclePawn>(FVector(0, 0, 31), FRotator::ZeroRotator);
		Movement = CastChecked<UMWVehicleMovementComponent>(Vehicle->GetMovementComponent());
		UMWVehicleDriveData* Data = NewObject<UMWVehicleDriveData>(Vehicle);
		// 실제 BeginPlay 검증과 설정 복사를 거치도록 생성 시점에만 설정한다.
		Config(Data) = InConfig;
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
	AMWGameplayGameState* UseRaceRules()
	{
		auto* State = World->SpawnActor<AMWGameplayGameState>();
		World->SetGameState(State);
		auto* Player = World->SpawnActor<AMWPlayerState>();
		Player->SetOwner(Controller);
		Controller->PlayerState = Player;
		Controller->Possess(Vehicle);
		Vehicle->SetPlayerState(Player);
		Vehicle->RefreshDrivePermission();
		return State;
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
	// 에디터 입력 제한과 별개로 저장된 벽 반발 값도 검사한다.
	Reject(TEXT("Wall restitution outside range"), [](auto& C) { C.WallRestitution = 1.1f; });
	Reject(TEXT("Negative wall restitution"), [](auto& C) { C.WallRestitution = -0.1f; });
	Reject(TEXT("Non-finite wall restitution"), [](auto& C) { C.WallRestitution = std::numeric_limits<float>::quiet_NaN(); });
	Reject(TEXT("Negative wall bounce threshold"), [](auto& C) { C.MinWallBounceSpeed = -1; });
	Reject(TEXT("Non-finite wall bounce threshold"), [](auto& C) { C.MinWallBounceSpeed = std::numeric_limits<float>::infinity(); });
	// 새 자세 조사 거리는 0 또는 비유한값을 허용하지 않는다.
	Reject(TEXT("Zero ground plane length"), [](auto& C) { C.GroundPlaneProbeHalfLength = 0; });
	Reject(TEXT("Non-finite ground plane width"), [](auto& C) { C.GroundPlaneProbeHalfWidth = std::numeric_limits<float>::infinity(); });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMWDriveControlsTest, "MW.Vehicle.LocalControls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMWDriveControlsTest::RunTest(const FString& Parameters)
{
	const FMWVehicleDriveConfig DriveConfig;
	MWVehicleTests::FScene Scene(DriveConfig);
	Scene.Input(0, 0, 1);
	Scene.Step(0.5f);
	TestTrue(TEXT("Stationary steering does not rotate"), Scene.Vehicle->GetActorRotation().IsNearlyZero());
	TestTrue(TEXT("Floor detected"), Scene.Movement->IsGrounded());
	TestTrue(TEXT("Ground clearance maintained"), FMath::IsNearlyEqual(Scene.Vehicle->GetActorLocation().Z, 31.f, 0.3f));
	Scene.Input(1);
	// 최고속도에 도달하고도 추진을 계속해 한계가 적용되는지 확인한다.
	Scene.Step(DriveConfig.MaxForwardSpeed / DriveConfig.DriveAcceleration + 0.25f);
	TestTrue(TEXT("Forward speed limit"), FMath::IsNearlyEqual(Scene.Movement->Velocity.X, DriveConfig.MaxForwardSpeed, 1.f));
	Scene.Input(0);
	constexpr float CoastDuration = 0.5f;
	Scene.Step(CoastDuration);
	const float ExpectedCoastSpeed = DriveConfig.MaxForwardSpeed - DriveConfig.CoastDeceleration * CoastDuration;
	TestTrue(TEXT("Coasting retains momentum and slows down"), FMath::IsNearlyEqual(Scene.Movement->Velocity.X, ExpectedCoastSpeed, 1.f));
	Scene.Input(1, 1);
	Scene.Step(DriveConfig.MaxForwardSpeed / DriveConfig.FullBrakeDeceleration + 0.25f);
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
	// 첫 스텝의 회전 거절을 확인한다. 반발로 멀어진 다음 스텝에서는 조향할 수 있다.
	Scene.Step(1.f / 120.f, 1.f / 120.f);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMWDriveWallBounceTest, "MW.Vehicle.WallBounce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMWDriveWallBounceTest::RunTest(const FString& Parameters)
{
	constexpr float StepTime = 1.f / 120.f;
	FMWVehicleDriveConfig Config;
	// 접촉 자체의 결과를 측정하는 경우에만 타이어 감속을 제거한다.
	// 기본 설정과 실제 입력을 사용하는 검증은 기존 조작 테스트와 PIE에서 수행한다.
	Config.CoastDeceleration = 0.f;
	Config.LateralGrip = 0.f;

	for (const float Restitution : {0.f, 0.3f, 1.f})
	{
		Config.WallRestitution = Restitution;
		MWVehicleTests::FScene Scene(Config);
		Scene.Step(StepTime, StepTime);
		Scene.AddBox(FVector(121, 0, 200), FVector(20, 1000, 200));
		Scene.Movement->Velocity = FVector(1200, 400, 0);
		Scene.Step(StepTime, StepTime);
		const FVector Velocity = Scene.Movement->Velocity;
		AddInfo(FString::Printf(TEXT("Wall restitution %.2f: location=%s velocity=%s"),
			Restitution, *Scene.Vehicle->GetActorLocation().ToString(), *Velocity.ToString()));
		TestTrue(TEXT("Wall reflects only the approach speed"), FMath::IsNearlyEqual(Velocity.X, -1200.0 * Restitution, 0.1));
		TestTrue(TEXT("Wall preserves tangential speed"), FMath::IsNearlyEqual(Velocity.Y, 400.0, 0.1));
		TestTrue(TEXT("Wall adds no vertical speed or lift"), FMath::Abs(Velocity.Z) < 0.01 && FMath::Abs(Scene.Vehicle->GetActorLocation().Z - 31.0) < 0.01);
		TestTrue(TEXT("Wall contact keeps ground support"), Scene.Movement->IsGrounded());
		TestTrue(TEXT("Stationary wall does not add kinetic energy"), Velocity.SizeSquared() <= FMath::Square(1200.0) + FMath::Square(400.0) + 0.1);
		if (Restitution > 0.f)
		{
			// 충돌 전 이동량을 재사용하면 같은 스텝의 반대 방향 이동이 누락된다.
			TestTrue(TEXT("Remaining time moves away from the wall in the impact step"), Scene.Vehicle->GetActorLocation().X < 0.0);
		}
	}

	Config.WallRestitution = 0.3f;
	{
		MWVehicleTests::FScene Scene(Config);
		Scene.Step(StepTime, StepTime);
		Scene.AddBox(FVector(120.1, 0, 200), FVector(20, 1000, 200));
		Scene.Movement->Velocity = FVector(100, 1500, 0);
		Scene.Step(StepTime, StepTime);
		TestTrue(TEXT("Fast grazing contact below normal threshold does not bounce"), FMath::Abs(Scene.Movement->Velocity.X) < 0.1);
		TestTrue(TEXT("Low-speed contact still travels along the wall"), Scene.Movement->Velocity.Y > 1499);
	}

	{
		MWVehicleTests::FScene Scene(Config);
		Scene.Step(StepTime, StepTime);
		Scene.AddBox(FVector(121, 0, 200), FVector(20, 1000, 200));
		Scene.AddBox(FVector(0, 71, 200), FVector(1000, 20, 200));
		Scene.Movement->Velocity = FVector(1200, 1200, 0);
		Scene.Step(StepTime, StepTime);
		TestTrue(TEXT("Both walls of a corner are resolved in one step"), Scene.Movement->Velocity.X < -350 && Scene.Movement->Velocity.Y < -350);
		TestTrue(TEXT("Corner contact does not launch or penetrate"), Scene.Movement->IsGrounded() && Scene.Vehicle->GetActorLocation().X <= 1.1 && Scene.Vehicle->GetActorLocation().Y <= 1.1);
	}

	{
		MWVehicleTests::FScene Scene(Config);
		Scene.Reset(FVector(0, 0, 300));
		Scene.AddBox(FVector(121, 0, 300), FVector(20, 1000, 300));
		Scene.Movement->Velocity = FVector(1200, 0, 0);
		Scene.Step(StepTime, StepTime);
		TestTrue(TEXT("Airborne contact keeps the existing non-bouncing response"), FMath::Abs(Scene.Movement->Velocity.X) < 0.1 && Scene.Movement->Velocity.Z < 0);
	}

	FVector ReferenceLocation = FVector::ZeroVector;
	FVector ReferenceVelocity = FVector::ZeroVector;
	for (const float FrameTime : {1.f / 120.f, 1.f / 60.f, 1.f / 30.f, 1.f / 240.f})
	{
		MWVehicleTests::FScene Scene(Config);
		Scene.Step(StepTime, StepTime);
		Scene.AddBox(FVector(121, 0, 200), FVector(20, 1000, 200));
		Scene.Movement->Velocity = FVector(1200, 400, 0);
		Scene.Step(1.f / 30.f, FrameTime);
		AddInfo(FString::Printf(TEXT("Wall frame time %.6f: location=%s velocity=%s"),
			FrameTime, *Scene.Vehicle->GetActorLocation().ToString(), *Scene.Movement->Velocity.ToString()));
		if (ReferenceLocation.IsNearlyZero())
		{
			ReferenceLocation = Scene.Vehicle->GetActorLocation();
			ReferenceVelocity = Scene.Movement->Velocity;
		}
		// 엔진 Sweep은 충돌 시 최대 약 1cm를 앞당긴다. 120Hz보다 짧은 스텝은
		// Sweep 거리도 달라지므로 그 범위의 위치 차이만 허용하고 속도는 동일하게 검사한다.
		const double PositionTolerance = FrameTime < StepTime ? 1.0 : 0.1;
		TestTrue(TEXT("30/60/120/240 FPS wall positions agree within sweep margin"), Scene.Vehicle->GetActorLocation().Equals(ReferenceLocation, PositionTolerance));
		TestTrue(TEXT("30/60/120/240 FPS wall velocities agree"), Scene.Movement->Velocity.Equals(ReferenceVelocity, 0.1));
	}

	// 중력과 타이어 힘을 제외하고 반발 방향이 현재 노면을 따르는지 검증한다.
	Config.GravityScale = 0.f;
	for (const FRotator Rotation : {FRotator(20, 0, 0), FRotator(0, 0, 90), FRotator(0, 0, 180)})
	{
		MWVehicleTests::FScene Scene(Config);
		const FVector Up = Rotation.Quaternion().GetAxisZ();
		const FVector Forward = Rotation.Quaternion().GetAxisX();
		const FVector Right = Rotation.Quaternion().GetAxisY();
		Scene.Floor->GetOwner()->SetActorLocationAndRotation(-Up * 50.0, Rotation);
		Scene.Reset(Up * 31.0, Rotation);
		Scene.Step(StepTime, StepTime);
		Scene.AddBox(Forward * 121.0 + Up * 200.0, FVector(20, 1000, 200), Rotation);
		Scene.Movement->Velocity = Forward * 1200.0 + Right * 400.0;
		Scene.Step(StepTime, StepTime);
		const FVector Velocity = Scene.Movement->Velocity;
		AddInfo(FString::Printf(TEXT("Oriented wall %s: velocity=%s grounded=%d"), *Rotation.ToString(), *Velocity.ToString(), Scene.Movement->IsGrounded()));
		TestTrue(TEXT("Slope and loop wall response follows the road plane"), Velocity.Equals(-Forward * 360.0 + Right * 400.0, 0.1));
		TestTrue(TEXT("Oriented contact adds no road-normal speed or lift"), FMath::Abs(FVector::DotProduct(Velocity, Up)) < 0.01 && FMath::Abs(FVector::DotProduct(Scene.Vehicle->GetActorLocation(), Up) - 31.0) < 0.01);
		TestTrue(TEXT("Oriented wall does not change chassis tilt"), Scene.Vehicle->GetActorQuat().Equals(Rotation.Quaternion(), 0.0001));
		TestTrue(TEXT("Oriented wall keeps ground support"), Scene.Movement->IsGrounded());
	}
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMWOverspeedRecoveryTest, "MW.Vehicle.OverspeedRecovery",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// 추진 입력을 유지해도 제한을 넘긴 속도가 기존 감속량으로 줄어드는지 검사한다.
bool FMWOverspeedRecoveryTest::RunTest(const FString& Parameters)
{
    const FMWVehicleDriveConfig DriveConfig;
    const float ForwardOverspeed = DriveConfig.MaxForwardSpeed + 450.f;
    // 가속키 유지·후진·공중 관성을 30/120프레임 간격으로 검사한다. 실제 키보드나 다중 PC 주행 검사는 아니다.
    for (const float FrameTime : {1.f / 30.f, 1.f / 120.f})
    {
        MWVehicleTests::FScene Scene(DriveConfig);
        Scene.Input(1.f);
        Scene.Movement->Velocity = FVector(ForwardOverspeed, 0.f, 0.f);
        Scene.Step(0.1f, FrameTime);
        TestTrue(TEXT("Overspeed decays without snapping to normal limit"),
                 Scene.Movement->Velocity.X > DriveConfig.MaxForwardSpeed && Scene.Movement->Velocity.X < ForwardOverspeed);
        Scene.Step(4.f, FrameTime);
        TestTrue(TEXT("Held throttle returns to the normal limit"),
                 FMath::IsNearlyEqual(Scene.Movement->Velocity.X, Scene.Movement->GetMaxSpeed(), 0.1f));

        Scene.Reset();
        Scene.Input(0.f, 1.f);
        Scene.Movement->Velocity = FVector(-800.f, 0.f, 0.f);
        Scene.Step(2.f, FrameTime);
        TestTrue(TEXT("Reverse overspeed also decays to its own limit"),
                 FMath::IsNearlyEqual(Scene.Movement->Velocity.X, -Scene.Movement->GetMaxSpeed(), 0.1f));

        Scene.Reset(FVector(0, 0, 1000));
        Scene.Input(1.f);
        Scene.Movement->Velocity = FVector(ForwardOverspeed, 0.f, 300.f);
        Scene.Step(0.1f, FrameTime);
        TestTrue(TEXT("Ground overspeed recovery does not clamp air momentum"),
                 FMath::IsNearlyEqual(Scene.Movement->Velocity.X, ForwardOverspeed, 0.1f));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMWRaceDrivePermissionTest, "MW.Gameplay.DriveControl.PermissionAndHold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMWRaceDrivePermissionTest::RunTest(const FString& Parameters)
{
	MWVehicleTests::FScene Scene;
	TestTrue(TEXT("Explicit DriveTest mode retains free driving"), Scene.Vehicle->CanDrive());
	Scene.World->SetGameState(nullptr);
	TestFalse(TEXT("Missing GameState never grants production driving"), Scene.Vehicle->CanDrive());
	auto* State = Scene.UseRaceRules();
	auto* Player = Scene.Vehicle->GetPlayerState<AMWPlayerState>();
	FMWGameplayProgress Progress;
	Progress.Phase = EMWRoomPhase::Racing;
	State->ApplyProgressFromServer(Progress);
	FMWPlayerRoundState Round;
	Round.RaceStatus = EMWRaceStatus::Active;
	Player->ApplyRoundStateFromServer(Round);
	TestFalse(TEXT("Default epoch zero cannot start a race"), Scene.Vehicle->CanDrive());
	Progress.RoundIndex = 1;
	Progress.RoundEpoch = 1;
	State->ApplyProgressFromServer(Progress);
	TestFalse(TEXT("An old personal epoch cannot drive"), Scene.Vehicle->CanDrive());
	Round.RoundEpoch = 1;
	Round.RaceStatus = EMWRaceStatus::Waiting;
	Player->ApplyRoundStateFromServer(Round);
	TestFalse(TEXT("Waiting cannot drive in Racing"), Scene.Vehicle->CanDrive());
	Round.RaceStatus = EMWRaceStatus::Active;
	Player->ApplyRoundStateFromServer(Round);
	TestTrue(TEXT("Matching nonzero epoch and Active enable driving"), Scene.Vehicle->CanDrive());
	Scene.Input(1, 0, 0.5f, true);
	Scene.Step(0.3f);
	TestTrue(TEXT("Allowed input moves the vehicle"), Scene.Movement->Velocity.Size() > 100);

	for (const EMWRoomPhase Phase : {EMWRoomPhase::PreparingRace, EMWRoomPhase::Countdown,
		EMWRoomPhase::RoundResults, EMWRoomPhase::MatchResults, EMWRoomPhase::Failed})
	{
		Progress.Phase = Phase;
		State->ApplyProgressFromServer(Progress);
		const FTransform Held = Scene.Vehicle->GetActorTransform();
		Scene.Movement->Velocity = FVector(500, 400, -300);
		// Movement의 공개 입력 API를 우회해도 경기 잠금을 우회할 수 없다.
		FMWVehicleDriveInput Input;
		Input.Throttle = 1;
		Input.Steering = 1;
		Scene.Movement->SetDriveInput(Input);
		Scene.Input(1, 0, 1, true);
		Scene.Step(0.2f);
		TestFalse(TEXT("Only Racing grants permission"), Scene.Vehicle->CanDrive());
		TestTrue(TEXT("Held vehicle has no linear velocity"), Scene.Movement->Velocity.IsZero());
		TestTrue(TEXT("Held vehicle neither translates nor rotates"), Held.Equals(Scene.Vehicle->GetActorTransform(), 0.001));
		TestTrue(TEXT("Denied Pawn input is cleared"), Scene.Vehicle->GetDriveInput().Throttle == 0
			&& !Scene.Vehicle->GetDriveInput().bDriftRequested);
		Progress.Phase = EMWRoomPhase::Racing;
		State->ApplyProgressFromServer(Progress);
		Scene.Step(0.1f);
		TestTrue(TEXT("Resume does not restore the old throttle"), Scene.Movement->Velocity.IsNearlyZero());
	}
	Scene.Input(1);
	Scene.Step(0.2f);
	TestTrue(TEXT("Fresh input works after resume"), Scene.Movement->Velocity.Size() > 100);
	Scene.Vehicle->SetPlayerState(nullptr);
	TestFalse(TEXT("Missing PlayerState blocks driving"), Scene.Vehicle->CanDrive());
	Scene.Vehicle->SetPlayerState(Player);
	Player->SetOwner(nullptr);
	TestFalse(TEXT("An unrelated PlayerState blocks driving"), Scene.Vehicle->CanDrive());
	Player->SetOwner(Scene.Controller);

	// Active 차량이 Waiting 차량을 때려도 대상에 속도·회전 충격을 주지 않는다.
	Scene.Reset();
	auto* OtherPawn = Scene.World->SpawnActor<AMWVehiclePawn>(FVector(202, 0, 31), FRotator::ZeroRotator);
	auto* Other = CastChecked<UMWVehicleMovementComponent>(OtherPawn->GetMovementComponent());
	FindFProperty<FObjectPropertyBase>(Other->GetClass(), TEXT("DriveData"))->SetObjectPropertyValue_InContainer(
		Other, NewObject<UMWVehicleDriveData>(OtherPawn));
	auto* OtherController = Scene.World->SpawnActor<APlayerController>();
	auto* OtherPlayer = Scene.World->SpawnActor<AMWPlayerState>();
	OtherPlayer->SetOwner(OtherController);
	OtherController->PlayerState = OtherPlayer;
	OtherController->Possess(OtherPawn);
	OtherPawn->SetPlayerState(OtherPlayer);
	OtherPawn->DispatchBeginPlay();
	const FTransform OtherHeld = OtherPawn->GetActorTransform();
	Scene.Movement->Velocity = FVector(900, 0, 0);
	Scene.Step(0.1f);
	TestTrue(TEXT("Locked target is not given impact velocity"), Other->Velocity.IsZero());
	TestTrue(TEXT("Locked target stays in place"), OtherHeld.Equals(OtherPawn->GetActorTransform()));
	TestTrue(TEXT("Active vehicle is blocked by the locked Box"), Scene.Vehicle->GetActorLocation().X < 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMWRaceDriveNetworkTest, "MW.Gameplay.DriveControl.StreamAndPrediction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMWRaceDriveNetworkTest::RunTest(const FString& Parameters)
{
	MWVehicleTests::FScene Scene;
	auto* State = Scene.UseRaceRules();
	auto* Player = Scene.Vehicle->GetPlayerState<AMWPlayerState>();
	auto* Pawn = Scene.Vehicle;
	FMWGameplayProgress Progress;
	Progress.Phase = EMWRoomPhase::Racing;
	Progress.RoundEpoch = 1;
	State->ApplyProgressFromServer(Progress);
	FMWPlayerRoundState Round;
	Round.RoundEpoch = 1;
	Round.RaceStatus = EMWRaceStatus::Active;
	Player->ApplyRoundStateFromServer(Round);
	// 엔진의 테스트 연결로 소유 연결 검사만 충족한다. 소켓/패킷 전달 시험은 아니다.
	auto* Connection = NewObject<USimulatedClientNetConnection>(Scene.World);
	Scene.Controller->Player = Connection;
	Scene.Controller->NetConnection = Connection;
	FMWVehicleMoveCommand Command;
	Command.MoveId = 1;
	Command.Input.Throttle = 1;
	const TArray<FMWVehicleMoveCommand> Packet{Command};
	const uint32 FirstStream = Pawn->ServerDriveSnapshot.StreamId;
	Pawn->ServerSubmitDriveMoves_Implementation(FirstStream, Packet);
	TestEqual(TEXT("Allowed current-stream command is queued"), Pawn->QueuedServerMoves.Num(), 1);
	Pawn->TickAuthorityDrive(1.f / 120.f);
	TestEqual(TEXT("Authority actually processes the command"), Pawn->LastProcessedMoveId, 1u);

	Progress.Phase = EMWRoomPhase::Countdown;
	State->ApplyProgressFromServer(Progress);
	const uint32 HeldStream = Pawn->ServerDriveSnapshot.StreamId;
	TestTrue(TEXT("Stopping creates a new stream"), HeldStream != FirstStream);
	TestFalse(TEXT("Locked snapshot announces denial"), Pawn->ServerDriveSnapshot.bDriveAllowed);
	TestTrue(TEXT("Stopping clears old queued commands"), Pawn->QueuedServerMoves.IsEmpty());
	TestTrue(TEXT("Stopping publishes zero velocity"), Pawn->ServerDriveSnapshot.Velocity.IsZero());
	Pawn->ServerSubmitDriveMoves_Implementation(HeldStream, Packet);
	TestTrue(TEXT("Even a current-stream packet is rejected while locked"), Pawn->QueuedServerMoves.IsEmpty());
	Pawn->RefreshDrivePermission();
	TestEqual(TEXT("Repeated refresh does not churn streams"), Pawn->ServerDriveSnapshot.StreamId, HeldStream);

	Progress.Phase = EMWRoomPhase::Racing;
	State->ApplyProgressFromServer(Progress);
	const uint32 ResumedStream = Pawn->ServerDriveSnapshot.StreamId;
	TestTrue(TEXT("Resuming creates another stream"), ResumedStream != HeldStream);
	Pawn->ServerSubmitDriveMoves_Implementation(FirstStream, Packet);
	Pawn->ServerSubmitDriveMoves_Implementation(HeldStream, Packet);
	TestTrue(TEXT("All previous streams are rejected after resume"), Pawn->QueuedServerMoves.IsEmpty());
	Pawn->ServerSubmitDriveMoves_Implementation(ResumedStream, Packet);
	Pawn->TickAuthorityDrive(1.f / 120.f);
	TestEqual(TEXT("Fresh resumed stream processes move one"), Pawn->LastProcessedMoveId, 1u);

	// 같은 Pawn을 소유자 역할로 전환해 실제 예측 함수를 검사한다.
	Scene.Controller->Player = nullptr;
	Scene.Controller->NetConnection = nullptr;
	Scene.Controller->SetAsLocalPlayerController();
	Pawn->SetRole(ROLE_AutonomousProxy);
	Pawn->ServerDriveSnapshot.bDriveAllowed = false;
	Pawn->ServerDriveSnapshot.ImpactYawRate = 90.f;
	Pawn->RefreshDrivePermission();
	TestFalse(TEXT("Phase and personal state cannot bypass missing server permission"), Pawn->CanDrive());
	Pawn->TickPredictedDrive(1.f / 120.f);
	TestTrue(TEXT("Denied owner builds no predicted inputs"), Pawn->PredictedMoves.IsEmpty());
	Pawn->ServerDriveSnapshot.bDriveAllowed = true;
	Pawn->ServerDriveSnapshot.RoundEpoch = 2;
	TestFalse(TEXT("A snapshot for another epoch cannot enable prediction"), Pawn->CanDrive());
	Pawn->ServerDriveSnapshot.RoundEpoch = 1;
	Pawn->RefreshDrivePermission();
	Scene.Input(1);
	Pawn->TickPredictedDrive(1.f / 120.f);
	TestEqual(TEXT("Fully ready owner predicts immediately"), Pawn->PredictedMoves.Num(), 1);
	Round.RaceStatus = EMWRaceStatus::Waiting;
	Player->ApplyRoundStateFromServer(Round);
	TestTrue(TEXT("Personal lock discards unacknowledged prediction"), Pawn->PredictedMoves.IsEmpty());
	Pawn->TickPredictedDrive(1.f / 120.f);
	const FTransform Corrected = Pawn->GetActorTransform();
	Pawn->TickPredictedDrive(1.f / 120.f);
	TestTrue(TEXT("Held owner stays fixed after applying the current authoritative pose"), Corrected.Equals(Pawn->GetActorTransform()));
	TestTrue(TEXT("Held owner never rebuilds predicted history"), Pawn->PredictedMoves.IsEmpty());
	Pawn->SetRole(ROLE_Authority);
	Pawn->PublishDriveSnapshot();
	TestEqual(TEXT("Holding clears collision-induced rotation as well"), Pawn->ServerDriveSnapshot.ImpactYawRate, 0.f);
	Round.RaceStatus = EMWRaceStatus::Active;
	Player->ApplyRoundStateFromServer(Round);
	Scene.Input(1);
	Pawn->TickAuthorityDrive(1.f / 120.f);
	TestTrue(TEXT("Listen-host input uses the same permission and resumes without an RPC"), Scene.Movement->Velocity.Size() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMWRaceDriveSnapshotTest, "MW.Gameplay.DriveControl.SnapshotSerialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMWRaceDriveSnapshotTest::RunTest(const FString& Parameters)
{
	for (const bool bAllowed : {false, true})
	{
		FMWVehicleDriveSnapshot Sent;
		Sent.StreamId = 11;
		Sent.Revision = 23;
		Sent.LastProcessedMoveId = 7;
		Sent.bDriveAllowed = bAllowed;
		Sent.RoundEpoch = 42;
		Sent.Location = FVector(100, 200, 300);
		Sent.Velocity = FVector(800, 0, 0);
		Sent.ImpactYawRate = 25.f;
		Sent.GroundNormal = FVector(0, 0.6, 0.8);
		Sent.bGrounded = true;
		Sent.ServerTimeSeconds = 17.25;
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes);
		bool bWrite = false;
		Sent.NetSerialize(Writer, nullptr, bWrite);
		FMWVehicleDriveSnapshot Received;
		Received.bDriveAllowed = !bAllowed;
		FMemoryReader Reader(Bytes);
		bool bRead = false;
		Received.NetSerialize(Reader, nullptr, bRead);
		TestTrue(TEXT("Full drive snapshot serializes successfully"), bWrite && bRead && Reader.Tell() == Reader.TotalSize());
		TestEqual(TEXT("Permission is transmitted"), Received.bDriveAllowed, bAllowed);
		TestEqual(TEXT("Round epoch is transmitted with the stream"), Received.RoundEpoch, Sent.RoundEpoch);
		TestEqual(TEXT("Stream survives serialization"), Received.StreamId, Sent.StreamId);
		TestEqual(TEXT("ACK survives serialization"), Received.LastProcessedMoveId, Sent.LastProcessedMoveId);
		TestTrue(TEXT("Pose and motion survive serialization"), Received.Location.Equals(Sent.Location, 0.1)
			&& Received.Velocity.Equals(Sent.Velocity, 0.1) && Received.ImpactYawRate == Sent.ImpactYawRate
			&& Received.bGrounded && Received.GroundNormal.Equals(Sent.GroundNormal, 0.001)
			&& Received.ServerTimeSeconds == Sent.ServerTimeSeconds);
	}
	return true;
}


#endif
