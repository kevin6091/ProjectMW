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

UENUM(BlueprintType)
enum class EMWRaceStatus : uint8
{
	Waiting = 0,
	Active = 1
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

	// PreparingRace에서 확인할 확정 코스. 0/0은 서버가 건설을 생략한 고정 코스다.
	UPROPERTY(BlueprintReadOnly, Category = "MW|Gameplay")
	int32 CourseSessionId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "MW|Gameplay")
	int32 CourseRevision = 0;

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

USTRUCT(BlueprintType)
struct MW_API FMWPlayerRoundState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "MW|Race")
	int32 RoundEpoch = 0;

	UPROPERTY(BlueprintReadOnly, Category = "MW|Race")
	EMWRaceStatus RaceStatus = EMWRaceStatus::Waiting;

	bool operator==(const FMWPlayerRoundState& Other) const;

	bool NetSerialize(
		FArchive& Ar,
		UPackageMap* Map,
		bool& bOutSuccess);
};

template<>
struct TStructOpsTypeTraits<FMWPlayerRoundState>
	: public TStructOpsTypeTraitsBase2<FMWPlayerRoundState>
{
	enum
	{
		WithNetSerializer = true
	};
};
