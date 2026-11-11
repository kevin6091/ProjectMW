

#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "Gameplay/Items/MWItemComponent.h"
#include "Gameplay/Vehicle/MWVehicleMovementComponent.h"
#include "Gameplay/Vehicle/Visual/MWVehicleSkidMarkComponent.h"

#include "CollisionQueryParams.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "PhysicsEngine/BodySetup.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "GameFramework/SpringArmComponent.h"


// 충돌 루트와 Pawn 전용 이동 컴포넌트를 연결한다.
AMWVehiclePawn::AMWVehiclePawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	bReplicates = true;
	SetReplicateMovement(false);
	bUseControllerRotationYaw = false;

	CollisionRoot = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionRoot"));
	SetRootComponent(CollisionRoot);

	CollisionRoot->InitBoxExtent(FVector(100.f, 50.f, 30.f));
	CollisionRoot->SetCollisionProfileName(TEXT("Pawn"));
	CollisionRoot->SetSimulatePhysics(false);

	VehicleMovement = CreateDefaultSubobject<UMWVehicleMovementComponent>(TEXT("VehicleMovement"));
	VehicleMovement->SetUpdatedComponent(CollisionRoot);
	ItemComponent = CreateDefaultSubobject<UMWItemComponent>(TEXT("Items"));

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 550.f;
	CameraBoom->SetRelativeRotation(FRotator(-15.f, 0.f, 0.f));
	CameraBoom->SocketOffset = FVector(0.f, 0.f, 100.f);
	CameraBoom->bDoCollisionTest = false;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

}

void AMWVehiclePawn::InitializeWheelVisuals()
{
    CachedVisualWheels.Reset();
    VisualSteering = 0;
    PreviousVisualLocation = GetActorLocation();
    bVisualLocationValid = false;
    VisualStreamId = ServerDriveSnapshot.StreamId;
    if (GetNetMode() == NM_DedicatedServer) return;

    if (RootComponent)
    {
        RootComponent->TransformUpdated.Remove(VisualRootTransformHandle);
        VisualRootTransformHandle = RootComponent->TransformUpdated.AddWeakLambda(this,
            [this](USceneComponent*, EUpdateTransformFlags, ETeleportType Teleport)
            {
                // Ordinary proxy interpolation also uses TeleportPhysics. Only a new
                // stream resets a simulated proxy; local/authority teleports reset here.
                if (Teleport != ETeleportType::None && GetLocalRole() != ROLE_SimulatedProxy)
                    bVisualLocationValid = false;
            });
    }

    // Resolve component references once; the owner already controls their lifetime.
    for (const FMWVehicleWheelVisual& Definition : WheelVisuals)
    {
        auto* Mesh = Cast<UStaticMeshComponent>(Definition.Mesh.GetComponent(this));
        const bool bValid = IsValid(Mesh) && Mesh->GetOwner() == this &&
            FMath::IsFinite(Definition.RadiusCm) && Definition.RadiusCm > KINDA_SMALL_NUMBER &&
            !Definition.RollAxis.ContainsNaN() && !Definition.RollAxis.IsNearlyZero() &&
            (!Definition.bSteers || (!Definition.SteeringAxis.ContainsNaN() && !Definition.SteeringAxis.IsNearlyZero())) &&
            FMath::IsFinite(Definition.MaxSteeringAngleDegrees) &&
            Definition.MaxSteeringAngleDegrees >= 0 && Definition.MaxSteeringAngleDegrees <= 90;
        if (!bValid)
        {
            UE_LOG(LogTemp, Warning, TEXT("%s: invalid visual wheel %s; skipped"), *GetName(), *Definition.Mesh.ComponentProperty.ToString());
            continue;
        }
        if (CachedVisualWheels.ContainsByPredicate([Mesh](const auto& Wheel) { return Wheel.Mesh == Mesh; }))
        {
            UE_LOG(LogTemp, Warning, TEXT("%s: duplicate visual wheel %s; skipped"), *GetName(), *Mesh->GetName());
            continue;
        }
        FCachedVisualWheel& Wheel = CachedVisualWheels.AddDefaulted_GetRef();
        Wheel.Mesh = Mesh;
        Wheel.RestRotation = Mesh->GetRelativeRotation().Quaternion();
        Wheel.RollAxis = Definition.RollAxis.GetSafeNormal();
        Wheel.SteeringAxis = Definition.SteeringAxis.GetSafeNormal();
        Wheel.RadiusCm = Definition.RadiusCm;
        Wheel.bSteers = Definition.bSteers;
        Wheel.MaxSteeringRadians = FMath::DegreesToRadians(Definition.MaxSteeringAngleDegrees);
    }
    bHasRearSkidWheels = CachedVisualWheels.ContainsByPredicate(
        [](const FCachedVisualWheel& Wheel) { return !Wheel.bSteers; });
    if (!CachedVisualWheels.IsEmpty()) AddTickPrerequisiteComponent(VehicleMovement);
}

void AMWVehiclePawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdateWheelVisuals(DeltaSeconds);
}

void AMWVehiclePawn::UpdateWheelVisuals(float DeltaSeconds)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(MWVehicle_UpdateWheelVisuals);
    if (GetNetMode() == NM_DedicatedServer || CachedVisualWheels.IsEmpty()) return;

    const FVector Location = GetActorLocation();
    if (Location.ContainsNaN()) { bVisualLocationValid = false; return; }
    const bool bNewVisualStream = VisualStreamId != ServerDriveSnapshot.StreamId;
    VisualStreamId = ServerDriveSnapshot.StreamId;
    const bool bResetSkidTrail = !bVisualLocationValid || bNewVisualStream;
    const double Distance = bVisualLocationValid && !bNewVisualStream ? FVector::DotProduct(Location - PreviousVisualLocation, GetActorForwardVector()) : 0;
    PreviousVisualLocation = Location;
    bVisualLocationValid = true;
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0 || !FMath::IsFinite(Distance)) return;

    // Use displayed displacement, so proxy interpolation and reverse motion roll correctly.
    // This preserves the existing steering source; the network snapshot has no steering field.
    const float Input = VehicleMovement ? VehicleMovement->GetSteeringInput() : 0.f;
    const double TargetSteering = FMath::IsFinite(Input) ? FMath::Clamp(Input, -1.f, 1.f) : 0;
    const double ActorScale = GetActorScale3D().GetAbsMax();
    if (!FMath::IsFinite(ActorScale) || ActorScale <= KINDA_SMALL_NUMBER) return;
    // Smooth once per Pawn, independent of wheel count and render frame rate.
    // The actual movement input and replicated driving state remain untouched.
    const double Alpha = FMath::IsFinite(WheelSteeringInterpSpeed) && WheelSteeringInterpSpeed > 0
        ? 1.0 - FMath::Exp(-double(WheelSteeringInterpSpeed) * DeltaSeconds) : 1.0;
    VisualSteering = FMath::Lerp(VisualSteering, TargetSteering, Alpha);
    if (FMath::IsNearlyEqual(VisualSteering, TargetSteering, 1.e-4)) VisualSteering = TargetSteering;
    for (FCachedVisualWheel& Wheel : CachedVisualWheels)
    {
        UStaticMeshComponent* Mesh = Wheel.Mesh.Get();
        if (!Mesh) continue;
        Wheel.RollRadians = FMath::Fmod(Wheel.RollRadians + Distance / (Wheel.RadiusCm * ActorScale), 2.0 * UE_DOUBLE_PI);
        FQuat Rotation = FQuat(Wheel.RollAxis, Wheel.RollRadians) * Wheel.RestRotation;
        // Do not gate this by speed: stationary left/right steering must remain visible.
        if (Wheel.bSteers) Rotation = FQuat(Wheel.SteeringAxis, VisualSteering * Wheel.MaxSteeringRadians) * Rotation;
        Rotation.Normalize();
        if (!Mesh->GetRelativeRotation().Quaternion().Equals(Rotation, 1.e-6))
            Mesh->SetRelativeRotation(Rotation, false, nullptr, ETeleportType::None);
    }

    UpdateSkidMarks(bResetSkidTrail, ActorScale);
}

void AMWVehiclePawn::UpdateSkidMarks(bool bResetTrail, double ActorScale)
{
    // Only the local Pawn has a trustworthy drift input and movement state.
    if (!IsLocallyControlled() || !VehicleMovement || !VehicleMovement->IsDriftControlActive())
    {
        if (SkidMarks) SkidMarks->ResetTrail();
        return;
    }

    const UPrimitiveComponent* DriveCollision = Cast<UPrimitiveComponent>(RootComponent);
    if (!DriveCollision)
    {
        return;
    }

    if (!SkidMarks)
    {
        SkidMarks = NewObject<UMWVehicleSkidMarkComponent>(this, TEXT("SkidMarks"), RF_Transient);
        SkidMarks->RegisterComponent();
    }
    if (bResetTrail)
    {
        SkidMarks->ResetTrail();
    }

    for (int32 Index = 0; Index < CachedVisualWheels.Num(); ++Index)
    {
        const FCachedVisualWheel& Wheel = CachedVisualWheels[Index];
        if (const UStaticMeshComponent* Mesh = Wheel.Mesh.Get(); Mesh && (!bHasRearSkidWheels || !Wheel.bSteers))
        {
            SkidMarks->SampleWheel(Index, Mesh->GetComponentLocation(), Wheel.RadiusCm * ActorScale, *DriveCollision);
        }
    }
}

void AMWVehiclePawn::SubmitDriveInput(const FMWVehicleDriveInput& NewInput)
{
	DriveInput = NewInput;
	if (UMWVehicleMovementComponent* Movement = Cast<UMWVehicleMovementComponent>(GetMovementComponent()))
	{
		Movement->SetDriveInput(NewInput);
	}
}

void AMWVehiclePawn::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	Super::CalcCamera(DeltaTime, OutResult);
	if (!IsLocallyControlled())
	{
		BoosterCameraBlend = 0.f;
		return;
	}

	const float TimeRemaining = ItemComponent->GetBoosterTimeRemaining();
	constexpr float PullbackSeconds = 1.40f;
	constexpr float ReturnSeconds = 0.36f;
	const float TargetBlend = FMath::Clamp(TimeRemaining / ReturnSeconds, 0.f, 1.f);
	const float BlendSeconds = TargetBlend > BoosterCameraBlend ? PullbackSeconds : ReturnSeconds;
	BoosterCameraBlend = FMath::FInterpConstantTo(BoosterCameraBlend, TargetBlend, DeltaTime, 1.f / BlendSeconds);
	const float CameraBlend = FMath::SmoothStep(0.f, 1.f, BoosterCameraBlend);
	if (CameraBlend <= 0.005f)
	{
		return;
	}

	constexpr float BoostDollyDistance = 190.f;
	const FVector TargetLocation = OutResult.Location - OutResult.Rotation.Vector() * (BoostDollyDistance * CameraBlend);
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BoosterCameraDolly), false, this);
	FHitResult Hit;
	const bool bBlocked = GetWorld()->SweepSingleByChannel(Hit, OutResult.Location, TargetLocation,
		FQuat::Identity, CameraBoom->ProbeChannel, FCollisionShape::MakeSphere(CameraBoom->ProbeSize), QueryParams);
	OutResult.Location = bBlocked ? Hit.Location : TargetLocation;
}

UPawnMovementComponent* AMWVehiclePawn::GetMovementComponent() const
{
	return VehicleMovement;
}

void AMWVehiclePawn::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	if (HasActorBegunPlay())
	{
		ResetDriveStream();
	}
}

void AMWVehiclePawn::UnPossessed()
{
	DriveInput = FMWVehicleDriveInput{};
	VehicleMovement->ResetDriveInput();
	ItemComponent->ResetItemState();
	BoosterCameraBlend = 0.f;
	Super::UnPossessed();
	if (HasActorBegunPlay())
	{
		ResetDriveStream();
	}
}

void AMWVehiclePawn::TeleportSucceeded(bool bIsATest)
{
	Super::TeleportSucceeded(bIsATest);
	if (!bIsATest)
	{
		ItemComponent->ResetItemState();
		BoosterCameraBlend = 0.f;
		bVisualLocationValid = false;
		DriveInput = FMWVehicleDriveInput{};
		if (HasActorBegunPlay())
		{
			ResetDriveStream();
		}
	}
}

void AMWVehiclePawn::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	CollisionRoot->SetGenerateOverlapEvents(true);

	if (!DrivingCollisionMesh)
	{
		return;
	}

	const UBodySetup* BodySetup = DrivingCollisionMesh->GetBodySetup();
	if (!BodySetup || BodySetup->CollisionTraceFlag == CTF_UseComplexAsSimple ||
		BodySetup->AggGeom.ConvexElems.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: Driving collision requires simple convex geometry."), *GetName());
		return;
	}

	auto* Hull = NewObject<UStaticMeshComponent>(this, TEXT("DrivingCollisionHull"), RF_Transient);
	Hull->SetStaticMesh(DrivingCollisionMesh);
	Hull->SetMobility(EComponentMobility::Movable);
	Hull->SetHiddenInGame(true);
	Hull->SetCastShadow(false);
	Hull->SetCollisionProfileName(CollisionRoot->GetCollisionProfileName());
	Hull->SetCollisionObjectType(CollisionRoot->GetCollisionObjectType());
	Hull->SetCollisionResponseToChannels(CollisionRoot->GetCollisionResponseToChannels());
	Hull->SetCollisionEnabled(CollisionRoot->GetCollisionEnabled());
	Hull->SetGenerateOverlapEvents(CollisionRoot->GetGenerateOverlapEvents());
	Hull->SetWorldTransform(CollisionRoot->GetComponentTransform());
	Hull->RegisterComponent();

	SetRootComponent(Hull);
	// Preserve the authored camera, mesh hierarchy, and relative wheel transforms.
	CollisionRoot->AttachToComponent(Hull, FAttachmentTransformRules::KeepWorldTransform);
	CollisionRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VehicleMovement->SetUpdatedComponent(Hull);
}

// Called when the game starts or when spawned
void AMWVehiclePawn::BeginPlay()
{
	Super::BeginPlay();
	InitializeWheelVisuals();
	if (HasAuthority())
	{
		// 기존 Blueprint에서 기본 이동 복제가 켜져 있어도 두 위치 갱신 경로가 경쟁하지 않게 한다.
		SetReplicateMovement(false);
		ResetDriveStream();
	}
}

void AMWVehiclePawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (RootComponent) RootComponent->TransformUpdated.Remove(VisualRootTransformHandle);
    VisualRootTransformHandle.Reset();
	DriveInput = FMWVehicleDriveInput();
	SubmitDriveInput(DriveInput);
	Super::EndPlay(EndPlayReason);
}
