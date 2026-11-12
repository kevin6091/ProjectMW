#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "Gameplay/Vehicle/MWVehicleMovementComponent.h"
#include "Gameplay/Vehicle/MWVehicleSpringArmComponent.h"
#include "Gameplay/Vehicle/MWVehiclePresentationComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Misc/ScopeExit.h"
#include "Net/UnrealNetwork.h"
#include "LogChannel.h"

namespace
{
	constexpr float DriveStep = 1.f / 120.f;
	constexpr float MaxDriveStepTime = 12.f * DriveStep;
	constexpr float SnapshotInterval = 1.f / 20.f;
	constexpr float SendInterval = 1.f / 30.f;
	constexpr float RemoteInputTimeout = 1.f;
	constexpr int32 MaxMovesPerPacket = 32;
	// 120Hz에서 1.5초 분량. 지연된 ACK와 짧은 손실이 겹쳐도 소유자 예측 이력을 유지한다.
	constexpr int32 MaxPendingMoves = 180;
	constexpr int32 MaxProxySnapshots = 12;
	constexpr double ProxyBufferTime = 0.075;
	constexpr double MaxProxyExtrapolation = 0.05;
	constexpr double MaxContactExtrapolation = 0.25;
	constexpr double MaxContactSampleAge = 0.35;
	constexpr float PredictedContactLifetime = 0.25f;
	constexpr float PresentationHalfLife = 0.055f;
	constexpr double MaxVisualCorrectionDistance = 150.0;
	constexpr double MaxVisualCorrectionAngle = 45.0;

	bool IsNewerSequence(uint32 Candidate, uint32 Previous)
	{
		const uint32 Difference = Candidate - Previous;
		return Difference != 0 && Difference < (1u << 31);
	}

	bool NeedsDriveCorrection(const FMWVehicleDriveSnapshot& Prediction, const FMWVehicleDriveSnapshot& Server)
	{
		// 움직이는 중에는 작은 오차를 허용하고, 정지하면 복제 정밀도에 가깝게 맞춘다.
		const bool bStopped = Prediction.Velocity.IsNearlyZero(1.0) && Server.Velocity.IsNearlyZero(1.0)
			&& FMath::IsNearlyZero(Prediction.ImpactYawRate) && FMath::IsNearlyZero(Server.ImpactYawRate);
		const double LocationTolerance = bStopped ? 0.15 : 3.0;
		const double RotationTolerance = bStopped ? 0.15 : 1.0;
		return !Prediction.Location.Equals(Server.Location, LocationTolerance)
			|| !Prediction.Rotation.Equals(Server.Rotation, RotationTolerance)
			|| !Prediction.Velocity.Equals(Server.Velocity, 5.f)
			|| Prediction.bGrounded != Server.bGrounded
			|| FMath::Abs(Prediction.ImpactYawRate - Server.ImpactYawRate) > 2.f;
	}

	bool IsValidDriveInput(const FMWVehicleDriveInput& Input)
	{
		return FMath::IsWithinInclusive(Input.Throttle, 0.f, 1.f)
			&& FMath::IsWithinInclusive(Input.Reverse, 0.f, 1.f)
			&& FMath::IsWithinInclusive(Input.Steering, -1.f, 1.f);
	}
}

double AMWVehiclePawn::GetDriveServerTime() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World->GetGameState();
	return !HasAuthority() && GameState ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
}

double AMWVehiclePawn::GetDrivePredictionTime() const
{
	if (!HasAuthority() && IsLocallyControlled() && AppliedStreamId != 0)
	{
		return LastAuthoritativeSnapshot.ServerTimeSeconds + PredictedMoves.Num() * DriveStep;
	}
	return GetDriveServerTime() - FMath::Clamp(DriveStepTime, 0.f, DriveStep);
}

void AMWVehiclePawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMWVehiclePawn, ServerDriveSnapshot);
}

void AMWVehiclePawn::ResetDriveStream()
{
	ResetDrivePresentation();
	if (!HasAuthority() || GetNetMode() == NM_Standalone)
	{
		return;
	}

	QueuedServerMoves.Reset();
	LastProcessedMoveId = 0;
	DriveStepTime = 0.f;
	SnapshotTime = 0.f;
	RemoteInputSilence = 0.f;
	RemoteInputGapTime = 0.f;
	bHadRemoteInput = false;
	bContactSnapshotPending = false;
	PublishDriveSnapshot(true);
	ForceNetUpdate();
}

void AMWVehiclePawn::PublishDriveSnapshot(bool bNewStream)
{
	FMWVehicleDriveSnapshot Snapshot = ServerDriveSnapshot;
	VehicleMovement->CaptureDriveSnapshot(Snapshot);

	if (!bNewStream
		&& Snapshot.LastProcessedMoveId == LastProcessedMoveId
		&& Snapshot.Location == ServerDriveSnapshot.Location
		&& Snapshot.Rotation == ServerDriveSnapshot.Rotation
		&& Snapshot.Velocity == ServerDriveSnapshot.Velocity
		&& Snapshot.ImpactYawRate == ServerDriveSnapshot.ImpactYawRate
		&& Snapshot.bGrounded == ServerDriveSnapshot.bGrounded)
	{
		return;
	}

	if (bNewStream && ++Snapshot.StreamId == 0)
	{
		// 0은 아직 서버 상태를 받은 적 없는 클라이언트에 예약한다.
		++Snapshot.StreamId;
	}
	Snapshot.LastProcessedMoveId = LastProcessedMoveId;
	Snapshot.ServerTimeSeconds = GetDriveServerTime();
	++Snapshot.Revision;
	ServerDriveSnapshot = Snapshot;
}

void AMWVehiclePawn::TickNetworkDrive(float DeltaTime)
{
	if (HasAuthority())
	{
		TickAuthorityDrive(DeltaTime);
		return;
	}

	const bool bPredicting = GetLocalRole() == ROLE_AutonomousProxy && IsLocallyControlled();
	if (bPredicting != bWasPredicting)
	{
		AppliedStreamId = 0;
		AppliedRevision = 0;
		DriveStepTime = 0.f;
		SendTime = 0.f;
		PredictedMoves.Reset();
		PredictedContactMotions.Reset();
		LastAckedContactMotions.Reset();
		LastAckedContactTime = 0.0;
		ProxySnapshots.Reset();
		bPredictionBufferFull = false;
		ResetDrivePresentation();
		bWasPredicting = bPredicting;
	}

	if (ServerDriveSnapshot.StreamId == 0)
	{
		return;
	}
	if (bPredicting)
	{
		TickPredictedDrive(DeltaTime);
	}
}

void AMWVehiclePawn::ServerSubmitDriveMoves_Implementation(uint32 StreamId, const TArray<FMWVehicleMoveCommand>& Moves)
{
	if (StreamId != ServerDriveSnapshot.StreamId || StreamId == 0 || !GetNetConnection()
		|| !IsPlayerControlled() || IsLocallyControlled() || Moves.Num() > MaxMovesPerPacket)
	{
		return;
	}

	for (const FMWVehicleMoveCommand& Move : Moves)
	{
		// 엔진이 RPC 소유 연결을 확인한다. 서버는 현재 스트림의 미처리 입력만 보관한다.
		const uint32 Ahead = Move.MoveId - LastProcessedMoveId;
		if (Ahead == 0 || Ahead > MaxPendingMoves || QueuedServerMoves.Contains(Move.MoveId) || !IsValidDriveInput(Move.Input))
		{
			continue;
		}
		QueuedServerMoves.Add(Move.MoveId, Move.Input);
		RemoteInputSilence = 0.f;
		bHadRemoteInput = true;
	}
}

void AMWVehiclePawn::TickAuthorityDrive(float DeltaTime)
{
	const bool bRemoteDriver = IsPlayerControlled() && !IsLocallyControlled();
	if (bRemoteDriver && bHadRemoteInput)
	{
		RemoteInputSilence += DeltaTime;
		if (RemoteInputSilence >= RemoteInputTimeout)
		{
			// 오래 끊긴 스트림을 폐기하고, 새 입력이 도착할 때까지 중립 입력으로 진행한다.
			UE_LOG(LogMWVehicleNet, Verbose, TEXT("%s drive stream %u reset: input timeout at ACK %u"),
				*GetName(), ServerDriveSnapshot.StreamId, LastProcessedMoveId);
			ResetDriveStream();
		}
	}

	DriveStepTime = FMath::Min(DriveStepTime + DeltaTime, MaxDriveStepTime);
	while (DriveStepTime >= DriveStep)
	{
		FMWVehicleDriveInput StepInput;
		if (bRemoteDriver && bHadRemoteInput)
		{
			// 누락된 순번은 재전송을 기다린다. 한 틱에 쓸 시간은 서버에서만 적립한다.
			const uint32 ExpectedMove = LastProcessedMoveId + 1;
			if (!QueuedServerMoves.RemoveAndCopyValue(ExpectedMove, StepInput))
			{
				// 뒤 순번만 계속 도착하면 입력 침묵 타이머가 초기화된다. 같은 누락은 별도로 센다.
				if (!QueuedServerMoves.IsEmpty())
				{
					RemoteInputGapTime += DeltaTime;
					if (RemoteInputGapTime >= RemoteInputTimeout)
					{
						UE_LOG(LogMWVehicleNet, Verbose, TEXT("%s drive stream %u reset: missing move %u (%d queued)"),
							*GetName(), ServerDriveSnapshot.StreamId, ExpectedMove, QueuedServerMoves.Num());
						ResetDriveStream();
					}
				}
				break;
			}
			RemoteInputGapTime = 0.f;
			LastProcessedMoveId = ExpectedMove;
		}
		else if (!bRemoteDriver && GetController())
		{
			StepInput = DriveInput;
		}
		VehicleMovement->SetDriveInput(StepInput);
		PreviousDriveTransform = CollisionRoot->GetComponentTransform();
		bHasPreviousDriveTransform = true;
		VehicleMovement->SimulateDriveStep(DriveStep);
		DriveStepTime -= DriveStep;
	}

	SnapshotTime += DeltaTime;
	if (bContactSnapshotPending || SnapshotTime >= SnapshotInterval)
	{
		SnapshotTime = FMath::Fmod(SnapshotTime, SnapshotInterval);
		PublishDriveSnapshot();
		if (bContactSnapshotPending)
		{
			// 피해 차량이 이번 틱에 입력을 처리하지 않아도 같은 ACK로 충돌 상태를 보낸다.
			bContactSnapshotPending = false;
			ForceNetUpdate();
		}
	}
}

void AMWVehiclePawn::TickPredictedDrive(float DeltaTime)
{
	// 표시를 위해 과거 위치에 둔 상대 루트는 이번 예측 Sweep에서 빼고, 아래에서 별도로 검사한다.
	TArray<AMWVehiclePawn*, TInlineAllocator<3>> ContactSources;
	for (TActorIterator<AMWVehiclePawn> It(GetWorld()); It; ++It)
	{
		AMWVehiclePawn* Other = *It;
		const UBoxComponent* OtherBox = Other->CollisionRoot;
		if (Other != this && Other->ServerDriveSnapshot.StreamId != 0 && Other->VehicleMovement->bDriveConfigReady
			&& OtherBox->IsQueryCollisionEnabled() && !CollisionRoot->GetMoveIgnoreActors().Contains(Other)
			&& CollisionRoot->GetCollisionResponseToChannel(OtherBox->GetCollisionObjectType()) == ECR_Block
			&& OtherBox->GetCollisionResponseToChannel(CollisionRoot->GetCollisionObjectType()) == ECR_Block)
		{
			ContactSources.Add(Other);
			CollisionRoot->IgnoreActorWhenMoving(Other, true);
		}
	}
	ON_SCOPE_EXIT
	{
		for (AMWVehiclePawn* Other : ContactSources)
		{
			CollisionRoot->IgnoreActorWhenMoving(Other, false);
		}
	};

	if (AppliedStreamId != ServerDriveSnapshot.StreamId)
	{
		PredictedMoves.Reset();
		PredictedContactMotions.Reset();
		LastAckedContactMotions.Reset();
		NextMoveId = ServerDriveSnapshot.LastProcessedMoveId + 1;
		DriveStepTime = 0.f;
		VehicleMovement->ApplyDriveSnapshot(ServerDriveSnapshot);
		LastAuthoritativeSnapshot = ServerDriveSnapshot;
		LastAckedContactTime = ServerDriveSnapshot.ServerTimeSeconds;
		ResetDrivePresentation();
		bPredictionBufferFull = false;
		AppliedStreamId = ServerDriveSnapshot.StreamId;
		AppliedRevision = ServerDriveSnapshot.Revision;
	}
	else if (IsNewerSequence(ServerDriveSnapshot.Revision, AppliedRevision))
	{
		ReconcilePredictedDrive();
		AppliedRevision = ServerDriveSnapshot.Revision;
	}

	DriveStepTime = FMath::Min(DriveStepTime + DeltaTime, MaxDriveStepTime);
	while (DriveStepTime >= DriveStep && PredictedMoves.Num() < MaxPendingMoves)
	{
		FPredictedMove& Move = PredictedMoves.AddDefaulted_GetRef();
		Move.Command.MoveId = NextMoveId++;
		Move.Command.Input = IsValidDriveInput(DriveInput) ? DriveInput : FMWVehicleDriveInput{};
		const double ContactTime = LastAuthoritativeSnapshot.ServerTimeSeconds
			+ (Move.Command.MoveId - LastAuthoritativeSnapshot.LastProcessedMoveId - 1) * DriveStep;
		for (const AMWVehiclePawn* Other : ContactSources)
		{
			FMWVehicleContactSample Sample;
			if (GetContactPrediction(Other, ContactTime, Sample))
			{
				Move.Contacts.Add(Sample);
			}
		}
		SimulatePredictedMove(Move);
		DriveStepTime -= DriveStep;
	}
	const bool bBufferFull = PredictedMoves.Num() == MaxPendingMoves;
	if (bPredictionBufferFull != bBufferFull)
	{
		UE_LOG(LogMWVehicleNet, Verbose, TEXT("%s prediction buffer %s at ACK %u"), *GetName(),
			bBufferFull ? TEXT("full") : TEXT("resumed"), LastAuthoritativeSnapshot.LastProcessedMoveId);
		bPredictionBufferFull = bBufferFull;
	}
	if (bBufferFull)
	{
		// 연결이 돌아왔을 때 멈춰 있던 동안의 최신 입력을 한 번에 여러 스텝 적용하지 않는다.
		DriveStepTime = FMath::Min(DriveStepTime, DriveStep);
	}

	SendTime += DeltaTime;
	if (SendTime >= SendInterval && !PredictedMoves.IsEmpty())
	{
		SendTime = FMath::Fmod(SendTime, SendInterval);
		// 미확인 입력의 앞부분은 유실 복구에, 뒷부분은 ACK를 기다리지 않고 최신 입력을 보내는 데 쓴다.
		TArray<FMWVehicleMoveCommand> Batch;
		const int32 NumMoves = PredictedMoves.Num();
		const int32 BatchSize = FMath::Min(MaxMovesPerPacket, NumMoves);
		Batch.Reserve(BatchSize);
		for (int32 Index = 0; Index < BatchSize; ++Index)
		{
			const int32 MoveIndex = Index < MaxMovesPerPacket / 2 ? Index : Index + NumMoves - BatchSize;
			Batch.Add(PredictedMoves[MoveIndex].Command);
		}
		ServerSubmitDriveMoves(AppliedStreamId, Batch);
	}
}

void AMWVehiclePawn::SimulatePredictedMove(FPredictedMove& Move, bool bReplaying)
{
	// 저장한 원본과 ACK 시점의 상대 추정치를 복원한다. 화면에 보이는 과거 위치는 읽지 않는다.
	TArray<FMWVehicleContactSample, TInlineAllocator<3>> Contacts = Move.Contacts;
	const double StepTime = LastAuthoritativeSnapshot.ServerTimeSeconds
		+ (Move.Command.MoveId - LastAuthoritativeSnapshot.LastProcessedMoveId - 1) * DriveStep;
	for (FMWVehicleContactSample& Contact : Contacts)
	{
		FMWVehicleContactSample Latest;
		if (GetContactPrediction(Cast<AMWVehiclePawn>(Contact.Vehicle.ResolveObjectPtr()), StepTime, Latest)
			&& Latest.StreamId == Contact.StreamId && Latest.SourceTime <= StepTime
			&& Latest.SourceTime > Contact.SourceTime)
		{
			// 새 서버 자료가 이 시각까지 도착했다면 이전 추측보다 우선한다.
			Contact = Latest;
		}
		// 내 차와 상대를 같은 시각으로 평가하되 오래된 자료로 계속 멀리 추정하지 않는다.
		const double Lead = FMath::Clamp(StepTime - Contact.SourceTime, 0.0, MaxContactExtrapolation);
		Contact.Location = Contact.SourceLocation + Contact.SourceVelocity * Lead;
		FRotator Rotation = Contact.SourceRotation.Rotator();
		Rotation.Yaw += Contact.ImpactYawRate * Lead;
		Contact.Rotation = Rotation.Quaternion();
	}
	for (int32 Index = PredictedContactMotions.Num() - 1; Index >= 0; --Index)
	{
		FPredictedContactMotion& Motion = PredictedContactMotions[Index];
		FMWVehicleContactSample* Sample = Contacts.FindByPredicate([&Motion](const FMWVehicleContactSample& Contact)
		{
			return Contact.Vehicle == Motion.Vehicle && Contact.StreamId == Motion.StreamId;
		});
		if (!Sample)
		{
			PredictedContactMotions.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			continue;
		}
		const bool bServerShowsImpact = IsNewerSequence(Sample->Revision, Motion.Revision)
			&& FVector::DistSquared(Sample->Velocity, Motion.Velocity) < FVector::DistSquared(Sample->Velocity, Motion.BeforeImpactVelocity);
		if (bServerShowsImpact || Motion.RemainingTime <= 0.f)
		{
			// 새 스냅샷이 박스 반대편으로 건너뛰더라도 중간 접촉을 빠뜨리지 않는다.
			const FVector DesiredEnd = Sample->Location + Sample->Velocity * DriveStep;
			Sample->CorrectionDelta = DesiredEnd - Motion.Location - Sample->Velocity * DriveStep;
			Sample->Location = Motion.Location;
			Motion.Location = DesiredEnd;
			Motion.Rotation = Sample->Rotation;
			Motion.Velocity = Sample->Velocity;
			Motion.ImpactYawRate = Sample->ImpactYawRate;
			Motion.Revision = Sample->Revision;
			Motion.RemainingTime = 0.f;
		}
		else
		{
			Sample->Location = Motion.Location;
			Sample->Velocity = Motion.Velocity;
			Sample->Rotation = Motion.Rotation;
			Sample->ImpactYawRate = Motion.ImpactYawRate;
			Motion.Location += Motion.Velocity * DriveStep;
			FRotator Rotation = Motion.Rotation.Rotator();
			Rotation.Yaw += Motion.ImpactYawRate * DriveStep;
			Motion.Rotation = Rotation.Quaternion();
			Motion.ImpactYawRate *= FMath::Exp(-MWVehicleImpactYawDamping * DriveStep);
			Motion.RemainingTime -= DriveStep;
		}
	}
	for (const FMWVehicleContactSample& Sample : Contacts)
	{
		if (!PredictedContactMotions.ContainsByPredicate([&Sample](const FPredictedContactMotion& Entry)
		{
			return Entry.Vehicle == Sample.Vehicle && Entry.StreamId == Sample.StreamId;
		}))
		{
			FPredictedContactMotion& Motion = PredictedContactMotions.AddDefaulted_GetRef();
			Motion.Vehicle = Sample.Vehicle;
			Motion.StreamId = Sample.StreamId;
			Motion.Revision = Sample.Revision;
			Motion.Location = Sample.Location + Sample.Velocity * DriveStep;
			Motion.Rotation = Sample.Rotation;
			Motion.Velocity = Sample.Velocity;
			Motion.ImpactYawRate = Sample.ImpactYawRate;
		}
	}

	FMWVehiclePredictedImpact Impact;
	VehicleMovement->SetDriveInput(Move.Command.Input);
	PreviousDriveTransform = CollisionRoot->GetComponentTransform();
	bHasPreviousDriveTransform = true;
	VehicleMovement->SimulateDriveStep(DriveStep, Contacts, &Impact);
	VehicleMovement->CaptureDriveSnapshot(Move.Result);

	if (Impact.SampleIndex != INDEX_NONE)
	{
		const FMWVehicleContactSample& Sample = Contacts[Impact.SampleIndex];
		if (!bReplaying)
		{
			AMWVehiclePawn* Other = Cast<AMWVehiclePawn>(Sample.Vehicle.ResolveObjectPtr());
			NotifyDriveContact(Other, Impact.Normal);
			if (Other)
			{
				Other->NotifyDriveContact(this, -Impact.Normal);
			}
		}
		FPredictedContactMotion* Motion = PredictedContactMotions.FindByPredicate([&Sample](const FPredictedContactMotion& Entry)
		{
			return Entry.Vehicle == Sample.Vehicle;
		});
		if (!Motion)
		{
			Motion = &PredictedContactMotions.AddDefaulted_GetRef();
		}

		// 다음 스텝에는 상대도 밀린 위치에 있다고 가정한다. 실제 상대 Pawn이나 화면은 건드리지 않는다.
		Motion->Vehicle = Sample.Vehicle;
		Motion->StreamId = Sample.StreamId;
		Motion->Revision = Sample.Revision;
		Motion->RemainingTime = PredictedContactLifetime;
		Motion->BeforeImpactVelocity = Move.Contacts[Impact.SampleIndex].Velocity;
		Motion->Velocity = Sample.Velocity + Impact.OtherVelocityDelta;
		Motion->Location = Sample.Location + (Sample.Velocity * DriveStep + Sample.CorrectionDelta) * Impact.Fraction
			+ Motion->Velocity * (DriveStep * (1.0 - Impact.Fraction));
		FRotator Rotation = Sample.Rotation.Rotator();
		Rotation.Yaw += Sample.ImpactYawRate * DriveStep;
		Motion->Rotation = Rotation.Quaternion();
		Motion->ImpactYawRate = FMath::Clamp(Sample.ImpactYawRate + Impact.OtherYawRateDelta, -150.f, 150.f);
	}
	Move.ContactMotions = PredictedContactMotions;
	Move.ResultTime = StepTime + DriveStep;
}

void AMWVehiclePawn::ReconcilePredictedDrive()
{
	const uint32 Ack = ServerDriveSnapshot.LastProcessedMoveId;
	const uint32 PreviousAck = LastAuthoritativeSnapshot.LastProcessedMoveId;
	const bool bSameAck = Ack == PreviousAck;
	if (!bSameAck && !IsNewerSequence(Ack, PreviousAck))
	{
		return;
	}

	int32 AckedCount = 0;
	while (AckedCount < PredictedMoves.Num() && !IsNewerSequence(PredictedMoves[AckedCount].Command.MoveId, Ack))
	{
		++AckedCount;
	}

	// ACK가 같아도 상대가 내 차량을 밀 수 있다. 이때는 같은 ACK의 이전 서버 상태와 비교한다.
	const FMWVehicleDriveSnapshot* Comparison = bSameAck ? &LastAuthoritativeSnapshot : nullptr;
	TArray<FPredictedContactMotion, TInlineAllocator<3>> AckedContactMotions = LastAckedContactMotions;
	double AckedContactTime = LastAckedContactTime;
	if (AckedCount > 0)
	{
		const FPredictedMove& LastAcked = PredictedMoves[AckedCount - 1];
		if (LastAcked.Command.MoveId == Ack)
		{
			Comparison = &LastAcked.Result;
			AckedContactMotions = LastAcked.ContactMotions;
			AckedContactTime = LastAcked.ResultTime;
		}
	}
	const bool bNeedsCorrection = !Comparison || NeedsDriveCorrection(*Comparison, ServerDriveSnapshot);
	const double ContactTimeShift = FMath::Clamp(ServerDriveSnapshot.ServerTimeSeconds - AckedContactTime,
		-MaxContactExtrapolation, MaxContactExtrapolation);
	for (FPredictedContactMotion& Motion : AckedContactMotions)
	{
		Motion.Location += Motion.Velocity * ContactTimeShift;
		if (bNeedsCorrection && Comparison && Motion.RemainingTime > 0.f)
		{
			// 내 위치를 서버에 맞출 때 붙어 있는 상대 추정치도 같은 양만큼 옮겨 초기 겹침을 막는다.
			Motion.Location += FVector(ServerDriveSnapshot.Location) - FVector(Comparison->Location);
		}
		FRotator Rotation = Motion.Rotation.Rotator();
		Rotation.Yaw += Motion.ImpactYawRate * ContactTimeShift;
		Motion.Rotation = Rotation.Quaternion();
	}
	if (AckedCount > 0)
	{
		PredictedMoves.RemoveAt(0, AckedCount, EAllowShrinking::No);
	}
	if (!bSameAck || bNeedsCorrection)
	{
		LastAuthoritativeSnapshot = ServerDriveSnapshot;
		LastAckedContactMotions = AckedContactMotions;
		LastAckedContactTime = ServerDriveSnapshot.ServerTimeSeconds;
	}

	if (!bNeedsCorrection)
	{
		return;
	}

	const FTransform PreviousPresentation = DrivePresentation->GetComponentTransform();
	const FTransform PreviousRoot = CollisionRoot->GetComponentTransform();
	// ACK 시점에 살아 있던 상대의 임시 반응도 복원해 같은 접촉을 처음부터 다시 만들지 않는다.
	PredictedContactMotions = AckedContactMotions;
	VehicleMovement->ApplyDriveSnapshot(ServerDriveSnapshot);
	PreviousDriveTransform = CollisionRoot->GetComponentTransform();
	bHasPreviousDriveTransform = false;
	for (FPredictedMove& Move : PredictedMoves)
	{
		SimulatePredictedMove(Move, true);
	}

	// 보정 오프셋은 원본 충돌체가 아니라 화면에 표시할 현재 시각의 위치를 기준으로 구한다.
	const FTransform CorrectedRoot = CollisionRoot->GetComponentTransform();
	const FTransform CorrectedPresentation = GetLocalDrivePresentation();
	const double VisibleDistance = FVector::Dist(PreviousPresentation.GetLocation(), CorrectedPresentation.GetLocation());
	const double VisibleAngle = FMath::RadiansToDegrees(PreviousPresentation.GetRotation().AngularDistance(CorrectedPresentation.GetRotation()));
	if (VisibleDistance <= MaxVisualCorrectionDistance && VisibleAngle <= MaxVisualCorrectionAngle)
	{
		PresentationCorrectionOffset = PreviousPresentation.GetLocation() - CorrectedPresentation.GetLocation();
		PresentationCorrectionRotation = (PreviousPresentation.GetRotation() * CorrectedPresentation.GetRotation().Inverse()).GetNormalized();
	}
	else
	{
		PresentationCorrectionOffset = FVector::ZeroVector;
		PresentationCorrectionRotation = FQuat::Identity;
	}
	UE_LOG(LogMWVehicleNet, Verbose, TEXT("%s corrected stream %u ack %u (same=%d); replaying %d moves; root delta %.1f cm / %.1f deg"),
		*GetName(), ServerDriveSnapshot.StreamId, Ack, bSameAck, PredictedMoves.Num(),
		FVector::Dist(PreviousRoot.GetLocation(), CorrectedRoot.GetLocation()),
		FMath::RadiansToDegrees(PreviousRoot.GetRotation().AngularDistance(CorrectedRoot.GetRotation())));
}

void AMWVehiclePawn::SmoothDrivePresentation(float DeltaTime)
{
	// 평상시 짧은 표시 예측과 서버 보정 완화는 서로 다른 값으로 관리한다.
	// 카메라와 차체가 같은 표시 기준을 공유하게 한다.
	const bool bNearContact = ContactPresentationTime > 0.f && !ContactPresentationNormal.IsNearlyZero();
	const float Remaining = FMath::Exp2(-DeltaTime / (bNearContact ? 0.035f : PresentationHalfLife));
	PresentationCorrectionOffset = PresentationCorrectionOffset.IsNearlyZero(0.05)
		? FVector::ZeroVector : PresentationCorrectionOffset * Remaining;
	PresentationCorrectionRotation = PresentationCorrectionRotation.IsIdentity(UE_KINDA_SMALL_NUMBER)
		? FQuat::Identity : FQuat::Slerp(FQuat::Identity, PresentationCorrectionRotation, Remaining);
	const FTransform Presentation = GetLocalDrivePresentation();
	DrivePresentation->SetWorldLocationAndRotation(Presentation.GetLocation() + PresentationCorrectionOffset,
		(PresentationCorrectionRotation * Presentation.GetRotation()).GetNormalized());
}

FTransform AMWVehiclePawn::GetLocalDrivePresentation() const
{
	const FTransform Current = CollisionRoot->GetComponentTransform();
	// 과거 화면을 한 스텝 늦게 재생하지 않고, 계산 뒤 남은 8.33ms 미만의 시간만 현재까지 추정한다.
	const float Remainder = FMath::Clamp(DriveStepTime, 0.f, DriveStep);
	FRotator Rotation = Current.Rotator();
	if (bHasPreviousDriveTransform)
	{
		Rotation.Yaw += FMath::FindDeltaAngleDegrees(PreviousDriveTransform.Rotator().Yaw, Rotation.Yaw) * (Remainder / DriveStep);
	}
	TArray<FMWVehicleContactSample, TInlineAllocator<3>> Contacts;
	const double SimulationTime = GetDrivePredictionTime();
	for (TActorIterator<AMWVehiclePawn> It(GetWorld()); It; ++It)
	{
		FMWVehicleContactSample Contact;
		if (GetContactPrediction(*It, SimulationTime, Contact, true))
		{
			Contacts.Add(Contact);
		}
	}
	const FTransform Desired(Rotation, Current.GetLocation() + VehicleMovement->Velocity * Remainder, Current.GetScale3D());
	return VehicleMovement->LimitDrivePresentation(Desired, Remainder, Contacts);
}

void AMWVehiclePawn::ResetDrivePresentation()
{
	PreviousDriveTransform = CollisionRoot->GetComponentTransform();
	bHasPreviousDriveTransform = false;
	PresentationCorrectionOffset = FVector::ZeroVector;
	PresentationCorrectionRotation = FQuat::Identity;
	ContactPresentationTime = 0.f;
	ContactPresentationWeight = 0.f;
	ContactPresentationOther.Reset();
	bContactPresentationPending = false;
	DrivePresentation->SetRelativeLocationAndRotation(FVector::ZeroVector, FQuat::Identity);
	if (UMWVehicleSpringArmComponent* SpringArm = Cast<UMWVehicleSpringArmComponent>(CameraBoom))
	{
		SpringArm->ResetCameraLag();
	}
}

void AMWVehiclePawn::TickProxyDrive(float DeltaTime)
{
	FMWVehicleDriveSnapshot Presentation = GetProxyDrivePresentation(DeltaTime);
	UpdateNearbyPresentation(Presentation, DeltaTime);
	VehicleMovement->ApplyDriveSnapshot(Presentation);
}

bool AMWVehiclePawn::GetContactPrediction(const AMWVehiclePawn* Other, double ServerTime,
	FMWVehicleContactSample& OutContact, bool bUseContactMotion) const
{
	if (!IsValid(Other) || Other == this || !Other->VehicleMovement->bDriveConfigReady
		|| !Other->CollisionRoot->IsQueryCollisionEnabled() || Other->ServerDriveSnapshot.StreamId == 0
		|| CollisionRoot->GetCollisionResponseToChannel(Other->CollisionRoot->GetCollisionObjectType()) != ECR_Block
		|| Other->CollisionRoot->GetCollisionResponseToChannel(CollisionRoot->GetCollisionObjectType()) != ECR_Block)
	{
		return false;
	}
	const FMWVehicleDriveSnapshot& Snapshot = Other->ServerDriveSnapshot;
	OutContact.Vehicle = FObjectKey(Other);
	OutContact.StreamId = Snapshot.StreamId;
	OutContact.Revision = Snapshot.Revision;
	OutContact.Extent = Other->CollisionRoot->GetScaledBoxExtent();
	OutContact.Tuning = Other->VehicleMovement->GetContactTuning();
	OutContact.SourceTime = Snapshot.ServerTimeSeconds;
	OutContact.SourceLocation = Snapshot.Location;
	OutContact.SourceVelocity = Snapshot.Velocity;
	OutContact.SourceRotation = Snapshot.Rotation.Quaternion();
	if (HasAuthority())
	{
		OutContact.Location = Other->GetActorLocation();
		OutContact.Rotation = Other->GetActorQuat();
		OutContact.Velocity = Other->VehicleMovement->Velocity;
		OutContact.ImpactYawRate = Other->VehicleMovement->ImpactYawRate;
		return true;
	}
	const double Age = ServerTime - Snapshot.ServerTimeSeconds;
	const double Lead = FMath::Clamp(Age, 0.0, MaxContactExtrapolation);
	OutContact.Location = FVector(Snapshot.Location) + FVector(Snapshot.Velocity) * Lead;
	FRotator Rotation = Snapshot.Rotation;
	Rotation.Yaw += Snapshot.ImpactYawRate * Lead;
	OutContact.Rotation = Rotation.Quaternion();
	const double SourceAge = GetDriveServerTime() - Snapshot.ServerTimeSeconds;
	OutContact.Velocity = SourceAge <= MaxContactSampleAge ? FVector(Snapshot.Velocity) : FVector::ZeroVector;
	OutContact.ImpactYawRate = Snapshot.ImpactYawRate;
	if (bUseContactMotion)
	{
		const FPredictedContactMotion* Motion = PredictedContactMotions.FindByPredicate([Other, &Snapshot](const FPredictedContactMotion& Entry)
		{
			return Entry.Vehicle == FObjectKey(Other) && Entry.StreamId == Snapshot.StreamId;
		});
		if (Motion)
		{
			const double MotionTime = GetDrivePredictionTime();
			const double MotionLead = FMath::Clamp(ServerTime - MotionTime, 0.0, static_cast<double>(DriveStep));
			OutContact.Location = Motion->Location + Motion->Velocity * MotionLead;
			OutContact.Velocity = Motion->Velocity;
			FRotator MotionRotation = Motion->Rotation.Rotator();
			MotionRotation.Yaw += Motion->ImpactYawRate * MotionLead;
			OutContact.Rotation = MotionRotation.Quaternion();
			OutContact.ImpactYawRate = Motion->ImpactYawRate;
		}
	}
	return true;
}

void AMWVehiclePawn::NotifyDriveContact(AMWVehiclePawn* Other, const FVector& Normal)
{
	ContactPresentationOther = Other;
	ContactPresentationNormal = Normal.GetSafeNormal2D();
	ContactPresentationTime = 0.18f;
	bContactPresentationPending = true;
}

void AMWVehiclePawn::UpdateDrivePresentation(float DeltaTime)
{
	ContactPresentationTime = FMath::Max(0.f, ContactPresentationTime - DeltaTime);
	const bool bLocalDriver = IsLocallyControlled() || !IsPlayerControlled();
	if (bLocalDriver)
	{
		PresentationTick->SetLocalPrerequisite(nullptr);
	}
	if (HasAuthority())
	{
		if (bLocalDriver)
		{
			SmoothDrivePresentation(DeltaTime);
		}
		else
		{
			FMWVehicleDriveSnapshot Presentation = GetProxyDrivePresentation(DeltaTime);
			UpdateNearbyPresentation(Presentation, DeltaTime);
			DrivePresentation->SetWorldLocationAndRotation(Presentation.Location, Presentation.Rotation);
		}
	}
	else if (GetLocalRole() == ROLE_AutonomousProxy && IsLocallyControlled())
	{
		SmoothDrivePresentation(DeltaTime);
	}
	else if (ServerDriveSnapshot.StreamId != 0)
	{
		TickProxyDrive(DeltaTime);
	}
	bContactPresentationPending = false;
}

void AMWVehiclePawn::UpdateNearbyPresentation(FMWVehicleDriveSnapshot& Presentation, float DeltaTime)
{
	AMWVehiclePawn* LocalVehicle = nullptr;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* LocalController = It->Get();
		if (LocalController && LocalController->IsLocalController())
		{
			LocalVehicle = Cast<AMWVehiclePawn>(LocalController->GetPawn());
			break;
		}
	}
	PresentationTick->SetLocalPrerequisite(LocalVehicle && LocalVehicle != this ? LocalVehicle->PresentationTick : nullptr);
	if (!LocalVehicle || LocalVehicle == this)
	{
		return;
	}
	FMWVehicleContactSample Contact;
	const double PresentationTime = LocalVehicle->GetDrivePredictionTime() + FMath::Clamp(LocalVehicle->DriveStepTime, 0.f, DriveStep);
	if (!LocalVehicle->GetContactPrediction(this, PresentationTime, Contact, true))
	{
		return;
	}
	const FTransform LocalPose = LocalVehicle->DrivePresentation->GetComponentTransform();
	const FVector Direction = (Contact.Location - LocalPose.GetLocation()).GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		return;
	}
	const auto Radius = [](const FVector& Axis, const FQuat& Rotation, const FVector& Extent)
	{
		return FMath::Abs(Axis | Rotation.GetAxisX()) * Extent.X
			+ FMath::Abs(Axis | Rotation.GetAxisY()) * Extent.Y
			+ FMath::Abs(Axis | Rotation.GetAxisZ()) * Extent.Z;
	};
	const double LocalRadius = Radius(Direction, LocalPose.GetRotation(), LocalVehicle->CollisionRoot->GetScaledBoxExtent());
	const double RemoteRadius = Radius(Direction, Contact.Rotation, Contact.Extent);
	const double Gap = FVector::Dist2D(LocalPose.GetLocation(), Contact.Location) - LocalRadius - RemoteRadius;
	const double ClosingSpeed = (LocalVehicle->VehicleMovement->Velocity - Contact.Velocity) | Direction;
	const bool bRecentContact = ContactPresentationOther == LocalVehicle && ContactPresentationTime > 0.f;
	const bool bApproaching = ClosingSpeed > 20.0 && Gap < ClosingSpeed * 0.35 + 50.0;
	const bool bNearContact = Gap < 35.0 || bApproaching || bRecentContact;
	ContactPresentationWeight = FMath::FInterpConstantTo(ContactPresentationWeight, bNearContact ? 1.f : 0.f,
		DeltaTime, bNearContact ? 10.f : 4.f);
	if (bNearContact && Gap < 35.0)
	{
		LocalVehicle->ContactPresentationNormal = -Direction;
		LocalVehicle->ContactPresentationTime = FMath::Max(LocalVehicle->ContactPresentationTime, 0.1f);
	}
	// 내 차가 서버 보정을 완화하는 동안 붙어 있는 두 외형은 같은 보정 좌표계를 쓴다.
	// 충돌체나 예측 원본은 바꾸지 않으므로 입력 처리와 서버 판정에 표시 지연을 더하지 않는다.
	const FVector LocalSimulationLocation = LocalPose.GetLocation() - LocalVehicle->PresentationCorrectionOffset;
	const FQuat LocalCorrection = LocalVehicle->PresentationCorrectionRotation;
	const FVector NearbyLocation = LocalPose.GetLocation() + LocalCorrection.RotateVector(
		Contact.Location - LocalSimulationLocation);
	const FQuat NearbyRotation = (LocalCorrection * Contact.Rotation).GetNormalized();
	// 멀리서는 기존 보간을 유지하고, 닿기 전에는 예측에 사용한 같은 위치로 표시 시점을 맞춘다.
	Presentation.Location = FMath::Lerp(FVector(Presentation.Location), NearbyLocation, ContactPresentationWeight);
	Presentation.Rotation = FQuat::Slerp(Presentation.Rotation.Quaternion(), NearbyRotation, ContactPresentationWeight).Rotator();
	Presentation.Velocity = FMath::Lerp(FVector(Presentation.Velocity), Contact.Velocity, ContactPresentationWeight);
	const FVector Normal = bRecentContact ? LocalCorrection.RotateVector(ContactPresentationNormal)
		: (NearbyLocation - LocalPose.GetLocation()).GetSafeNormal2D();
	const FTransform Adjusted = LocalVehicle->VehicleMovement->ConstrainContactPresentation(LocalPose,
		FTransform(Presentation.Rotation, Presentation.Location), Contact.Extent, Normal,
		bContactPresentationPending && bRecentContact);
	Presentation.Location = Adjusted.GetLocation();
}

FMWVehicleDriveSnapshot AMWVehiclePawn::GetProxyDrivePresentation(float DeltaTime)
{
	const double ServerNow = GetDriveServerTime();
	if (AppliedStreamId != ServerDriveSnapshot.StreamId)
	{
		// 텔레포트나 조종 변경은 이전 경로와 이어서 보간하지 않는다.
		ContactPresentationWeight = 0.f;
		ContactPresentationTime = 0.f;
		ContactPresentationOther.Reset();
		bContactPresentationPending = false;
		AppliedStreamId = ServerDriveSnapshot.StreamId;
		AppliedRevision = ServerDriveSnapshot.Revision;
		ProxySnapshots.Reset();
		ProxySnapshots.Add(ServerDriveSnapshot);
		ProxyInterpolationDelay = FMath::Clamp(ServerNow - ServerDriveSnapshot.ServerTimeSeconds, 0.0, 0.30) + ProxyBufferTime;
		ProxyPlaybackTime = ServerDriveSnapshot.ServerTimeSeconds - ProxyBufferTime;
		return ServerDriveSnapshot;
	}

	if (IsNewerSequence(ServerDriveSnapshot.Revision, AppliedRevision))
	{
		AppliedRevision = ServerDriveSnapshot.Revision;
		const double SampleTime = ServerDriveSnapshot.ServerTimeSeconds;
		const FMWVehicleDriveSnapshot& Last = ProxySnapshots.Last();
		if (SampleTime > Last.ServerTimeSeconds)
		{
			// 정지 중에는 같은 스냅샷을 보내지 않는다. 새 움직임을 정지 구간 전체로 늘려 보간하지 않는다.
			if (SampleTime - Last.ServerTimeSeconds > 0.2 && Last.Velocity.IsNearlyZero(1.0)
				&& FMath::IsNearlyZero(Last.ImpactYawRate))
			{
				FMWVehicleDriveSnapshot Hold = Last;
				Hold.ServerTimeSeconds = SampleTime - SnapshotInterval;
				ProxySnapshots.Add(Hold);
			}
			ProxySnapshots.Add(ServerDriveSnapshot);
		}
		else if (SampleTime == Last.ServerTimeSeconds)
		{
			ProxySnapshots.Last() = ServerDriveSnapshot;
		}
		if (ProxySnapshots.Num() > MaxProxySnapshots)
		{
			ProxySnapshots.RemoveAt(0, ProxySnapshots.Num() - MaxProxySnapshots, EAllowShrinking::No);
		}

		const double DesiredDelay = FMath::Clamp(ServerNow - SampleTime, 0.0, 0.30) + ProxyBufferTime;
		ProxyInterpolationDelay = FMath::Lerp(ProxyInterpolationDelay, DesiredDelay, 0.12);
	}

	// 패킷 도착 시점으로 돌아가지 않고 서버 시간축을 재생한다. 시간 오차는 재생 속도로 조금씩 줄인다.
	const double TargetTime = ServerNow - ProxyInterpolationDelay;
	const double TimeError = TargetTime - ProxyPlaybackTime;
	ProxyPlaybackTime += FMath::Min(static_cast<double>(DeltaTime), 0.10) * FMath::Clamp(1.0 + TimeError * 3.0, 0.85, 1.15);
	while (ProxySnapshots.Num() > 2 && ProxySnapshots[1].ServerTimeSeconds <= ProxyPlaybackTime)
	{
		ProxySnapshots.RemoveAt(0, 1, EAllowShrinking::No);
	}

	const FMWVehicleDriveSnapshot& First = ProxySnapshots[0];
	FMWVehicleDriveSnapshot Presentation = First;
	if (ProxySnapshots.Num() >= 2)
	{
		const FMWVehicleDriveSnapshot& Next = ProxySnapshots[1];
		const double Duration = Next.ServerTimeSeconds - First.ServerTimeSeconds;
		const double Alpha = FMath::Clamp((ProxyPlaybackTime - First.ServerTimeSeconds) / Duration, 0.0, 1.0);
		Presentation.Location = FMath::Lerp(FVector(First.Location), FVector(Next.Location), Alpha);
		Presentation.Rotation = FQuat::Slerp(First.Rotation.Quaternion(), Next.Rotation.Quaternion(), Alpha).Rotator();
		Presentation.Velocity = FMath::Lerp(FVector(First.Velocity), FVector(Next.Velocity), Alpha);
		Presentation.ImpactYawRate = FMath::Lerp(First.ImpactYawRate, Next.ImpactYawRate, Alpha);
		Presentation.bGrounded = Alpha < 0.5 ? First.bGrounded : Next.bGrounded;
	}

	const FMWVehicleDriveSnapshot& Latest = ProxySnapshots.Last();
	if (ProxyPlaybackTime > Latest.ServerTimeSeconds)
	{
		const double Elapsed = ProxyPlaybackTime - Latest.ServerTimeSeconds;
		const double Lead = FMath::Min(Elapsed, MaxProxyExtrapolation);
		Presentation = Latest;
		Presentation.Location = FVector(Latest.Location) + FVector(Latest.Velocity) * Lead;
		Presentation.Rotation.Yaw += Latest.ImpactYawRate * Lead;
		if (Elapsed > MaxProxyExtrapolation)
		{
			// 장시간 입력 유실 동안 마지막 속도로 계속 화면 밖까지 밀어내지 않는다.
			Presentation.Velocity = FVector::ZeroVector;
			Presentation.ImpactYawRate = 0.f;
		}
	}
	return Presentation;
}
