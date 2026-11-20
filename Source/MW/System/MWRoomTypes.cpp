#include "System/MWRoomTypes.h"

bool FMWGameplayProgress::operator==(const FMWGameplayProgress& Other) const
{
	return Phase == Other.Phase
		&& RoundIndex == Other.RoundIndex
		&& RoundEpoch == Other.RoundEpoch
		&& PhaseEndServerTime == Other.PhaseEndServerTime
		&& RaceMode == Other.RaceMode
		&& CourseSessionId == Other.CourseSessionId
		&& CourseRevision == Other.CourseRevision;
}

bool FMWGameplayProgress::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	uint8 PhaseValue = static_cast<uint8>(Phase);
	uint8 RaceModeValue = static_cast<uint8>(RaceMode);

	Ar << PhaseValue;
	Ar << RoundIndex;
	Ar << RoundEpoch;
	Ar << PhaseEndServerTime;
	Ar << RaceModeValue;
	Ar << CourseSessionId;
	Ar << CourseRevision;

	bOutSuccess = !Ar.IsError();

	if (Ar.IsLoading() && bOutSuccess)
	{
		Phase = static_cast<EMWRoomPhase>(PhaseValue);
		RaceMode = static_cast<EMWRaceMode>(RaceModeValue);
	}

	return true;
}

bool FMWPlayerRoundState::operator==(
	const FMWPlayerRoundState& Other) const
{
	return RoundEpoch == Other.RoundEpoch
		&& RaceStatus == Other.RaceStatus;
}

bool FMWPlayerRoundState::NetSerialize(
	FArchive& Ar,
	UPackageMap* Map,
	bool& bOutSuccess)
{
	uint8 StatusValue = static_cast<uint8>(RaceStatus);

	Ar << RoundEpoch;
	Ar << StatusValue;

	bOutSuccess = !Ar.IsError();

	if (Ar.IsLoading() && bOutSuccess)
	{
		RaceStatus = static_cast<EMWRaceStatus>(StatusValue);
	}

	return true;
}
