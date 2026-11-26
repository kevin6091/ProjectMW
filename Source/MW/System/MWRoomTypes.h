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
	Active = 1,
	Finished = 2,
	Eliminated = 3,
	Spectating = 4
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
struct MW_API FMWPlayerRoundState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "MW|Race")
	int32 RoundEpoch = 0;

	UPROPERTY(BlueprintReadOnly, Category = "MW|Race")
	EMWRaceStatus RaceStatus = EMWRaceStatus::Waiting;

	UPROPERTY(BlueprintReadOnly, Category = "MW|Race")
	int32 FinishOrder = 0;

	// 결과 스냅샷에서도 기존 개인 상태를 그대로 사용한다.
	UPROPERTY(BlueprintReadOnly, Category = "MW|Race")
	int32 MemberId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "MW|Race")
	int32 CheckpointsPassed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "MW|Race")
	int32 CheckpointScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "MW|Race")
	int32 FinishScore = 0;

	// 표시용 통과 기록의 전송 한도다. 이 한도를 넘겨도 누적 점수는 계속 집계한다.
	static constexpr int32 MaxCheckpointAwardEntries = 1024;

	UPROPERTY()
	TArray<int32> CheckpointAwards;

	UPROPERTY(BlueprintReadOnly, Category = "MW|Race")
	int32 FirstFinishBonus = 0;

	UPROPERTY(BlueprintReadOnly, Category = "MW|Race")
	int32 TotalScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "MW|Race")
	int32 EliminationOrder = 0;

	UPROPERTY(BlueprintReadOnly, Category = "MW|Race")
	int32 RoundRank = 0;

	UPROPERTY(BlueprintReadOnly, Category = "MW|Race")
	int32 FinalRank = 0;

	UPROPERTY(BlueprintReadOnly, Category = "MW|Race")
	bool bRaceParticipant = true;

	UPROPERTY(BlueprintReadOnly, Category = "MW|Race")
	bool bCheckpointEligible = true;

	bool operator==(const FMWPlayerRoundState& Other) const;
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
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
	UPROPERTY(BlueprintReadOnly, Category = "MW|Gameplay")
	int32 CourseSessionId = 0;
	UPROPERTY(BlueprintReadOnly, Category = "MW|Gameplay")
	int32 CourseRevision = 0;
	UPROPERTY(BlueprintReadOnly, Category = "MW|Gameplay")
	int32 TargetScore = 4000;
	// Phase와 같은 스냅샷으로 전달한다. 새 준비에서도 직전 결과는 보존한다.
	UPROPERTY(BlueprintReadOnly, Category = "MW|Gameplay")
	int32 ResultsEpoch = 0;
	UPROPERTY(BlueprintReadOnly, Category = "MW|Gameplay")
	TArray<FMWPlayerRoundState> Results;

	bool operator==(const FMWGameplayProgress& Other) const;
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
};

template<>
struct TStructOpsTypeTraits<FMWGameplayProgress> : TStructOpsTypeTraitsBase2<FMWGameplayProgress>
{
	enum { WithNetSerializer = true };
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
