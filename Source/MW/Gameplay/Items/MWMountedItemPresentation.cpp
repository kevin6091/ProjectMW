#include "Gameplay/Items/MWMountedItemPresentation.h"

#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Data/MWItemDefinitionData.h"
#include "Engine/World.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "Kismet/GameplayStatics.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "MovieSceneBinding.h"
#include "MovieScenePossessable.h"
#include "Sound/SoundBase.h"

namespace
{
double GetSequenceDuration(const ULevelSequence* Sequence)
{
    const UMovieScene* Scene = Sequence ? Sequence->GetMovieScene() : nullptr;
    if (!Scene)
    {
        return 0.0;
    }
    const TRange<FFrameNumber> Range = Scene->GetPlaybackRange();
    return Range.HasLowerBound() && Range.HasUpperBound()
        ? Scene->GetTickResolution().AsSeconds(Range.GetUpperBoundValue() - Range.GetLowerBoundValue()) : 0.0;
}
}

AMWMountedItemPresentation::AMWMountedItemPresentation()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
    bReplicates = false;
}

AMWMountedItemPresentation* AMWMountedItemPresentation::Equip(AMWVehiclePawn& Vehicle, const UMWItemDefinitionData& Definition,
    bool bAnimateAttach)
{
    UWorld* World = Vehicle.GetWorld();
    if (!World || World->GetNetMode() == NM_DedicatedServer)
    {
        return nullptr;
    }

    FActorSpawnParameters Params;
    Params.Owner = &Vehicle;
    Params.ObjectFlags = RF_Transient;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AMWMountedItemPresentation* Presentation = World->SpawnActor<AMWMountedItemPresentation>(Params);
    if (Presentation && !Presentation->Start(Vehicle, Definition, bAnimateAttach))
    {
        UE_LOG(LogTemp, Warning, TEXT("[MWMountedItem] Could not mount %s"), *Definition.GetName());
        Presentation->Destroy();
        return nullptr;
    }
    return Presentation;
}

double AMWMountedItemPresentation::GetUseDuration(const UMWItemDefinitionData& Definition, bool bIncludeDetach)
{
    double UseDuration = GetSequenceDuration(Definition.Presentation.Use.LoadSynchronous());
    if (UseDuration <= 0.0)
    {
        return 0.0;
    }
    if (Definition.UsesBurstFire())
    {
        UseDuration = static_cast<double>(Definition.BurstFire.ShotInterval) * Definition.BurstFire.ShotCount;
    }
    if (!bIncludeDetach)
    {
        return UseDuration;
    }
    const double DetachDuration = GetSequenceDuration(Definition.Presentation.Detach.LoadSynchronous());
    const double WindDownDuration = GetSequenceDuration(Definition.Presentation.WindDown.LoadSynchronous());
    return DetachDuration > 0.0 && (Definition.Presentation.WindDown.IsNull() || WindDownDuration > 0.0)
        ? UseDuration + WindDownDuration + DetachDuration : 0.0;
}

void AMWMountedItemPresentation::PlayUse(bool bShouldDetach)
{
    if (Stage == EStage::Attach)
    {
        FinishAttach();
    }
    else
    {
        ReleaseSequence();
    }
    bDetachAfterUse = bShouldDetach;
    bImpactSoundPlayed = false;
    Stage = EStage::Use;
    if (!PlaySequence(*UseSequence, UseLoopCount, UsePlayRate))
    {
        Destroy();
        return;
    }
    if (UseSound)
    {
        UGameplayStatics::SpawnSoundAttached(UseSound, Visual->GetRootComponent());
    }
}

bool AMWMountedItemPresentation::Start(AMWVehiclePawn& Vehicle, const UMWItemDefinitionData& Definition, bool bAnimateAttach)
{
    const FMWItemPresentationData& Art = Definition.Presentation;
    UClass* VisualClass = Art.VisualClass.LoadSynchronous();
    ULevelSequence* AttachSequence = Art.Attach.LoadSynchronous();
    UseSequence = Art.Use.LoadSynchronous();
    WindDownSequence = Art.WindDown.LoadSynchronous();
    DetachSequence = Art.Detach.LoadSynchronous();
    UseSound = Art.UseSound.LoadSynchronous();
    if (Definition.ItemId == EMWItemId::Hammer)
    {
        ImpactSound = Art.ImpactSound.LoadSynchronous();
        ImpactDelay = Definition.AreaHit.ImpactDelay;
    }
    const UBoxComponent* Hull = Cast<UBoxComponent>(Vehicle.GetRootComponent());
    if (!VisualClass || !AttachSequence || !UseSequence || !DetachSequence || !Hull)
    {
        return false;
    }
    if (Definition.UsesBurstFire())
    {
        const double ShotAnimationDuration = GetSequenceDuration(UseSequence);
        if (ShotAnimationDuration <= 0.0)
        {
            return false;
        }
        UseLoopCount = Definition.BurstFire.ShotCount - 1;
        UsePlayRate = ShotAnimationDuration / Definition.BurstFire.ShotInterval;
    }

    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.ObjectFlags = RF_Transient;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Visual = GetWorld()->SpawnActor<AActor>(VisualClass, Vehicle.GetActorTransform(), Params);
    if (!Visual || !Visual->GetRootComponent())
    {
        return false;
    }
    Visual->SetReplicates(false);
    Visual->SetActorEnableCollision(false);
    Visual->AttachToComponent(Vehicle.GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
    const FVector Extent = Hull->GetUnscaledBoxExtent();
    FTransform Mount = Art.MountOffset;
    Mount.AddToTranslation(FVector(0.0, 0.0, Extent.Z));
    if (Definition.ItemId == EMWItemId::Hammer)
    {
        // 기존 망치는 차량 길이에 맞춰 머리 크기를 보정한다.
        constexpr double AuthoredHeadWidthCm = 290.0;
        Mount = FTransform(FRotator(0.f, -90.f, 0.f),
            FVector(Extent.X * 0.28, 0.0, Extent.Z + 10.0), FVector(2.0 * Extent.X / AuthoredHeadWidthCm));
    }
    Visual->SetActorRelativeTransform(Mount);
    if (!PlaySequence(*AttachSequence))
    {
        return false;
    }
    if (!bAnimateAttach)
    {
        // 같은 패킷으로 지급과 사용을 받으면 장착 끝 자세를 먼저 적용한다.
        FinishAttach();
    }
    return true;
}

bool AMWMountedItemPresentation::PlaySequence(ULevelSequence& Sequence, int32 LoopCount, float PlayRate)
{
    FMovieSceneSequencePlaybackSettings Settings;
    Settings.FinishCompletionStateOverride = EMovieSceneCompletionModeOverride::ForceKeepState;
    Settings.LoopCount.Value = LoopCount;
    Settings.PlayRate = PlayRate;
    ALevelSequenceActor* NewSequenceActor = nullptr;
    ULevelSequencePlayer* Player = ULevelSequencePlayer::CreateLevelSequencePlayer(this, &Sequence, Settings, NewSequenceActor);
    SequenceActor = NewSequenceActor;
    if (!Player || !SequenceActor)
    {
        return false;
    }

    UMovieScene& Scene = *Sequence.GetMovieScene();
    const TInlineComponentArray<USceneComponent*> Components(Visual.Get());
    for (const FMovieSceneBinding& Binding : AsConst(Scene).GetBindings())
    {
        // 원본 시퀀스의 바인딩 이름을 실제 장착 부품과 맞춘다.
        const FGuid Id = Binding.GetObjectGuid();
        UObject* Target = Visual.Get();
        const FMovieScenePossessable* Possessable = Scene.FindPossessable(Id);
        if (Possessable && Possessable->GetParent().IsValid())
        {
            const FName ComponentName(*Possessable->GetName());
            USceneComponent* const* Component = Components.FindByPredicate([ComponentName](const USceneComponent* Node)
            {
                return Node->GetFName() == ComponentName;
            });
            if (!Component)
            {
                UE_LOG(LogTemp, Warning, TEXT("[MWMountedItem] Missing component for %s in %s"),
                    *ComponentName.ToString(), *Sequence.GetName());
                return false;
            }
            Target = *Component;
        }
        SequenceActor->BindingOverrides->SetBinding(
            UE::MovieScene::FFixedObjectBindingID(Id, MovieSceneSequenceID::Root), {Target});
    }

    Player->Play();
    UE_LOG(LogTemp, Log, TEXT("[MWMountedItem] Playing %s"), *Sequence.GetName());
    return true;
}

void AMWMountedItemPresentation::FinishAttach()
{
    if (ULevelSequencePlayer* Player = SequenceActor ? SequenceActor->GetSequencePlayer() : nullptr)
    {
        Player->GoToEndAndStop();
    }
    ReleaseSequence();
    Stage = EStage::Held;
}

void AMWMountedItemPresentation::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!IsValid(GetOwner()) || !IsValid(Visual))
    {
        Destroy();
        return;
    }

    if (Stage == EStage::Held) return;

    auto* Player = IsValid(SequenceActor) ? SequenceActor->GetSequencePlayer() : nullptr;
    if (!Player)
    {
        Destroy();
        return;
    }

    const bool bPlaying = Player->IsPlaying();
    if (Stage == EStage::Use && ImpactSound && !bImpactSoundPlayed
        && (!bPlaying || (Player->GetCurrentTime().AsSeconds() - Player->GetStartTime().AsSeconds()) >= ImpactDelay))
    {
        if (!bPlaying)
        {
            // 프레임 지연으로 타격을 건너뛰었으면 타격 자세에서 소리 위치를 읽는다.
            const FQualifiedFrameTime Start = Player->GetStartTime();
            Player->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(
                Start.Time + Start.Rate.AsFrameTime(ImpactDelay), EUpdatePositionMethod::Jump));
        }
        PlayImpactSound();
    }
    if (bPlaying)
    {
        return;
    }

    ReleaseSequence();
    if (Stage == EStage::Attach)
    {
        Stage = EStage::Held;
        return;
    }
    if (Stage == EStage::Use && !bDetachAfterUse)
    {
        Stage = EStage::Held;
        return;
    }
    if (Stage == EStage::Use && WindDownSequence)
    {
        Stage = EStage::WindDown;
        if (PlaySequence(*WindDownSequence))
        {
            return;
        }
        Destroy();
        return;
    }
    if (Stage == EStage::Use || Stage == EStage::WindDown)
    {
        Stage = EStage::Detach;
        if (PlaySequence(*DetachSequence))
        {
            return;
        }
    }
    Destroy();
}

void AMWMountedItemPresentation::PlayImpactSound()
{
    bImpactSoundPlayed = true;
    const FName HeadTag(TEXT("MWItemNode=sharedassets0.assets:1580"));
    FVector Location = Visual->GetActorLocation();
    for (const USceneComponent* Node : TInlineComponentArray<USceneComponent*>(Visual.Get()))
    {
        if (Node->ComponentHasTag(HeadTag))
        {
            Location = Node->GetComponentLocation();
            break;
        }
    }

    const UAudioComponent* Audio = UGameplayStatics::SpawnSoundAtLocation(this, ImpactSound, Location);
    UE_LOG(LogTemp, Log, TEXT("[MWMountedItem] Impact audio spawned: %s"), Audio ? TEXT("yes") : TEXT("no"));
}

void AMWMountedItemPresentation::ReleaseSequence()
{
    if (IsValid(SequenceActor))
    {
        SequenceActor->Destroy();
    }
    SequenceActor = nullptr;
}

void AMWMountedItemPresentation::EndPlay(EEndPlayReason::Type EndPlayReason)
{
    ReleaseSequence();
    if (IsValid(Visual))
    {
        Visual->Destroy();
    }
    Super::EndPlay(EndPlayReason);
}
