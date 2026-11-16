

#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "Gameplay/Vehicle/MWVehicleMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Math/RotationMatrix.h"

// 충돌 루트와 Pawn 전용 이동 컴포넌트를 연결한다.
AMWVehiclePawn::AMWVehiclePawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

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

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(VisualRoot);
	CameraBoom->TargetArmLength = 550.f;
	CameraBoom->SetRelativeRotation(FRotator(-15.f, 0.f, 0.f));
	CameraBoom->SocketOffset = FVector(0.f, 0.f, 100.f);
	CameraBoom->bDoCollisionTest = false;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
}

void AMWVehiclePawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	UpdateGroundVisual(DeltaTime);
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
			// 실제 텔레포트 이후에는 이전 표시 위치·기울기를 이어 쓰지 않는다.
			ResetGroundVisual();
		}
	}
}

// Called when the game starts or when spawned
void AMWVehiclePawn::BeginPlay()
{
	Super::BeginPlay();
	
	// 실제 이동 계산 이후에 표시 상태를 갱신한다.
	AddTickPrerequisiteComponent(VehicleMovement);

	// 카메라는 표시 부모의 최종 변환을 사용한다.
	CameraBoom->AddTickPrerequisiteActor(this);

	ResetGroundVisual();
	
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

void AMWVehiclePawn::ResetGroundVisual()
{
	if (!CollisionRoot || !VisualRoot)
	{
		bGroundVisualInitialized = false;
		return;
	}
	
	const FTransform CollisionTransform = CollisionRoot->GetComponentTransform();
	
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
		!CollisionRoot ||
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
	
	const FTransform CollisionTransform = CollisionRoot->GetComponentTransform();
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

	VisualRoot->SetWorldLocationAndRotation(
		VisualLocation,
		VisualRotation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);

	// 다음 표시 프레임에서 사용할 상태.
	PreviousVisualWorldLocation = VisualLocation;
	PreviousCollisionTransform = CollisionTransform;
}
