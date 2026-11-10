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

void AMWPickupBox::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		Trigger->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::HandleOverlap);
	}
	UpdateAvailability();
}

void AMWPickupBox::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMWPickupBox, bAvailable);
}

void AMWPickupBox::HandleOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*,
	int32, bool, const FHitResult&)
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

void AMWPickupBox::UpdateAvailability()
{
	Visual->SetVisibility(bAvailable);
	LidVisual->SetVisibility(bAvailable);
	HandleVisual->SetVisibility(bAvailable);
	Trigger->SetCollisionEnabled(HasAuthority() && bAvailable
		? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}
