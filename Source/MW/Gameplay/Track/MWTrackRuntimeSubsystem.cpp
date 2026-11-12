#include "Gameplay/Track/MWTrackRuntimeSubsystem.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Gameplay/Track/MWTrackRuntimeComponent.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FName PreviewTag(TEXT("MWTrackVisualOnly"));
}

UMWTrackRuntimeSubsystem::UMWTrackRuntimeSubsystem()
{
	static ConstructorHelpers::FObjectFinderOptional<UMWTrackRuntimeData> RuntimeData(
		TEXT("/Game/Gameplay/Tracks/Data/DA_TrackRuntime.DA_TrackRuntime"));
	Catalog = RuntimeData.Get();
}

bool UMWTrackRuntimeSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UMWTrackRuntimeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!Catalog)
	{
		return;
	}

	for (const FMWTrackPieceRuntimeData& Piece : Catalog->Pieces)
	{
		Pieces.Add(Piece.VisualClass, &Piece);
	}
	ActorSpawnHandle = InWorld.AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(
		this, &UMWTrackRuntimeSubsystem::OnActorSpawned));
	for (TActorIterator<AActor> Actor(&InWorld); Actor; ++Actor)
	{
		AttachTrack(*Actor, true);
	}
}

void UMWTrackRuntimeSubsystem::Deinitialize()
{
	if (ActorSpawnHandle.IsValid())
	{
		GetWorld()->RemoveOnActorSpawnedHandler(ActorSpawnHandle);
	}
	Super::Deinitialize();
}

void UMWTrackRuntimeSubsystem::OnActorSpawned(AActor* Actor)
{
	if (!Pieces.Contains(Actor->GetClass()->GetFName()))
	{
		return;
	}
	GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,
		[this, Pending = TWeakObjectPtr<AActor>(Actor)] { AttachTrack(Pending.Get()); }));
}

void UMWTrackRuntimeSubsystem::AttachTrack(AActor* Actor, bool bInitialWorldActor)
{
	if (!IsValid(Actor) || !Actor->GetRootComponent() || Actor->FindComponentByClass<UMWTrackRuntimeComponent>())
	{
		return;
	}
	const FMWTrackPieceRuntimeData* const* Data = Pieces.Find(Actor->GetClass()->GetFName());
	if (!Data)
	{
		return;
	}

	const bool bLift = (*Data)->Motions.ContainsByPredicate([](const FMWTrackMotionData& Motion) { return Motion.Type == EMWTrackMotion::Lift; });
	if (GetWorld()->GetNetMode() == NM_Client && bLift && !Actor->ActorHasTag(PreviewTag))
	{
		// 서버가 같은 컴포넌트를 복제하므로 클라이언트에서 별도 생성해 이중 이동시키지 않는다.
		return;
	}
	UMWTrackRuntimeComponent* Runtime = NewObject<UMWTrackRuntimeComponent>(Actor);
	Actor->AddInstanceComponent(Runtime);
	Runtime->SetPreview(Actor->ActorHasTag(PreviewTag));
	Runtime->Configure(**Data);
	const bool bReplicatedLift = bLift && !Actor->ActorHasTag(PreviewTag);
	Runtime->SetIsReplicated(bReplicatedLift);
	Runtime->RegisterComponent();
	// 레벨에 저장된 승강기도 액터 채널이 있어야 작동 시각이 복제된다.
	// 건설 중인 미확정 트랙은 기존 게시 절차가 끝날 때까지 공개하지 않는다.
	if (bReplicatedLift && bInitialWorldActor && GetWorld()->GetNetMode() != NM_Client)
	{
		// 탑승자가 트랙 원점에서 멀어도 발판의 복제가 끊기지 않도록 기존 확정 트랙과 같은 가시성 규칙을 쓴다.
		Actor->bAlwaysRelevant = true;
		Actor->SetReplicates(true);
	}
}

void UMWTrackRuntimeSubsystem::ConfigureRuntimeComponent(UMWTrackRuntimeComponent& Component)
{
	const AActor* Actor = Component.GetOwner();
	if (!Catalog || !IsValid(Actor))
	{
		return;
	}
	// 클라이언트의 월드 초기화보다 먼저 호출될 수 있으므로 아직 완성되지 않은 조회 표에 의존하지 않는다.
	const FMWTrackPieceRuntimeData* Data = Catalog->Pieces.FindByPredicate([Actor](const FMWTrackPieceRuntimeData& Entry)
	{
		return Entry.VisualClass == Actor->GetClass()->GetFName();
	});
	if (Data)
	{
		Component.SetPreview(Actor->ActorHasTag(PreviewTag));
		Component.Configure(*Data);
	}
}

void UMWTrackRuntimeSubsystem::SetTrackPreview(AActor& Actor, bool bPreview)
{
	if (bPreview)
	{
		Actor.Tags.AddUnique(PreviewTag);
	}
	else
	{
		Actor.Tags.Remove(PreviewTag);
	}
	if (UMWTrackRuntimeComponent* Runtime = Actor.FindComponentByClass<UMWTrackRuntimeComponent>())
	{
		Runtime->SetPreview(bPreview);
	}
}

void UMWTrackRuntimeSubsystem::ProcessRaceTrigger(AMWVehiclePawn& Vehicle, AActor& Track,
	EMWTrackTrigger Trigger, const TArray<FTransform>& RespawnPoints)
{
	if (!Vehicle.HasAuthority())
	{
		return;
	}
	for (auto It = Racers.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}

	FRacer& Racer = Racers.FindOrAdd(&Vehicle);
	const double Now = GetWorld()->GetTimeSeconds();
	if (Racer.Progress.bFinished)
	{
		return;
	}
	if (Racer.StartTime < 0)
	{
		if (Trigger == EMWTrackTrigger::Finish)
		{
			return;
		}
		Racer.StartTime = Now;
	}
	if (Trigger == EMWTrackTrigger::Checkpoint && !Racer.Passed.Contains(&Track))
	{
		Racer.Passed.Add(&Track);
		++Racer.Progress.Checkpoints;
		Racer.RespawnTransforms.Reset(RespawnPoints.Num());
		for (const FTransform& Point : RespawnPoints)
		{
			Racer.RespawnTransforms.Add(Point * Track.GetActorTransform());
		}
	}
	if (Trigger == EMWTrackTrigger::Finish)
	{
		Racer.Progress.bFinished = true;
		Racer.Progress.ElapsedSeconds = static_cast<float>(Now - Racer.StartTime);
	}
}

FMWTrackRaceProgress UMWTrackRuntimeSubsystem::GetTrackRaceProgress(AMWVehiclePawn* Vehicle) const
{
	if (const FRacer* Racer = Racers.Find(Vehicle))
	{
		return Racer->Progress;
	}
	return {};
}

bool UMWTrackRuntimeSubsystem::RespawnAtTrackCheckpoint(AMWVehiclePawn* Vehicle)
{
	if (!IsValid(Vehicle) || !Vehicle->HasAuthority())
	{
		return false;
	}
	const FRacer* Racer = Racers.Find(Vehicle);
	if (!Racer || Racer->Progress.bFinished)
	{
		return false;
	}
	for (const FTransform& Point : Racer->RespawnTransforms)
	{
		if (Vehicle->TeleportTo(Point.GetLocation(), FRotationMatrix::MakeFromXZ(Point.GetUnitAxis(EAxis::Y), Point.GetUnitAxis(EAxis::Z)).Rotator()))
		{
			return true;
		}
	}
	return false;
}
