// 기존 아이템 지급·사용·거절 계약을 검사하는 자동화 테스트다. 실제 입력 PIE를 대신하지 않는다.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/BoxComponent.h"
#include "Data/MWItemDefinitionData.h"
#include "Data/MWVehicleDriveData.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "Gameplay/Items/MWItemComponent.h"
#include "Gameplay/Items/MWMountedItemPresentation.h"
#include "Gameplay/Items/MWPickupBox.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "Gameplay/Vehicle/MWVehicleMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "LevelSequence.h"
#include "System/GameState/MWGameplayGameState.h"
#include "System/MWPlayerState.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMWMountedItemCatalog, "MW.Items.MountedPresentationCatalog",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMWMountedItemCatalog::RunTest(const FString& Parameters)
{
    const UEnum* ItemEnum = StaticEnum<EMWItemId>();
    for (int32 Index = 0; Index < ItemEnum->NumEnums() - 1; ++Index)
    {
        const EMWItemId Id = static_cast<EMWItemId>(ItemEnum->GetValueByIndex(Index));
        if (Id == EMWItemId::None || Id == EMWItemId::Booster)
        {
            continue;
        }
        const FString Name = ItemEnum->GetNameStringByIndex(Index);
        const UMWItemDefinitionData* Item = UMWItemDefinitionData::FindById(Id);
        if (TestNotNull(Name + TEXT(" resolves through MWAssetData"), Item))
        {
            TestTrue(Name + TEXT(" has a complete mount, use and detach kit"),
                Item->Presentation.VisualClass.LoadSynchronous() && Item->Presentation.Attach.LoadSynchronous()
                && AMWMountedItemPresentation::GetUseDuration(*Item, true) > 0.0);
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMWItemPickupFlow, "MW.Items.PickupAndUse",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// 서버 지급·사용과 중복·오래된 지급 ID 거절을 같은 테스트 월드에서 확인한다.
bool FMWItemPickupFlow::RunTest(const FString& Parameters)
{
    const auto* Booster = UMWItemDefinitionData::FindById(EMWItemId::Booster);
    const auto* Hammer = UMWItemDefinitionData::FindById(EMWItemId::Hammer);
    const auto* TeslaCoil = UMWItemDefinitionData::FindById(EMWItemId::TeslaCoil);
    const auto* MachineGun = UMWItemDefinitionData::FindById(EMWItemId::MachineGun);
    if (!TestNotNull(TEXT("Booster definition is available through the catalog"), Booster)
        || !TestNotNull(TEXT("Hammer definition is available through the catalog"), Hammer)
        || !TestNotNull(TEXT("Tesla coil definition is available through the catalog"), TeslaCoil)
        || !TestNotNull(TEXT("Machine gun definition is available through the catalog"), MachineGun))
    {
        return false;
    }

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
    auto* Player = World->SpawnActor<AMWPlayerState>();
    Player->SetOwner(Controller);
    Controller->PlayerState = Player;
    Controller->Possess(Vehicle);
    Vehicle->SetPlayerState(Player);
    Vehicle->DispatchBeginPlay();
    UMWItemComponent* Items = Vehicle->GetItemComponent();
    TestTrue(TEXT("Vehicle is possessed"), Vehicle->GetController() == Controller);
    TestTrue(TEXT("Vehicle generates overlaps"),
             CastChecked<UPrimitiveComponent>(Vehicle->GetRootComponent())->GetGenerateOverlapEvents());

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
    {
        FMWGameplayProgress Progress = State->GetProgress();
        Progress.Phase = EMWRoomPhase::Racing;
        Progress.RoundEpoch = 1;
        State->ApplyProgressFromServer(Progress);
        FMWPlayerRoundState Round;
        Round.RoundEpoch = 1;
        Round.RaceStatus = EMWRaceStatus::Active;
        Player->ApplyRoundStateFromServer(Round);
    }
    MoveTo(Middle->GetActorLocation());
    TestTrue(TEXT("Middle grants booster"), Items->GetCurrentItemId() == EMWItemId::Booster);
    TestFalse(TEXT("Middle is consumed"), IsAvailable(Middle));
    TestTrue(TEXT("Left and right are independent"), IsAvailable(Left) && IsAvailable(Right));
    MoveTo(Left->GetActorLocation());
    TestTrue(TEXT("Full slot leaves other box available"), IsAvailable(Left));
    const float NormalMaxSpeed = Movement->GetMaxSpeed();
    Items->RequestUseItem();
    TestTrue(TEXT("Using booster empties slot and activates boost"),
             Items->GetCurrentItemId() == EMWItemId::None && Items->IsBoosterActive());
    TestTrue(TEXT("Booster raises the forward speed limit"),
             NormalMaxSpeed > 0.f && Movement->GetMaxSpeed() > NormalMaxSpeed);
    Items->RequestUseItem();
    TestTrue(TEXT("Repeated use leaves the empty slot empty"), Items->GetCurrentItemId() == EMWItemId::None);
    MoveTo(Right->GetActorLocation());
    TestTrue(TEXT("A new booster can be held while the previous one is active"),
             Items->GetCurrentItemId() == EMWItemId::Booster);
    Items->RequestUseItem();
    TestTrue(TEXT("Active booster cannot be stacked or consume the next one"),
             Items->GetCurrentItemId() == EMWItemId::Booster);
    {
        FMWGameplayProgress Progress = State->GetProgress();
        Progress.Phase = EMWRoomPhase::PreparingRace;
        State->ApplyProgressFromServer(Progress);
    }
    TestTrue(TEXT("Leaving Racing clears item and effect"),
             Items->GetCurrentItemId() == EMWItemId::None && !Items->IsBoosterActive());
    TestEqual(TEXT("Normal speed limit is restored"), Movement->GetMaxSpeed(), NormalMaxSpeed);

    Left->SetItemPool({EMWItemId::Hammer});
    MoveTo(FVector(1000, 0, 200));
    FMWGameplayProgress Progress = State->GetProgress();
    Progress.Phase = EMWRoomPhase::Racing;
    State->ApplyProgressFromServer(Progress);
    TestTrue(TEXT("A presentation-only item uses the shared slot"), Items->TryGrantItem(EMWItemId::TeslaCoil));
    TestFalse(TEXT("A random grant does not replace a held item"), Items->RequestRandomItem());
    const FVector VelocityBeforePreview = Vehicle->GetVelocity();
    Items->RequestUseItem();
    TestTrue(TEXT("The visual cycle consumes the slot without an effect on driving"),
             Items->GetCurrentItemId() == EMWItemId::None && !Items->IsBoosterActive()
             && !Vehicle->IsItemControlLocked() && Vehicle->GetVelocity().Equals(VelocityBeforePreview));

    auto AdvanceThroughDetach = [World, TeslaCoil]()
    {
        constexpr float Step = .05f;
        const int32 Steps = FMath::CeilToInt(AMWMountedItemPresentation::GetUseDuration(*TeslaCoil, true) / Step) + 2;
        // 이동은 진행하지 않고 서버 시각과 예약 지급 타이머만 진행한다.
        for (int32 Tick = 0; Tick < Steps; ++Tick)
        {
            ++GFrameCounter;
            World->Tick(LEVELTICK_TimeOnly, Step);
            World->GetTimerManager().Tick(Step);
        }
    };
    TestTrue(TEXT("A random draw queues while a used item is detaching"), Items->RequestRandomItem());
    TestFalse(TEXT("Pressing again does not queue a second draw"), Items->RequestRandomItem());
    TestEqual(TEXT("An early draw does not give a guaranteed booster"), Items->GetCurrentItemId(), EMWItemId::None);
    AdvanceThroughDetach();
    TestTrue(TEXT("The queued draw grants an item when the detach window ends"),
             Items->GetCurrentItemId() != EMWItemId::None);
    Items->ResetItemState();

    TestTrue(TEXT("The preview can be granted again after a reset"), Items->TryGrantItem(EMWItemId::TeslaCoil));
    Items->RequestUseItem();
    TestTrue(TEXT("A second draw can be queued before a reset"), Items->RequestRandomItem());
    Items->ResetItemState();
    AdvanceThroughDetach();
    TestEqual(TEXT("Resetting cancels any queued draw"), Items->GetCurrentItemId(), EMWItemId::None);

    MoveTo(Left->GetActorLocation());
    TestEqual(TEXT("A configured pickup grants a hammer through the same slot"),
              Items->GetCurrentItemId(), EMWItemId::Hammer);
    TestFalse(TEXT("The hammer pickup is consumed"), IsAvailable(Left));

    const FVector AttackOrigin(0, 1000, 200);
    MoveTo(AttackOrigin);
    auto SpawnTarget = [&](const FVector& Offset)
    {
        const FTransform Transform(FRotator::ZeroRotator, AttackOrigin + Offset);
        auto* Target = World->SpawnActorDeferred<AMWVehiclePawn>(AMWVehiclePawn::StaticClass(), Transform, nullptr, nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        auto* TargetMovement = CastChecked<UMWVehicleMovementComponent>(Target->GetMovementComponent());
        DriveDataProperty->SetObjectPropertyValue_InContainer(TargetMovement, NewObject<UMWVehicleDriveData>(Target));
        Target->FinishSpawning(Transform);
        auto* TargetController = World->SpawnActor<APlayerController>();
        TargetController->SetAsLocalPlayerController();
        auto* TargetPlayer = World->SpawnActor<AMWPlayerState>();
        TargetPlayer->SetOwner(TargetController);
        TargetController->PlayerState = TargetPlayer;
        TargetController->Possess(Target);
        Target->SetPlayerState(TargetPlayer);
        FMWPlayerRoundState Round;
        Round.RoundEpoch = Progress.RoundEpoch;
        Round.RaceStatus = EMWRaceStatus::Active;
        TargetPlayer->ApplyRoundStateFromServer(Round);
        return Target;
    };
    const double Radius = Hammer->AreaHit.Radius;
    const FVector HullExtent = CastChecked<UBoxComponent>(Vehicle->GetRootComponent())->GetScaledBoxExtent();
    const double CenterX = HullExtent.X + Radius;
    AMWVehiclePawn* Edge = SpawnTarget(FVector(CenterX, HullExtent.Y + 175, 0));
    AMWVehiclePawn* OutsideCircle = SpawnTarget(FVector(CenterX + HullExtent.X + Radius * .8, -HullExtent.Y - Radius * .8, 0));
    AMWVehiclePawn* Above = SpawnTarget(FVector(CenterX, 0, Hammer->AreaHit.HalfHeight + HullExtent.Z + 100));
    TestTrue(TEXT("The nearby target can drive before the strike"), Edge->CanDrive());
    UMWItemComponent* TargetItems = Edge->GetItemComponent();
    TestTrue(TEXT("The target holds a usable booster before the strike"), TargetItems->TryGrantItem(EMWItemId::Booster));
    World->GetTimerManager().Tick(0.f);
    Items->RequestUseItem();
    Items->RequestUseItem();
    TestEqual(TEXT("The first swing keeps the hammer in the slot"), Items->GetCurrentItemId(), EMWItemId::Hammer);
    TestTrue(TEXT("The target is untouched before the impact"), Edge->GetVelocity().IsNearlyZero() && !Edge->IsItemControlLocked());
    ++GFrameCounter;
    World->GetTimerManager().Tick(Hammer->AreaHit.ImpactDelay + .01f);
    TestTrue(TEXT("A vehicle 175 cm off the centerline at its nearest edge receives one knockback"),
             FMath::IsNearlyEqual(Edge->GetVelocity().Size2D(), static_cast<double>(Hammer->AreaHit.KnockbackSpeed), 1.0));
    TestTrue(TEXT("The struck vehicle keeps simulating movement with driver input locked"),
             Edge->CanDrive() && Edge->IsItemControlLocked());
    TargetItems->RequestUseItem();
    TestTrue(TEXT("Using the held item while stunned has no effect"),
             TargetItems->GetCurrentItemId() == EMWItemId::Booster && !TargetItems->IsBoosterActive());
    TestTrue(TEXT("A target in the broad-phase corner outside the circle is untouched"),
             OutsideCircle->GetVelocity().SizeSquared2D() < 1.0 && !OutsideCircle->IsItemControlLocked());
    TestTrue(TEXT("A target above the cylinder is untouched"),
             Above->GetVelocity().SizeSquared2D() < 1.0 && !Above->IsItemControlLocked());

    for (const EMWItemId ExpectedItem : {EMWItemId::Hammer, EMWItemId::None})
    {
        // 시간만 흘려 차체를 같은 타격 위치에 두고 다음 사용의 서버 제한을 해제한다.
        constexpr float ClockStep = 0.05f;
        const int32 ClockTicks = FMath::CeilToInt(Hammer->AreaHit.UseDuration / ClockStep) + 1;
        for (int32 Tick = 0; Tick < ClockTicks; ++Tick)
        {
            World->Tick(LEVELTICK_TimeOnly, ClockStep);
        }
        const FVector VelocityBeforeSwing = Edge->GetVelocity();
        Items->RequestUseItem();
        Items->RequestUseItem();
        TestEqual(TEXT("Only the final swing empties the slot"), Items->GetCurrentItemId(), ExpectedItem);
        ++GFrameCounter;
        World->GetTimerManager().Tick(Hammer->AreaHit.ImpactDelay + .01f);
        const double SpeedChange = (Edge->GetVelocity() - VelocityBeforeSwing).Size2D();
        TestTrue(TEXT("Every remaining swing delivers one hit"),
            FMath::IsNearlyEqual(SpeedChange, static_cast<double>(Hammer->AreaHit.KnockbackSpeed), 1.0));
    }
    const FVector VelocityAfterFinalSwing = Edge->GetVelocity();
    Items->RequestUseItem();
    ++GFrameCounter;
    World->GetTimerManager().Tick(Hammer->AreaHit.ImpactDelay + .01f);
    TestTrue(TEXT("An exhausted hammer cannot hit again"), Edge->GetVelocity().Equals(VelocityAfterFinalSwing));

    const FMWItemBurstFireData& Burst = MachineGun->BurstFire;
    const FVector BurstOrigin = AttackOrigin + FVector(0, 650, 0);
    MoveTo(BurstOrigin);
    AMWVehiclePawn* Front = SpawnTarget(FVector(400, 650, 0));
    AMWVehiclePawn* Behind = SpawnTarget(FVector(750, 650, 0));
    AMWVehiclePawn* Side = SpawnTarget(FVector(0, 1050, 0));
    const auto TickBurst = [World](float Step)
    {
        ++GFrameCounter;
        World->Tick(LEVELTICK_TimeOnly, Step);
        World->GetTimerManager().Tick(Step);
    };

    Items->ResetItemState();
    TestTrue(TEXT("The machine gun can be granted through the shared slot"), Items->TryGrantItem(EMWItemId::MachineGun));
    Items->RequestUseItem();
    Items->RequestUseItem();
    TestEqual(TEXT("Starting a burst consumes the item once"), Items->GetCurrentItemId(), EMWItemId::None);
    TickBurst(.01f);
    TestTrue(TEXT("The first burst shot hits immediately"),
        Front->GetVelocity().Equals(FVector(Burst.KnockbackSpeed, 0, 0), 1.0));
    for (int32 Shot = 1; Shot <= Burst.ShotCount; ++Shot)
    {
        TickBurst(Burst.ShotInterval);
    }
    TestTrue(TEXT("Only the configured shots hit the front vehicle, with no input lock"),
        Front->GetVelocity().Equals(FVector(Burst.ShotCount * Burst.KnockbackSpeed, 0, 0), 1.0)
        && !Front->IsItemControlLocked());
    TestTrue(TEXT("A vehicle behind the first hit receives no knockback"), Behind->GetVelocity().IsNearlyZero());
    TestTrue(TEXT("A vehicle outside the firing line receives no knockback"), Side->GetVelocity().IsNearlyZero());

    AActor* WallActor = World->SpawnActor<AActor>();
    UBoxComponent* Wall = NewObject<UBoxComponent>(WallActor);
    WallActor->SetRootComponent(Wall);
    Wall->SetBoxExtent(FVector(20, 150, 140));
    Wall->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Wall->SetCollisionObjectType(ECC_WorldStatic);
    Wall->SetCollisionResponseToAllChannels(ECR_Block);
    Wall->RegisterComponent();
    WallActor->SetActorLocation(BurstOrigin + FVector(250, 0, 0));

    Items->ResetItemState();
    TestTrue(TEXT("A new machine gun can be granted after a reset"), Items->TryGrantItem(EMWItemId::MachineGun));
    const FVector FrontVelocityBeforeWall = Front->GetVelocity();
    Items->RequestUseItem();
    TickBurst(.01f);
    TestTrue(TEXT("A wall in front of the target blocks the shot"), Front->GetVelocity().Equals(FrontVelocityBeforeWall));

    Vehicle->SetActorRotation(FRotator(0, 90, 0));
    TickBurst(Burst.ShotInterval);
    TestTrue(TEXT("The next shot uses the current vehicle heading"),
        Side->GetVelocity().Equals(FVector(0, Burst.KnockbackSpeed, 0), 1.0));
    Items->ResetItemState();
    const FVector SideVelocityBeforeReset = Side->GetVelocity();
    TickBurst(Burst.ShotInterval);
    TestTrue(TEXT("Resetting cancels the remaining shots"), Side->GetVelocity().Equals(SideVelocityBeforeReset));

    // 파괴 뒤 구독이 남는 회귀를 검사한다. 실제 화면 클릭이나 효과음 재생까지 보증하는 검사는 아니다.
    const FName StateChangedFunction(TEXT("HandleGameStateChanged"));
    TestTrue(TEXT("Item component subscribes while playing"),
             State->OnGameplayStateChanged.Contains(Items, StateChangedFunction));
    Items->DestroyComponent();
    TestFalse(TEXT("Item component releases its GameState subscription on destruction"),
              State->OnGameplayStateChanged.Contains(Items, StateChangedFunction));
    World->EndPlay(EEndPlayReason::Quit);
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}

#endif
