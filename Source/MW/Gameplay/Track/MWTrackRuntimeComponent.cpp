// 복구 트랙 데이터의 충돌·기믹·음향을 실행한다. 트랙 배치 최종 승인을 새로 만들지 않는다.

#include "Gameplay/Track/MWTrackRuntimeComponent.h"

#include "Components/BoxComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Gameplay/Track/MWTrackRuntimeSubsystem.h"
#include "Gameplay/Vehicle/MWVehicleMovementComponent.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "Sound/SoundBase.h"
#include "Net/UnrealNetwork.h"

namespace
{
    const FName RoadTag(TEXT("MWTrackDriveCollision"));
    const FName TriggerTag(TEXT("MWTrackRaceTrigger"));
    const FName TrainTag(TEXT("MWTrackTrain"));
    const FName TrainHitTag(TEXT("MWTrackTrainHit"));
    const FName TrainBellTag(TEXT("MWTrackTrainBell"));
    const FName TrafficTag(TEXT("MWTrackTraffic"));
    const FName TrafficHitTag(TEXT("MWTrackTrafficHit"));

    void SetTriggerCollision(UPrimitiveComponent& Component)
    {
        Component.SetCollisionObjectType(ECC_WorldDynamic);
        Component.SetCollisionResponseToAllChannels(ECR_Ignore);
        Component.SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
        Component.SetGenerateOverlapEvents(true);
    }
}

UMWTrackRuntimeComponent::UMWTrackRuntimeComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

// 기존 월드 카탈로그의 복구 설정을 준비하고 현재 트랙 수명에 연결한다.
void UMWTrackRuntimeComponent::BeginPlay()
{
    Super::BeginPlay();
    // 클라이언트의 동적 컴포넌트는 액터 채널 복제로 생성될 수 있어 초기 설정을 여기서도 보완한다.
    if (!bConfigured)
    {
        if (UMWTrackRuntimeSubsystem* Runtime = GetWorld()->GetSubsystem<UMWTrackRuntimeSubsystem>())
        {
            Runtime->ConfigureRuntimeComponent(*this);
        }
    }
}

// 기존 복제 계약에 따라 소유자 전용 값과 다른 플레이어에게 보일 상태를 등록한다.
void UMWTrackRuntimeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UMWTrackRuntimeComponent, LiftActiveAt);
}

// 탑승 관계·생성 컴포넌트·교통·음향을 정리해 이전 트랙 참조가 남지 않게 한다.
void UMWTrackRuntimeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bPreview = true;
    SetComponentTickEnabled(false);
    SetTrainBell(false);
    // 소유 액터에 추가한 충돌·시각 컴포넌트는 자동으로 함께 사라지지 않으므로 직접 해제한다.
    ClearRiders();
    for (const TWeakObjectPtr<UActorComponent>& Generated : GeneratedComponents)
    {
        if (UActorComponent* Component = Generated.Get())
        {
            if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
            {
                Primitive->OnComponentBeginOverlap.RemoveAll(this);
                Primitive->OnComponentEndOverlap.RemoveAll(this);
            }
            Component->DestroyComponent();
        }
    }
    GeneratedComponents.Reset();
    Triggers.Reset();
    RiderTriggers.Reset();
    Super::EndPlay(EndPlayReason);
}

// 복구 데이터의 값과 에셋 참조를 소유한 뒤 충돌·움직임·교통을 구성한다.
void UMWTrackRuntimeComponent::Configure(const FMWTrackPieceRuntimeData& InData)
{
    if (bConfigured || !IsValid(GetOwner()) || !GetOwner()->GetRootComponent())
    {
        return;
    }
    Piece = InData;
    bConfigured = true;
    const FMWTrackPieceRuntimeData& Data = Piece;
    AActor& Actor = *GetOwner();
    TInlineComponentArray<UStaticMeshComponent*> Visuals(&Actor);
    Motions.Reserve(Data.Motions.Num());
    for (const FMWTrackMotionData& Motion : Data.Motions)
    {
        FMotion& Entry = Motions.AddDefaulted_GetRef();
        Entry.Data = Motion;
        for (UStaticMeshComponent* Visual : Visuals)
        {
            if (Motion.RendererTags.ContainsByPredicate(
                    [Visual](FName Tag)
                    {
                        return Visual->ComponentHasTag(Tag);
                    }))
            {
                Visual->SetMobility(EComponentMobility::Movable);
                Entry.Parts.Add({Visual, Visual->GetRelativeTransform()});
            }
        }
    }

    for (const FMWTrackShapeData& Shape : Data.Shapes)
    {
        if (UPrimitiveComponent* Component = CreateShape(Shape))
        {
            if (Motions.IsValidIndex(Shape.MotionIndex))
            {
                Motions[Shape.MotionIndex].Parts.Add({Component, Shape.Local});
            }
            if (Shape.Trigger == EMWTrackTrigger::LiftRide)
            {
                LiftRide = Component;
            }
        }
    }

    if (!Data.Rails.IsEmpty() && !Data.TrainParts.IsEmpty())
    {
        FBox TrainBounds(EForceInit::ForceInit);
        for (const FMWTrackTrainPart& Part : Data.TrainParts)
        {
            UStaticMeshComponent* Visual = NewObject<UStaticMeshComponent>(&Actor, NAME_None, RF_Transient);
            Visual->SetupAttachment(Actor.GetRootComponent());
            Visual->SetMobility(EComponentMobility::Movable);
            Visual->SetStaticMesh(Part.Mesh);
            Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Visual->SetHiddenInGame(true);
            Visual->ComponentTags.Add(TrainTag);
            Actor.AddInstanceComponent(Visual);
            GeneratedComponents.Add(Visual);
            Visual->RegisterComponent();
            Train.Add(Visual);
            TrainBounds += Part.Mesh->GetBoundingBox().TransformBy(Part.Local);
        }
        UBoxComponent* Hit = NewObject<UBoxComponent>(&Actor, NAME_None, RF_Transient);
        Hit->InitBoxExtent(TrainBounds.GetExtent());
        TrainCenter = TrainBounds.GetCenter();
        Hit->SetupAttachment(Actor.GetRootComponent());
        Hit->SetMobility(EComponentMobility::Movable);
        Hit->ComponentTags.Add(TrainHitTag);
        Hit->OnComponentBeginOverlap.AddDynamic(this, &UMWTrackRuntimeComponent::OnTrigger);
        SetTriggerCollision(*Hit);
        Hit->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Actor.AddInstanceComponent(Hit);
        GeneratedComponents.Add(Hit);
        Hit->RegisterComponent();
        TrainHitbox = Hit;
        if (Data.TrainBell && GetNetMode() != NM_DedicatedServer)
        {
            UAudioComponent* Audio = NewObject<UAudioComponent>(&Actor, NAME_None, RF_Transient);
            Audio->SetupAttachment(Actor.GetRootComponent());
            FVector Center = FVector::ZeroVector;
            for (const FTransform& Rail : Data.Rails)
            {
                Center += Rail.GetLocation();
            }
            Audio->SetRelativeLocation(Center / Data.Rails.Num());
            Audio->bAutoActivate = false;
            Audio->bOverrideAttenuation = true;
            Audio->AttenuationOverrides.AttenuationShapeExtents = FVector(2200, 0, 0);
            Audio->AttenuationOverrides.FalloffDistance = 7500;
            Audio->SetSound(Data.TrainBell);
            Audio->ComponentTags.Add(TrainBellTag);
            Actor.AddInstanceComponent(Audio);
            GeneratedComponents.Add(Audio);
            Audio->RegisterComponent();
            Bell = Audio;
        }
    }
    ConfigureTraffic();
    SetPreview(bPreview);
}

// 복구한 로컬 변환과 형상 데이터로 실제 트랙의 충돌 또는 트리거를 만든다.
UPrimitiveComponent* UMWTrackRuntimeComponent::CreateShape(const FMWTrackShapeData& Shape)
{
    AActor& Actor = *GetOwner();
    UPrimitiveComponent* Component = nullptr;
    switch (Shape.Shape)
    {
    case EMWTrackShape::Mesh:
        if (Shape.Mesh)
        {
            UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(&Actor, NAME_None, RF_Transient);
            Mesh->SetStaticMesh(Shape.Mesh);
            Component = Mesh;
        }
        break;
    case EMWTrackShape::Box:
    {
        UBoxComponent* Box = NewObject<UBoxComponent>(&Actor, NAME_None, RF_Transient);
        Box->InitBoxExtent(Shape.Extent);
        Component = Box;
        break;
    }
    case EMWTrackShape::Capsule:
    {
        UCapsuleComponent* Capsule = NewObject<UCapsuleComponent>(&Actor, NAME_None, RF_Transient);
        Capsule->InitCapsuleSize(Shape.Extent.X, Shape.Extent.Z);
        Component = Capsule;
        break;
    }
    case EMWTrackShape::Sphere:
    {
        USphereComponent* Sphere = NewObject<USphereComponent>(&Actor, NAME_None, RF_Transient);
        Sphere->InitSphereRadius(Shape.Extent.X);
        Component = Sphere;
        break;
    }
    }
    if (!Component)
    {
        return nullptr;
    }
    Component->SetupAttachment(Actor.GetRootComponent());
    Component->SetRelativeTransform(Shape.Local);
    Component->SetMobility(EComponentMobility::Movable);
    Component->SetHiddenInGame(true);
    Component->SetCastShadow(false);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (Shape.Trigger == EMWTrackTrigger::None)
    {
        Component->SetCollisionObjectType(ECC_WorldStatic);
        Component->SetCollisionResponseToAllChannels(ECR_Block);
        Component->SetGenerateOverlapEvents(false);
        Component->ComponentTags.Add(RoadTag);
        RoadCollision.Add(Component);
    }
    else
    {
        SetTriggerCollision(*Component);
        Component->ComponentTags.Add(TriggerTag);
        Triggers.Add(Component, &Shape);
        if (Shape.Trigger == EMWTrackTrigger::Roller || Shape.Trigger == EMWTrackTrigger::LiftRide)
        {
            RiderTriggers.Add(Component).Shape = &Shape;
            Component->OnComponentEndOverlap.AddDynamic(this, &UMWTrackRuntimeComponent::OnRiderEndOverlap);
        }
        Component->OnComponentBeginOverlap.AddDynamic(this, &UMWTrackRuntimeComponent::OnTrigger);
    }
    Actor.AddInstanceComponent(Component);
    GeneratedComponents.Add(Component);
    Component->RegisterComponent();
    return Component;
}

// 미리보기 조각에서 실제 경기 트리거·충돌·음향이 동작하지 않도록 표시 상태를 구분한다.
void UMWTrackRuntimeComponent::SetPreview(bool bInPreview)
{
    bPreview = bInPreview;
    if (!bConfigured)
    {
        return;
    }
    const ECollisionEnabled::Type Collision = bPreview ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly;
    for (const TWeakObjectPtr<UPrimitiveComponent>& Road : RoadCollision)
    {
        if (Road.IsValid())
        {
            Road->SetCollisionEnabled(Collision);
        }
    }
    for (const auto& Trigger : Triggers)
    {
        if (UPrimitiveComponent* Component = Trigger.Key.Get())
        {
            Component->SetCollisionEnabled(Collision);
        }
    }
    if (bPreview)
    {
        ClearRiders();
        SetTrainActive(false);
        SetTrainBell(false);
        for (FTrafficSlot& Slot : Traffic)
        {
            SetTrafficActive(Slot, false);
        }
    }
    const bool bHasMotion = !Motions.IsEmpty() || !Train.IsEmpty() || !Traffic.IsEmpty();
    SetComponentTickEnabled(!bPreview && bHasMotion);
}

// 경기 연출이 호스트·원격에서 같은 시간축을 쓰도록 서버 시각을 조회한다.
double UMWTrackRuntimeComponent::GetRaceTime() const
{
    const UWorld* World = GetWorld();
    if (const AGameStateBase* GameState = World->GetGameState())
    {
        return GameState->GetServerWorldTimeSeconds();
    }
    return World->GetTimeSeconds();
}

// 유효한 차량과 현재 단계·권한을 검사해 원래 트랙 효과를 실행한다.
void UMWTrackRuntimeComponent::OnTrigger(UPrimitiveComponent* Overlapped, AActor* OtherActor,
                                         UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
                                         const FHitResult& Sweep)
{
    AMWVehiclePawn* Vehicle = Cast<AMWVehiclePawn>(OtherActor);
    if (!bConfigured || bPreview || !IsValid(Vehicle))
    {
        return;
    }
    if (FRiderTrigger* RiderTrigger = RiderTriggers.Find(Overlapped))
    {
        RiderTrigger->Riders.Add(Vehicle);
        if (RiderTrigger->Shape->Trigger == EMWTrackTrigger::LiftRide)
        {
            CastChecked<UMWVehicleMovementComponent>(Vehicle->GetMovementComponent())->SetTrackLiftBase(GetOwner());
        }
        return;
    }
    if (!Vehicle->HasAuthority() && !Vehicle->IsLocallyControlled())
    {
        return;
    }
    UMWTrackRuntimeSubsystem* Runtime = GetWorld()->GetSubsystem<UMWTrackRuntimeSubsystem>();
    const bool bTrafficHit = Overlapped->ComponentHasTag(TrafficHitTag);
    if (Overlapped == TrainHitbox.Get() || bTrafficHit)
    {
        if (Vehicle->HasAuthority() && !Runtime->RespawnAtTrackCheckpoint(Vehicle))
        {
            CastChecked<UMWVehicleMovementComponent>(Vehicle->GetMovementComponent())
                ->ApplyTrackSurfaceVelocity(
                    (bTrafficHit ? Overlapped->GetForwardVector() : Overlapped->GetRightVector()) * 1300.f, 0.25f);
        }
        return;
    }
    const FMWTrackShapeData* const* Entry = Triggers.Find(Overlapped);
    if (!Entry)
    {
        return;
    }
    const FMWTrackShapeData& Shape = **Entry;
    switch (Shape.Trigger)
    {
    case EMWTrackTrigger::Boost:
        CastChecked<UMWVehicleMovementComponent>(Vehicle->GetMovementComponent())
            ->ApplyTrackBoost(Shape.Amount, Shape.Duration);
        break;
    case EMWTrackTrigger::LiftButton:
        if (GetNetMode() != NM_Client && GetOwner()->HasAuthority())
        {
            const FMotion* Lift = Motions.FindByPredicate(
                [](const FMotion& Motion)
                {
                    return Motion.Data.Type == EMWTrackMotion::Lift;
                });
            if (Lift && Lift->Data.Rate > 0.f && Lift->Data.Amount > 0.f)
            {
                const double Now = GetRaceTime();
                const double CycleSeconds = 2.0 * Lift->Data.Amount / Lift->Data.Rate + Piece.LiftReturnDelay;
                // 여러 충돌체나 플레이어의 중복 진입으로 진행 중인 승강기 시작 시각이 계속 밀리지 않게 한다.
                if (LiftActiveAt < 0.0 || Now >= LiftActiveAt + CycleSeconds)
                {
                    LiftActiveAt = Now + FMath::Max(0.f, Piece.LiftDelay);
                    GetOwner()->ForceNetUpdate();
                }
            }
        }
        break;
    case EMWTrackTrigger::Start:
    case EMWTrackTrigger::Checkpoint:
    case EMWTrackTrigger::Finish:
        Runtime->ProcessRaceTrigger(*Vehicle, *GetOwner(), Shape.Trigger, Piece.RespawnPoints);
        break;
    default:
        break;
    }
}

// 같은 차량의 다른 충돌 본체가 남았는지 확인한 뒤 발판 탑승 관계를 해제한다.
void UMWTrackRuntimeComponent::OnRiderEndOverlap(UPrimitiveComponent* Overlapped, AActor* OtherActor,
                                                 UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex)
{
    AMWVehiclePawn* Vehicle = Cast<AMWVehiclePawn>(OtherActor);
    FRiderTrigger* RiderTrigger = RiderTriggers.Find(Overlapped);
    // 같은 Pawn의 다른 충돌 본체가 아직 트리거 위에 있을 수 있어 완전히 벗어났는지 다시 확인한다.
    if (Vehicle && RiderTrigger && !Overlapped->IsOverlappingActor(Vehicle))
    {
        RiderTrigger->Riders.Remove(Vehicle);
        if (RiderTrigger->Shape->Trigger == EMWTrackTrigger::LiftRide)
        {
            CastChecked<UMWVehicleMovementComponent>(Vehicle->GetMovementComponent())->ClearTrackLiftBase(GetOwner());
        }
    }
}

// 모든 탑승 차량의 발판 참조와 선행 틱 관계를 해제한다.
void UMWTrackRuntimeComponent::ClearRiders()
{
    for (auto& Trigger : RiderTriggers)
    {
        for (const TWeakObjectPtr<AMWVehiclePawn>& Rider : Trigger.Value.Riders)
        {
            if (AMWVehiclePawn* Vehicle = Rider.Get())
            {
                CastChecked<UMWVehicleMovementComponent>(Vehicle->GetMovementComponent())
                    ->ClearTrackLiftBase(GetOwner());
            }
        }
        Trigger.Value.Riders.Reset();
    }
}

// 로컬 이동축과 현재 높이로 발판의 월드 이동량을 조회한다.
FVector UMWTrackRuntimeComponent::GetLiftWorldOffset() const
{
    const AActor* Owner = GetOwner();
    return IsValid(Owner) ? Owner->GetActorTransform().TransformVector(FVector(0, 0, LiftHeight)) : FVector::ZeroVector;
}

// 복제된 시작 시각으로 발판 높이를 구하고 안전한 탑승자 사본에 이동량을 적용한다.
void UMWTrackRuntimeComponent::UpdateLift(double Time, float DeltaTime)
{
    const FMotion* Lift = Motions.FindByPredicate(
        [](const FMotion& Motion)
        {
            return Motion.Data.Type == EMWTrackMotion::Lift;
        });
    if (!Lift || LiftActiveAt < 0 || Lift->Data.Rate <= 0.f || Lift->Data.Amount <= 0.f)
    {
        return;
    }
    const double Elapsed = Time - LiftActiveAt;
    const double RiseSeconds = Lift->Data.Amount / Lift->Data.Rate;
    const double ReturnAt = RiseSeconds + FMath::Max(0.f, Piece.LiftReturnDelay);
    const float PreviousHeight = LiftHeight;
    // 서버 기준 시작 시각에서 높이를 다시 계산하면 늦은 복제나 프레임 정지 뒤에도 같은 높이를 복원한다.
    // 각 기기에서 프레임 간격을 따로 누적하면 승강기 위치가 서로 어긋난다.
    const double Height =
        Elapsed <= ReturnAt ? Elapsed * Lift->Data.Rate : Lift->Data.Amount - (Elapsed - ReturnAt) * Lift->Data.Rate;
    LiftHeight = static_cast<float>(FMath::Clamp(Height, 0.0, static_cast<double>(Lift->Data.Amount)));
    FRiderTrigger* RiderTrigger = RiderTriggers.Find(LiftRide.Get());
    if (!RiderTrigger || FMath::IsNearlyEqual(LiftHeight, PreviousHeight))
    {
        return;
    }

    // Pawn 이동이 중첩 이벤트를 일으켜 갱신 중 탑승자 집합을 바꿀 수 있다.
    TArray<TWeakObjectPtr<AMWVehiclePawn>, TInlineAllocator<4>> Riders;
    for (auto It = RiderTrigger->Riders.CreateIterator(); It; ++It)
    {
        if (It->IsValid())
            Riders.Add(*It);
        else
            It.RemoveCurrent();
    }
    const FVector Delta = GetOwner()->GetActorTransform().TransformVector(FVector(0, 0, LiftHeight - PreviousHeight));
    for (const TWeakObjectPtr<AMWVehiclePawn>& Rider : Riders)
    {
        if (AMWVehiclePawn* Vehicle = Rider.Get())
        {
            // 원격 차량도 보간 시작 위치를 기록하기 전에 이번 프레임의 발판 이동량을 반영해야 한다.
            Vehicle->AddActorWorldOffset(Delta, false, nullptr, ETeleportType::None);
        }
    }
}

// 원본 축·기준 자세·진행 시간으로 복구 트랙 부품을 움직인다.
void UMWTrackRuntimeComponent::ApplyMotions(double Time, float GateAlpha)
{
    for (const FMotion& Motion : Motions)
    {
        const FMWTrackMotionData& Data = Motion.Data;
        double Degrees = 0;
        if (Data.Type == EMWTrackMotion::Barrier)
        {
            Degrees = Data.Amount * (1.f - GateAlpha);
        }
        else if (Data.Type == EMWTrackMotion::Roller)
        {
            Degrees = FMath::Fmod(Time * Data.Rate, 360.0);
        }
        else if (Data.Type == EMWTrackMotion::TrafficGate && TrafficLaneStates.IsValidIndex(Data.TrafficLaneIndex))
        {
            const FTrafficLaneState& Lane = TrafficLaneStates[Data.TrafficLaneIndex];
            const double EdgeTime = Data.bAtLaneEnd ? Lane.DriveSeconds : 0;
            const double Open = FMath::Clamp(1.0 - FMath::Abs(Lane.Phase - EdgeTime) * Data.Rate, 0.0, 1.0);
            Degrees = Data.Amount * Open;
        }
        const FQuat Rotation(Data.Axis, FMath::DegreesToRadians(Degrees));
        for (const FMovedPart& Part : Motion.Parts)
        {
            if (!Part.Component.IsValid())
            {
                continue;
            }
            FTransform Pose = Part.Rest;
            if (Data.Type == EMWTrackMotion::Lift)
            {
                Pose.AddToTranslation(FVector(0, 0, LiftHeight));
            }
            else
            {
                Pose.SetLocation(Data.Pivot.GetLocation() +
                                 Rotation.RotateVector(Part.Rest.GetLocation() - Data.Pivot.GetLocation()));
                Pose.SetRotation(Rotation * Part.Rest.GetRotation());
            }
            Part.Component->SetRelativeTransform(Pose);
        }
    }
}

// 원본 레일과 시간 설정에 맞춰 기차 부품·충돌·경고음을 갱신한다.
void UMWTrackRuntimeComponent::UpdateTrain(double Time)
{
    const double Interval = Piece.TrainInterval;
    const int32 Cycle = FMath::FloorToInt(Time / Interval);
    const double Phase = FMath::Fmod(Time, Interval);
    constexpr double TransitSeconds = 2.5;
    if (Phase >= TransitSeconds)
    {
        SetTrainActive(false);
        return;
    }
    const FTransform& Rail = Piece.Rails[Cycle % Piece.Rails.Num()];
    const double Distance = FMath::Lerp(-4400.0, 4400.0, Phase / TransitSeconds);
    FTransform Route = Rail;
    Route.AddToTranslation(Rail.GetUnitAxis(EAxis::Y) * Distance);
    for (int32 Index = 0; Index < Train.Num(); ++Index)
    {
        if (UStaticMeshComponent* Visual = Train[Index].Get())
        {
            Visual->SetRelativeTransform(Piece.TrainParts[Index].Local * Route);
        }
    }
    if (UPrimitiveComponent* Hit = TrainHitbox.Get())
    {
        Hit->SetRelativeTransform(FTransform(TrainCenter) * Route);
    }
    SetTrainActive(true);
}

// 기차의 외형과 충돌 활성 여부를 같은 상태로 맞춘다.
void UMWTrackRuntimeComponent::SetTrainActive(bool bActive)
{
    if (bTrainActive == bActive)
    {
        return;
    }
    bTrainActive = bActive;
    for (const TWeakObjectPtr<UStaticMeshComponent>& Part : Train)
    {
        if (UStaticMeshComponent* Visual = Part.Get())
            Visual->SetHiddenInGame(!bActive);
    }
    if (UPrimitiveComponent* Hit = TrainHitbox.Get())
    {
        Hit->SetCollisionEnabled(bActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
    }
}

// 기존 경고음의 활성 상태를 전환하고 정지 시 재생 참조를 정리한다.
void UMWTrackRuntimeComponent::SetTrainBell(bool bActive)
{
    if (bBellActive == bActive || !Bell.IsValid())
    {
        return;
    }
    bBellActive = bActive;
    if (bActive)
    {
        Bell->Play();
    }
    else
    {
        Bell->Stop();
    }
}

// 차선·차종 가중치·슬롯 수로 복구 교통 외형과 충돌을 준비한다.
void UMWTrackRuntimeComponent::ConfigureTraffic()
{
    if (Piece.TrafficLanes.IsEmpty() || Piece.TrafficModels.IsEmpty())
    {
        return;
    }
    int32 Weight = 0;
    for (const FMWTrackTrafficModelData& Model : Piece.TrafficModels)
    {
        Weight += Model.VisualClass ? Model.Weight : 0;
    }
    if (Weight <= 0)
    {
        return;
    }

    AActor& Actor = *GetOwner();
    FRandomStream Random(FCrc::StrCrc32(*Piece.VisualClass.ToString()));
    TrafficLaneStates.SetNum(Piece.TrafficLanes.Num());
    for (int32 LaneIndex = 0; LaneIndex < Piece.TrafficLanes.Num(); ++LaneIndex)
    {
        const FMWTrackTrafficLaneData& Lane = Piece.TrafficLanes[LaneIndex];
        if (Lane.Length <= 0 || Lane.Speed <= 0)
        {
            continue;
        }
        const double DriveSeconds = Lane.Length / Lane.Speed;
        TrafficLaneStates[LaneIndex].DriveSeconds = DriveSeconds;
        for (int32 Index = 0; Index < Lane.Slots; ++Index)
        {
            int32 Draw = Random.RandRange(1, Weight);
            const FMWTrackTrafficModelData* Choice = Piece.TrafficModels.FindByPredicate(
                [&Draw](const FMWTrackTrafficModelData& Model)
                {
                    return Model.VisualClass && (Draw -= Model.Weight) <= 0;
                });
            if (!Choice)
            {
                continue;
            }

            UChildActorComponent* Visual = NewObject<UChildActorComponent>(&Actor, NAME_None, RF_Transient);
            Visual->SetupAttachment(Actor.GetRootComponent());
            Visual->SetMobility(EComponentMobility::Movable);
            Visual->SetChildActorClass(Choice->VisualClass);
            Visual->ComponentTags.Add(TrafficTag);
            Actor.AddInstanceComponent(Visual);
            GeneratedComponents.Add(Visual);
            Visual->RegisterComponent();
            AActor* Child = Visual->GetChildActor();
            if (!Child)
            {
                Visual->DestroyComponent();
                continue;
            }
            Child->SetActorEnableCollision(false);
            Child->SetActorHiddenInGame(true);
            const FBox Bounds = Child->CalculateComponentsBoundingBoxInLocalSpace(true);
            if (!Bounds.IsValid)
            {
                Visual->DestroyComponent();
                continue;
            }
            const FVector Extent = Bounds.GetExtent();
            const FRotator Correction(0, Extent.Y > Extent.X ? -90.0 : 0.0, 0);
            const FBox AlignedBounds = Bounds.TransformBy(FTransform(Correction));

            UBoxComponent* Hit = NewObject<UBoxComponent>(&Actor, NAME_None, RF_Transient);
            Hit->InitBoxExtent(AlignedBounds.GetExtent());
            Hit->SetupAttachment(Actor.GetRootComponent());
            Hit->SetMobility(EComponentMobility::Movable);
            Hit->ComponentTags.Add(TrafficHitTag);
            Hit->OnComponentBeginOverlap.AddDynamic(this, &UMWTrackRuntimeComponent::OnTrigger);
            SetTriggerCollision(*Hit);
            Hit->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Actor.AddInstanceComponent(Hit);
            GeneratedComponents.Add(Hit);
            Hit->RegisterComponent();

            FTrafficSlot& Slot = Traffic.AddDefaulted_GetRef();
            Slot.Visual = Visual;
            Slot.Hitbox = Hit;
            Slot.BoundsCenter = AlignedBounds.GetCenter();
            Slot.VisualCorrection = Correction;
            Slot.LaneIndex = LaneIndex;
            Slot.OffsetSeconds = Lane.Radius > 0 ? DriveSeconds * Index / Lane.Slots : LaneIndex * 0.75;
        }
    }
}

// 현재 슬롯의 교통 외형과 충돌 표시를 함께 바꾼다.
void UMWTrackRuntimeComponent::SetTrafficActive(FTrafficSlot& Slot, bool bActive)
{
    if (bActive == Slot.bActive)
    {
        return;
    }
    Slot.bActive = bActive;
    if (AActor* Child = Slot.Visual.IsValid() ? Slot.Visual->GetChildActor() : nullptr)
    {
        Child->SetActorHiddenInGame(!bActive);
    }
    if (Slot.Hitbox.IsValid())
    {
        Slot.Hitbox->SetCollisionEnabled(bActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
    }
}

// 원본 차선 시간과 이동 상태를 사용해 교통 슬롯을 갱신한다.
void UMWTrackRuntimeComponent::UpdateTraffic(double Time)
{
    for (FTrafficSlot& Slot : Traffic)
    {
        const FMWTrackTrafficLaneData& Lane = Piece.TrafficLanes[Slot.LaneIndex];
        FTrafficLaneState& LaneState = TrafficLaneStates[Slot.LaneIndex];
        const double Phase = FMath::Fmod(Time + Slot.OffsetSeconds, LaneState.DriveSeconds + Lane.SpawnDelay);
        LaneState.Phase = Phase;
        if (Phase >= LaneState.DriveSeconds)
        {
            SetTrafficActive(Slot, false);
            continue;
        }

        FVector Position = Lane.Start.GetLocation();
        FVector Heading = Lane.Start.GetUnitAxis(EAxis::Y);
        if (Lane.Radius > 0)
        {
            const double Angle = Phase * Lane.Speed * Lane.Direction / Lane.Radius;
            double Sine, Cosine;
            FMath::SinCos(&Sine, &Cosine, Angle);
            Position += Lane.Start.TransformVectorNoScale(FVector(Cosine, Sine, 0) * Lane.Radius);
            Heading = Lane.Start.TransformVectorNoScale(FVector(-Sine, Cosine, 0) * Lane.Direction);
        }
        else
        {
            Position += Heading * (Phase * Lane.Speed);
        }
        const FQuat Rotation = FRotationMatrix::MakeFromXZ(Heading, Lane.Start.GetUnitAxis(EAxis::Z)).ToQuat();
        if (Slot.Visual.IsValid())
        {
            Slot.Visual->SetRelativeLocationAndRotation(Position, Rotation * Slot.VisualCorrection.Quaternion());
        }
        if (Slot.Hitbox.IsValid())
        {
            Slot.Hitbox->SetRelativeLocationAndRotation(Position + Rotation.RotateVector(Slot.BoundsCenter), Rotation);
        }
        SetTrafficActive(Slot, true);
    }
}

// 설정된 실제 트랙에만 원본 시간 기반 움직임·기차·롤러·교통을 갱신한다.
void UMWTrackRuntimeComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                             FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!bConfigured || bPreview || !IsValid(GetOwner()))
    {
        return;
    }
    const double Time = FMath::Max(0.0, GetRaceTime());
    float GateAlpha = 0.f;
    if (!Train.IsEmpty())
    {
        UpdateTrain(Time);
        const double Phase = FMath::Fmod(Time, static_cast<double>(Piece.TrainInterval));
        GateAlpha = static_cast<float>(FMath::Clamp(FMath::Min(4.0 - Phase, Phase + 0.25), 0.0, 1.0));
        SetTrainBell(GateAlpha > 0.f);
    }
    if (!Traffic.IsEmpty())
    {
        UpdateTraffic(Time);
    }
    UpdateLift(Time, DeltaTime);
    ApplyMotions(Time, GateAlpha);
    ApplyRollers();
}

// 실제 롤러에 닿은 차량에 기존 표면 속도 API로 이동 효과를 전달한다.
void UMWTrackRuntimeComponent::ApplyRollers()
{
    for (auto& Trigger : RiderTriggers)
    {
        FRiderTrigger& Entry = Trigger.Value;
        if (!Trigger.Key.IsValid() || Entry.Shape->Trigger != EMWTrackTrigger::Roller || Entry.Riders.IsEmpty())
            continue;
        const FVector Velocity = Trigger.Key->GetForwardVector() * Entry.Shape->Amount;
        for (auto It = Entry.Riders.CreateIterator(); It; ++It)
        {
            if (AMWVehiclePawn* Vehicle = It->Get())
            {
                if (Vehicle->HasAuthority() || Vehicle->IsLocallyControlled())
                {
                    CastChecked<UMWVehicleMovementComponent>(Vehicle->GetMovementComponent())
                        ->ApplyTrackSurfaceVelocity(Velocity, 0.10f);
                }
            }
            else
            {
                It.RemoveCurrent();
            }
        }
    }
}
