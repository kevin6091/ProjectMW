#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Components/BoxComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedPlayerInput.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "MWGameplayTags.h"
#include "Data/MWInputData.h"
#include "Data/MWVehicleDriveData.h"
#include "Gameplay/Construction/MWTrackConstructionComponent.h"
#include "Gameplay/PlayerStart/MWPlayerStartManager.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "Gameplay/Vehicle/MWVehicleMovementComponent.h"
#include "Player/MWGameplayPlayerController.h"
#include "System/GameMode/MWGameplayGameMode.h"
#include "System/GameState/MWGameplayGameState.h"
#include "System/MWGameInstance.h"
#include "System/MWPlayerState.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

// Runtime용 테스트 GameMode/설정 setter를 추가하지 않고 실제 GameMode를 임시 월드에서 실행한다.
struct FMWRaceStartTestScene
{
	UWorld* World = nullptr;
	UMWGameInstance* App = nullptr;
	AMWGameplayGameMode* Mode = nullptr;
	AMWGameplayGameState* State = nullptr;
	TArray<AMWGameplayPlayerController*> Players;
	ULocalPlayer* LocalPlayer = nullptr;
	uint64 TestFrame = GFrameCounter;

	FMWRaceStartTestScene()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		auto& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
		Context.SetCurrentWorld(World);
		App = NewObject<UMWGameInstance>(GEngine);
		Context.OwningGameInstance = App;
		World->SetGameInstance(App);
		FURL URL;
		URL.AddOption(TEXT("game=/Script/MW.MWGameplayGameMode"));
		check(World->SetGameMode(URL));
		World->InitializeActorsForPlay(URL);
		Mode = World->GetAuthGameMode<AMWGameplayGameMode>();
		State = World->GetGameState<AMWGameplayGameState>();
		check(Mode && State);
		Mode->StartManager = World->SpawnActor<AMWPlayerStartManager>();
		// 연결 모듈/로비 성공을 꾸미지 않고 이 테스트의 고정 시작 명단만 구성한다.
		const_cast<TArray<int32>&>(App->GetStartingPlayers()) = {1, 2, 3};
		for (int32 Index = 0; Index < 3; ++Index)
		{
			auto* PC = World->SpawnActor<AMWGameplayPlayerController>();
			auto* PS = PC->GetPlayerState<AMWPlayerState>();
			if (!PS)
			{
				PS = World->SpawnActor<AMWPlayerState>();
				PS->SetOwner(PC);
				PC->PlayerState = PS;
			}
			PS->SetLobbyMemberIdFromServer(Index + 1);
			PS->ConfirmVehicleAssetIDFromServer(MWGameplayTags::Asset_Vehicle_FRED);
			State->AddPlayerState(PS);
			FActorSpawnParameters Spawn;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* Pawn = World->SpawnActor<AMWVehiclePawn>(FVector(Index * 1000, 0, 31), FRotator::ZeroRotator, Spawn);
			auto* Movement = CastChecked<UMWVehicleMovementComponent>(Pawn->GetMovementComponent());
			FindFProperty<FObjectPropertyBase>(Movement->GetClass(), TEXT("DriveData"))->SetObjectPropertyValue_InContainer(
				Movement, NewObject<UMWVehicleDriveData>(Pawn));
			PC->Possess(Pawn);
			Players.Add(PC);
		}
		World->BeginPlay();
	}

	~FMWRaceStartTestScene()
	{
		if (LocalPlayer)
		{
			Players[0]->ClearDrivingInput();
			LocalPlayer->PlayerRemoved();
			LocalPlayer->PlayerController = nullptr;
			Players[0]->Player = nullptr;
		}
		World->EndPlay(EEndPlayReason::Quit);
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	}

	AMWVehiclePawn* Vehicle(int32 Index) const { return CastChecked<AMWVehiclePawn>(Players[Index]->GetPawn()); }
	int32 ReadyCount() const { return Mode->RaceReadyMembers.Num(); }
	void Enter(EMWRoomPhase Phase) { Mode->EnterPhase(Phase); }
	void Report(int32 Index)
	{
		const auto Progress = State->GetProgress();
		Mode->ReportRaceReady(Players[Index], Progress.RoundEpoch, Progress.CourseSessionId, Progress.CourseRevision);
	}
	void Advance(float Seconds)
	{
		const uint64 PreviousFrame = GFrameCounter;
		ON_SCOPE_EXIT { GFrameCounter = PreviousFrame; };
		const int32 Steps = FMath::CeilToInt(Seconds / 0.05f);
		const float Delta = Seconds / Steps;
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			GFrameCounter = ++TestFrame;
			World->TimeSeconds += Delta;
			World->GetTimerManager().Tick(Delta);
			for (int32 Index = 0; Index < Players.Num(); ++Index)
			{
				Vehicle(Index)->GetMovementComponent()->TickComponent(Delta, LEVELTICK_All, nullptr);
			}
		}
	}
	FMWTrackPlacementSession& Course()
	{
		auto* Construction = State->GetConstructionComponent();
		return *FindFProperty<FStructProperty>(Construction->GetClass(), TEXT("PlacementSession"))
			->ContainerPtrToValuePtr<FMWTrackPlacementSession>(Construction);
	}
	UBoxComponent* BuildCourse()
	{
		auto* Road = World->SpawnActor<AActor>();
		auto* Box = NewObject<UBoxComponent>(Road);
		Road->SetRootComponent(Box);
		Box->SetBoxExtent(FVector(100, 100, 10));
		Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Box->RegisterComponent();
		FMWTrackPlacementSession& Session = Course();
		Session.SessionId = 9;
		Session.CourseRevision = 4;
		Session.PlacedTracks.Add({1, {}, Road, Road->GetActorTransform()});
		FMWGameplayProgress Progress;
		Progress.Phase = EMWRoomPhase::TrackPlacement;
		Progress.RoundEpoch = 1;
		Progress.RoundIndex = 1;
		State->ApplyProgressFromServer(Progress);
		for (auto* PC : Players)
		{
			FMWPlayerRoundState Round;
			Round.RoundEpoch = 1;
			PC->GetPlayerState<AMWPlayerState>()->ApplyRoundStateFromServer(Round);
		}
		return Box;
	}
	void FinishCourse()
	{
		State->GetConstructionComponent()->OnPlacementFinished.Broadcast(Course().SessionId, Course().CourseRevision);
	}
	void ConfigureLocalInput()
	{
		auto* PC = Players[0];
		auto* Data = NewObject<UMWInputData>(PC);
		auto* Mapping = NewObject<UInputMappingContext>(Data);
		FindFProperty<FObjectPropertyBase>(Data->GetClass(), TEXT("InputMappingContext"))->SetObjectPropertyValue_InContainer(Data, Mapping);
		auto& Actions = *FindFProperty<FArrayProperty>(Data->GetClass(), TEXT("InputActions"))
			->ContainerPtrToValuePtr<TArray<FMWInputAction>>(Data);
		for (const FGameplayTag Tag : {MWGameplayTags::Input_Accel.GetTag(), MWGameplayTags::Input_Reverse.GetTag(),
			MWGameplayTags::Input_Steer.GetTag(), MWGameplayTags::Input_Drift.GetTag()})
		{
			auto* Action = NewObject<UInputAction>(Data);
			Action->ValueType = Tag == MWGameplayTags::Input_Drift ? EInputActionValueType::Boolean : EInputActionValueType::Axis1D;
			Actions.Add({Tag, Action});
		}
		FindFProperty<FObjectPropertyBase>(Vehicle(0)->GetClass(), TEXT("DrivingInputData"))->SetObjectPropertyValue_InContainer(Vehicle(0), Data);
		LocalPlayer = NewObject<ULocalPlayer>(GEngine);
		LocalPlayer->PlayerAdded(nullptr, 0);
		PC->PlayerInput = NewObject<UEnhancedPlayerInput>(PC);
		PC->Player = LocalPlayer;
		LocalPlayer->PlayerController = PC;
		PC->SetAsLocalPlayerController();
		PC->InputComponent = NewObject<UEnhancedInputComponent>(PC);
		PC->InputComponent->RegisterComponent();
		PC->RefreshDrivingInput();
		PC->ApplyGameplayInputMode();
	}
	void LocalReport() { Players[0]->TryReportRaceReady(); }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMWRaceStartFlowTest, "MW.Gameplay.RaceStart.AllReadyAndCountdown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMWRaceStartFlowTest::RunTest(const FString& Parameters)
{
	FMWRaceStartTestScene Scene;
	Scene.Enter(EMWRoomPhase::PreparingRace);
	TestEqual(TEXT("Spawning does not immediately start Racing"), Scene.State->GetRoomPhase(), EMWRoomPhase::PreparingRace);
	const auto Target = Scene.State->GetProgress();
	TestEqual(TEXT("Initial epoch is issued"), Target.RoundEpoch, 1);
	TestEqual(TEXT("Fixed course explicitly uses zero session"), Target.CourseSessionId, 0);
	TestTrue(TEXT("Preparation has a deadline"), Target.PhaseEndServerTime > Scene.World->GetTimeSeconds());
	Scene.Mode->ReportRaceReady(Scene.Players[0], 2, 0, 0);
	Scene.Mode->ReportRaceReady(Scene.Players[0], 1, 1, 0);
	TestEqual(TEXT("Wrong epoch and course are rejected"), Scene.ReadyCount(), 0);
	Scene.Report(0);
	Scene.Report(0);
	Scene.Report(1);
	TestEqual(TEXT("Duplicate readiness is counted once"), Scene.ReadyCount(), 2);
	TestEqual(TEXT("Two out of three cannot start Countdown"), Scene.State->GetRoomPhase(), EMWRoomPhase::PreparingRace);
	Scene.Report(2);
	const double CountdownEnd = Scene.State->GetProgress().PhaseEndServerTime;
	TestEqual(TEXT("The last report starts Countdown"), Scene.State->GetRoomPhase(), EMWRoomPhase::Countdown);
	TestTrue(TEXT("Default countdown is exactly three seconds"), FMath::IsNearlyEqual(CountdownEnd - Scene.World->GetTimeSeconds(), 3.0));
	Scene.Report(2);
	Scene.Enter(EMWRoomPhase::Countdown);
	Scene.Enter(EMWRoomPhase::Racing);
	TestEqual(TEXT("Duplicate and early transitions preserve the deadline"), Scene.State->GetProgress().PhaseEndServerTime, CountdownEnd);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		TestFalse(TEXT("Countdown keeps every vehicle locked"), Scene.Vehicle(Index)->CanDrive());
	}
	Scene.Advance(2.9f);
	TestEqual(TEXT("Racing cannot start early"), Scene.State->GetRoomPhase(), EMWRoomPhase::Countdown);
	Scene.Advance(0.2f);
	TestEqual(TEXT("The server timer starts Racing"), Scene.State->GetRoomPhase(), EMWRoomPhase::Racing);
	TestEqual(TEXT("Racing clears the countdown deadline"), Scene.State->GetProgress().PhaseEndServerTime, 0.0);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		TestTrue(TEXT("Every current participant becomes Active and can drive"), Scene.Vehicle(Index)->CanDrive());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMWRaceStartCourseTest, "MW.Gameplay.RaceStart.CourseAndLocalReadiness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMWRaceStartCourseTest::RunTest(const FString& Parameters)
{
	FMWRaceStartTestScene Scene;
	auto* Collision = Scene.BuildCourse();
	Scene.FinishCourse();
	TestEqual(TEXT("Placement completion enters PreparingRace before any client ACK"), Scene.State->GetRoomPhase(), EMWRoomPhase::PreparingRace);
	TestEqual(TEXT("Target session is frozen in Progress"), Scene.State->GetProgress().CourseSessionId, 9);
	auto* Construction = Scene.State->GetConstructionComponent();
	TestTrue(TEXT("Published road and registered collision are ready"), Construction->IsCourseReadyForRace(9, 4));
	Collision->GetOwner()->AddActorWorldOffset(FVector(10, 0, 0));
	TestFalse(TEXT("The wrong transform is not ready"), Construction->IsCourseReadyForRace(9, 4));
	Collision->GetOwner()->SetActorTransform(Scene.Course().PlacedTracks[0].WorldTransform);
	Collision->UnregisterComponent();
	TestFalse(TEXT("Actor presence alone does not prove collision readiness"), Construction->IsCourseReadyForRace(9, 4));
	Collision->RegisterComponent();
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TestFalse(TEXT("Missing required query collision blocks readiness"), Construction->IsCourseReadyForRace(9, 4));
	Scene.Course().PlacedTracks[0].bRequiresCollision = false;
	TestTrue(TEXT("An explicitly decorative piece does not require collision"), Construction->IsCourseReadyForRace(9, 4));
	Scene.Course().PlacedTracks[0].bRequiresCollision = true;
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Scene.ConfigureLocalInput();
	TestTrue(TEXT("Actual local input, camera, Pawn, and course can report readiness while held"), Scene.Players[0]->IsLocalRaceReady());
	auto* LoadingLevel = NewObject<ULevelStreamingDynamic>(Scene.World);
	LoadingLevel->SetShouldBeLoaded(true);
	LoadingLevel->SetShouldBeVisible(true);
	Scene.World->AddStreamingLevel(LoadingLevel);
	TestFalse(TEXT("A requested visible level must be loaded before readiness"), Scene.Players[0]->IsLocalRaceReady());
	Scene.World->RemoveStreamingLevel(LoadingLevel);
	Scene.LocalReport();
	Scene.LocalReport();
	TestEqual(TEXT("The host uses the same RPC entry and reports this key once"), Scene.ReadyCount(), 1);
	Scene.Players[0]->SetViewTarget(Scene.Mode);
	TestFalse(TEXT("The wrong view target blocks readiness"), Scene.Players[0]->IsLocalRaceReady());
	Scene.Players[0]->SetViewTarget(Scene.Vehicle(0));
	Scene.Vehicle(0)->SetRole(ROLE_AutonomousProxy);
	Scene.Vehicle(0)->SetActorLocation(FVector(7000, 0, 31));
	TestFalse(TEXT("Owner waits for the authoritative starting pose"), Scene.Vehicle(0)->IsReadyForRace(1));
	Scene.Vehicle(0)->SetRole(ROLE_Authority);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMWRaceStartFailureTest, "MW.Gameplay.RaceStart.TimeoutDisconnectAndStaleTimer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMWRaceStartFailureTest::RunTest(const FString& Parameters)
{
	{
		FMWRaceStartTestScene Scene;
		Scene.Enter(EMWRoomPhase::PreparingRace);
		Scene.Report(0);
		AddExpectedError(TEXT("제한 시간 안에 전원의 출발 준비"), EAutomationExpectedErrorFlags::Contains, 1);
		Scene.Advance(30.2f);
		TestEqual(TEXT("Missing reports time out to Failed"), Scene.State->GetRoomPhase(), EMWRoomPhase::Failed);
	}
	{
		FMWRaceStartTestScene Scene;
		Scene.Enter(EMWRoomPhase::PreparingRace);
		for (int32 Index = 0; Index < 3; ++Index) Scene.Report(Index);
		AddExpectedError(TEXT("건설 또는 출발 준비 중 시작 참가자가 나갔습니다"), EAutomationExpectedErrorFlags::Contains, 1);
		Scene.Mode->Logout(Scene.Players[2]);
		TestEqual(TEXT("Departure during Countdown fails immediately"), Scene.State->GetRoomPhase(), EMWRoomPhase::Failed);
		// Logout으로 Pawn 소유가 해제됐으므로 타이머만 진행한다.
		Scene.Players.RemoveAt(2);
		Scene.Advance(3.2f);
		TestEqual(TEXT("The cancelled countdown cannot later start Racing"), Scene.State->GetRoomPhase(), EMWRoomPhase::Failed);
	}
	{
		FMWRaceStartTestScene Scene;
		Scene.BuildCourse();
		Scene.FinishCourse();
		for (int32 Index = 0; Index < 3; ++Index) Scene.Report(Index);
		++Scene.Course().CourseRevision;
		AddExpectedError(TEXT("출발 전 참가자 또는 코스·차량 준비 상태가 변경"), EAutomationExpectedErrorFlags::Contains, 1);
		Scene.Advance(3.2f);
		TestEqual(TEXT("A changed course cannot start Racing after Countdown"), Scene.State->GetRoomPhase(), EMWRoomPhase::Failed);
	}
	{
		FMWRaceStartTestScene Scene;
		Scene.Enter(EMWRoomPhase::PreparingRace);
		auto NewProgress = Scene.State->GetProgress();
		NewProgress.RoundEpoch = 2;
		NewProgress.PhaseEndServerTime += 50.0;
		Scene.State->ApplyProgressFromServer(NewProgress);
		Scene.Advance(30.2f);
		TestTrue(TEXT("An old timer cannot change a newer epoch"), Scene.State->GetProgress() == NewProgress);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMWRaceStartProgressTest, "MW.Gameplay.RaceStart.ProgressSerialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMWRaceStartProgressTest::RunTest(const FString& Parameters)
{
	FMWGameplayProgress Sent;
	Sent.Phase = EMWRoomPhase::Countdown;
	Sent.RoundIndex = 3;
	Sent.RoundEpoch = 7;
	Sent.CourseSessionId = 9;
	Sent.CourseRevision = 14;
	Sent.PhaseEndServerTime = 100.25;
	Sent.RaceMode = EMWRaceMode::SuddenDeath;
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	bool bWrite = false;
	Sent.NetSerialize(Writer, nullptr, bWrite);
	FMWGameplayProgress Received;
	FMemoryReader Reader(Bytes);
	bool bRead = false;
	Received.NetSerialize(Reader, nullptr, bRead);
	TestTrue(TEXT("Phase, target course, epoch, and absolute deadline serialize together"), bWrite && bRead
		&& Sent == Received && Reader.Tell() == Reader.TotalSize());
	++Received.CourseRevision;
	TestFalse(TEXT("Course changes participate in Progress equality"), Sent == Received);
	return true;
}

#endif
