#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MWMultiplayerTypes.generated.h"

UENUM(BlueprintType)
enum class EMWSessionState : uint8
{
	None,
	Creating,
	Hosting,
	Searching,
	Joining,
	Joined,
	Leaving,
	CleanupRequired
};

USTRUCT(BlueprintType)
struct MW_API FMWRoomSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString RoomName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxPlayers = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Categories = "Asset.MapTheme"))
	FGameplayTag MapThemeId;
};

// A display snapshot. The original online result remains private to the subsystem.
USTRUCT(BlueprintType)
struct MW_API FMWRoomInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FString SessionId;
	UPROPERTY(BlueprintReadOnly) FString RoomName;
	UPROPERTY(BlueprintReadOnly) int32 CurrentPlayers = 0;
	UPROPERTY(BlueprintReadOnly) int32 MaxPlayers = 0;
	UPROPERTY(BlueprintReadOnly) FName MapThemeId;
	UPROPERTY(BlueprintReadOnly) bool bCanJoin = false;
	UPROPERTY(BlueprintReadOnly) FString UnavailableReason;
};
