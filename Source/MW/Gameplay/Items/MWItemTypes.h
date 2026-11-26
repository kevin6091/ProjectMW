#pragma once

#include "CoreMinimal.h"
#include "MWItemTypes.generated.h"

UENUM(BlueprintType)
enum class EMWItemId : uint8
{
    None,
    Booster,
    Hammer,
    MachineGun,
    Shotgun,
    MortarLauncher,
    MineLayer,
    StickyBlobLauncher,
    GrowPotion,
    TeslaCoil
};

