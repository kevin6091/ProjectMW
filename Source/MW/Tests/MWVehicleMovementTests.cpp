#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "Gameplay/Track/MWTrackRuntimeSubsystem.h"
#include "Gameplay/Track/MWTrackRuntimeComponent.h"
#include "Gameplay/Track/MWTrackRuntimeSettings.h"
#include "Gameplay/Construction/MWTrackConstructionComponent.h"
#include "Gameplay/Items/MWItemComponent.h"
#include "Gameplay/Vehicle/MWVehicleMovementComponent.h"
#include "Data/MWVehicleDriveData.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "System/MWDriveTestGameMode.h"
#include "System/GameState/MWGameplayGameState.h"
#include "System/MWPlayerState.h"
#include "System/MWGameInstance.h"
#include "System/GameMode/MWGameplayGameMode.h"
#include "Player/MWGameplayPlayerController.h"
#include "Gameplay/PlayerStart/MWPlayerStartManager.h"
#include "Gameplay/Construction/MWTrackSelectionBoard.h"
#include "Gameplay/Construction/MWPlayerConstructionComponent.h"
#include "Gameplay/Construction/MWTrackSnap.h"
#include "Data/MWTrackDefinitionData.h"
#include "System/MWAssetManager.h"
#include "Components/ArrowComponent.h"
#include "Algo/Count.h"
#include "Algo/AllOf.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/NetConnection.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/UnrealType.h"
#include <limits>
#if WITH_EDITOR
#include "AssetCompilingManager.h"
#endif

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

	explicit FScene(const FMWVehicleDriveConfig& InConfig = FMWVehicleDriveConfig(), bool bInitializeActors = false)
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		if (bInitializeActors)
		{
			World->InitializeActorsForPlay(FURL());
		}
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
		if (World->HasBegunPlay()) World->EndPlay(EEndPlayReason::Quit);
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

	// 안쪽으로 줄인 스윕도 막히지만 차체 아래의 바닥은 이어지는 상황을 만든다.
	Scene.Floor = Scene.AddBox(FVector(0, 0, -50), FVector(1000, 1000, 50));
	Scene.Reset(Start);
	Scene.Step(StepTime, StepTime);
	UBoxComponent* SideBlocker = Scene.AddBox(FVector(0, 69, 30), FVector(500, 20, 20));
	SideBlocker->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Scene.Step(StepTime, StepTime);
	TestTrue(TEXT("Continuous floor supports the vehicle when both box sweeps are blocked"), Scene.Movement->IsGrounded());
	TestTrue(TEXT("Fallback preserves ground clearance"),
		FMath::IsNearlyEqual(Scene.Vehicle->GetActorLocation().Z, Start.Z, 0.02));

	// 기존 스윕 거리보다 조금 멀어진 연속 노면까지는 보조 검사로 다시 따라간다.
	Scene.Floor->GetOwner()->SetActorLocation(FVector(0, 0, -56));
	Scene.Step(StepTime, StepTime);
	TestTrue(TEXT("Fallback retains nearby continuous ground beyond the box sweep"), Scene.Movement->IsGrounded());
	TestTrue(TEXT("Fallback uses the existing downward clearance correction"), Scene.Vehicle->GetActorLocation().Z < Start.Z);

	// 중앙에 틈이 생기면 앞뒤 지점만으로 접지를 이어 가지 않는다.
	Scene.Floor->GetOwner()->Destroy();
	Scene.AddBox(FVector(70, 0, -56), FVector(50, 1000, 50));
	Scene.AddBox(FVector(-70, 0, -56), FVector(50, 1000, 50));
	Scene.Step(StepTime, StepTime);
	TestFalse(TEXT("Fallback does not bridge a gap under the vehicle"), Scene.Movement->IsGrounded());
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMWCheckpointVelocityTest, "MW.Vehicle.CheckpointVelocity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMWCheckpointVelocityTest::RunTest(const FString& Parameters)
{
	MWVehicleTests::FScene Scene;
	UMWTrackRuntimeSubsystem* Tracks = Scene.World->GetSubsystem<UMWTrackRuntimeSubsystem>();
	if (!TestNotNull(TEXT("트랙 런타임 생성"), Tracks))
	{
		return false;
	}

	Scene.Input(1.f);
	Scene.Step(.5f);
	const FVector PreviousVelocity = Scene.Movement->Velocity;
	TestFalse(TEXT("복귀 전 차량 주행"), PreviousVelocity.IsNearlyZero());
	TestFalse(TEXT("기록된 체크포인트가 없으면 복귀 실패"), Tracks->RespawnAtTrackCheckpoint(Scene.Vehicle));
	TestTrue(TEXT("실패한 복귀는 주행 속도 유지"), Scene.Movement->Velocity.Equals(PreviousVelocity));

	const FVector Target(1000, 1000, 60);
	Scene.Vehicle->TeleportTo(Target, FRotator::ZeroRotator, false, true);
	TestTrue(TEXT("일반 텔레포트는 속도 유지"), Scene.Movement->Velocity.Equals(PreviousVelocity));

	// 트랙의 로컬 Y축을 출발 방향으로 쓰는 실제 복귀 경로를 지난다.
	AActor* Track = Scene.World->SpawnActor<AActor>();
	const FVector Checkpoint = Target + FVector(0, 500, 0);
	Tracks->ProcessRaceTrigger(*Scene.Vehicle, *Track, EMWTrackTrigger::Checkpoint, { FTransform(Checkpoint) });
	TestTrue(TEXT("기록된 체크포인트 복귀"), Tracks->RespawnAtTrackCheckpoint(Scene.Vehicle));
	TestTrue(TEXT("저장 위치로 복귀"), Scene.Vehicle->GetActorLocation().Equals(Checkpoint, 1.f));
	TestTrue(TEXT("체크포인트의 진행 방향 적용"), Scene.Vehicle->GetActorForwardVector().Equals(FVector::RightVector));
	TestTrue(TEXT("복귀 직후 정지"), Scene.Movement->Velocity.IsNearlyZero());
	Scene.Input(1.f);
	Scene.Step(.3f);
	TestTrue(TEXT("복귀 후 새 방향으로 재출발"), Scene.Movement->Velocity.Y > 0.f);
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

	// 완주/다음 준비의 충돌 비트가 PlayerState보다 먼저 와도 서버 위치에서만 복원한다.
	const auto CollisionBeforeFinish = Pawn->CollisionRoot->GetCollisionEnabled();
	Round.RaceStatus = EMWRaceStatus::Finished;
	Round.FinishOrder = 1;
	Player->ApplyRoundStateFromServer(Round);
	const FMWVehicleDriveSnapshot FinishedSnapshot = Pawn->ServerDriveSnapshot;
	TestTrue(TEXT("Authority publishes disabled collision and a stopped new stream"),
		FinishedSnapshot.bFinishCollisionDisabled && !FinishedSnapshot.bDriveAllowed
		&& FinishedSnapshot.Velocity.IsNearlyZero() && FMath::IsNearlyZero(FinishedSnapshot.ImpactYawRate));
	TestFalse(TEXT("Finished root cannot block another vehicle"), Pawn->CollisionRoot->IsQueryCollisionEnabled());
	Pawn->SetFinishCollisionDisabled(false);
	Pawn->SetRole(ROLE_AutonomousProxy);
	Pawn->OnRep_ServerDriveSnapshot();
	TestFalse(TEXT("Owner applies the finish collision bit independently of personal-state order"), Pawn->CollisionRoot->IsQueryCollisionEnabled());
	TestTrue(TEXT("Finishing discards the owner's prediction history"), Pawn->PredictedMoves.IsEmpty());
	Pawn->TickPredictedDrive(1.f / 120.f);
	TestTrue(TEXT("A finished owner does not resume prediction"), Pawn->PredictedMoves.IsEmpty());
	Pawn->SetFinishCollisionDisabled(false);
	Pawn->SetRole(ROLE_SimulatedProxy);
	Pawn->SetPlayerState(nullptr);
	Pawn->ServerDriveSnapshot = FinishedSnapshot;
	Pawn->OnRep_ServerDriveSnapshot();
	TestFalse(TEXT("A proxy without PlayerState also disables collision from the drive snapshot"), Pawn->CollisionRoot->IsQueryCollisionEnabled());
	FMWVehicleDriveSnapshot PreparedSnapshot = FinishedSnapshot;
	++PreparedSnapshot.StreamId;
	PreparedSnapshot.RoundEpoch = 2;
	PreparedSnapshot.Location = FVector(1500, 300, 31);
	PreparedSnapshot.bFinishCollisionDisabled = false;
	Pawn->ServerDriveSnapshot = PreparedSnapshot;
	Pawn->OnRep_ServerDriveSnapshot();
	TestEqual(TEXT("Proxy restores its original collision setting"), Pawn->CollisionRoot->GetCollisionEnabled(), CollisionBeforeFinish);
	TestTrue(TEXT("Proxy restores collision at the new authoritative pose"), Pawn->GetActorLocation().Equals(PreparedSnapshot.Location, 0.1));
	Pawn->TickProxyDrive(1.f / 60.f);
	TestTrue(TEXT("Proxy presentation never returns to the old finish pose"), Pawn->GetActorLocation().Equals(PreparedSnapshot.Location, 0.1));
	Pawn->SetPlayerState(Player);
	Pawn->SetRole(ROLE_Authority);

	// 머지 회귀: 실제 예측 경로에서 한 스텝의 충돌 회전과 상대 접촉 검사를 함께 확인한다.
	Round.RaceStatus = EMWRaceStatus::Active;
	Round.FinishOrder = 0;
	Player->ApplyRoundStateFromServer(Round);
	Scene.Reset(FVector(0, 0, 1000));
	Pawn->SetRole(ROLE_AutonomousProxy);
	const auto ResetAirPrediction = [&]()
	{
		FMWVehicleDriveSnapshot Air;
		Air.StreamId = Pawn->AppliedStreamId + 1;
		Air.bDriveAllowed = true;
		Air.RoundEpoch = Progress.RoundEpoch;
		Air.Location = FVector(0, 0, 1000);
		Air.ImpactYawRate = 90.f;
		Pawn->ServerDriveSnapshot = Air;
		Pawn->TickPredictedDrive(0.f); // 새 스트림 복원만 하고 이동은 아래 한 스텝에서 수행한다.
	};
	ResetAirPrediction();
	AMWVehiclePawn::FPredictedMove FreeRotation;
	FreeRotation.ResultTime = 1.0 / 120.0;
	Pawn->SimulatePredictedMove(FreeRotation);
	TestTrue(TEXT("One predicted step integrates 90 degrees per second into 0.75 degrees"),
		FMath::IsNearlyEqual(FreeRotation.Result.Rotation.Yaw, 0.75, 0.001));
	TestTrue(TEXT("Collision yaw damping is applied once per predicted step"),
		FMath::IsNearlyEqual(FreeRotation.Result.ImpactYawRate, 90.f * FMath::Exp(-MWVehicleImpactYawDamping / 120.f), 0.001f));

	ResetAirPrediction();
	AMWVehiclePawn::FPredictedMove BlockedRotation;
	BlockedRotation.ResultTime = 1.0 / 120.0;
	FMWVehicleContactSample Contact{};
	Contact.Vehicle = FObjectKey(Scene.Floor->GetOwner()); // 실제 차량 충돌체 대신 저장된 예측 접촉만 사용한다.
	Contact.StreamId = Contact.Revision = 1;
	Contact.Extent = Pawn->CollisionRoot->GetScaledBoxExtent();
	Contact.Location = Contact.SourceLocation = FVector(0, 2 * Contact.Extent.Y + 0.5, 1000);
	Contact.Rotation = Contact.SourceRotation = FQuat::Identity;
	Contact.Velocity = Contact.SourceVelocity = FVector::ZeroVector;
	Contact.Tuning = {1200.f, 0.1f, 1.f, 100.f};
	BlockedRotation.Contacts.Add(Contact);
	Pawn->SimulatePredictedMove(BlockedRotation);
	TestTrue(TEXT("A saved predicted contact blocks rotation into another vehicle"),
		FMath::IsNearlyZero(BlockedRotation.Result.Rotation.Yaw, 0.001));
	TestFalse(TEXT("Airborne rotation does not invent ground support"), BlockedRotation.Result.bGrounded);
	Pawn->SetRole(ROLE_Authority);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMWPersonalFinishTest, "MW.Gameplay.DriveControl.PersonalFinish",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMWPersonalFinishTest::RunTest(const FString& Parameters)
{
	MWVehicleTests::FScene Scene(FMWVehicleDriveConfig{}, true);
	Scene.Controller->UnPossess();
	Scene.Controller->Destroy();
	Scene.Controller = Scene.World->SpawnActor<AMWGameplayPlayerController>();
	auto* State = Scene.UseRaceRules();
	auto* Player = Scene.Vehicle->GetPlayerState<AMWPlayerState>();
	Player->SetLobbyMemberIdFromServer(1);
	State->AddPlayerState(Player);
	auto* App = NewObject<UMWGameInstance>(GEngine);
	Scene.World->SetGameInstance(App);
	// 이 비상수 fixture의 최초 명단만 설정한다. 테스트용 Runtime API나 GameMode를 추가하지 않는다.
	const_cast<TArray<int32>&>(App->GetStartingPlayers()) = {1, 2};
	auto* Mode = Scene.World->SpawnActor<AMWGameplayGameMode>();
	Scene.World->SetGameState(State);
	FindFProperty<FObjectPropertyBase>(Mode->GetClass(), TEXT("GameState"))->SetObjectPropertyValue_InContainer(Mode, State);
	FindFProperty<FObjectPropertyBase>(Scene.World->GetClass(), TEXT("AuthorityGameMode"))->SetObjectPropertyValue_InContainer(Scene.World, Mode);
	auto* FinishTrack = Scene.World->SpawnActor<AActor>();
	auto* FinishRoot = NewObject<USceneComponent>(FinishTrack, TEXT("Root"));
	FinishTrack->SetRootComponent(FinishRoot);
	FinishRoot->RegisterComponent();
	FinishTrack->SetActorLocation(FVector(500, 0, 0));
	auto* Port = NewObject<USceneComponent>(FinishTrack, TEXT("ConnectionPoint1"));
	Port->SetupAttachment(FinishRoot);
	Port->SetRelativeRotation(FRotator(0, -90, 0));
	Port->RegisterComponent();
	auto* TrackComponent = NewObject<UMWTrackRuntimeComponent>(FinishTrack);
	FinishTrack->AddInstanceComponent(TrackComponent);
	TrackComponent->Configure(FMWTrackDefinition{});
	TrackComponent->RegisterComponent();
	FinishTrack->DispatchBeginPlay();
	auto* Runtime = Scene.World->GetSubsystem<UMWTrackRuntimeSubsystem>();
	TestTrue(TEXT("A plain end cap receives the existing server finish trigger"), Runtime->PrepareCourseFinish(*FinishTrack, FVector(50, 500, 150)));
	const int32 TriggerCount = FinishTrack->GetComponents().Num();
	TestTrue(TEXT("Repeated preparation reuses the same finish trigger"), Runtime->PrepareCourseFinish(*FinishTrack, FVector(50, 500, 150)));
	TestEqual(TEXT("Finish preparation does not duplicate components"), FinishTrack->GetComponents().Num(), TriggerCount);
	auto* OtherTrack = Scene.World->SpawnActor<AActor>();
	auto* Construction = State->GetConstructionComponent();
	auto& Course = *FindFProperty<FStructProperty>(Construction->GetClass(), TEXT("PlacementSession"))
		->ContainerPtrToValuePtr<FMWTrackPlacementSession>(Construction);
	Course.SessionId = 7;
	Course.CourseRevision = 3;
	FMWPlacedTrack Finish;
	Finish.PieceId = 1;
	Finish.TrackClass = GetDefault<UMWTrackRuntimeSettings>()->FinishTrackClass;
	Finish.Actor = FinishTrack;
	Finish.WorldTransform = FinishTrack->GetActorTransform();
	Finish.bRequiresCollision = true;
	Course.PlacedTracks.Add(Finish);
	TestTrue(TEXT("Fixture has a complete current course"), Construction->IsCourseReadyForRace(7, 3));
	FMWGameplayProgress Progress;
	Progress.Phase = EMWRoomPhase::Racing;
	Progress.RoundEpoch = 1;
	Progress.RoundIndex = 1;
	Progress.CourseSessionId = 7;
	Progress.CourseRevision = 3;
	State->ApplyProgressFromServer(Progress);
	FMWPlayerRoundState Round;
	Round.RoundEpoch = 1;
	Round.RaceStatus = EMWRaceStatus::Active;
	Player->ApplyRoundStateFromServer(Round);
	Runtime->BeginPlayerRace(*Scene.Vehicle, 1);
	auto* Root = CastChecked<UBoxComponent>(Scene.Vehicle->GetRootComponent());
	Root->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Scene.Input(1);
	Scene.Step(0.15f);
	TestTrue(TEXT("Participant is moving before finishing"), Scene.Movement->Velocity.Size() > 0);
	TestTrue(TEXT("Participant can hold an item before finishing"), Scene.Vehicle->GetItemComponent()->TryGrantBooster());
	TestFalse(TEXT("Old epoch cannot finish"), Mode->TryFinishPlayerRace(*Scene.Vehicle, *FinishTrack, 0));
	TestFalse(TEXT("An unrelated track cannot finish"), Mode->TryFinishPlayerRace(*Scene.Vehicle, *OtherTrack, 1));
	++Course.CourseRevision;
	TestFalse(TEXT("A changed course cannot finish the previous race"), Mode->TryFinishPlayerRace(*Scene.Vehicle, *FinishTrack, 1));
	--Course.CourseRevision;
	Scene.World->SetBegunPlay(true);
	Scene.Step(2.0f);
	Scene.World->SetBegunPlay(false);
	TestTrue(TEXT("Existing runtime trigger stores official Finished"), Player->GetRoundState().RaceStatus == EMWRaceStatus::Finished);
	TestEqual(TEXT("Rejected attempts do not consume the first finish order"), Player->GetRoundState().FinishOrder, 1);
	TestTrue(TEXT("Legacy progress mirrors only approved completion"), Runtime->GetTrackRaceProgress(Scene.Vehicle).bFinished);
	TestFalse(TEXT("Finished vehicle has no blocking or overlap collision"), Root->IsCollisionEnabled());
	TestTrue(TEXT("Completion clears velocity and held input"), Scene.Movement->Velocity.IsZero() && Scene.Vehicle->GetDriveInput().Throttle == 0);
	TestTrue(TEXT("Completion clears the held item"), Scene.Vehicle->GetItemComponent()->GetCurrentItemId() == EMWItemId::None);
	TestFalse(TEXT("Completion rejects another item grant"), Scene.Vehicle->GetItemComponent()->TryGrantBooster());
	TestFalse(TEXT("Duplicate completion is rejected"), Mode->TryFinishPlayerRace(*Scene.Vehicle, *FinishTrack, 1));
	TestFalse(TEXT("Finished player cannot respawn through the checkpoint path"), Runtime->RespawnAtTrackCheckpoint(Scene.Vehicle));
	const FTransform Held = Scene.Vehicle->GetActorTransform();
	Scene.Input(1, 0, 1, true);
	Scene.Step(0.2f);
	TestTrue(TEXT("Finished vehicle remains fixed despite more input"), Held.Equals(Scene.Vehicle->GetActorTransform(), 0.001));

	// 같은 서버의 두 번째 참가자도 실제 소유/명단 검사를 거쳐 다음 순서를 받는다.
	auto* OtherVehicle = Scene.World->SpawnActor<AMWVehiclePawn>(FVector(1000, 0, 31), FRotator::ZeroRotator);
	auto* OtherMovement = CastChecked<UMWVehicleMovementComponent>(OtherVehicle->GetMovementComponent());
	FindFProperty<FObjectPropertyBase>(OtherMovement->GetClass(), TEXT("DriveData"))->SetObjectPropertyValue_InContainer(
		OtherMovement, NewObject<UMWVehicleDriveData>(OtherVehicle));
	auto* OtherController = Scene.World->SpawnActor<AMWGameplayPlayerController>();
	auto* OtherPlayer = Scene.World->SpawnActor<AMWPlayerState>();
	OtherPlayer->SetOwner(OtherController);
	OtherController->PlayerState = OtherPlayer;
	OtherController->Possess(OtherVehicle);
	OtherVehicle->SetPlayerState(OtherPlayer);
	OtherVehicle->DispatchBeginPlay();
	OtherPlayer->SetLobbyMemberIdFromServer(2);
	State->AddPlayerState(OtherPlayer);
	OtherPlayer->ApplyRoundStateFromServer(Round);
	Runtime->BeginPlayerRace(*OtherVehicle, 1);
	const_cast<TArray<int32>&>(App->GetStartingPlayers()).Remove(2);
	TestFalse(TEXT("A player outside the starting roster cannot finish"), Mode->TryFinishPlayerRace(*OtherVehicle, *FinishTrack, 1));
	const_cast<TArray<int32>&>(App->GetStartingPlayers()).Add(2);
	Runtime->ProcessRaceTrigger(*OtherVehicle, *FinishTrack, EMWTrackTrigger::Finish, {});
	TestEqual(TEXT("The second accepted completion receives order two"), OtherPlayer->GetRoundState().FinishOrder, 2);
	TestTrue(TEXT("All personal finishes advance to RoundResults"), State->GetRoomPhase() == EMWRoomPhase::RoundResults);
	TestEqual(TEXT("RoundResults publishes an immutable result snapshot"), State->GetProgress().Results.Num(), 2);

	// 다음 준비는 공개 진입점을 사용한다. 전체 라운드 반복 전환은 이 작업에 추가하지 않는다.
	Progress.Phase = EMWRoomPhase::PreparingRace;
	Progress.RoundEpoch = 2;
	State->ApplyProgressFromServer(Progress);
	Round.RoundEpoch = 2;
	Round.RaceStatus = EMWRaceStatus::Waiting;
	Player->ApplyRoundStateFromServer(Round);
	TestFalse(TEXT("Waiting alone cannot restore collision at the old finish line"), Root->IsCollisionEnabled());
	const FVector NewStart(3000, 500, 31);
	TestTrue(TEXT("The completed vehicle can be moved to a new start"), Scene.Vehicle->TeleportTo(NewStart, FRotator::ZeroRotator));
	Scene.Vehicle->PrepareForRaceFromServer();
	TestTrue(TEXT("Preparation restores the exact pre-finish collision mode"), Root->GetCollisionEnabled() == ECollisionEnabled::QueryOnly);
	TestTrue(TEXT("Collision is restored at the new start"), Scene.Vehicle->GetActorLocation().Equals(NewStart, 0.1));
	TestFalse(TEXT("Prepared vehicle remains locked until Racing/Active"), Scene.Vehicle->CanDrive());
	Runtime->BeginPlayerRace(*Scene.Vehicle, 2);
	TestFalse(TEXT("New race clears the previous runtime finish record"), Runtime->GetTrackRaceProgress(Scene.Vehicle).bFinished);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMWGameplayRoundFlowTest, "MW.Gameplay.RoundLoop.ScoringExpansionAndSuddenDeath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMWGameplayRoundFlowTest::RunTest(const FString& Parameters)
{
	MWVehicleTests::FScene Scene(FMWVehicleDriveConfig{}, true);
	Scene.Controller->UnPossess();
	Scene.Controller->Destroy();
	Scene.Controller = Scene.World->SpawnActor<AMWGameplayPlayerController>();
	auto* State = Scene.UseRaceRules();
	TArray<AMWGameplayPlayerController*> Controllers{CastChecked<AMWGameplayPlayerController>(Scene.Controller)};
	TArray<AMWVehiclePawn*> Vehicles{Scene.Vehicle};
	for (int32 Index = 1; Index < 3; ++Index)
	{
		auto* Vehicle = Scene.World->SpawnActor<AMWVehiclePawn>(FVector(-1000, Index * 500, 60), FRotator::ZeroRotator);
		auto* Movement = CastChecked<UMWVehicleMovementComponent>(Vehicle->GetMovementComponent());
		FindFProperty<FObjectPropertyBase>(Movement->GetClass(), TEXT("DriveData"))->SetObjectPropertyValue_InContainer(Movement, NewObject<UMWVehicleDriveData>(Vehicle));
		auto* PC = Scene.World->SpawnActor<AMWGameplayPlayerController>();
		auto* PS = Scene.World->SpawnActor<AMWPlayerState>();
		PS->SetOwner(PC);
		PC->PlayerState = PS;
		PC->Possess(Vehicle);
		Vehicle->SetPlayerState(PS);
		Vehicle->DispatchBeginPlay();
		Controllers.Add(PC);
		Vehicles.Add(Vehicle);
	}
	TArray<AMWPlayerState*> Players;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		auto* PS = Vehicles[Index]->GetPlayerState<AMWPlayerState>();
		PS->SetLobbyMemberIdFromServer(Index + 1);
		PS->ConfirmVehicleAssetIDFromServer(FGameplayTag::RequestGameplayTag(FName(TEXT("Asset.Vehicle.InvisiblePilot"))));
		State->AddPlayerState(PS);
		Players.Add(PS);
	}
	auto* App = NewObject<UMWGameInstance>(GEngine);
	Scene.World->SetGameInstance(App);
	const_cast<TArray<int32>&>(App->GetStartingPlayers()) = {1, 2, 3};
	auto* Mode = Scene.World->SpawnActor<AMWGameplayGameMode>();
	Scene.World->SetGameState(State);
	FindFProperty<FObjectPropertyBase>(Mode->GetClass(), TEXT("GameState"))->SetObjectPropertyValue_InContainer(Mode, State);
	FindFProperty<FObjectPropertyBase>(Scene.World->GetClass(), TEXT("AuthorityGameMode"))->SetObjectPropertyValue_InContainer(Scene.World, Mode);
	Mode->InitGameState();
	Mode->StartManager = Scene.World->SpawnActor<AMWPlayerStartManager>();
	TInlineComponentArray<UArrowComponent*> Slots(Mode->StartManager.Get());
	for (int32 Index = 0; Index < Slots.Num(); ++Index) Slots[Index]->SetRelativeLocation(FVector(-300, Index * 400 - 600, 60));
	Mode->SelectionBoard = Scene.World->SpawnActor<AMWTrackSelectionBoard>();
	FindFProperty<FDoubleProperty>(Mode->SelectionBoard->GetClass(), TEXT("LayoutScale"))->SetPropertyValue_InContainer(Mode->SelectionBoard.Get(), 10.0);
	auto* Runtime = Scene.World->GetSubsystem<UMWTrackRuntimeSubsystem>();
	Runtime->OnWorldBeginPlay(*Scene.World); // 트랙 서비스만 시작한다. 다른 서비스의 테스트용 수명은 만들지 않는다.
	Scene.World->SetBegunPlay(true);
	auto* Construction = State->GetConstructionComponent();
	const auto Advance = [&Scene](double Seconds)
	{
		for (int32 Index = 0; Index < FMath::CeilToInt(Seconds / .1); ++Index)
		{
			++GFrameCounter;
			Scene.World->Tick(LEVELTICK_All, .1f);
		}
	};
	const auto PlaceRound = [&]() -> bool
	{
		const auto Selection = Construction->GetSelectionSession();
		if (!TestEqual(TEXT("The shared catalog offers six slots"), Selection.Offers.Num(), MWTrackSelection::OfferCount)) return false;
		TSet<FSoftObjectPath> OfferedClasses;
		for (const auto& Offer : Selection.Offers) OfferedClasses.Add(Offer.TrackClass.ToSoftObjectPath());
		TestEqual(TEXT("The six offered track classes are distinct"), OfferedClasses.Num(), MWTrackSelection::OfferCount);
		for (int32 Index = 0; Index < Controllers.Num(); ++Index)
		{
			if (!TestTrue(TEXT("Current selection is accepted"), Construction->ConfirmSelection(Controllers[Index], Selection.SelectionSessionId,
				Selection.OffersRevision, Index + 1, State->GetProgress().RoundEpoch * 10 + Index).bAccepted)) return false;
			if (Index == 0)
			{
				const auto Claimed = Construction->GetSelectionSession();
				TestEqual(TEXT("Claimed ownership is published without changing the offer revision"), Claimed.OffersRevision, Selection.OffersRevision);
				TestEqual(TEXT("The claimed slot remains available as placement data"), Claimed.Offers[0].ClaimedByMemberId, Players[0]->GetLobbyMemberId());
				const auto Duplicate = Construction->ConfirmSelection(Controllers[1], Selection.SelectionSessionId, Selection.OffersRevision,
					1, State->GetProgress().RoundEpoch * 10 + 9);
				TestTrue(TEXT("Another player cannot claim the same slot"), !Duplicate.bAccepted && Duplicate.Reason == EMWTrackSelectionRejectReason::OfferTaken);
				TestFalse(TEXT("Rejected duplicate selection leaves the other player free to choose"), Players[1]->GetPlayerConstructionComponent()->GetSelection().bConfirmed);
			}
		}
		if (!TestTrue(TEXT("All selections enter TrackPlacement"), State->GetRoomPhase() == EMWRoomPhase::TrackPlacement)) return false;
		for (int32 Index = 0; Index < Controllers.Num(); ++Index)
		{
			const auto TrackClass = Players[Index]->GetPlayerConstructionComponent()->GetPlacement().TrackClass;
			UClass* Class = TrackClass.LoadSynchronous();
			if (!TestNotNull(TEXT("Selected track class exists"), Class)) return false;
			AActor* Probe = Scene.World->SpawnActor<AActor>(Class, FVector(-100000, 0, 0), FRotator::ZeroRotator);
			FMWTrackPortFrames Ports;
			if (!TestTrue(TEXT("Selected track has both ports"), Probe && MWTrackSnap::ReadPorts(*Probe, Ports))) return false;
			Probe->Destroy();
			const auto Session = Construction->GetPlacementSession();
			const auto End = Session.OpenEnds[0];
			FMWTrackPlacementIntent Intent;
			Intent.Target = End.Id;
			Intent.SourcePort = 0;
			Intent.bSnapRequested = true;
			Intent.CursorLocation = MWTrackSnap::AlignPorts(Ports[0], End.WorldFrame).TransformPosition((Ports[0].GetLocation() + Ports[1].GetLocation()) * .5);
			if (!TestTrue(TEXT("Server accepts a real socket placement"), Construction->PlaceTrack(Controllers[Index], Session.SessionId,
				Session.CourseRevision, Intent) == EMWTrackPlacementResult::Accepted)) return false;
		}
		return TestTrue(TEXT("Completed placement enters PreparingRace"), State->GetRoomPhase() == EMWRoomPhase::PreparingRace);
	};
	const auto StartRace = [&]() -> bool
	{
		const auto Progress = State->GetProgress();
#if WITH_EDITOR
		FAssetCompilingManager::Get().FinishAllCompilation();
#endif
		// 실제 클라이언트도 매 틱 코스·물리·차량 준비를 확인한다. 지연 연결 콜백을 먼저 진행한다.
		for (int32 Retry = 0; Retry < 20; ++Retry)
		{
			const bool bVehiclesReady = Algo::AllOf(Vehicles, [&Progress](const AMWVehiclePawn* Vehicle)
				{ return !Vehicle->GetPlayerState<AMWPlayerState>()->GetRoundState().bRaceParticipant || Vehicle->IsReadyForRace(Progress.RoundEpoch); });
			if (bVehiclesReady && Mode->IsRaceCourseCurrent()) break;
			Advance(.1);
		}
		if (!Mode->IsRaceCourseCurrent()) for (const auto& Piece : Construction->GetPlacementSession().PlacedTracks)
		{
			AddInfo(FString::Printf(TEXT("CoursePiece %s Begun=%d Transform=%d Collision=%d Required=%d"), *GetNameSafe(Piece.Actor.Get()),
				Piece.Actor->HasActorBegunPlay(), Piece.Actor->GetActorTransform().Equals(Piece.WorldTransform), Piece.Actor->GetActorEnableCollision(), Piece.bRequiresCollision));
			TInlineComponentArray<UPrimitiveComponent*> Parts(Piece.Actor.Get());
			for (const auto* Part : Parts) if (Part->IsQueryCollisionEnabled() && !Part->IsPhysicsStateCreated())
			{
				const auto* ISM = Cast<UInstancedStaticMeshComponent>(Part);
				AddInfo(FString::Printf(TEXT("  %s Class=%s Registered=%d Should=%d Async=%d Instances=%d Scene=%d"), *Part->GetName(), *Part->GetClass()->GetName(), Part->IsRegistered(),
					Part->ShouldCreatePhysicsState(), Part->IsAsyncCreatePhysicsStateRunning(), ISM ? ISM->GetInstanceCount() : -1, Scene.World->GetPhysicsScene() != nullptr));
			}
		}
		TestTrue(TEXT("Server course is ready before readiness reports"), Mode->IsRaceCourseCurrent());
		// 이미 전송된 보고가 서버의 일시적인 코스 미준비로 거절된 뒤 같은 키로 다시 도착하는 경우.
		AActor* CourseActor = Construction->GetPlacementSession().PlacedTracks[0].Actor;
		CourseActor->SetActorEnableCollision(false);
		Mode->ReportRaceReady(Controllers[0], Progress.RoundEpoch, Progress.CourseSessionId, Progress.CourseRevision);
		TestTrue(TEXT("A temporarily unready course does not accept a readiness report"), Mode->RaceReadyMembers.IsEmpty());
		CourseActor->SetActorEnableCollision(true);
		Mode->ReportRaceReady(Controllers[0], Progress.RoundEpoch - 1, Progress.CourseSessionId, Progress.CourseRevision);
		TestTrue(TEXT("An old epoch cannot contribute readiness"), Mode->RaceReadyMembers.IsEmpty());
		// 마지막 보고 때 앞서 보고한 차량만 미준비로 바꿔 중복 보고의 전원 재평가도 검증한다.
		const int32 LastRacer = Players[2]->GetRoundState().bRaceParticipant ? 2 : 1;
		for (int32 Index = 0; Index < Players.Num(); ++Index)
			if (Players[Index]->GetRoundState().bRaceParticipant)
			{
				TestTrue(FString::Printf(TEXT("Vehicle %d is ready before report"), Index), Vehicles[Index]->IsReadyForRace(Progress.RoundEpoch));
				if (Index == LastRacer)
					Vehicles[0]->GetMovementComponent()->SetComponentTickEnabled(false);
				Mode->ReportRaceReady(Controllers[Index], Progress.RoundEpoch, Progress.CourseSessionId, Progress.CourseRevision);
			}
		Vehicles[0]->GetMovementComponent()->SetComponentTickEnabled(true);
		TestTrue(TEXT("Readiness waits while an earlier reporter is temporarily unready"), State->GetRoomPhase() == EMWRoomPhase::PreparingRace);
		const int32 ReadyCount = Mode->RaceReadyMembers.Num();
		Mode->ReportRaceReady(Controllers[LastRacer], Progress.RoundEpoch, Progress.CourseSessionId, Progress.CourseRevision);
		TestEqual(TEXT("A duplicate report cannot add a second readiness entry"), Mode->RaceReadyMembers.Num(), ReadyCount);
		if (!TestTrue(TEXT("Only participating players are needed for Countdown"), State->GetRoomPhase() == EMWRoomPhase::Countdown)) return false;
		Advance(3.2);
		return TestTrue(TEXT("Server countdown deadline starts Racing"), State->GetRoomPhase() == EMWRoomPhase::Racing);
	};
	const auto PassCheckpoints = [&](int32 Index)
	{
		for (const auto& Track : Construction->GetPlacementSession().PlacedTracks)
			if (Track.bCheckpoint) Runtime->ProcessRaceTrigger(*Vehicles[Index], *Track.Actor, EMWTrackTrigger::Checkpoint, {});
	};
	const auto FinishAll = [&]() -> bool
	{
		for (int32 Index = 0; Index < 3; ++Index)
			if (!TestTrue(TEXT("Current finish is accepted"), Mode->TryFinishPlayerRace(*Vehicles[Index], *Construction->GetPlacementSession().PlacedTracks.Last().Actor, State->GetProgress().RoundEpoch))) return false;
		return TestTrue(TEXT("All racers enter RoundResults once"), State->GetRoomPhase() == EMWRoomPhase::RoundResults);
	};
	Mode->EnterPhase(EMWRoomPhase::PreparingConstruction);
	if (!PlaceRound() || !StartRace() || !FinishAll()) return false;
	TestEqual(TEXT("First place receives 300"), Players[0]->GetRoundState().FinishScore, 300);
	TestEqual(TEXT("Second place receives 150"), Players[1]->GetRoundState().FinishScore, 150);
	TestEqual(TEXT("Third place receives 50"), Players[2]->GetRoundState().FinishScore, 50);
	const auto FirstResults = State->GetProgress().Results;
	AActor* FirstRoad = Construction->GetPlacementSession().PlacedTracks[0].Actor;
	TWeakObjectPtr<AActor> OldCap = Construction->GetPlacementSession().PlacedTracks.Last().Actor;
	const int32 FirstPieceCount = Construction->GetPlacementSession().PlacedTracks.Num();
	Advance(3.2);
	TestEqual(TEXT("Next normal round issues exactly one new epoch"), State->GetProgress().RoundEpoch, 2);
	TestTrue(TEXT("Starting a new round preserves the last results"), State->GetProgress().Results == FirstResults);
	TestEqual(TEXT("New round preserves accumulated score"), Players[0]->GetRoundState().TotalScore, 300);
	if (!PlaceRound() || !StartRace()) return false;
	TestTrue(TEXT("Start and previous road actors are preserved"), Construction->GetPlacementSession().PlacedTracks[0].Actor == FirstRoad);
	TestTrue(TEXT("Previous end cap is removed"), !OldCap.IsValid() || OldCap->IsActorBeingDestroyed());
	TestEqual(TEXT("Second round appends three road pieces"), Construction->GetPlacementSession().PlacedTracks.Num(), FirstPieceCount + 3);
	TestTrue(TEXT("Old finish becomes a checkpoint"), Algo::CountIf(Construction->GetPlacementSession().PlacedTracks, [](const FMWPlacedTrack& Track) { return Track.bCheckpoint; }) >= 1);
	const double FallZ = Construction->GetPlacementSession().CourseBounds.Min.Z - Mode->FallRecoveryDepth - 100.0;
	Vehicles[0]->SetActorLocation(FVector(0, 0, FallZ));
	Mode->CheckPlayerRaceFailure(*Vehicles[0]);
	TestTrue(TEXT("Server fall detection restores the vehicle before any checkpoint"), Vehicles[0]->GetActorLocation().Z > FallZ + 500);
	TestFalse(TEXT("A recovered fall invalidates only the current segment"), Players[0]->GetRoundState().bCheckpointEligible);
	for (int32 Index = 0; Index < 3; ++Index) PassCheckpoints(Index);
	TestEqual(TEXT("Failed segment awards no checkpoint points"), Players[0]->GetRoundState().CheckpointScore, 0);
	TestEqual(TEXT("Clean segment awards 100 checkpoint points"), Players[1]->GetRoundState().CheckpointScore, 100);
	PassCheckpoints(1);
	TestEqual(TEXT("Checkpoint cannot award twice"), Players[1]->GetRoundState().CheckpointScore, 100);
	if (!FinishAll()) return false;
	TestEqual(TEXT("Round and cumulative scoring remain separate"), Players[1]->GetRoundState().TotalScore, 400);
	Advance(3.2);
	if (!PlaceRound() || !StartRace()) return false;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		auto Round = Players[Index]->GetRoundState();
		Round.TotalScore = 3900;
		Players[Index]->ApplyRoundStateFromServer(Round);
	}
	for (int32 Index = 0; Index < 3; ++Index) PassCheckpoints(Index);
	if (!FinishAll()) return false;
	const int32 BeforeSuddenDeath = Construction->GetPlacementSession().PlacedTracks.Num();
	const int32 WinnerScore = Players[0]->GetRoundState().TotalScore;
	Advance(3.2);
	TestTrue(TEXT("Multiple goal scorers enter SuddenDeath"), State->GetProgress().RaceMode == EMWRaceMode::SuddenDeath);
	TestTrue(TEXT("Non-qualifier becomes a spectator"), !Players[2]->GetRoundState().bRaceParticipant && !Vehicles[2]->CanDrive());
	TestEqual(TEXT("SuddenDeath adds no new track"), Construction->GetPlacementSession().PlacedTracks.Num(), BeforeSuddenDeath);
	if (!StartRace()) return false;
	const FTransform BeforeFailure = Vehicles[1]->GetActorTransform();
	TestTrue(TEXT("SuddenDeath failure is consumed without a respawn"), Runtime->RespawnAtTrackCheckpoint(Vehicles[1]));
	TestTrue(TEXT("SuddenDeath failure stays eliminated"), Players[1]->GetRoundState().RaceStatus == EMWRaceStatus::Eliminated);
	TestTrue(TEXT("Eliminated vehicle is not teleported"), BeforeFailure.Equals(Vehicles[1]->GetActorTransform()));
	PassCheckpoints(0);
	TestTrue(TEXT("Survivor can finish"), Mode->TryFinishPlayerRace(*Vehicles[0], *Construction->GetPlacementSession().PlacedTracks.Last().Actor, State->GetProgress().RoundEpoch));
	Advance(3.2);
	TestTrue(TEXT("SuddenDeath ends in MatchResults"), State->GetRoomPhase() == EMWRoomPhase::MatchResults);
	TestEqual(TEXT("SuddenDeath does not add score"), Players[0]->GetRoundState().TotalScore, WinnerScore);
	TestEqual(TEXT("Finisher ranks first"), Players[0]->GetRoundState().FinalRank, 1);
	TestEqual(TEXT("Eliminated contender ranks before spectators"), Players[1]->GetRoundState().FinalRank, 2);
	TestEqual(TEXT("Non-qualifier receives the remaining final rank"), Players[2]->GetRoundState().FinalRank, 3);

	// 같은 기존 객체로 종료 규칙의 두 경계를 검증한다. Runtime에 재경기 API는 추가하지 않는다.
	const auto ResetRuleCase = [&](EMWRaceMode RaceMode, int32 FirstScore, int32 SecondScore)
	{
		Scene.World->GetTimerManager().ClearTimer(Mode->RacePhaseTimer);
		Mode->MatchMembers = {1, 2, 3};
		for (AMWPlayerState* Player : Players) if (!State->PlayerArray.Contains(Player)) State->AddPlayerState(Player);
		auto Progress = State->GetProgress();
		Progress.Phase = EMWRoomPhase::PreparingRace;
		Progress.RaceMode = RaceMode;
		++Progress.RoundEpoch;
		State->ApplyProgressFromServer(Progress);
		Mode->NextFinishOrder = Mode->NextEliminationOrder = 1;
		const int32 Checkpoints = Algo::CountIf(Construction->GetPlacementSession().PlacedTracks, [](const FMWPlacedTrack& Track) { return Track.bCheckpoint; });
		for (int32 Index = 0; Index < 3; ++Index)
		{
			FMWPlayerRoundState Round;
			Round.RoundEpoch = Progress.RoundEpoch;
			Round.MemberId = Index + 1;
			Round.TotalScore = Index == 0 ? FirstScore : Index == 1 ? SecondScore : 500;
			Round.bRaceParticipant = RaceMode == EMWRaceMode::Normal || Index < 2;
			Round.RaceStatus = Round.bRaceParticipant ? EMWRaceStatus::Waiting : EMWRaceStatus::Spectating;
			Round.CheckpointsPassed = Checkpoints;
			Players[Index]->ApplyRoundStateFromServer(Round);
			if (Round.bRaceParticipant) Vehicles[Index]->PrepareForRaceFromServer();
		}
		Progress.Phase = EMWRoomPhase::Racing;
		State->ApplyProgressFromServer(Progress);
		for (int32 Index = 0; Index < 3; ++Index)
		{
			auto Round = Players[Index]->GetRoundState();
			if (!Round.bRaceParticipant) continue;
			Round.RaceStatus = EMWRaceStatus::Active;
			Players[Index]->ApplyRoundStateFromServer(Round);
			Runtime->BeginPlayerRace(*Vehicles[Index], Progress.RoundEpoch);
		}
	};
	ResetRuleCase(EMWRaceMode::Normal, 3700, 1000);
	if (!FinishAll()) return false;
	Advance(3.2);
	TestTrue(TEXT("A single player reaching exactly 4000 ends the match"), State->GetRoomPhase() == EMWRoomPhase::MatchResults && Players[0]->GetRoundState().TotalScore == 4000);
	ResetRuleCase(EMWRaceMode::SuddenDeath, 4000, 4000);
	Runtime->RespawnAtTrackCheckpoint(Vehicles[0]);
	Runtime->RespawnAtTrackCheckpoint(Vehicles[1]);
	Advance(3.2);
	TestEqual(TEXT("If everyone falls, the last elimination ranks first"), Players[1]->GetRoundState().FinalRank, 1);
	TestEqual(TEXT("Earlier elimination ranks behind it"), Players[0]->GetRoundState().FinalRank, 2);

	ResetRuleCase(EMWRaceMode::Normal, 3700, 1000);
	if (!FinishAll()) return false;
	const auto SoleWinnerResults = State->GetProgress().Results;
	Mode->Logout(Controllers[0]);
	State->RemovePlayerState(Players[0]);
	TestTrue(TEXT("Leaving after completion preserves the published round snapshot"), State->GetProgress().Results == SoleWinnerResults);
	Advance(3.2);
	TestTrue(TEXT("A completed sole qualifier leaving cannot start another normal round"), State->GetRoomPhase() == EMWRoomPhase::MatchResults);
	const auto* SoleWinner = State->GetProgressRef().Results.FindByPredicate([](const FMWPlayerRoundState& Entry) { return Entry.MemberId == 1; });
	if (!TestNotNull(TEXT("Final results retain the disconnected winner without a PlayerState"), SoleWinner)) return false;
	TestEqual(TEXT("The completed winner keeps first place"), SoleWinner->FinalRank, 1);
	TestEqual(TEXT("The completed winner keeps exactly 4000 points"), SoleWinner->TotalScore, 4000);

	ResetRuleCase(EMWRaceMode::SuddenDeath, 4000, 4000);
	Mode->HandlePlayerRaceFailure(*Vehicles[1], State->GetProgress().RoundEpoch);
	if (!TestTrue(TEXT("SuddenDeath winner finishes before disconnecting"), Mode->TryFinishPlayerRace(*Vehicles[0],
		*Construction->GetPlacementSession().PlacedTracks.Last().Actor, State->GetProgress().RoundEpoch))) return false;
	Mode->Logout(Controllers[0]);
	State->RemovePlayerState(Players[0]);
	Advance(3.2);
	const auto* SuddenDeathWinner = State->GetProgressRef().Results.FindByPredicate([](const FMWPlayerRoundState& Entry) { return Entry.MemberId == 1; });
	if (!TestNotNull(TEXT("SuddenDeath final results retain the disconnected finisher"), SuddenDeathWinner)) return false;
	TestEqual(TEXT("Disconnect cannot promote an eliminated player over the finisher"), SuddenDeathWinner->FinalRank, 1);
	TestEqual(TEXT("The eliminated contender remains second"), Players[1]->GetRoundState().FinalRank, 2);

	ResetRuleCase(EMWRaceMode::Normal, 3700, 3850);
	if (!FinishAll()) return false;
	Mode->Logout(Controllers[0]);
	State->RemovePlayerState(Players[0]);
	Advance(3.2);
	TestTrue(TEXT("Two frozen qualifiers keep the SuddenDeath route after one leaves"), State->GetRoomPhase() == EMWRoomPhase::PreparingRace
		&& State->GetProgress().RaceMode == EMWRaceMode::SuddenDeath);
	TestTrue(TEXT("Only the connected qualifier participates in the next race"), Players[1]->GetRoundState().bRaceParticipant
		&& !Players[2]->GetRoundState().bRaceParticipant && !Mode->IsMatchMember(1));

	ResetRuleCase(EMWRaceMode::Normal, 3700, 3850);
	if (!FinishAll()) return false;
	Mode->Logout(Controllers[0]);
	Mode->Logout(Controllers[1]);
	AddExpectedError(TEXT("서든데스에 참가할 목표점수 도달자가 남아 있지 않습니다."), EAutomationExpectedErrorFlags::Contains, 1);
	Advance(3.2);
	TestTrue(TEXT("No connected qualifier exits through the existing failure path"), State->GetRoomPhase() == EMWRoomPhase::Failed);

	ResetRuleCase(EMWRaceMode::SuddenDeath, 4000, 4000);
	Mode->Logout(Controllers[0]);
	TestTrue(TEXT("One remaining contender can continue while a spectator is connected"), State->GetRoomPhase() == EMWRoomPhase::Racing);
	AddExpectedError(TEXT("주행 참가자가 없습니다."), EAutomationExpectedErrorFlags::Contains, 1);
	Mode->Logout(Controllers[1]);
	TestTrue(TEXT("Losing every contender cannot leave spectators in an endless race"), State->GetRoomPhase() == EMWRoomPhase::Failed);
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
		Sent.bFinishCollisionDisabled = !bAllowed;
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
		Received.bFinishCollisionDisabled = bAllowed;
		FMemoryReader Reader(Bytes);
		bool bRead = false;
		Received.NetSerialize(Reader, nullptr, bRead);
		TestTrue(TEXT("Full drive snapshot serializes successfully"), bWrite && bRead && Reader.Tell() == Reader.TotalSize());
		TestEqual(TEXT("Permission is transmitted"), Received.bDriveAllowed, bAllowed);
		TestEqual(TEXT("Finish collision state shares the authoritative pose"), Received.bFinishCollisionDisabled, Sent.bFinishCollisionDisabled);
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
