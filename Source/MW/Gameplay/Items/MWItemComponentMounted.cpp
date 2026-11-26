#include "Gameplay/Items/MWItemComponent.h"

#include "Data/MWItemDefinitionData.h"
#include "Gameplay/Items/MWMountedItemPresentation.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"

const UMWItemDefinitionData* UMWItemComponent::GetMountedItemDefinition(EMWItemId ItemId) const
{
    if (ItemId == EMWItemId::Hammer)
    {
        return HammerDefinition;
    }
    return ItemId == EMWItemId::Booster ? nullptr : UMWItemDefinitionData::FindById(ItemId);
}

void UMWItemComponent::RemoveMountedVisual()
{
    if (AMWMountedItemPresentation* Visual = MountedVisual.Get())
    {
        Visual->Destroy();
    }
    MountedVisual.Reset();
}

void UMWItemComponent::OnRep_MountedPresentation()
{
    auto* Vehicle = Cast<AMWVehiclePawn>(GetOwner());
    if (!Vehicle || !HasBegunPlay() || GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    const EMWItemId ItemId = MountedPresentation.ItemId;
    const double UseEnd = MountedPresentation.UseEndsAt;
    const bool bNewEquip = UseEnd < 0.0;
    if (ItemId != PresentedMountedItem || (bNewEquip && LastPresentedUseEnd >= 0.0))
    {
        RemoveMountedVisual();
        LastPresentedUseEnd = -1.0;
        PresentedMountedItem = ItemId;
    }

    if (ItemId == EMWItemId::None || (!bNewEquip && UseEnd == LastPresentedUseEnd))
    {
        return;
    }
    const bool bHasUsesRemaining = MountedPresentation.UsesRemaining > 0;
    const bool bUseInProgress = UseEnd > GetServerTime();
    if (!bUseInProgress && !bHasUsesRemaining)
    {
        RemoveMountedVisual();
        return;
    }
    if (!MountedVisual.IsValid())
    {
        if (const UMWItemDefinitionData* Definition = GetMountedItemDefinition(ItemId))
        {
            MountedVisual = AMWMountedItemPresentation::Equip(*Vehicle, *Definition, bNewEquip);
        }
    }
    if (bUseInProgress)
    {
        LastPresentedUseEnd = UseEnd;
        if (AMWMountedItemPresentation* Visual = MountedVisual.Get())
        {
            Visual->PlayUse(!bHasUsesRemaining);
        }
    }
}
