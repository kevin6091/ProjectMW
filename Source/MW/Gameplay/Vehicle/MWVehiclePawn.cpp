

#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "Gameplay/Vehicle/MWVehicleMovementComponent.h"
#include "Gameplay/Vehicle/MWVehicleSpringArmComponent.h"
#include "Gameplay/Vehicle/MWVehiclePresentationComponent.h"

#include "Components/BoxComponent.h"
#include "Components/MeshComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"
#include "System/GameState/MWGameplayGameState.h"
#include "System/MWPlayerState.h"
#include "System/MWDriveTestGameMode.h"
#include "LogChannel.h"

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
	PrimaryActorTick.bCanEverTick = false;

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

	DrivePresentation = CreateDefaultSubobject<USceneComponent>(TEXT("DrivePresentation"));
	DrivePresentation->SetupAttachment(CollisionRoot);
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
	const FMWGameplayProgress Progress = GS->GetProgress();
	const FMWPlayerRoundState Round = PS->GetRoundState();
	return Progress.Phase == EMWRoomPhase::Racing && Progress.RoundEpoch > 0
		&& Round.RoundEpoch == Progress.RoundEpoch && Round.RaceStatus == EMWRaceStatus::Active
		&& (HasAuthority() || (ServerDriveSnapshot.bDriveAllowed && ServerDriveSnapshot.RoundEpoch == Progress.RoundEpoch));
}

void AMWVehiclePawn::RefreshDrivePermission()
{
	const bool bAllowed = CanDrive();
	if (bDriveAllowed == bAllowed)
	{
		return;
	}
	bDriveAllowed = bAllowed;
	// 정지와 재출발 모두 이전 키 입력·속도·충돌 회전·예측 이력을 이어받지 않는다.
	DriveInput = FMWVehicleDriveInput{};
	VehicleMovement->ResetDriveInput();
	VehicleMovement->StopMovementImmediately();
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
	DriveInput = FMWVehicleDriveInput{};
	VehicleMovement->ResetDriveInput();
	VehicleMovement->StopMovementImmediately();
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
		&& DrivePresentation->IsRegistered() && DrivePresentation->GetAttachParent() == CollisionRoot
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
	Super::UnPossessed();
	RefreshDrivePermission();
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
		CameraBoom->AddTickPrerequisiteComponent(PresentationTick);
	}
	ResetDrivePresentation();
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
	DriveInput = FMWVehicleDriveInput();
	SubmitDriveInput(DriveInput);
	Super::EndPlay(EndPlayReason);
}
