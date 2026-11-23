#include "System/MWRoomTypes.h"

bool FMWGameplayProgress::operator==(const FMWGameplayProgress& Other) const
{
	return Phase == Other.Phase
		&& RoundIndex == Other.RoundIndex
		&& RoundEpoch == Other.RoundEpoch
		&& PhaseEndServerTime == Other.PhaseEndServerTime
		&& RaceMode == Other.RaceMode
		&& CourseSessionId == Other.CourseSessionId
		&& CourseRevision == Other.CourseRevision
		&& TargetScore == Other.TargetScore
		&& ResultsEpoch == Other.ResultsEpoch && Results == Other.Results;
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
	Ar << TargetScore;
	Ar << ResultsEpoch;
	uint8 Count = static_cast<uint8>(Results.Num());
	Ar << Count;
	if (Count > 16 || (Ar.IsSaving() && Results.Num() > 16))
	{
		Ar.SetError();
		bOutSuccess = false;
		return true;
	}
	if (Ar.IsLoading()) Results.SetNum(Count);
	bool bResultsSuccess = true;
	for (FMWPlayerRoundState& Entry : Results)
	{
		bool bEntrySuccess = false;
		Entry.NetSerialize(Ar, Map, bEntrySuccess);
		bResultsSuccess &= bEntrySuccess;
	}

	bOutSuccess = bResultsSuccess && !Ar.IsError();

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
		&& RaceStatus == Other.RaceStatus
		&& FinishOrder == Other.FinishOrder && MemberId == Other.MemberId
		&& CheckpointsPassed == Other.CheckpointsPassed && CheckpointScore == Other.CheckpointScore
		&& FinishScore == Other.FinishScore && TotalScore == Other.TotalScore
		&& EliminationOrder == Other.EliminationOrder && RoundRank == Other.RoundRank
		&& FinalRank == Other.FinalRank && bRaceParticipant == Other.bRaceParticipant
		&& bCheckpointEligible == Other.bCheckpointEligible;
}

bool FMWPlayerRoundState::NetSerialize(
	FArchive& Ar,
	UPackageMap* Map,
	bool& bOutSuccess)
{
	uint8 StatusValue = static_cast<uint8>(RaceStatus);

	Ar << RoundEpoch;
	Ar << StatusValue;
	Ar << FinishOrder;
	Ar << MemberId;
	Ar << CheckpointsPassed;
	Ar << CheckpointScore;
	Ar << FinishScore;
	Ar << TotalScore;
	Ar << EliminationOrder;
	Ar << RoundRank;
	Ar << FinalRank;
	Ar << bRaceParticipant;
	Ar << bCheckpointEligible;

	bOutSuccess = !Ar.IsError();

	if (Ar.IsLoading() && bOutSuccess)
	{
		RaceStatus = static_cast<EMWRaceStatus>(StatusValue);
	}

	return true;
}
