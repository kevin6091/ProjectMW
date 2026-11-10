#include "Gameplay/Items/MWItemComponent.h"

#include "Engine/World.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "Net/UnrealNetwork.h"
#include "System/GameState/MWGameplayGameState.h"
#include "System/MWDriveTestGameMode.h"

namespace
{
	constexpr double BoosterDurationSeconds = 2.0;
}

UMWItemComponent::UMWItemComponent()
{
	SetIsReplicatedByDefault(true);
}

void UMWItemComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AMWGameplayGameState* State = GetWorld()->GetGameState<AMWGameplayGameState>())
	{
		State->OnGameplayStateChanged.AddUniqueDynamic(this, &ThisClass::HandleGameStateChanged);
	}
}

void UMWItemComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UMWItemComponent, CurrentItemId, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UMWItemComponent, GrantId, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UMWItemComponent, BoosterEndsAt, COND_OwnerOnly);
}

bool UMWItemComponent::IsItemPlayAllowed() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	if (const AMWGameplayGameState* State = World->GetGameState<AMWGameplayGameState>())
	{
		return State->GetRoomPhase() == EMWRoomPhase::Racing;
	}
	return World->GetAuthGameMode<AMWDriveTestGameMode>() != nullptr;
}

double UMWItemComponent::GetServerTime() const
{
	const UWorld* World = GetWorld();
	if (const AGameStateBase* State = World->GetGameState())
	{
		return State->GetServerWorldTimeSeconds();
	}
	return World->GetTimeSeconds();
}

bool UMWItemComponent::IsBoosterActive() const
{
	return GetBoosterTimeRemaining() > 0.f;
}

float UMWItemComponent::GetBoosterTimeRemaining() const
{
	return BoosterEndsAt > 0.0 && GetWorld()
		? static_cast<float>(FMath::Max(0.0, BoosterEndsAt - GetServerTime())) : 0.f;
}

bool UMWItemComponent::TryGrantBooster()
{
	const AMWVehiclePawn* Vehicle = Cast<AMWVehiclePawn>(GetOwner());
	if (!Vehicle || !Vehicle->HasAuthority() || !Vehicle->GetController()
		|| !IsItemPlayAllowed() || CurrentItemId != EMWItemId::None)
	{
		return false;
	}
	++GrantId;
	SetCurrentItem(EMWItemId::Booster);
	return true;
}

void UMWItemComponent::RequestUseItem()
{
	const AMWVehiclePawn* Vehicle = Cast<AMWVehiclePawn>(GetOwner());
	if (Vehicle && Vehicle->IsLocallyControlled() && CurrentItemId == EMWItemId::Booster)
	{
		ServerUseItem(GrantId);
	}
}

void UMWItemComponent::ServerUseItem_Implementation(uint32 ExpectedGrantId)
{
	const AMWVehiclePawn* Vehicle = Cast<AMWVehiclePawn>(GetOwner());
	if (!Vehicle || !Vehicle->GetController() || !IsItemPlayAllowed()
		|| CurrentItemId != EMWItemId::Booster || ExpectedGrantId != GrantId || IsBoosterActive())
	{
		return;
	}
	BoosterEndsAt = GetServerTime() + BoosterDurationSeconds;
	SetCurrentItem(EMWItemId::None);
}

void UMWItemComponent::ResetItemState()
{
	AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority() && (CurrentItemId != EMWItemId::None || BoosterEndsAt > 0.0))
	{
		BoosterEndsAt = 0.0;
		SetCurrentItem(EMWItemId::None);
	}
}

void UMWItemComponent::HandleGameStateChanged()
{
	if (!IsItemPlayAllowed())
	{
		ResetItemState();
	}
}

void UMWItemComponent::SetCurrentItem(EMWItemId ItemId)
{
	const bool bChanged = CurrentItemId != ItemId;
	CurrentItemId = ItemId;
	GetOwner()->ForceNetUpdate();
	if (bChanged)
	{
		OnRep_CurrentItem();
	}
}

void UMWItemComponent::OnRep_CurrentItem()
{
	OnHeldItemChanged.Broadcast(CurrentItemId);
}
