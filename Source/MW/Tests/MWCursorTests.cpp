#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Player/Cursor/MWPlayerCursorComponent.h"
#include "System/MWPlayerState.h"
#include "UI/Cursor/MWCursorProjection.h"
#include "Components/WidgetComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include <limits>

namespace MWCursorTests
{
struct FScene
{
	UWorld* World;
	AMWPlayerState* Player;
	UMWPlayerCursorComponent* Cursor;
	FScene()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		Player = World->SpawnActor<AMWPlayerState>();
		Cursor = Player->GetCursorComponent();
	}
	~FScene()
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMWCursorValidationTest, "MW.Cursor.StreamValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMWCursorValidationTest::RunTest(const FString& Parameters)
{
	MWCursorTests::FScene Scene;
	auto* C = Scene.Cursor;
	TestFalse(TEXT("No samples without a server-opened stream"), C->ReceiveSampleForServer(1, 1, true, FVector::ZeroVector));
	TestFalse(TEXT("Context must be explicit"), C->OpenStreamForServer(NAME_None, EMWCursorSpace::NormalizedWidget));
	TestTrue(TEXT("Open vehicle stream"), C->OpenStreamForServer(MWCursorContexts::VehicleSelection, EMWCursorSpace::NormalizedWidget));
	const int32 First = C->GetCursorState().StreamId;
	const FVector Position(0.25, 0.5, 0.0);
	TestTrue(TEXT("Accept normalized sample"), C->ReceiveSampleForServer(First, 1, true, Position));
	TestTrue(TEXT("State visible"), C->GetCursorState().bVisible);
	TestTrue(TEXT("Duplicate/out-of-order rejected"), !C->ReceiveSampleForServer(First, 1, true, FVector::ZeroVector));
	TestFalse(TEXT("Unknown generation rejected"), C->ReceiveSampleForServer(First + 1, 2, true, Position));
	TestFalse(TEXT("Outside layer rejected"), C->ReceiveSampleForServer(First, 2, true, FVector(1.1, 0, 0)));
	TestFalse(TEXT("Normalized Z must be zero"), C->ReceiveSampleForServer(First, 2, true, FVector(0.5, 0.5, 1)));
	TestFalse(TEXT("NaN rejected"), C->ReceiveSampleForServer(First, 2, true, FVector(std::numeric_limits<double>::quiet_NaN(), 0, 0)));
	TestFalse(TEXT("Infinity rejected"), C->ReceiveSampleForServer(First, 2, true, FVector(std::numeric_limits<double>::infinity(), 0, 0)));
	TestTrue(TEXT("Invalid samples did not consume sequence"), C->ReceiveSampleForServer(First, 2, true, Position));
	TestTrue(TEXT("Hide independent of position"), C->ReceiveSampleForServer(First, 3, false, FVector::ZeroVector));
	TestFalse(TEXT("Hidden after request"), C->GetCursorState().bVisible);
	C->CloseStreamForServer();
	TestFalse(TEXT("Closed stream rejects late packet"), C->ReceiveSampleForServer(First, 4, true, Position));
	C->OpenStreamForServer(MWCursorContexts::VehicleSelection, EMWCursorSpace::NormalizedWidget);
	const int32 Second = C->GetCursorState().StreamId;
	TestTrue(TEXT("Reopen gets a new generation"), Second > First);
	TestFalse(TEXT("Old screen cannot update reopened screen"), C->ReceiveSampleForServer(First, 5, true, Position));
	TestTrue(TEXT("Sequence restarts for new generation"), C->ReceiveSampleForServer(Second, 1, true, Position));
	AMWPlayerState* Destination = Scene.World->SpawnActor<AMWPlayerState>();
	Scene.Player->CopyProperties(Destination);
	TestFalse(TEXT("Map transfer does not copy transient cursor"), Destination->GetCursorComponent()->GetCursorState().IsOpen());

	C->OpenStreamForServer(TEXT("TrackSelection"), EMWCursorSpace::World);
	const int32 Track = C->GetCursorState().StreamId;
	TestTrue(TEXT("Same component accepts track world positions"), C->ReceiveSampleForServer(Track, 1, true, FVector(1000, -400, 80)));
	TestFalse(TEXT("World bounds enforced"), C->ReceiveSampleForServer(Track, 2, true, FVector(1000001, 0, 0)));
	C->OpenStreamForServer(TEXT("Budget"), EMWCursorSpace::World);
	int32 Accepted = 0;
	for (int32 Sequence = 1; Sequence <= 100; ++Sequence)
		Accepted += C->ReceiveSampleForServer(C->GetCursorState().StreamId, Sequence, true, FVector::ZeroVector) ? 1 : 0;
	TestTrue(TEXT("Server burst budget limits floods"), Accepted >= 30 && Accepted <= 31);
	return true;
}

// TimerManager는 같은 엔진 프레임에 두 번 Tick하지 않는다. 실제 후속 프레임에서 만료를 확인한다.
class FMWCursorExpiryCommand : public IAutomationLatentCommand
{
public:
	explicit FMWCursorExpiryCommand(FAutomationTestBase* InTest) : Test(InTest), Scene(MakeUnique<MWCursorTests::FScene>())
	{
		Scene->Cursor->OpenStreamForServer(TEXT("Expiry"), EMWCursorSpace::NormalizedWidget);
		Scene->Cursor->ReceiveSampleForServer(Scene->Cursor->GetCursorState().StreamId, 1, true, FVector(0.5, 0.5, 0));
	}
	virtual bool Update() override
	{
		Scene->World->GetTimerManager().Tick(0.1f);
		++Frames;
		if (Frames == 5)
		{
			Test->TestTrue(TEXT("Before expiry remains visible"), Scene->Cursor->GetCursorState().bVisible);
			// Stationary heartbeat must extend expiry even when no replicated value changes.
			Scene->Cursor->ReceiveSampleForServer(Scene->Cursor->GetCursorState().StreamId, 2, true, FVector(0.5, 0.5, 0));
		}
		if (Frames == 10) Test->TestTrue(TEXT("Stationary heartbeat extended lifetime"), Scene->Cursor->GetCursorState().bVisible);
		if (Frames < 15) return false;
		Test->TestFalse(TEXT("Stopped sender expires"), Scene->Cursor->GetCursorState().bVisible);
		return true;
	}
private:
	FAutomationTestBase* Test;
	TUniquePtr<MWCursorTests::FScene> Scene;
	int32 Frames = 0;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMWCursorExpiryTest, "MW.Cursor.ExpiryAndHeartbeat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMWCursorExpiryTest::RunTest(const FString& Parameters)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FMWCursorExpiryCommand>(this));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMWCursorProjectionTest, "MW.Cursor.WorldWidgetProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMWCursorProjectionTest::RunTest(const FString& Parameters)
{
	MWCursorTests::FScene Scene;
	auto* Surface = NewObject<UWidgetComponent>(Scene.Player);
	Surface->SetWidgetSpace(EWidgetSpace::World);
	Surface->SetDrawSize(FVector2D(1920, 1080));
	Surface->SetPivot(FVector2D(0.3, 0.7));
	Surface->SetWorldTransform(FTransform(FRotator(12, 67, -4), FVector(1200, -400, 230), FVector(1.33, 1.33, 1.33)));
	// CurrentDrawSize is established by an actual WidgetComponent update, without requiring a GPU.
	Surface->RegisterComponent();
	Surface->UpdateWidget();
	const FVector2D DrawSize = Surface->GetCurrentDrawSize();
	TestTrue(TEXT("Actual component has draw size"), DrawSize.X > 0 && DrawSize.Y > 0);
	for (const FVector2D UV : {FVector2D(0, 0), FVector2D(0.25, 0.5), FVector2D(1, 1)})
	{
		const FVector2D Point = UV * DrawSize;
		const FVector WorldPoint = MWCursorProjection::WidgetPointToWorld(*Surface, Point);
		FVector2D RoundTrip;
		Surface->GetLocalHitLocation(WorldPoint, RoundTrip);
		TestTrue(TEXT("Real engine hit mapping agrees after rotation, pivot and scale"), Point.Equals(RoundTrip, 0.0001));
	}
	return true;
}

#endif
