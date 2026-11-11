#include "Gameplay/Items/MWItemComponent.h"

#include "CollisionQueryParams.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "MovieSceneBinding.h"
#include "MovieSceneBindingOverrides.h"
#include "MovieSceneObjectBindingID.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
const FName BoosterBaseTag(TEXT("MWItemNode=sharedassets0.assets:1715"));
const FName BoosterBodyTag(TEXT("MWItemNode=sharedassets0.assets:1487"));
const FName BoosterNozzleTag(TEXT("MWItemNode=sharedassets0.assets:1645"));

USceneComponent* FindVisualNode(const AActor& Visual, FName NodeTag)
{
	for (USceneComponent* Component : TInlineComponentArray<USceneComponent*>(&Visual))
	{
		if (Component->ComponentHasTag(NodeTag))
		{
			return Component;
		}
	}
	return nullptr;
}

FBox GetLocalMeshBounds(const AActor& Actor)
{
	FBox Bounds(ForceInit);
	const FTransform ActorTransform = Actor.GetActorTransform();
	for (const UStaticMeshComponent* Component : TInlineComponentArray<UStaticMeshComponent*>(&Actor))
	{
		if (Component->IsVisible() && !Component->bHiddenInGame && Component->GetStaticMesh())
		{
			Bounds += Component->GetStaticMesh()->GetBoundingBox().TransformBy(
				Component->GetComponentTransform().GetRelativeTransform(ActorTransform));
		}
	}
	return Bounds;
}

struct FBoosterMount
{
	FTransform Transform;
	double ExhaustLengthScale = 6.2;
};

FBoosterMount GetBoosterMount(const AActor& Vehicle, const AActor& Booster)
{
	struct FMount
	{
		FName BodyMesh;
		FVector Origin;
		double Scale;
		double ExhaustLengthScale = 6.2;
	};
	// Seat the mounting feet into the rear deck of the visible body mesh.
	static const FMount Mounts[] = {
		// Center the mount on Doug's rear deck and end the plume before the tail fin.
		{TEXT("SM_MW_Doug_Body"), {-46.0, 0.0, 35.4}, 0.099, 2.5},
		{TEXT("SM_MW_Fred_Body"), {-37.6, 0.0, 1.4}, 0.155},
		{TEXT("SM_MW_Mammoth_Body"), {-43.7, 0.0, 43.9}, 0.115},
		{TEXT("SM_MW_Smiet_Body"), {-14.1, 0.0, 34.2}, 0.142},
		{TEXT("SM_MW_Ufology_Body"), {-44.0, 0.0, 8.2}, 0.161}
	};
	for (const UStaticMeshComponent* Component : TInlineComponentArray<UStaticMeshComponent*>(&Vehicle))
	{
		if (const UStaticMesh* Mesh = Component->GetStaticMesh())
		{
			for (const FMount& Mount : Mounts)
			{
				if (Mesh->GetFName() == Mount.BodyMesh)
				{
					return {FTransform(FRotator(0.0, -90.0, 0.0), Mount.Origin, FVector(Mount.Scale)),
						Mount.ExhaustLengthScale};
				}
			}
		}
	}

	const FBox Bounds = GetLocalMeshBounds(Vehicle);
	const FBox BoosterBounds = GetLocalMeshBounds(Booster);
	if (!Bounds.IsValid || !BoosterBounds.IsValid || BoosterBounds.GetSize().X <= UE_SMALL_NUMBER)
	{
		return {FTransform(FRotator(0.f, -90.f, 0.f), FVector(-65.f, 0.f, 10.f), FVector(0.05f))};
	}

	const FVector Size = Bounds.GetSize();
	// The source assembly faces +Y, so its X size becomes the mounted width.
	const double Scale = 0.58 * Size.Y / BoosterBounds.GetSize().X;
	FVector Mount(Bounds.Min.X + Size.X * 0.23, Bounds.GetCenter().Y, Bounds.Max.Z - Size.Z * 0.18);
	const FTransform VehicleTransform = Vehicle.GetActorTransform();
	if (UPrimitiveComponent* Hull = Cast<UPrimitiveComponent>(Vehicle.GetRootComponent()))
	{
		FHitResult Hit;
		const FVector High(Mount.X, Mount.Y, Bounds.Max.Z + 100.f);
		const FVector Low(Mount.X, Mount.Y, Bounds.Min.Z);
		if (Hull->LineTraceComponent(Hit, VehicleTransform.TransformPosition(High),
			VehicleTransform.TransformPosition(Low), FCollisionQueryParams()))
		{
			Mount.Z = VehicleTransform.InverseTransformPosition(Hit.ImpactPoint).Z;
		}
	}

	Mount.Z -= BoosterBounds.Min.Z * Scale;
	return {FTransform(FRotator(0.f, -90.f, 0.f), Mount, FVector(Scale))};
}
}

void UMWItemComponent::LoadBoosterVisualAssets()
{
	static const ConstructorHelpers::FClassFinder<AActor> Model(
		TEXT("/Game/Gameplay/Items/Art/Blueprints/BP_MWVisual_Booster_3d816410"));
	// Imported sequence spawnables need the editor's element registry, which is not ready during CDO construction.
	BoosterAttach = TSoftObjectPtr<ULevelSequence>(FSoftObjectPath(
		TEXT("/Game/Gameplay/Items/Art/Animations/LS_MW_Booster_Attach_17505c5d.LS_MW_Booster_Attach_17505c5d")));
	BoosterFireStart = TSoftObjectPtr<ULevelSequence>(FSoftObjectPath(
		TEXT("/Game/Gameplay/Items/Art/Animations/LS_MW_Booster_Fire_Start_a6dbc896.LS_MW_Booster_Fire_Start_a6dbc896")));
	BoosterFireLoop = TSoftObjectPtr<ULevelSequence>(FSoftObjectPath(
		TEXT("/Game/Gameplay/Items/Art/Animations/LS_MW_Booster_Fire_Loop_341814c0.LS_MW_Booster_Fire_Loop_341814c0")));
	BoosterDetach = TSoftObjectPtr<ULevelSequence>(FSoftObjectPath(
		TEXT("/Game/Gameplay/Items/Art/Animations/LS_MW_Booster_Detach_7c732ff0.LS_MW_Booster_Detach_7c732ff0")));
	static const ConstructorHelpers::FObjectFinder<UStaticMesh> Jet(
		TEXT("/Game/Gameplay/Items/Art/Meshes/SM_MW_BoosterExhaust.SM_MW_BoosterExhaust"));
	static const ConstructorHelpers::FObjectFinder<USoundBase> Sound(
		TEXT("/Game/Gameplay/Items/Art/Audio/S_MW_Booster_Fire_65e7084e.S_MW_Booster_Fire_65e7084e"));
	BoosterVisualClass = Model.Class;
	BoosterJetMesh = Jet.Object;
	BoosterFireSound = Sound.Object;
}

bool UMWItemComponent::SpawnBoosterVisual()
{
	if (IsValid(BoosterVisual))
	{
		return true;
	}
	AActor* Vehicle = GetOwner();
	if (!BoosterVisualClass || !Vehicle || !Vehicle->GetRootComponent())
	{
		return false;
	}

	FActorSpawnParameters Params;
	Params.Owner = Vehicle;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	BoosterVisual = GetWorld()->SpawnActor<AActor>(BoosterVisualClass,
		FTransform(Vehicle->GetActorLocation()), Params);
	if (!BoosterVisual)
	{
		return false;
	}
	BoosterVisual->SetReplicates(false);
	BoosterVisual->SetActorEnableCollision(false);
	const FBoosterMount Mount = GetBoosterMount(*Vehicle, *BoosterVisual);
	BoosterVisual->SetActorTransform(Mount.Transform * Vehicle->GetActorTransform());
	BoosterVisual->AttachToComponent(Vehicle->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
	if (!FindVisualNode(*BoosterVisual, BoosterNozzleTag))
	{
		// The art import omitted this marker because it has no renderable mesh.
		if (UStaticMeshComponent* Body = Cast<UStaticMeshComponent>(FindVisualNode(*BoosterVisual, BoosterBodyTag)))
		{
			USceneComponent* Nozzle = NewObject<USceneComponent>(BoosterVisual, TEXT("BoosterNozzle"), RF_Transient);
			Nozzle->ComponentTags.Add(BoosterNozzleTag);
			Nozzle->SetupAttachment(Body);
			// The rear lip is at Y=-87.9 and the inner ring starts at Y=-83.8.
			Nozzle->SetRelativeLocation(FVector(0.0, -84.0, 0.0));
			Nozzle->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
			// Match the exhaust inlet to the bore and scale its length along the nozzle axis.
			Nozzle->SetRelativeScale3D(FVector(Mount.ExhaustLengthScale, 4.10, 4.10));
			Nozzle->RegisterComponent();
		}
	}
	return true;
}

void UMWItemComponent::PlayBoosterAnimation(const TSoftObjectPtr<ULevelSequence>& SequenceAsset, bool bLoop)
{
	StopBoosterAnimation();
	if (!IsValid(BoosterVisual))
	{
		return;
	}
	ULevelSequence* Sequence = SequenceAsset.LoadSynchronous();
	if (!Sequence)
	{
		UE_LOG(LogTemp, Warning, TEXT("Booster animation could not load %s"), *SequenceAsset.ToString());
		return;
	}
	const bool bMovesBase = SequenceAsset == BoosterAttach || SequenceAsset == BoosterDetach;
	USceneComponent* AnimatedNode = FindVisualNode(*BoosterVisual, bMovesBase ? BoosterBaseTag : BoosterBodyTag);
	if (!AnimatedNode)
	{
		return;
	}

	FMovieSceneSequencePlaybackSettings Settings;
	Settings.FinishCompletionStateOverride = EMovieSceneCompletionModeOverride::ForceKeepState;
	ALevelSequenceActor* SequenceActor = nullptr;
	ULevelSequencePlayer* Player = ULevelSequencePlayer::CreateLevelSequencePlayer(this, Sequence, Settings, SequenceActor);
	if (!Player || !SequenceActor)
	{
		return;
	}

	BoosterSequenceActor = SequenceActor;
	// The imported clips bind a preview actor and one component. Explicit instance
	// overrides keep the clip on this car and suppress its preview spawnable.
	const UMovieScene* MovieScene = Sequence->GetMovieScene();
	for (const FMovieSceneBinding& Binding : MovieScene->GetBindings())
	{
		UObject* Object = Binding.GetTracks().IsEmpty()
			? static_cast<UObject*>(BoosterVisual.Get()) : AnimatedNode;
		SequenceActor->BindingOverrides->SetBinding(
			UE::MovieScene::FFixedObjectBindingID(Binding.GetObjectGuid(), MovieSceneSequenceID::Root), {Object});
	}
	if (bLoop)
	{
		Player->PlayLooping();
	}
	else
	{
		Player->Play();
	}
}

void UMWItemComponent::StopBoosterAnimation()
{
	if (IsValid(BoosterSequenceActor))
	{
		if (ULevelSequencePlayer* Player = BoosterSequenceActor->GetSequencePlayer())
		{
			Player->Stop();
		}
		BoosterSequenceActor->Destroy();
	}
	BoosterSequenceActor = nullptr;
}

void UMWItemComponent::RestoreBoosterPose()
{
	if (IsValid(BoosterVisual))
	{
		for (const FName Tag : {BoosterBaseTag, BoosterBodyTag})
		{
			if (USceneComponent* Node = FindVisualNode(*BoosterVisual, Tag))
			{
				if (const USceneComponent* Template = Cast<USceneComponent>(Node->GetArchetype()))
				{
					Node->SetRelativeTransform(Template->GetRelativeTransform());
				}
			}
		}
	}
}

void UMWItemComponent::StartBoosterJet()
{
	if (USceneComponent* Nozzle = FindVisualNode(*BoosterVisual, BoosterNozzleTag))
	{
		if (BoosterJetMesh)
		{
			BoosterJet = NewObject<UStaticMeshComponent>(BoosterVisual, NAME_None, RF_Transient);
			BoosterJet->ComponentTags.Add(TEXT("BoosterExhaust"));
			BoosterJet->SetStaticMesh(BoosterJetMesh);
			BoosterJet->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			BoosterJet->SetCastShadow(false);
			BoosterJet->SetupAttachment(Nozzle);
			BoosterJet->RegisterComponent();
		}
		if (BoosterFireSound)
		{
			UGameplayStatics::SpawnSoundAttached(BoosterFireSound, Nozzle, NAME_None, FVector::ZeroVector,
				FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true);
		}
	}
}

void UMWItemComponent::StopBoosterJet()
{
	if (IsValid(BoosterJet))
	{
		BoosterJet->DestroyComponent();
	}
	BoosterJet = nullptr;
}

void UMWItemComponent::PlayBoosterFireLoop()
{
	if (DisplayedFiringEnd > GetServerTime())
	{
		PlayBoosterAnimation(BoosterFireLoop, true);
	}
}

void UMWItemComponent::RemoveBoosterVisual()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BoosterExpiryTimer);
		World->GetTimerManager().ClearTimer(BoosterAnimationTimer);
	}
	StopBoosterAnimation();
	StopBoosterJet();
	if (IsValid(BoosterVisual))
	{
		BoosterVisual->Destroy();
	}
	BoosterVisual = nullptr;
	DisplayedFiringEnd = 0.0;
	bBoosterDetaching = false;
}

void UMWItemComponent::OnRep_BoosterPresentation()
{
	UWorld* World = GetWorld();
	if (!World || !HasBegunPlay() || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	FTimerManager& Timers = World->GetTimerManager();
	const float Remaining = static_cast<float>(BoosterPresentation.FiringEndsAt - GetServerTime());
	if (Remaining > 0.f)
	{
		Timers.SetTimer(BoosterExpiryTimer, this, &ThisClass::OnRep_BoosterPresentation, Remaining + 0.01f, false);
		if (DisplayedFiringEnd == BoosterPresentation.FiringEndsAt || !SpawnBoosterVisual())
		{
			return;
		}
		bBoosterDetaching = false;
		DisplayedFiringEnd = BoosterPresentation.FiringEndsAt;
		Timers.ClearTimer(BoosterAnimationTimer);
		StopBoosterAnimation();
		RestoreBoosterPose();
		StartBoosterJet();
		PlayBoosterAnimation(BoosterFireStart);
		if (BoosterSequenceActor && BoosterSequenceActor->GetSequencePlayer())
		{
			const float StartDuration = BoosterSequenceActor->GetSequencePlayer()->GetDuration().AsSeconds();
			Timers.SetTimer(BoosterAnimationTimer, this, &ThisClass::PlayBoosterFireLoop, StartDuration, false);
		}
		else
		{
			PlayBoosterFireLoop();
		}
		return;
	}

	const bool bFinishedFiring = DisplayedFiringEnd > 0.0;
	if (bFinishedFiring)
	{
		DisplayedFiringEnd = 0.0;
		Timers.ClearTimer(BoosterAnimationTimer);
		StopBoosterAnimation();
		StopBoosterJet();
		RestoreBoosterPose();
	}

	if (BoosterPresentation.bHeld)
	{
		if ((!IsValid(BoosterVisual) || bBoosterDetaching) && SpawnBoosterVisual())
		{
			bBoosterDetaching = false;
			Timers.ClearTimer(BoosterAnimationTimer);
			PlayBoosterAnimation(BoosterAttach);
		}
		return;
	}

	if (BoosterPresentation.FiringEndsAt <= 0.0)
	{
		RemoveBoosterVisual();
	}
	else if (bFinishedFiring && IsValid(BoosterVisual))
	{
		PlayBoosterAnimation(BoosterDetach);
		if (BoosterSequenceActor && BoosterSequenceActor->GetSequencePlayer())
		{
			bBoosterDetaching = true;
			const float Duration = BoosterSequenceActor->GetSequencePlayer()->GetDuration().AsSeconds();
			Timers.SetTimer(BoosterAnimationTimer, this, &ThisClass::RemoveBoosterVisual, Duration, false);
		}
		else
		{
			RemoveBoosterVisual();
		}
	}
}
