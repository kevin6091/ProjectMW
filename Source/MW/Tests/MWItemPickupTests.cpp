#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/BoxComponent.h"
#include "Data/MWVehicleDriveData.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "Gameplay/Items/MWItemComponent.h"
#include "Gameplay/Items/MWPickupBox.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "Gameplay/Vehicle/MWVehicleMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "System/GameState/MWGameplayGameState.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMWItemPickupFlow, "MW.Items.PickupAndUse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMWItemPickupFlow::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());

	auto* State = World->SpawnActor<AMWGameplayGameState>();
	World->SetGameState(State);
	auto* Vehicle = World->SpawnActor<AMWVehiclePawn>(FVector(1000, 0, 200), FRotator::ZeroRotator);
	auto* Movement = CastChecked<UMWVehicleMovementComponent>(Vehicle->GetMovementComponent());
	auto* DriveDataProperty = FindFProperty<FObjectPropertyBase>(Movement->GetClass(), TEXT("DriveData"));
	check(DriveDataProperty);
	DriveDataProperty->SetObjectPropertyValue_InContainer(Movement, NewObject<UMWVehicleDriveData>(Vehicle));
	auto* Controller = World->SpawnActor<APlayerController>();
	Controller->SetAsLocalPlayerController();
	Controller->Possess(Vehicle);
	Vehicle->DispatchBeginPlay();
	UMWItemComponent* Items = Vehicle->GetItemComponent();
	TestTrue(TEXT("Vehicle is possessed"), Vehicle->GetController() == Controller);
	TestTrue(TEXT("Vehicle generates overlaps"), CastChecked<UPrimitiveComponent>(Vehicle->GetRootComponent())->GetGenerateOverlapEvents());

	auto* Left = World->SpawnActor<AMWPickupBox>(FVector(0, -300, 200), FRotator::ZeroRotator);
	auto* Middle = World->SpawnActor<AMWPickupBox>(FVector(0, 0, 200), FRotator::ZeroRotator);
	auto* Right = World->SpawnActor<AMWPickupBox>(FVector(0, 300, 200), FRotator::ZeroRotator);
	Left->DispatchBeginPlay();
	Middle->DispatchBeginPlay();
	Right->DispatchBeginPlay();
	World->SetBegunPlay(true);
	TestTrue(TEXT("World allows overlap notifications"), World->HasBegunPlay());

	auto IsAvailable = [](const AMWPickupBox* Box)
	{
		return Box->FindComponentByClass<UBoxComponent>()->IsCollisionEnabled();
	};
	auto MoveTo = [Vehicle](const FVector& Location)
	{
		Vehicle->SetActorLocation(Location);
	};

	MoveTo(Middle->GetActorLocation());
	TestTrue(TEXT("No pickup before Racing"), Items->GetCurrentItemId() == EMWItemId::None && IsAvailable(Middle));
	MoveTo(FVector(1000, 0, 200));
	State->SetRoomPhaseFromServer(EMWRoomPhase::Racing);
	MoveTo(Middle->GetActorLocation());
	TestTrue(TEXT("Middle grants booster"), Items->GetCurrentItemId() == EMWItemId::Booster);
	TestFalse(TEXT("Middle is consumed"), IsAvailable(Middle));
	TestTrue(TEXT("Left and right are independent"), IsAvailable(Left) && IsAvailable(Right));
	MoveTo(Left->GetActorLocation());
	TestTrue(TEXT("Full slot leaves other box available"), IsAvailable(Left));
	const float NormalMaxSpeed = Movement->GetMaxSpeed();
	Items->RequestUseItem();
	TestTrue(TEXT("Using booster empties slot and activates boost"), Items->GetCurrentItemId() == EMWItemId::None && Items->IsBoosterActive());
	TestTrue(TEXT("Booster raises the forward speed limit"), NormalMaxSpeed > 0.f && Movement->GetMaxSpeed() > NormalMaxSpeed);
	Items->RequestUseItem();
	TestTrue(TEXT("Repeated use leaves the empty slot empty"), Items->GetCurrentItemId() == EMWItemId::None);
	MoveTo(Right->GetActorLocation());
	TestTrue(TEXT("A new booster can be held while the previous one is active"), Items->GetCurrentItemId() == EMWItemId::Booster);
	Items->RequestUseItem();
	TestTrue(TEXT("Active booster cannot be stacked or consume the next one"), Items->GetCurrentItemId() == EMWItemId::Booster);
	State->SetRoomPhaseFromServer(EMWRoomPhase::PreparingRace);
	TestTrue(TEXT("Leaving Racing clears item and effect"), Items->GetCurrentItemId() == EMWItemId::None && !Items->IsBoosterActive());
	TestEqual(TEXT("Normal speed limit is restored"), Movement->GetMaxSpeed(), NormalMaxSpeed);

	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif
