// 서버가 실제 차량의 지급 가능 여부를 검사하는 아이템 상자와 재등장 수명을 관리한다.

#include "Gameplay/Items/MWPickupBox.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Gameplay/Items/MWItemComponent.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AMWPickupBox::AMWPickupBox()
{
    bReplicates = true;

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

// 서버 중첩 처리를 등록하고 현재 획득 가능 상태를 충돌·표시에 반영한다.
void AMWPickupBox::BeginPlay()
{
    Super::BeginPlay();
    if (HasAuthority())
    {
        Trigger->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::HandleOverlap);
    }
    UpdateAvailability();
}

// 기존 복제 계약에 따라 소유자 전용 값과 다른 플레이어에게 보일 상태를 등록한다.
void AMWPickupBox::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AMWPickupBox, bAvailable);
}

// 서버에서만 실제 차량의 지급 가능 여부를 검사하고 성공 시 상자를 재등장 대기로 바꾼다.
void AMWPickupBox::HandleOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool,
                                 const FHitResult&)
{
    if (!HasAuthority() || !bAvailable)
    {
        return;
    }
    AMWVehiclePawn* Vehicle = Cast<AMWVehiclePawn>(OtherActor);
    UMWItemComponent* Items = Vehicle ? Vehicle->FindComponentByClass<UMWItemComponent>() : nullptr;
    if (Items && Items->TryGrantBooster())
    {
        bAvailable = false;
        UpdateAvailability();
        ForceNetUpdate();
    }
}

// 기존 재등장 상태에 맞춰 충돌과 외형 표시를 함께 갱신한다.
void AMWPickupBox::UpdateAvailability()
{
    Visual->SetVisibility(bAvailable);
    LidVisual->SetVisibility(bAvailable);
    HandleVisual->SetVisibility(bAvailable);
    Trigger->SetCollisionEnabled(HasAuthority() && bAvailable ? ECollisionEnabled::QueryOnly
                                                              : ECollisionEnabled::NoCollision);
}
