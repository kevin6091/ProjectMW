

#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "Gameplay/Items/MWItemComponent.h"
#include "Gameplay/Vehicle/MWVehicleMovementComponent.h"
#include "Gameplay/Vehicle/MWVehicleSpringArmComponent.h"
#include "Gameplay/Vehicle/MWVehiclePresentationComponent.h"
#include "Gameplay/Vehicle/Visual/MWVehicleSkidMarkComponent.h"

#include "CollisionQueryParams.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Components/SceneComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "Math/RotationMatrix.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "System/GameState/MWGameplayGameState.h"
#include "System/MWPlayerState.h"
#include "System/MWDriveTestGameMode.h"
#include "System/GameMode/MWGameplayGameMode.h"
#include "Gameplay/Track/MWTrackRuntimeSubsystem.h"
#include "LogChannel.h"

bool AMWVehiclePawn::IsVisualBranch(USceneComponent* Component)
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

// 충돌 루트와 Pawn 전용 이동 컴포넌트를 연결한다.
AMWVehiclePawn::AMWVehiclePawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	bReplicates = true;
	SetReplicateMovement(false);
	bUseControllerRotationYaw = false;

	CollisionRoot = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionRoot"));
	SetRootComponent(CollisionRoot);

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(CollisionRoot);
	VisualRoot->SetRelativeTransform(FTransform::Identity);

	CollisionRoot->InitBoxExtent(FVector(100.f, 50.f, 30.f));
	CollisionRoot->SetCollisionProfileName(TEXT("Pawn"));
	CollisionRoot->SetSimulatePhysics(false);

	VehicleMovement = CreateDefaultSubobject<UMWVehicleMovementComponent>(TEXT("VehicleMovement"));
	VehicleMovement->SetUpdatedComponent(CollisionRoot);
	ItemComponent = CreateDefaultSubobject<UMWItemComponent>(TEXT("Items"));

	DrivePresentation = CreateDefaultSubobject<USceneComponent>(TEXT("DrivePresentation"));
	DrivePresentation->SetupAttachment(VisualRoot);
	PresentationTick = CreateDefaultSubobject<UMWVehiclePresentationComponent>(TEXT("PresentationTick"));
	PresentationTick->AddTickPrerequisiteComponent(VehicleMovement);

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

// BP의 바퀴 참조·축·반지름을 한 번 검증해 캐시한다. 매 프레임 컴포넌트를 다시 찾지 않는다.
void AMWVehiclePawn::InitializeWheelVisuals()
{
    CachedVisualWheels.Reset();
    CachedVirtualSkidWheels.Reset();
    VisualSteering = 0;
    PreviousVisualLocation = GetActorLocation();
    bVisualLocationValid = false;
    VisualStreamId = ServerDriveSnapshot.StreamId;
    // 표시 콜백만 전용 서버에서 생략한다. 서버도 같은 바퀴 위치와 반지름으로 주행한다.
    if (RootComponent && GetNetMode() != NM_DedicatedServer)
    {
        RootComponent->TransformUpdated.Remove(VisualRootTransformHandle);
        VisualRootTransformHandle = RootComponent->TransformUpdated.AddWeakLambda(
            this,
            [this](USceneComponent*, EUpdateTransformFlags, ETeleportType Teleport)
            {
                // 일반 원격 보간도 TeleportPhysics를 사용하므로 원격 차량은 새 스트림일 때만
                // 표시 이력을 초기화한다. 로컬·서버의 실제 순간이동은 여기서 초기화한다.
                if (Teleport != ETeleportType::None && GetLocalRole() != ROLE_SimulatedProxy)
                    bVisualLocationValid = false;
            });
    }

    // 컴포넌트 참조는 초기화 때 한 번만 찾고 이후에는 소유 Pawn 수명 안에서 캐시를 사용한다.
    for (const FMWVehicleWheelVisual& Definition : WheelVisuals)
    {
        auto* Mesh = Cast<UStaticMeshComponent>(Definition.Mesh.GetComponent(this));
        const bool bValid = IsValid(Mesh) && Mesh->GetOwner() == this && FMath::IsFinite(Definition.RadiusCm) &&
                            Definition.RadiusCm > KINDA_SMALL_NUMBER && !Definition.RollAxis.ContainsNaN() &&
                            !Definition.RollAxis.IsNearlyZero() &&
                            (!Definition.bSteers ||
                             (!Definition.SteeringAxis.ContainsNaN() && !Definition.SteeringAxis.IsNearlyZero())) &&
                            FMath::IsFinite(Definition.MaxSteeringAngleDegrees) &&
                            Definition.MaxSteeringAngleDegrees >= 0 && Definition.MaxSteeringAngleDegrees <= 90;
        if (!bValid)
        {
            UE_LOG(LogTemp, Warning, TEXT("%s: invalid visual wheel %s; skipped"), *GetName(),
                   *Definition.Mesh.ComponentProperty.ToString());
            continue;
        }
        if (CachedVisualWheels.ContainsByPredicate(
                [Mesh](const auto& Wheel)
                {
                    return Wheel.Mesh == Mesh;
                }))
        {
            UE_LOG(LogTemp, Warning, TEXT("%s: duplicate visual wheel %s; skipped"), *GetName(), *Mesh->GetName());
            continue;
        }
        FCachedVisualWheel& Wheel = CachedVisualWheels.AddDefaulted_GetRef();
        Wheel.Mesh = Mesh;
        Wheel.RestRotation = Mesh->GetRelativeRotation().Quaternion();
        Wheel.RestLocation = Mesh->GetRelativeLocation();
        Wheel.RollAxis = Definition.RollAxis.GetSafeNormal();
        Wheel.SteeringAxis = Definition.SteeringAxis.GetSafeNormal();
        Wheel.RadiusCm = Definition.RadiusCm;
        Wheel.bSteers = Definition.bSteers;
        Wheel.MaxSteeringRadians = FMath::DegreesToRadians(Definition.MaxSteeringAngleDegrees);
    }
    bHasRearSkidWheels = CachedVisualWheels.ContainsByPredicate(
        [](const FCachedVisualWheel& Wheel)
        {
            return !Wheel.bSteers;
        });
    // BP 설정은 시작할 때 한 번 검증한다. Tick에서 컴포넌트를 찾거나 차종을 구분하지 않는다.
    for (const FMWVehicleVirtualSkidWheel& Wheel : VirtualSkidWheels)
    {
        if (Wheel.LocalCenter.ContainsNaN() || !FMath::IsFinite(Wheel.RadiusCm) || Wheel.RadiusCm <= KINDA_SMALL_NUMBER)
        {
            UE_LOG(LogTemp, Warning, TEXT("%s: 가상 바퀴 위치 또는 반지름이 유효하지 않아 자국 접지점에서 제외합니다."),
                   *GetName());
            continue;
        }
        CachedVirtualSkidWheels.Add(Wheel);
    }

    if (CachedVisualWheels.IsEmpty() && CachedVirtualSkidWheels.IsEmpty())
        return;

    AppliedDriveVehicleScale = DriveVehicleScale;
    // 표시 부모를 키우기 전에 현재 BP 메시에서 바퀴의 중심을 Pawn 로컬 좌표로 얻는다.
    const FTransform ToPawn = GetActorTransform();
    for (FCachedVisualWheel& Wheel : CachedVisualWheels)
    {
        const UStaticMeshComponent* Mesh = Wheel.Mesh.Get();
        const FVector Center = Mesh->GetStaticMesh()
            ? Mesh->GetComponentTransform().TransformPosition(Mesh->GetStaticMesh()->GetBounds().Origin)
            : Mesh->GetComponentLocation();
        Wheel.Collision = CreateWheelSuspensionCollision(ToPawn.InverseTransformPosition(Center), Wheel.RadiusCm);
    }

    if (CachedVisualWheels.IsEmpty())
    {
        // 외형이 없는 차량은 기존 가상 바퀴를 사용한다. 뒤쪽 바퀴만 있으면 앞쪽에도 지지점을 둔다.
        const bool bOnlyRear = !CachedVirtualSkidWheels.ContainsByPredicate(
            [](const FMWVehicleVirtualSkidWheel& Wheel) { return Wheel.LocalCenter.X >= 0.0; });
        for (const FMWVehicleVirtualSkidWheel& Wheel : CachedVirtualSkidWheels)
        {
            CreateWheelSuspensionCollision(Wheel.LocalCenter, Wheel.RadiusCm);
            if (bOnlyRear)
                CreateWheelSuspensionCollision(
                    FVector(-Wheel.LocalCenter.X, Wheel.LocalCenter.Y, Wheel.LocalCenter.Z), Wheel.RadiusCm);
        }
    }

    FVector Extent = CollisionRoot->GetUnscaledBoxExtent() * AppliedDriveVehicleScale;
    double TireBottomDepth = 0.0;
    for (const USphereComponent* Wheel : WheelSuspensionCollisions)
        TireBottomDepth = FMath::Max(TireBottomDepth, Wheel->GetUnscaledSphereRadius() - Wheel->GetRelativeLocation().Z);

    // 하부 박스가 바퀴 밑면을 덮으면 턱에 바퀴보다 차체가 먼저 걸린다.
    // 기본 확대율은 유지하고 차체 아래쪽에 작은 압축 여유가 생기도록 박스 높이를 맞춘다.
    constexpr double UnderbodyClearanceCm = 8.0;
    Extent.Z = FMath::Clamp(TireBottomDepth - UnderbodyClearanceCm, Extent.Z * 0.75, Extent.Z);
    CollisionRoot->SetBoxExtent(Extent, true);
    DrivePresentation->SetRelativeScale3D(FVector(AppliedDriveVehicleScale));
}

USphereComponent* AMWVehiclePawn::CreateWheelSuspensionCollision(const FVector& LocalCenter, double RadiusCm)
{
    USphereComponent* Wheel = NewObject<USphereComponent>(this, NAME_None, RF_Transient);
    Wheel->SetupAttachment(CollisionRoot);
    Wheel->SetRelativeLocation(LocalCenter * AppliedDriveVehicleScale);
    Wheel->InitSphereRadius(RadiusCm * AppliedDriveVehicleScale);
    Wheel->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    // 이 구체는 이동 컴포넌트의 노면 Sweep에서만 사용한다. 차량 간 충돌과 아이템 판정은 박스가 담당한다.
    Wheel->SetCollisionResponseToAllChannels(ECR_Ignore);
    Wheel->SetGenerateOverlapEvents(false);
    AddInstanceComponent(Wheel);
    WheelSuspensionCollisions.Add(Wheel);
    Wheel->RegisterComponent();
    return Wheel;
}

// 기존 Pawn 틱 뒤에 캐시한 바퀴·자국 표시만 갱신한다.
void AMWVehiclePawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
	if (HasAuthority() && CheckpointRecoveryEndServerTime > 0.0 && GetDriveServerTime() >= CheckpointRecoveryEndServerTime)
	{
		CheckpointRecoveryEndServerTime = 0.0;
		RefreshDrivePermission();
	}
	if (HasAuthority() && GetWorld()->GetTimeSeconds() >= NextRaceFailureCheck)
	{
		NextRaceFailureCheck = GetWorld()->GetTimeSeconds() + 0.25;
		if (auto* Mode = GetWorld()->GetAuthGameMode<AMWGameplayGameMode>()) Mode->CheckPlayerRaceFailure(*this);
	}
    // main의 차체 완충을 먼저 계산한 뒤 바퀴·자국 표시를 갱신한다.
    UpdateGroundVisual(DeltaSeconds);
    UpdateWheelVisuals(DeltaSeconds);
    UpdateExhaustEffect();
    UpdateEngineAudio(DeltaSeconds);
}

// 캐시한 시각 바퀴의 회전·조향만 갱신한다. 실제 이동 입력과 서버 스냅샷을 덮어쓰지 않는다.
void AMWVehiclePawn::UpdateWheelVisuals(float DeltaSeconds)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(MWVehicle_UpdateWheelVisuals);
    if (GetNetMode() == NM_DedicatedServer || (CachedVisualWheels.IsEmpty() && CachedVirtualSkidWheels.IsEmpty()))
        return;

    const FVector Location = GetActorLocation();
    if (Location.ContainsNaN())
    {
        bVisualLocationValid = false;
        return;
    }
    const bool bNewVisualStream = VisualStreamId != ServerDriveSnapshot.StreamId;
    VisualStreamId = ServerDriveSnapshot.StreamId;
    const bool bResetSkidTrail = !bVisualLocationValid || bNewVisualStream;
    const double Distance = bVisualLocationValid && !bNewVisualStream
                                ? FVector::DotProduct(Location - PreviousVisualLocation, GetActorForwardVector())
                                : 0;
    PreviousVisualLocation = Location;
    bVisualLocationValid = true;
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0 || !FMath::IsFinite(Distance))
        return;

    // 전후진과 원격 보간에 맞춰 바퀴가 돌도록 현재 코드의 변위 기준으로 회전량을 구한다.
    // 스냅샷에는 조향 전용 값이 없으므로 기존 조향 입력 조회 경로를 유지한다.
    const float Input = VehicleMovement ? VehicleMovement->GetSteeringInput() : 0.f;
    const double TargetSteering = FMath::IsFinite(Input) ? FMath::Clamp(Input, -1.f, 1.f) : 0;
    // 바퀴 반지름과 드리프트 자국에도 실제 표시 크기를 사용한다.
    const double ActorScale = GetActorScale3D().GetAbsMax() * AppliedDriveVehicleScale;
    if (!FMath::IsFinite(ActorScale) || ActorScale <= KINDA_SMALL_NUMBER)
        return;
    // 바퀴 수와 화면 갱신률에 영향을 받지 않도록 Pawn마다 한 번 조향을 보간한다.
    // 여기서는 바퀴 표시만 바꾸고 실제 이동 입력과 복제된 주행 상태는 수정하지 않는다.
    const double Alpha = FMath::IsFinite(WheelSteeringInterpSpeed) && WheelSteeringInterpSpeed > 0
                             ? 1.0 - FMath::Exp(-double(WheelSteeringInterpSpeed) * DeltaSeconds)
                             : 1.0;
    VisualSteering = FMath::Lerp(VisualSteering, TargetSteering, Alpha);
    if (FMath::IsNearlyEqual(VisualSteering, TargetSteering, 1.e-4))
        VisualSteering = TargetSteering;
    for (FCachedVisualWheel& Wheel : CachedVisualWheels)
    {
        UStaticMeshComponent* Mesh = Wheel.Mesh.Get();
        if (!Mesh)
            continue;
        const USphereComponent* Collision = Wheel.Collision.Get();
        const USceneComponent* Parent = Mesh->GetAttachParent();
        if (Collision && Parent)
        {
            // 충돌 루트에서 얻은 압축량을 표시 부모의 로컬 위치로 변환한다.
            FHitResult Contact;
            double Offset = 0.0;
            if (!VehicleMovement->TraceSuspensionWheel(*Collision, Contact, Offset))
                Offset = -VehicleMovement->DriveConfig.SuspensionReboundTravel * ActorScale;
            const FVector WorldOffset = CollisionRoot->GetUpVector() * Offset;
            const FVector LocalOffset = Parent->GetComponentTransform().InverseTransformVector(WorldOffset);
            Mesh->SetRelativeLocation(Wheel.RestLocation + LocalOffset);
        }

        Wheel.RollRadians =
            FMath::Fmod(Wheel.RollRadians + Distance / (Wheel.RadiusCm * ActorScale), 2.0 * UE_DOUBLE_PI);
        FQuat Rotation = FQuat(Wheel.RollAxis, Wheel.RollRadians) * Wheel.RestRotation;
        // 정지 상태의 좌우 조향도 보여야 하므로 속도가 0이라고 표시 갱신을 막지 않는다.
        if (Wheel.bSteers)
            Rotation = FQuat(Wheel.SteeringAxis, VisualSteering * Wheel.MaxSteeringRadians) * Rotation;
        Rotation.Normalize();
        if (!Mesh->GetRelativeRotation().Quaternion().Equals(Rotation, 1.e-6))
            Mesh->SetRelativeRotation(Rotation, false, nullptr, ETeleportType::None);
    }

    UpdateSkidMarks(bResetSkidTrail, ActorScale);
}

// 로컬 드리프트에 실제 바퀴 또는 BP 가상 접지점을 적용해 자국을 표시한다.
void AMWVehiclePawn::UpdateSkidMarks(bool bResetTrail, double ActorScale)
{
    // 신뢰할 수 있는 드리프트 입력을 가진 로컬 조종 Pawn의 자국만 표시한다.
    if (!IsLocallyControlled() || !VehicleMovement || !VehicleMovement->IsDriftControlActive())
    {
        if (SkidMarks)
            SkidMarks->ResetTrail();
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
            SkidMarks->SampleWheel(Index, PawnTransform.TransformPosition(Wheel.LocalCenter),
                                   Wheel.RadiusCm * ActorScale, *DriveCollision);
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
	RefreshDrivePermission();
	DriveInput = CanDrive() ? NewInput : FMWVehicleDriveInput{};
	if (UMWVehicleMovementComponent* Movement = Cast<UMWVehicleMovementComponent>(GetMovementComponent()))
	{
		Movement->SetDriveInput(DriveInput);
	}
}

bool AMWVehiclePawn::CanDrive() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* BaseState = World ? World->GetGameState() : nullptr;
	if (!IsValid(BaseState))
	{
		return false;
	}
	// 개발자 주행 맵만 명시적으로 자유 주행한다. GameState 미수신은 허용 사유가 아니다.
	if (BaseState->GameModeClass && BaseState->GameModeClass->IsChildOf(AMWDriveTestGameMode::StaticClass()))
	{
		return HasAuthority() || ServerDriveSnapshot.bDriveAllowed;
	}
	const auto* GS = Cast<AMWGameplayGameState>(BaseState);
	const auto* PS = GetPlayerState<AMWPlayerState>();
	const AController* Driver = GetController();
	if (!IsValid(GS) || !IsValid(PS) || PS->IsInactive() || !IsValid(Driver)
		|| Driver->GetPawn() != this || Driver->PlayerState != PS || !PS->IsOwnedBy(Driver))
	{
		return false;
	}
	const FMWGameplayProgress& Progress = GS->GetProgressRef();
	const FMWPlayerRoundState Round = PS->GetRoundState();
	return !bFinishCollisionDisabled && GetCheckpointRecoveryRemaining() <= 0.f
		&& Progress.Phase == EMWRoomPhase::Racing && Progress.RoundEpoch > 0
		&& Round.RoundEpoch == Progress.RoundEpoch && Round.bRaceParticipant && Round.RaceStatus == EMWRaceStatus::Active
		&& (HasAuthority() || (ServerDriveSnapshot.bDriveAllowed && ServerDriveSnapshot.RoundEpoch == Progress.RoundEpoch));
}

bool AMWVehiclePawn::SetFinishCollisionDisabled(bool bDisabled)
{
	if (bFinishCollisionDisabled == bDisabled)
	{
		return false;
	}
	bFinishCollisionDisabled = bDisabled;
	if (bDisabled)
	{
		CollisionBeforeFinish = CollisionRoot->GetCollisionEnabled();
		CollisionRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	else
	{
		CollisionRoot->SetCollisionEnabled(CollisionBeforeFinish);
	}
	return true;
}

void AMWVehiclePawn::RefreshDrivePermission()
{
	bool bCollisionChanged = false;
	if (HasAuthority())
	{
		const auto* GS = GetWorld()->GetGameState<AMWGameplayGameState>();
		const auto* PS = GetPlayerState<AMWPlayerState>();
		if (GS && PS && GS->GetProgressRef().RoundEpoch > 0
			&& PS->GetRoundState().RoundEpoch == GS->GetProgressRef().RoundEpoch
			&& (PS->GetRoundState().RaceStatus == EMWRaceStatus::Finished || PS->GetRoundState().RaceStatus == EMWRaceStatus::Eliminated
				|| !PS->GetRoundState().bRaceParticipant))
		{
			bCollisionChanged = SetFinishCollisionDisabled(true);
		}
	}
	else if (ServerDriveSnapshot.StreamId != 0)
	{
		if (bFinishCollisionDisabled && !ServerDriveSnapshot.bFinishCollisionDisabled)
		{
			// PlayerState/Progress의 수신 순서와 무관하게 새 위치부터 적용한 뒤 충돌을 켠다.
			if (VehicleMovement->UpdatedComponent != CollisionRoot)
			{
				return;
			}
			VehicleMovement->ApplyDriveSnapshot(ServerDriveSnapshot);
		}
		bCollisionChanged = SetFinishCollisionDisabled(ServerDriveSnapshot.bFinishCollisionDisabled);
	}
	const bool bAllowed = CanDrive();
	if (bDriveAllowed == bAllowed && !bCollisionChanged)
	{
		return;
	}
	bDriveAllowed = bAllowed;
	// 정지와 재출발 모두 이전 키 입력·속도·충돌 회전·예측 이력을 이어받지 않는다.
	DriveInput = FMWVehicleDriveInput{};
	VehicleMovement->ResetDriveInput();
	VehicleMovement->StopMovementImmediately();
	if (!bAllowed)
	{
		ItemComponent->ResetItemState();
	}
	DriveStepTime = 0.f;
	SendTime = 0.f;
	PredictedMoves.Reset();
	PredictedContactMotions.Reset();
	LastAckedContactMotions.Reset();
	bPredictionBufferFull = false;
	if (!HasAuthority())
	{
		AppliedStreamId = 0;
		AppliedRevision = 0;
	}
	ResetDriveStream();
	UE_LOG(LogMWVehicleNet, Log, TEXT("%s drive permission=%d stream=%u epoch=%d"),
		*GetName(), bDriveAllowed ? 1 : 0, ServerDriveSnapshot.StreamId, ServerDriveSnapshot.RoundEpoch);
}

// 기존 차량 시점에 부스터 표시용 카메라 변화를 적용하며 이동·서버 보정 값은 바꾸지 않는다.
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
    const FVector TargetLocation =
        OutResult.Location - OutResult.Rotation.Vector() * (BoostDollyDistance * CameraBlend);
    const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BoosterCameraDolly), false, this);
    FHitResult Hit;
    const bool bBlocked = GetWorld()->SweepSingleByChannel(
        Hit, OutResult.Location, TargetLocation, FQuat::Identity, CameraBoom->ProbeChannel,
        FCollisionShape::MakeSphere(CameraBoom->ProbeSize), QueryParams);
    OutResult.Location = bBlocked ? Hit.Location : TargetLocation;
}

UPawnMovementComponent* AMWVehiclePawn::GetMovementComponent() const
{
	return VehicleMovement;
}

void AMWVehiclePawn::PrepareForRaceFromServer()
{
	if (!HasAuthority())
	{
		return;
	}
	const auto* GS = GetWorld()->GetGameState<AMWGameplayGameState>();
	const auto* PS = GetPlayerState<AMWPlayerState>();
	if (GS && (GS->GetRoomPhase() != EMWRoomPhase::PreparingRace || !PS
		|| PS->GetRoundState().RoundEpoch != GS->GetProgressRef().RoundEpoch
		|| PS->GetRoundState().RaceStatus != EMWRaceStatus::Waiting))
	{
		return;
	}
	DriveInput = FMWVehicleDriveInput{};
	VehicleMovement->ResetDriveInput();
	VehicleMovement->StopMovementImmediately();
	// GameMode가 출발 위치를 적용한 뒤 호출한다. Waiting 복제만으로는 충돌을 복원하지 않는다.
	CheckpointRecoveryEndServerTime = 0.0;
	SetFinishCollisionDisabled(false);
	RefreshDrivePermission();
	// 이미 정지 중이어도 새 Epoch를 포함한 시작 위치를 반드시 공개한다.
	ResetDriveStream();
}

bool AMWVehiclePawn::IsReadyForRace(int32 RoundEpoch) const
{
	return RoundEpoch > 0 && HasActorBegunPlay() && !IsActorBeingDestroyed()
		&& CollisionRoot->IsRegistered() && CollisionRoot->IsQueryCollisionEnabled()
		&& CollisionRoot->IsPhysicsStateCreated() && !CollisionRoot->IsSimulatingPhysics()
		&& VehicleMovement->bDriveConfigReady && VehicleMovement->IsComponentTickEnabled()
		&& VehicleMovement->UpdatedComponent == CollisionRoot
		&& PresentationTick->IsRegistered() && (GetNetMode() == NM_DedicatedServer || PresentationTick->IsComponentTickEnabled())
		&& VisualRoot->IsRegistered() && VisualRoot->GetAttachParent() == CollisionRoot
		&& DrivePresentation->IsRegistered() && DrivePresentation->GetAttachParent() == VisualRoot
		&& CameraBoom->IsRegistered() && CameraBoom->IsActive() && CameraBoom->GetAttachParent() == DrivePresentation
		&& FollowCamera->IsRegistered() && FollowCamera->IsActive() && FollowCamera->GetAttachParent() == CameraBoom
		&& ServerDriveSnapshot.StreamId != 0 && ServerDriveSnapshot.RoundEpoch == RoundEpoch
		&& CollisionRoot->GetComponentLocation().Equals(ServerDriveSnapshot.Location, 0.2)
		&& CollisionRoot->GetComponentRotation().Equals(ServerDriveSnapshot.Rotation, 0.1)
		&& ServerDriveSnapshot.Velocity.IsNearlyZero(0.1) && FMath::IsNearlyZero(ServerDriveSnapshot.ImpactYawRate)
		&& !ServerDriveSnapshot.bDriveAllowed && !CanDrive();
}

void AMWVehiclePawn::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	RefreshDrivePermission();
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
	RefreshDrivePermission();
	if (HasActorBegunPlay())
	{
		ResetDriveStream();
	}
}

void AMWVehiclePawn::FellOutOfWorld(const UDamageType& DamageType)
{
	if (HasAuthority() && GetWorld()->GetAuthGameMode<AMWGameplayGameMode>() && CanDrive())
	{
		GetWorld()->GetSubsystem<UMWTrackRuntimeSubsystem>()->RespawnAtTrackCheckpoint(this);
		return;
	}
	Super::FellOutOfWorld(DamageType);
}

bool AMWVehiclePawn::RespawnAtCheckpoint(const FVector& Location, const FRotator& Rotation)
{
	if (!HasAuthority())
	{
		return false;
	}

	// 텔레포트 성공 콜백에서만 이 요청을 체크포인트 복귀로 구분하고 호출이 끝나면 플래그를 복원한다.
	TGuardValue<bool> RespawnGuard(bRespawningAtCheckpoint, true);
	return TeleportTo(Location, Rotation);
}

float AMWVehiclePawn::GetCheckpointRecoveryRemaining() const
{
    return CheckpointRecoveryEndServerTime > 0.0 && GetWorld()
        ? static_cast<float>(FMath::Max(0.0, CheckpointRecoveryEndServerTime - GetDriveServerTime())) : 0.f;
}

void AMWVehiclePawn::TeleportSucceeded(bool bIsATest)
{
	Super::TeleportSucceeded(bIsATest);
	if (!bIsATest)
	{
		// 첫 복귀 스냅샷에 이전 주행 방향의 속도가 실리지 않도록 성공 콜백에서 정지한다.
		if (bRespawningAtCheckpoint)
		{
			VehicleMovement->StopMovementImmediately();
		}

		ItemComponent->ResetItemState();
		BoosterCameraBlend = 0.f;
		bVisualLocationValid = false;
		CastChecked<UMWVehicleSpringArmComponent>(CameraBoom)->ResetCameraLag();
		DriveInput = FMWVehicleDriveInput{};
		if (HasActorBegunPlay())
		{
			const auto* State = GetWorld()->GetGameState<AMWGameplayGameState>();
			const bool bStartRecovery = HasAuthority() && bRespawningAtCheckpoint && State
				&& State->GetRoomPhase() == EMWRoomPhase::Racing
				&& FMath::IsFinite(CheckpointRecoverySeconds) && CheckpointRecoverySeconds > 0.f;
			if (bStartRecovery) CheckpointRecoveryEndServerTime = GetDriveServerTime() + CheckpointRecoverySeconds;
			const bool bWasAllowed = bDriveAllowed;
			if (bStartRecovery) RefreshDrivePermission();
			// 권한 갱신에서 스트림을 새로 열었으면 같은 위치를 두 번 전송하지 않는다.
			if (bWasAllowed == bDriveAllowed) ResetDriveStream();
			// 실제 텔레포트 이후에는 이전 표시 위치·기울기를 이어 쓰지 않는다.
			ResetGroundVisual();
		}
	}
}

// 현재 통합의 박스 이동·충돌 루트를 유지하고 기존 아이템 중첩 이벤트를 허용한다.
void AMWVehiclePawn::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    // 이번 통합은 main의 박스 충돌 루트를 따른다.
    // DrivingCollisionMesh와 복구 에셋은 보존하되 루트나 UpdatedComponent를 교체하지 않는다.
    CollisionRoot->SetGenerateOverlapEvents(true);
}

// Called when the game starts or when spawned
void AMWVehiclePawn::BeginPlay()
{
	Super::BeginPlay();
	// 기존 BP의 표시 부모 이름은 유지하면서 두 브랜치의 부착 관계를 같은 순서로 맞춘다.
	VisualRoot->AttachToComponent(CollisionRoot, FAttachmentTransformRules::KeepWorldTransform);
	DrivePresentation->AttachToComponent(VisualRoot, FAttachmentTransformRules::KeepWorldTransform);
	if (GetNetMode() != NM_DedicatedServer)
	{
		// 외형 그룹을 통째로 옮겨 바퀴·차체의 상대 위치를 유지한다. 충돌을 가진 그룹은 그대로 둔다.
		TArray<TObjectPtr<USceneComponent>> RootChildren = CollisionRoot->GetAttachChildren();
		RootChildren.Append(VisualRoot->GetAttachChildren());
		for (USceneComponent* Child : RootChildren)
		{
			if (IsValid(Child) && Child != DrivePresentation && Child != VisualRoot && IsVisualBranch(Child))
			{
				Child->AttachToComponent(DrivePresentation, FAttachmentTransformRules::KeepWorldTransform);
			}
		}
		if (CameraBoom->GetAttachParent() == CollisionRoot || CameraBoom->GetAttachParent() == VisualRoot)
		{
			CameraBoom->AttachToComponent(DrivePresentation, FAttachmentTransformRules::KeepWorldTransform);
		}
		CameraBoom->AddTickPrerequisiteComponent(PresentationTick);
	}
	ResetDrivePresentation();

	// 실제 이동 계산 이후에 표시 상태를 갱신한다.
	AddTickPrerequisiteComponent(PresentationTick);

	// 카메라는 표시 부모의 최종 변환을 사용한다.
	CameraBoom->AddTickPrerequisiteActor(this);

	ResetGroundVisual();

	InitializeWheelVisuals();
	if (HasAuthority())
	{
		// 기존 Blueprint에서 기본 이동 복제가 켜져 있어도 두 위치 갱신 경로가 경쟁하지 않게 한다.
		SetReplicateMovement(false);
		RefreshDrivePermission();
		ResetDriveStream();
	}
}

void AMWVehiclePawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopExhaustEffect();
    StopEngineAudio();
    if (RootComponent)
        RootComponent->TransformUpdated.Remove(VisualRootTransformHandle);
    VisualRootTransformHandle.Reset();
	DriveInput = FMWVehicleDriveInput();
	SubmitDriveInput(DriveInput);
	Super::EndPlay(EndPlayReason);
}

void AMWVehiclePawn::ResetGroundVisual()
{
	if (!DrivePresentation || !VisualRoot)
	{
		bGroundVisualInitialized = false;
		return;
	}

	const FTransform CollisionTransform = DrivePresentation->GetComponentTransform();

	PreviousVisualWorldLocation = CollisionTransform.GetLocation();
	SmoothedVisualUp = CollisionTransform.GetRotation().GetAxisZ();

	VisualHeightOffset = 0.0;

	PreviousCollisionTransform = CollisionTransform;

	// VisualRoot는 아트 배치 오프셋을 갖지 않는 부모로 사용한다.
	// 메시의 기존 배치 오프셋은 메시 쪽에 유지한다.
	VisualRoot->SetRelativeTransform(FTransform::Identity);

	bGroundVisualInitialized = true;
}

void AMWVehiclePawn::UpdateGroundVisual(float DeltaTime)
{
	if (GetNetMode() == NM_DedicatedServer ||
		!DrivePresentation ||
		!VisualRoot ||
		!VehicleMovement ||
		!FMath::IsFinite(DeltaTime) ||
		DeltaTime <= 0.f)
	{
		return;
	}

	if (!bGroundVisualInitialized)
	{
		ResetGroundVisual();
		return;
	}

	const FTransform CollisionTransform = DrivePresentation->GetComponentTransform();
	const FVector RootLocation = CollisionTransform.GetLocation();
	const FQuat RootRotation = CollisionTransform.GetRotation();
	const FVector RootUp = RootRotation.GetAxisZ();

	// 큰 위치·회전 변화는 과거 표시 상태에서 이어서 보간하지 않는다.
	const double PositionChangeSquared = FVector::DistSquared(PreviousCollisionTransform.GetLocation(), RootLocation);
	const double RotationChange = PreviousCollisionTransform.GetRotation().AngularDistance(RootRotation);
	const bool bLargePositionChange = PositionChangeSquared > FMath::Square(static_cast<double>(VisualResetDistance));
	const bool bLargeRotationChange = RotationChange > FMath::DegreesToRadians(static_cast<double>(VisualResetAngle));

	if (bLargePositionChange || bLargeRotationChange)
	{
		ResetGroundVisual();
		return;
	}

	// ── 높이 완충 ──
	const double HeightDecay = FMath::Exp(-FMath::Max(VisualHeightResponse, 0.1f) * DeltaTime);

	if (VehicleMovement->IsGrounded())
	{
		// 이전 표시 위치가 현재 충돌체에서 얼마나 떨어져 있는지,
		// 현재 차체의 위쪽 축으로만 측정한다.
		VisualHeightOffset = FVector::DotProduct(PreviousVisualWorldLocation - RootLocation, RootUp);
	}

	// 공중에서는 새로운 높이 오차를 만들지 않고,
	// 남아 있던 표시 오프셋만 0으로 감쇠시킨다.
	VisualHeightOffset = FMath::Clamp(
		VisualHeightOffset * HeightDecay,
		-static_cast<double>(MaxVisualHeightOffset),
		static_cast<double>(MaxVisualHeightOffset));

	const FVector VisualLocation = RootLocation + RootUp * VisualHeightOffset;

	// ── 기울기 완충 ──
	const double TiltAlpha = 1.0 - FMath::Exp(-FMath::Max(VisualTiltResponse, 0.1f) * DeltaTime);
	const FQuat TowardRootUp = FQuat::FindBetweenNormals(SmoothedVisualUp, RootUp);

	SmoothedVisualUp = FQuat::Slerp(FQuat::Identity, TowardRootUp, TiltAlpha).RotateVector(SmoothedVisualUp).GetSafeNormal();

	// 실제 차체와의 기울기 차이를 제한한다.
	const FQuat TiltError = FQuat::FindBetweenNormals(RootUp, SmoothedVisualUp);
	const double ErrorAngle = TiltError.GetAngle();
	const double MaxAngle = FMath::DegreesToRadians(static_cast<double>(MaxVisualTiltLagAngle));

	if (ErrorAngle > MaxAngle && ErrorAngle > SMALL_NUMBER)
	{
		SmoothedVisualUp = FQuat::Slerp(
			FQuat::Identity,
			TiltError,
			MaxAngle / ErrorAngle)
			.RotateVector(RootUp)
			.GetSafeNormal();
	}

	// 조향 방향 전체를 시간 보간하지 않고,
	// 현재 전방을 표시 기울기 평면에 투영한다.
	const FVector VisualForward = FVector::VectorPlaneProject(RootRotation.GetAxisX(), SmoothedVisualUp).GetSafeNormal();

	if (VisualForward.IsNearlyZero())
	{
		ResetGroundVisual();
		return;
	}

	const FQuat VisualRotation = FRotationMatrix::MakeFromXZ(VisualForward, SmoothedVisualUp).ToQuat();

	// 이미 계산된 네트워크 표시 위치에 노면 완충을 합성한다. 부모를 다시 움직여 보정을 덮어쓰지 않는다.
	DrivePresentation->SetWorldLocationAndRotation(
		VisualLocation,
		VisualRotation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);

	// 다음 표시 프레임에서 사용할 상태.
	PreviousVisualWorldLocation = VisualLocation;
	PreviousCollisionTransform = CollisionTransform;
}
