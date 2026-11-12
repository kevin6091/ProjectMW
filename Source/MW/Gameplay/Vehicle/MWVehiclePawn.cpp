

#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "Gameplay/Items/MWItemComponent.h"
#include "Gameplay/Vehicle/MWVehicleMovementComponent.h"
#include "Gameplay/Vehicle/MWVehicleSpringArmComponent.h"
#include "Gameplay/Vehicle/Visual/MWVehicleSkidMarkComponent.h"

#include "CollisionQueryParams.h"
#include "Components/BoxComponent.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "GameFramework/SpringArmComponent.h"


namespace
{
	bool IsVisualBranch(USceneComponent* Component)
	{
		TArray<USceneComponent*> Parts;
		Component->GetChildrenComponents(true, Parts);
		Parts.Add(Component);

		bool bHasMesh = false;
		for (const USceneComponent* Part : Parts)
		{
			const UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Part);
			if (Part->GetOwner() != Component->GetOwner() || (Primitive && Primitive->IsCollisionEnabled()))
			{
				return false;
			}
			bHasMesh |= Part->IsA<UMeshComponent>();
		}
		return bHasMesh;
	}
}

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

	DrivePresentation = CreateDefaultSubobject<USceneComponent>(TEXT("DrivePresentation"));
	DrivePresentation->SetupAttachment(CollisionRoot);

	CameraBoom = CreateDefaultSubobject<UMWVehicleSpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(DrivePresentation);
	CameraBoom->TargetArmLength = 750.f;
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
	CachedVirtualSkidWheels.Reset();
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
	// BP 설정은 시작할 때 한 번 검증한다. Tick에서 컴포넌트를 찾거나 차종을 구분하지 않는다.
	for (const FMWVehicleVirtualSkidWheel& Wheel : VirtualSkidWheels)
	{
		if (Wheel.LocalCenter.ContainsNaN() || !FMath::IsFinite(Wheel.RadiusCm) || Wheel.RadiusCm <= KINDA_SMALL_NUMBER)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: 가상 바퀴 위치 또는 반지름이 유효하지 않아 자국 접지점에서 제외합니다."), *GetName());
			continue;
		}
		CachedVirtualSkidWheels.Add(Wheel);
	}
	if (!CachedVisualWheels.IsEmpty() || !CachedVirtualSkidWheels.IsEmpty())
	{
		AddTickPrerequisiteComponent(VehicleMovement);
	}
}

void AMWVehiclePawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdateWheelVisuals(DeltaSeconds);
}

void AMWVehiclePawn::UpdateWheelVisuals(float DeltaSeconds)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(MWVehicle_UpdateWheelVisuals);
    if (GetNetMode() == NM_DedicatedServer || (CachedVisualWheels.IsEmpty() && CachedVirtualSkidWheels.IsEmpty())) return;

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

	if (!VirtualSkidWheels.IsEmpty())
	{
		// 가상 바퀴는 메시 없이 Pawn 로컬 위치를 월드로 변환한다. 이동·충돌 판정에는 사용하지 않는다.
		const FTransform& PawnTransform = GetActorTransform();
		for (int32 Index = 0; Index < CachedVirtualSkidWheels.Num(); ++Index)
		{
			const FMWVehicleVirtualSkidWheel& Wheel = CachedVirtualSkidWheels[Index];
			SkidMarks->SampleWheel(Index, PawnTransform.TransformPosition(Wheel.LocalCenter), Wheel.RadiusCm * ActorScale, *DriveCollision);
		}
		return;
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
	// 이번 통합은 Kevin의 박스 충돌·접촉 예측 계약을 따른다.
	// DrivingCollisionMesh와 복구 에셋은 보존하되 루트나 UpdatedComponent를 교체하지 않는다.
	CollisionRoot->SetGenerateOverlapEvents(true);
}

// Called when the game starts or when spawned
void AMWVehiclePawn::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() != NM_DedicatedServer)
	{
		// 외형 그룹을 통째로 옮겨 바퀴·차체의 상대 위치를 유지한다. 충돌을 가진 그룹은 그대로 둔다.
		const TArray<TObjectPtr<USceneComponent>> RootChildren = CollisionRoot->GetAttachChildren();
		for (USceneComponent* Child : RootChildren)
		{
			if (IsValid(Child) && Child != DrivePresentation && IsVisualBranch(Child))
			{
				Child->AttachToComponent(DrivePresentation, FAttachmentTransformRules::KeepWorldTransform);
			}
		}
		if (CameraBoom->GetAttachParent() == CollisionRoot)
		{
			CameraBoom->AttachToComponent(DrivePresentation, FAttachmentTransformRules::KeepWorldTransform);
		}
		CameraBoom->AddTickPrerequisiteComponent(VehicleMovement);
	}
	ResetDrivePresentation();
	// 외형 계층과 표시 이력을 초기화한 뒤 바퀴·가상 접지점 설정을 한 번 캐시한다.
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
