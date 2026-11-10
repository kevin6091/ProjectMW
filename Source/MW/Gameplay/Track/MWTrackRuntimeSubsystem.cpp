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
		AttachTrack(*Actor);
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

void UMWTrackRuntimeSubsystem::AttachTrack(AActor* Actor)
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

	UMWTrackRuntimeComponent* Runtime = NewObject<UMWTrackRuntimeComponent>(Actor);
	Actor->AddInstanceComponent(Runtime);
	Runtime->SetPreview(Actor->ActorHasTag(PreviewTag));
	Runtime->RegisterComponent();
	Runtime->Configure(**Data);
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
