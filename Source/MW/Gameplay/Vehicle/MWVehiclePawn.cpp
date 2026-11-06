

#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "Gameplay/Vehicle/MWVehicleMovementComponent.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Camera/CameraComponent.h"
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
    const double Distance = bVisualLocationValid && !bNewVisualStream ? FVector::DotProduct(Location - PreviousVisualLocation, GetActorForwardVector()) : 0;
    PreviousVisualLocation = Location;
    bVisualLocationValid = true;
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0 || !FMath::IsFinite(Distance)) return;

    // Use displayed displacement, so proxy interpolation and reverse motion roll correctly.
    // This preserves the existing steering source; the network snapshot has no steering field.
    const float Input = VehicleMovement ? VehicleMovement->GetSteeringInput() : 0.f;
    const double Steering = FMath::IsFinite(Input) ? FMath::Clamp(Input, -1.f, 1.f) : 0;
    const double ActorScale = GetActorScale3D().GetAbsMax();
    if (!FMath::IsFinite(ActorScale) || ActorScale <= KINDA_SMALL_NUMBER) return;
    for (FCachedVisualWheel& Wheel : CachedVisualWheels)
    {
        UStaticMeshComponent* Mesh = Wheel.Mesh.Get();
        if (!Mesh) continue;
        Wheel.RollRadians = FMath::Fmod(Wheel.RollRadians + Distance / (Wheel.RadiusCm * ActorScale), 2.0 * UE_DOUBLE_PI);
        FQuat Rotation = FQuat(Wheel.RollAxis, Wheel.RollRadians) * Wheel.RestRotation;
        // Do not gate this by speed: stationary left/right steering must remain visible.
        if (Wheel.bSteers) Rotation = FQuat(Wheel.SteeringAxis, Steering * Wheel.MaxSteeringRadians) * Rotation;
        Rotation.Normalize();
        if (!Mesh->GetRelativeRotation().Quaternion().Equals(Rotation, 1.e-6))
            Mesh->SetRelativeRotation(Rotation, false, nullptr, ETeleportType::None);
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
		bVisualLocationValid = false;
		DriveInput = FMWVehicleDriveInput{};
		if (HasActorBegunPlay())
		{
			ResetDriveStream();
		}
	}
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
