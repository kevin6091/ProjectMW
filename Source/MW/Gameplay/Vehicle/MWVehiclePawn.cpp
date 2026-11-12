

#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "Gameplay/Vehicle/MWVehicleMovementComponent.h"
#include "Gameplay/Vehicle/MWVehicleSpringArmComponent.h"
#include "Gameplay/Vehicle/MWVehiclePresentationComponent.h"

#include "Components/BoxComponent.h"
#include "Components/MeshComponent.h"
#include "Camera/CameraComponent.h"
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
		ResetDriveStream();
	}
}

void AMWVehiclePawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DriveInput = FMWVehicleDriveInput();
	SubmitDriveInput(DriveInput);
	Super::EndPlay(EndPlayReason);
}
