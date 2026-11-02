#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MWRoomTypes.generated.h"

UENUM(BlueprintType)
enum class EMWRoomPhase : uint8
{
    Lobby, Starting, VehicleSelection, Traveling, PreparingConstruction,
    TrackPlacement, ElementPlacement, PreparingRace, Countdown, Racing, Failed
};

USTRUCT(BlueprintType)
struct MW_API FMWRoomSettings
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString RoomName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MaxPlayers = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(Categories="Asset.MapTheme")) FGameplayTag MapThemeId;
};
