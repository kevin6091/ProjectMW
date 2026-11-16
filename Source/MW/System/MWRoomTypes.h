#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MWRoomTypes.generated.h"

UENUM(BlueprintType)
enum class EMWRoomPhase : uint8
{
	Lobby,
	Starting,
	VehicleSelection,
	Traveling,
	PreparingConstruction,
	TrackPlacement,
	ElementPlacement,
	PreparingRace,
	Countdown,
	Racing,
	Failed,

	RoundResults,
	MatchResults
};

UENUM(BlueprintType)
enum class EMWRaceMode : uint8
{
	Normal,
	SuddenDeath
};

USTRUCT(BlueprintType)
struct MW_API FMWRoomSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString RoomName;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxPlayers = 3;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(Categories="Asset.MapTheme"))
	FGameplayTag MapThemeId;
};

USTRUCT(BlueprintType)
struct MW_API FMWGameplayProgress
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "MW|Gameplay")
	EMWRoomPhase Phase = EMWRoomPhase::PreparingConstruction;

	UPROPERTY(BlueprintReadOnly, Category = "MW|Gameplay")
	int32 RoundIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "MW|Gameplay")
	int32 RoundEpoch = 0;

	UPROPERTY(BlueprintReadOnly, Category = "MW|Gameplay")
	double PhaseEndServerTime = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "MW|Gameplay")
	EMWRaceMode RaceMode = EMWRaceMode::Normal;

	bool operator==(const FMWGameplayProgress& Other) const;

	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
};

template<>
struct TStructOpsTypeTraits<FMWGameplayProgress> : TStructOpsTypeTraitsBase2<FMWGameplayProgress>
{
	enum
	{
		WithNetSerializer = true
	};
};