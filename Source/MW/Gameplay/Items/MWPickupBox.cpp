// 서버가 실제 차량의 지급 가능 여부를 검사하는 아이템 상자의 획득 수명을 관리한다.

#include "Gameplay/Items/MWPickupBox.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Gameplay/Items/MWCoursePickupSettings.h"
#include "Gameplay/Items/MWItemComponent.h"
#include "LogChannel.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AMWPickupBox::AMWPickupBox()
{
    bReplicates = true;
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    PrimaryActorTick.bAllowTickOnDedicatedServer = false;

    Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("PickupTrigger"));
    SetRootComponent(Trigger);
    Trigger->InitBoxExtent(FVector(55.0, 55.0, 70.0));
    Trigger->SetCollisionObjectType(ECC_WorldDynamic);
    Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
    Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    Trigger->SetGenerateOverlapEvents(true);
    Trigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    PresentationRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PickupPresentation"));
    PresentationRoot->SetupAttachment(Trigger);
    Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupVisual"));
    LidVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupLid"));
    HandleVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupHandle"));
    for (UStaticMeshComponent* Part : {Visual.Get(), LidVisual.Get(), HandleVisual.Get()})
    {
        Part->SetupAttachment(PresentationRoot);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    Visual->SetRelativeScale3D(FVector(0.8));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (Cube.Succeeded())
    {
        Visual->SetStaticMesh(Cube.Object);
    }
}

void AMWPickupBox::ApplyDefaultPresentation()
{
    // 레벨과 BP에 지정한 외형이 먼저다. 런타임 기본 상자만 공용 설정에서 채운다.
    if (LidVisual->GetStaticMesh() || HandleVisual->GetStaticMesh()
        || Visual->GetStaticMesh() != GetDefault<ThisClass>()->Visual->GetStaticMesh())
    {
        return;
    }

    const auto& Settings = *GetDefault<UMWCoursePickupSettings>();
    UStaticMesh* BodyMesh = Settings.Body.Mesh.LoadSynchronous();
    UStaticMesh* LidMesh = Settings.Lid.Mesh.LoadSynchronous();
    if (!BodyMesh || !LidMesh)
    {
        return;
    }
    const auto ApplyPart = [](UStaticMeshComponent& Part, UStaticMesh* Mesh, const FMWPickupVisualPart& Data)
    {
        Part.SetStaticMesh(Mesh);
        Part.SetRelativeTransform(FTransform(Data.Rotation, Data.Offset, FVector(Data.Scale)));
    };
    ApplyPart(*Visual, BodyMesh, Settings.Body);
    ApplyPart(*LidVisual, LidMesh, Settings.Lid);
    ApplyPart(*HandleVisual, Settings.Handle.Mesh.LoadSynchronous(), Settings.Handle);
}

void AMWPickupBox::BeginPlay()
{
    Super::BeginPlay();

    if (GetNetMode() != NM_DedicatedServer)
    {
        // 기존 맵은 일부 부품을 충돌 루트에 직접 연결해 저장했다. 월드 자세를 보존하면서 연출 루트로 옮긴다.
        for (UStaticMeshComponent* Part : {Visual.Get(), LidVisual.Get(), HandleVisual.Get()})
        {
            if (Part->GetAttachParent() != PresentationRoot)
            {
                Part->AttachToComponent(PresentationRoot, FAttachmentTransformRules::KeepWorldTransform);
            }
        }
        ApplyDefaultPresentation();
        RestPresentation = PresentationRoot->GetRelativeTransform();
        RestLid = LidVisual->GetRelativeTransform();
        RestHandle = HandleVisual->GetRelativeTransform();
        bPresentationInitialized = true;
        SetPresentationVisible(bAvailable);
        if (bAvailable)
        {
            IdleStartedAt = GetWorld()->GetTimeSeconds();
            UpdateIdlePresentation();
            SetActorTickEnabled(true);
        }
    }

    if (HasAuthority())
    {
        Trigger->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::HandleOverlap);
    }
    RefreshPickupCollision();
}

void AMWPickupBox::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bAvailable)
    {
        UpdateIdlePresentation();
    }
    else
    {
        UpdateOpeningPresentation();
    }
}

// 서버가 확정한 기존 획득 가능 상태를 클라이언트 표시에서도 사용한다.
void AMWPickupBox::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AMWPickupBox, bAvailable);
}

void AMWPickupBox::SetItemPool(const TArray<EMWItemId>& Items)
{
    if (HasAuthority())
    {
        ItemPool = Items;
    }
}

// 서버에서만 실제 차량의 지급 가능 여부를 검사하고 성공 시 상자를 비활성화한다.
void AMWPickupBox::HandleOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool,
                                 const FHitResult&)
{
    if (!HasAuthority() || !bAvailable)
    {
        return;
    }
    AMWVehiclePawn* Vehicle = Cast<AMWVehiclePawn>(OtherActor);
    UMWItemComponent* Items = Vehicle ? Vehicle->FindComponentByClass<UMWItemComponent>() : nullptr;
    if (!Items || Items->GetCurrentItemId() != EMWItemId::None || ItemPool.IsEmpty())
    {
        return;
    }

    const int32 First = FMath::RandHelper(ItemPool.Num());
    for (int32 Offset = 0; Offset < ItemPool.Num(); ++Offset)
    {
        const EMWItemId Item = ItemPool[(First + Offset) % ItemPool.Num()];
        if (!Items->TryGrantItem(Item))
        {
            continue;
        }
        bAvailable = false;
        UpdateAvailability();
        ForceNetUpdate();
        UE_LOG(LogMWServer, Log, TEXT("[MWPickup] box=%s vehicle=%s location=%s speed=%.1f item=%s"),
            *GetName(), *GetNameSafe(Vehicle), *Vehicle->GetActorLocation().ToCompactString(), Vehicle->GetVelocity().Size2D(),
            *StaticEnum<EMWItemId>()->GetNameStringByValue(static_cast<int64>(Item)));
        return;
    }
}

void AMWPickupBox::UpdateAvailability()
{
    RefreshPickupCollision();
    if (!bPresentationInitialized)
    {
        return;
    }

    if (bAvailable)
    {
        PresentationRoot->SetRelativeTransform(RestPresentation);
        LidVisual->SetRelativeTransform(RestLid);
        HandleVisual->SetRelativeTransform(RestHandle);
        IdleStartedAt = GetWorld()->GetTimeSeconds();
        SetPresentationVisible(true);
        UpdateIdlePresentation();
        SetActorTickEnabled(true);
        return;
    }

    // 메시 하나로 구성된 테스트 상자는 열림 자세 없이 기존처럼 즉시 숨긴다.
    if (!LidVisual->GetStaticMesh() || OpeningDuration <= 0.f)
    {
        SetPresentationVisible(false);
        SetActorTickEnabled(false);
        return;
    }

    OpeningStartedAt = GetWorld()->GetTimeSeconds();
    SetActorTickEnabled(true);
}

void AMWPickupBox::RefreshPickupCollision()
{
    // 렌더링이 계속되는 동안에도 서버 획득 트리거는 즉시 꺼야 한다.
    Trigger->SetCollisionEnabled(HasAuthority() && bAvailable ? ECollisionEnabled::QueryOnly
                                                              : ECollisionEnabled::NoCollision);
}

void AMWPickupBox::SetPresentationVisible(bool bVisible)
{
    PresentationRoot->SetVisibility(bVisible, true);
}

void AMWPickupBox::UpdateIdlePresentation()
{
    const double Seconds = GetWorld()->GetTimeSeconds() - IdleStartedAt;
    const double Bob = BobAmplitude * FMath::Sin(UE_TWO_PI * BobFrequency * Seconds);
    const double Spin = FMath::Fmod(SpinDegreesPerSecond * Seconds, 360.0);
    const FQuat Yaw(FVector::UpVector, FMath::DegreesToRadians(Spin));
    const FQuat Tilt(FVector::RightVector, FMath::DegreesToRadians(IdleTilt));

    PresentationRoot->SetRelativeLocationAndRotation(
        RestPresentation.GetLocation() + FVector::UpVector * (HoverHeight + Bob),
        Yaw * RestPresentation.GetRotation() * Tilt);
}

void AMWPickupBox::UpdateOpeningPresentation()
{
    const double Seconds = GetWorld()->GetTimeSeconds() - OpeningStartedAt;
    if (Seconds >= OpeningDuration + DisappearDuration)
    {
        SetPresentationVisible(false);
        SetActorTickEnabled(false);
        return;
    }

    // 뚜껑 메시 원점은 경첩이다. 손잡이도 같은 원점을 중심으로 회전시킨다.
    const double OpenAlpha = FMath::Min(Seconds / OpeningDuration, 1.0);
    const double Angle = FMath::DegreesToRadians(-LidOpenAngle) * FMath::InterpEaseOut(0.0, 1.0, OpenAlpha, 2.0);
    const FQuat OpenRotation(RestLid.GetRotation().GetForwardVector(), Angle);
    LidVisual->SetRelativeRotation(OpenRotation * RestLid.GetRotation());
    const FVector Hinge = RestLid.GetLocation();
    HandleVisual->SetRelativeLocationAndRotation(
        Hinge + OpenRotation.RotateVector(RestHandle.GetLocation() - Hinge),
        OpenRotation * RestHandle.GetRotation());

    if (Seconds > OpeningDuration && DisappearDuration > 0.f)
    {
        const double Remaining = 1.0 - (Seconds - OpeningDuration) / DisappearDuration;
        PresentationRoot->SetRelativeScale3D(RestPresentation.GetScale3D() * Remaining);
    }
}
