


#include "System/MWDriveTestGameMode.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "Player/MWGameplayPlayerController.h"

AMWDriveTestGameMode::AMWDriveTestGameMode()
{
	PlayerControllerClass = AMWGameplayPlayerController::StaticClass();

	DefaultPawnClass = AMWVehiclePawn::StaticClass();

	bStartPlayersAsSpectators = false;
}

void AMWDriveTestGameMode::StartPlay()
{
	ConfigureItemPickupGround(*GetWorld());
	Super::StartPlay();
}

bool AMWDriveTestGameMode::IsItemPickupTestMap(const UWorld& World)
{
	return UWorld::RemovePIEPrefix(World.GetOutermost()->GetName()) == TEXT("/Game/Developers/user/L_Dev_Drive_ItemPickup");
}

void AMWDriveTestGameMode::ConfigureItemPickupGround(UWorld& World)
{
	if (!IsItemPickupTestMap(World))
	{
		return;
	}

	// 이 테스트 맵의 평면은 차체를 막지만 차량 접지 채널을 무시한다. 출발점 아래 지면만 보완한다.
	const FCollisionObjectQueryParams StaticObjects(ECC_WorldStatic);
	for (TActorIterator<APlayerStart> Start(&World); Start; ++Start)
	{
		const FVector Location = Start->GetActorLocation();
		FHitResult Floor;
		const FCollisionQueryParams Query(SCENE_QUERY_STAT(ItemPickupTestGround), false, *Start);
		if (World.LineTraceSingleByObjectType(Floor, Location, Location - FVector(0, 0, 300), StaticObjects, Query)
			&& Floor.ImpactNormal.Z > 0.9)
		{
			UPrimitiveComponent* Component = Floor.GetComponent();
			if (Component && Component->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block
				&& Component->GetCollisionResponseToChannel(ECC_GameTraceChannel1) != ECR_Block)
			{
				Component->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
			}
		}
	}
}
