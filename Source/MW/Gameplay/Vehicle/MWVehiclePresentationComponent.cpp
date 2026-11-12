#include "Gameplay/Vehicle/MWVehiclePresentationComponent.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"

UMWVehiclePresentationComponent::UMWVehiclePresentationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UMWVehiclePresentationComponent::BeginPlay()
{
	Super::BeginPlay();
	SetComponentTickEnabled(GetNetMode() != NM_DedicatedServer);
}

void UMWVehiclePresentationComponent::SetLocalPrerequisite(UMWVehiclePresentationComponent* LocalPresentation)
{
	if (LocalPresentation == this || LocalPrerequisite == LocalPresentation)
	{
		return;
	}
	if (UMWVehiclePresentationComponent* Previous = LocalPrerequisite.Get())
	{
		RemoveTickPrerequisiteComponent(Previous);
	}
	LocalPrerequisite = LocalPresentation;
	if (LocalPresentation)
	{
		// 조종 차량이 바뀌어도 이전 선행 관계를 남겨 Tick 순환을 만들지 않는다.
		AddTickPrerequisiteComponent(LocalPresentation);
	}
}

void UMWVehiclePresentationComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (AMWVehiclePawn* Vehicle = Cast<AMWVehiclePawn>(GetOwner()))
	{
		Vehicle->UpdateDrivePresentation(DeltaTime);
	}
}
