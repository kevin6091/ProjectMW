#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "Gameplay/Vehicle/MWVehicleMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogMWVehicleNet, Log, All);

namespace
{
	constexpr float DriveStep = 1.f / 120.f;
	constexpr float MaxDriveStepTime = 12.f * DriveStep;
	constexpr float SnapshotInterval = 1.f / 20.f;
	constexpr float SendInterval = 1.f / 30.f;
	constexpr float RemoteInputTimeout = 1.f;
	constexpr int32 MaxMovesPerPacket = 32;
	constexpr int32 MaxPendingMoves = 120;

	bool IsValidDriveInput(const FMWVehicleDriveInput& Input)
	{
		return FMath::IsWithinInclusive(Input.Throttle, 0.f, 1.f)
			&& FMath::IsWithinInclusive(Input.Reverse, 0.f, 1.f)
			&& FMath::IsWithinInclusive(Input.Steering, -1.f, 1.f);
	}
}

void AMWVehiclePawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMWVehiclePawn, ServerDriveSnapshot);
}

void AMWVehiclePawn::ResetDriveStream()
{
	if (!HasAuthority() || GetNetMode() == NM_Standalone)
	{
		return;
	}

	QueuedServerMoves.Reset();
	LastProcessedMoveId = 0;
	DriveStepTime = 0.f;
	SnapshotTime = 0.f;
	RemoteInputSilence = 0.f;
	bHadRemoteInput = false;
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
		&& Snapshot.bGrounded == ServerDriveSnapshot.bGrounded)
	{
		return;
	}

	Snapshot.StreamId += bNewStream ? 1 : 0;
	Snapshot.LastProcessedMoveId = LastProcessedMoveId;
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
				break;
			}
			LastProcessedMoveId = ExpectedMove;
		}
		else if (!bRemoteDriver && GetController())
		{
			StepInput = DriveInput;
		}
		VehicleMovement->SetDriveInput(StepInput);
		VehicleMovement->SimulateDriveStep(DriveStep);
		DriveStepTime -= DriveStep;
	}

	SnapshotTime += DeltaTime;
	if (SnapshotTime >= SnapshotInterval)
	{
		SnapshotTime = FMath::Fmod(SnapshotTime, SnapshotInterval);
		PublishDriveSnapshot();
	}
}

void AMWVehiclePawn::TickPredictedDrive(float DeltaTime)
{
	if (AppliedStreamId != ServerDriveSnapshot.StreamId)
	{
		PredictedMoves.Reset();
		NextMoveId = ServerDriveSnapshot.LastProcessedMoveId + 1;
		DriveStepTime = 0.f;
		VehicleMovement->ApplyDriveSnapshot(ServerDriveSnapshot);
		AppliedStreamId = ServerDriveSnapshot.StreamId;
		AppliedRevision = ServerDriveSnapshot.Revision;
	}
	else if (AppliedRevision != ServerDriveSnapshot.Revision)
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
		VehicleMovement->SetDriveInput(Move.Command.Input);
		VehicleMovement->SimulateDriveStep(DriveStep);
		VehicleMovement->CaptureDriveSnapshot(Move.Result);
		DriveStepTime -= DriveStep;
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

void AMWVehiclePawn::ReconcilePredictedDrive()
{
	const uint32 Ack = ServerDriveSnapshot.LastProcessedMoveId;
	int32 AckedCount = 0;
	while (AckedCount < PredictedMoves.Num() && Ack - PredictedMoves[AckedCount].Command.MoveId < MaxPendingMoves)
	{
		++AckedCount;
	}

	// 현재 화면 위치에는 서버가 아직 처리하지 않은 입력이 반영되어 있다.
	// 오차는 서버와 같은 순번의 예측 결과끼리 비교한다.
	bool bNeedsCorrection = AckedCount == 0;
	if (AckedCount > 0)
	{
		const FMWVehicleDriveSnapshot& Prediction = PredictedMoves[AckedCount - 1].Result;
		bNeedsCorrection = !Prediction.Location.Equals(ServerDriveSnapshot.Location, 3.f)
			|| !Prediction.Rotation.Equals(ServerDriveSnapshot.Rotation, 1.f)
			|| !Prediction.Velocity.Equals(ServerDriveSnapshot.Velocity, 5.f)
			|| Prediction.bGrounded != ServerDriveSnapshot.bGrounded;
		PredictedMoves.RemoveAt(0, AckedCount, EAllowShrinking::No);
	}

	if (bNeedsCorrection)
	{
		UE_LOG(LogMWVehicleNet, Verbose, TEXT("%s corrected stream %u ack %u; replaying %d moves"),
			*GetName(), ServerDriveSnapshot.StreamId, Ack, PredictedMoves.Num());
		VehicleMovement->ApplyDriveSnapshot(ServerDriveSnapshot);
		for (FPredictedMove& Move : PredictedMoves)
		{
			VehicleMovement->SetDriveInput(Move.Command.Input);
			VehicleMovement->SimulateDriveStep(DriveStep);
			VehicleMovement->CaptureDriveSnapshot(Move.Result);
		}
	}
}

void AMWVehiclePawn::TickProxyDrive(float DeltaTime)
{
	if (AppliedStreamId != ServerDriveSnapshot.StreamId)
	{
		// 텔레포트나 새 조종 스트림은 중간 위치를 거치지 않는다.
		VehicleMovement->ApplyDriveSnapshot(ServerDriveSnapshot);
		AppliedStreamId = ServerDriveSnapshot.StreamId;
		AppliedRevision = ServerDriveSnapshot.Revision;
		ProxyBlendTime = SnapshotInterval;
	}
	else if (AppliedRevision != ServerDriveSnapshot.Revision)
	{
		VehicleMovement->CaptureDriveSnapshot(ProxyStart);
		AppliedRevision = ServerDriveSnapshot.Revision;
		ProxyBlendTime = 0.f;
	}

	if (ProxyBlendTime < SnapshotInterval)
	{
		ProxyBlendTime = FMath::Min(ProxyBlendTime + DeltaTime, SnapshotInterval);
		const float Alpha = ProxyBlendTime / SnapshotInterval;
		FMWVehicleDriveSnapshot Presentation = ServerDriveSnapshot;
		Presentation.Location = FMath::Lerp(FVector(ProxyStart.Location), FVector(ServerDriveSnapshot.Location), Alpha);
		Presentation.Rotation = FQuat::Slerp(ProxyStart.Rotation.Quaternion(), ServerDriveSnapshot.Rotation.Quaternion(), Alpha).Rotator();
		VehicleMovement->ApplyDriveSnapshot(Presentation);
	}
}
