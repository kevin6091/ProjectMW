#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "Gameplay/Vehicle/MWVehicleMovementComponent.h"
#include "Gameplay/Vehicle/MWVehicleSpringArmComponent.h"
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
	constexpr double MaxContactExtrapolation = 0.10;
	constexpr double MaxContactSampleAge = 0.35;
	constexpr float PredictedContactLifetime = 0.10f;
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
		if (GetNetMode() != NM_DedicatedServer)
		{
			if (IsPlayerControlled() && !IsLocallyControlled())
			{
				// 호스트가 보는 참가자도 입력 패킷 도착 간격과 화면 표시 간격을 분리한다.
				const FMWVehicleDriveSnapshot Presentation = GetProxyDrivePresentation(DeltaTime);
				DrivePresentation->SetWorldLocationAndRotation(Presentation.Location, Presentation.Rotation);
			}
			else
			{
				SmoothDrivePresentation(DeltaTime);
			}
		}
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
	else
	{
		TickProxyDrive(DeltaTime);
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
		NextMoveId = ServerDriveSnapshot.LastProcessedMoveId + 1;
		DriveStepTime = 0.f;
		VehicleMovement->ApplyDriveSnapshot(ServerDriveSnapshot);
		LastAuthoritativeSnapshot = ServerDriveSnapshot;
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

	const double ServerNow = GetDriveServerTime();
	DriveStepTime = FMath::Min(DriveStepTime + DeltaTime, MaxDriveStepTime);
	while (DriveStepTime >= DriveStep && PredictedMoves.Num() < MaxPendingMoves)
	{
		FPredictedMove& Move = PredictedMoves.AddDefaulted_GetRef();
		Move.Command.MoveId = NextMoveId++;
		Move.Command.Input = IsValidDriveInput(DriveInput) ? DriveInput : FMWVehicleDriveInput{};
		for (const AMWVehiclePawn* Other : ContactSources)
		{
			const FMWVehicleDriveSnapshot& Snapshot = Other->ServerDriveSnapshot;
			const double Age = ServerNow - DriveStepTime - Snapshot.ServerTimeSeconds;
			const double Lead = FMath::Clamp(Age, 0.0, MaxContactExtrapolation);
			const FVector Velocity = Snapshot.Velocity;
			FMWVehicleContactSample& Sample = Move.Contacts.AddDefaulted_GetRef();
			Sample.Vehicle = FObjectKey(Other);
			Sample.StreamId = Snapshot.StreamId;
			Sample.Revision = Snapshot.Revision;
			Sample.Location = FVector(Snapshot.Location) + Velocity * Lead;
			FRotator Rotation = Snapshot.Rotation;
			Rotation.Yaw += Snapshot.ImpactYawRate * Lead;
			Sample.Rotation = Rotation.Quaternion();
			// 오랫동안 새 상태가 없으면 마지막 근사 위치를 사용하되 상대의 이동은 더 추정하지 않는다.
			Sample.Velocity = Age <= MaxContactSampleAge ? Velocity : FVector::ZeroVector;
			Sample.Extent = Other->CollisionRoot->GetScaledBoxExtent();
			Sample.Tuning = Other->VehicleMovement->GetContactTuning();
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
	SmoothDrivePresentation(DeltaTime);
}

void AMWVehiclePawn::SimulatePredictedMove(FPredictedMove& Move)
{
	// 원본은 그대로 저장한다. 재실행에서도 같은 상대 스냅샷으로 출발해 임시 반응을 다시 계산한다.
	TArray<FMWVehicleContactSample, TInlineAllocator<3>> Contacts = Move.Contacts;
	for (int32 Index = PredictedContactMotions.Num() - 1; Index >= 0; --Index)
	{
		FPredictedContactMotion& Motion = PredictedContactMotions[Index];
		FMWVehicleContactSample* Sample = Contacts.FindByPredicate([&Motion](const FMWVehicleContactSample& Contact)
		{
			return Contact.Vehicle == Motion.Vehicle && Contact.StreamId == Motion.StreamId;
		});
		if (!Sample || Motion.RemainingTime <= 0.f)
		{
			PredictedContactMotions.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			continue;
		}
		const bool bServerShowsImpact = IsNewerSequence(Sample->Revision, Motion.Revision)
			&& FVector::DistSquared(Sample->Velocity, Motion.Velocity) < FVector::DistSquared(Sample->Velocity, Motion.BeforeImpactVelocity);
		if (bServerShowsImpact)
		{
			PredictedContactMotions.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			continue;
		}

		Sample->Location = Motion.Location;
		Sample->Velocity = Motion.Velocity;
		Motion.Location += Motion.Velocity * DriveStep;
		Motion.RemainingTime -= DriveStep;
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
		Motion->Location = Sample.Location + Sample.Velocity * DriveStep + Impact.OtherVelocityDelta * (DriveStep * (1.0 - Impact.Fraction));
	}
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
	if (AckedCount > 0)
	{
		const FPredictedMove& LastAcked = PredictedMoves[AckedCount - 1];
		if (LastAcked.Command.MoveId == Ack)
		{
			Comparison = &LastAcked.Result;
		}
	}
	const bool bNeedsCorrection = !Comparison || NeedsDriveCorrection(*Comparison, ServerDriveSnapshot);
	if (AckedCount > 0)
	{
		PredictedMoves.RemoveAt(0, AckedCount, EAllowShrinking::No);
	}
	if (!bSameAck || bNeedsCorrection)
	{
		LastAuthoritativeSnapshot = ServerDriveSnapshot;
	}

	if (!bNeedsCorrection)
	{
		return;
	}

	const FTransform PreviousPresentation = DrivePresentation->GetComponentTransform();
	const FTransform PreviousRoot = CollisionRoot->GetComponentTransform();
	PredictedContactMotions.Reset();
	VehicleMovement->ApplyDriveSnapshot(ServerDriveSnapshot);
	PreviousDriveTransform = CollisionRoot->GetComponentTransform();
	bHasPreviousDriveTransform = false;
	for (FPredictedMove& Move : PredictedMoves)
	{
		SimulatePredictedMove(Move);
	}

	// 보정 오프셋은 원본 충돌체가 아니라 화면에 표시할 보간 위치를 기준으로 구한다.
	const FTransform CorrectedRoot = CollisionRoot->GetComponentTransform();
	const FTransform CorrectedPresentation = GetInterpolatedDriveTransform();
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
	// 평상시 고정 스텝 보간과 서버 보정 완화는 서로 다른 값으로 관리한다.
	// 카메라와 차체가 같은 표시 기준을 공유하게 한다.
	const float Remaining = FMath::Exp2(-DeltaTime / PresentationHalfLife);
	PresentationCorrectionOffset = PresentationCorrectionOffset.IsNearlyZero(0.05)
		? FVector::ZeroVector : PresentationCorrectionOffset * Remaining;
	PresentationCorrectionRotation = PresentationCorrectionRotation.IsIdentity(UE_KINDA_SMALL_NUMBER)
		? FQuat::Identity : FQuat::Slerp(FQuat::Identity, PresentationCorrectionRotation, Remaining);
	const FTransform Presentation = GetInterpolatedDriveTransform();
	DrivePresentation->SetWorldLocationAndRotation(Presentation.GetLocation() + PresentationCorrectionOffset,
		(PresentationCorrectionRotation * Presentation.GetRotation()).GetNormalized());
}

FTransform AMWVehiclePawn::GetInterpolatedDriveTransform() const
{
	const FTransform Current = CollisionRoot->GetComponentTransform();
	if (!bHasPreviousDriveTransform)
	{
		return Current;
	}
	// 한 스텝 이전부터 현재 결과까지 재생해 100/144FPS에서도 프레임마다 일정하게 진행한다.
	const double Alpha = FMath::Clamp(DriveStepTime / DriveStep, 0.f, 1.f);
	const FVector Location = FMath::Lerp(PreviousDriveTransform.GetLocation(), Current.GetLocation(), Alpha);
	const FQuat Rotation = FQuat::Slerp(PreviousDriveTransform.GetRotation(), Current.GetRotation(), Alpha);
	return FTransform(Rotation, Location, Current.GetScale3D());
}

void AMWVehiclePawn::ResetDrivePresentation()
{
	PreviousDriveTransform = CollisionRoot->GetComponentTransform();
	bHasPreviousDriveTransform = false;
	PresentationCorrectionOffset = FVector::ZeroVector;
	PresentationCorrectionRotation = FQuat::Identity;
	DrivePresentation->SetRelativeLocationAndRotation(FVector::ZeroVector, FQuat::Identity);
	if (UMWVehicleSpringArmComponent* SpringArm = Cast<UMWVehicleSpringArmComponent>(CameraBoom))
	{
		SpringArm->ResetCameraLag();
	}
}

void AMWVehiclePawn::TickProxyDrive(float DeltaTime)
{
	VehicleMovement->ApplyDriveSnapshot(GetProxyDrivePresentation(DeltaTime));
}

FMWVehicleDriveSnapshot AMWVehiclePawn::GetProxyDrivePresentation(float DeltaTime)
{
	const double ServerNow = GetDriveServerTime();
	if (AppliedStreamId != ServerDriveSnapshot.StreamId)
	{
		// 텔레포트나 조종 변경은 이전 경로와 이어서 보간하지 않는다.
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
