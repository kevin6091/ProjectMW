#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "System/MWPlayerState.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMWPlayerRoundSerializationTest, "MW.Gameplay.PlayerRound.Serialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMWPlayerRoundSerializationTest::RunTest(const FString& Parameters)
{
	for (const EMWRaceStatus Status : {EMWRaceStatus::Waiting, EMWRaceStatus::Active, EMWRaceStatus::Finished, EMWRaceStatus::Eliminated, EMWRaceStatus::Spectating})
	{
		FMWPlayerRoundState Sent;
		Sent.RoundEpoch = 42;
		Sent.RaceStatus = Status;
		Sent.FinishOrder = Status == EMWRaceStatus::Finished ? 3 : 0;
		Sent.MemberId = 7;
		Sent.CheckpointsPassed = 3;
		Sent.CheckpointScore = 200;
		Sent.FinishScore = 50;
		Sent.TotalScore = 4250;
		Sent.EliminationOrder = Status == EMWRaceStatus::Eliminated ? 2 : 0;
		Sent.RoundRank = 3;
		Sent.FinalRank = 2;
		Sent.bRaceParticipant = Status != EMWRaceStatus::Spectating;
		Sent.bCheckpointEligible = false;

		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes);
		bool bWriteSucceeded = false;
		TestTrue(TEXT("The custom serializer handles the snapshot"), Sent.NetSerialize(Writer, nullptr, bWriteSucceeded));
		TestTrue(TEXT("The complete snapshot was written"), bWriteSucceeded && !Writer.IsError());

		FMWPlayerRoundState Received;
		Received.RoundEpoch = 7;
		Received.FinishOrder = 99;
		Received.RaceStatus = Status == EMWRaceStatus::Waiting ? EMWRaceStatus::Active : EMWRaceStatus::Waiting;

		FMemoryReader Reader(Bytes);
		bool bReadSucceeded = false;
		TestTrue(TEXT("The custom serializer reads the snapshot"), Received.NetSerialize(Reader, nullptr, bReadSucceeded));
		TestTrue(TEXT("The complete snapshot was read"), bReadSucceeded && !Reader.IsError());
		TestTrue(TEXT("Epoch, status and finish order replace the previous snapshot together"), Received == Sent);
		++Received.FinishOrder;
		TestFalse(TEXT("A changed finish order is a changed snapshot"), Received == Sent);
		TestEqual(TEXT("The snapshot has no unread trailing fields"), Reader.Tell(), Reader.TotalSize());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMWProgressResultsSerializationTest, "MW.Gameplay.PlayerRound.ProgressResultsSerialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMWProgressResultsSerializationTest::RunTest(const FString& Parameters)
{
	FMWGameplayProgress Sent;
	Sent.Phase = EMWRoomPhase::RoundResults;
	Sent.RoundEpoch = Sent.ResultsEpoch = 9;
	Sent.RoundIndex = 4;
	Sent.TargetScore = 4000;
	Sent.RaceMode = EMWRaceMode::SuddenDeath;
	FMWPlayerRoundState Entry;
	Entry.MemberId = 3;
	Entry.RoundEpoch = 9;
	Entry.RaceStatus = EMWRaceStatus::Finished;
	Entry.FinishOrder = Entry.RoundRank = Entry.FinalRank = 1;
	Entry.TotalScore = 4200;
	Sent.Results.Add(Entry);
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	bool bWrite = false;
	Sent.NetSerialize(Writer, nullptr, bWrite);
	FMWGameplayProgress Received;
	FMemoryReader Reader(Bytes);
	bool bRead = false;
	Received.NetSerialize(Reader, nullptr, bRead);
	TestTrue(TEXT("Phase, mode, target and result entries travel in one snapshot"), bWrite && bRead && Sent == Received);
	TestEqual(TEXT("All snapshot bytes were consumed"), Reader.Tell(), Reader.TotalSize());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMWPlayerRoundAuthorityAndNotifyTest, "MW.Gameplay.PlayerRound.AuthorityAndNotify",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMWPlayerRoundAuthorityAndNotifyTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	// Actor의 ProcessEvent와 동적 Delegate를 실제 실행할 수 있게 초기화한다.
	World->InitializeActorsForPlay(FURL());
	auto* Player = World->SpawnActor<AMWPlayerState>();

	const auto* Property = FindFProperty<FStructProperty>(Player->GetClass(), TEXT("PlayerRound"));
	if (TestNotNull(TEXT("PlayerRound is a reflected property"), Property))
	{
		TestTrue(TEXT("The round state is replicated"), Property->HasAnyPropertyFlags(CPF_Net));
		TestTrue(TEXT("The round state has RepNotify"), Property->HasAnyPropertyFlags(CPF_RepNotify));
		TestEqual(TEXT("Replication uses the round-state notification"), Property->RepNotifyFunc, FName(TEXT("OnRep_PlayerRound")));
	}

	TestTrue(TEXT("A newly created player has not joined a round"), Player->GetRoundState() == FMWPlayerRoundState{});

	// 인자 없는 기존 UFUNCTION을 관찰자로 사용해 테스트용 UObject 클래스를 추가하지 않는다.
	auto* AuthorityObserver = World->SpawnActor<AActor>();
	Player->OnPlayerStateChanged.AddDynamic(AuthorityObserver, &AActor::K2_DestroyActor);
	Player->ApplyRoundStateFromServer(FMWPlayerRoundState{});
	TestFalse(TEXT("An unchanged state does not notify"), AuthorityObserver->IsActorBeingDestroyed());

	FMWPlayerRoundState Round;
	Round.RoundEpoch = 1;
	Player->ApplyRoundStateFromServer(Round);
	TestTrue(TEXT("The authority applies the initial Waiting state"), Player->GetRoundState() == Round);
	TestTrue(TEXT("The authority notifies without relying on OnRep"), AuthorityObserver->IsActorBeingDestroyed());

	Round.RaceStatus = EMWRaceStatus::Active;
	Player->ApplyRoundStateFromServer(Round);
	TestTrue(TEXT("Racing can activate the existing epoch"), Player->GetRoundState() == Round);
	Round.RaceStatus = EMWRaceStatus::Finished;
	Round.FinishOrder = 2;
	Player->ApplyRoundStateFromServer(Round);
	TestTrue(TEXT("Finished and server order are stored together"), Player->GetRoundState() == Round);

	Player->SetRole(ROLE_SimulatedProxy);
	auto* RemoteObserver = World->SpawnActor<AActor>();
	Player->OnPlayerStateChanged.AddDynamic(RemoteObserver, &AActor::K2_DestroyActor);
	FMWPlayerRoundState Rejected;
	Rejected.RoundEpoch = 2;
	Player->ApplyRoundStateFromServer(Rejected);
	TestTrue(TEXT("A client cannot overwrite the authoritative state"), Player->GetRoundState() == Round);
	TestFalse(TEXT("A rejected client mutation does not notify"), RemoteObserver->IsActorBeingDestroyed());

	// 수신 알림의 연결을 단위 검증한다. 실제 네트워크 복제 시험과는 구분한다.
	if (UFunction* RepNotify = Player->FindFunction(FName(TEXT("OnRep_PlayerRound"))))
	{
		Player->ProcessEvent(RepNotify, nullptr);
		TestTrue(TEXT("RepNotify reaches the same player-state event"), RemoteObserver->IsActorBeingDestroyed());
	}
	else
	{
		AddError(TEXT("The round-state RepNotify function is missing"));
	}

	Player->SetRole(ROLE_Authority);
	auto* Destination = World->SpawnActor<AMWPlayerState>();
	Player->CopyProperties(Destination);
	TestTrue(TEXT("Lobby travel copying does not carry Active round state"), Destination->GetRoundState() == FMWPlayerRoundState{});

	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif
