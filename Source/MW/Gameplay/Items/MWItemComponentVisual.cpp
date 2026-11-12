// 같은 ItemComponent의 표시 구현이다. 부스터 외형·시퀀스·음향의 시작과 정리를 담당한다.

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

        // 보이는 차체의 뒤 데크 안쪽에 부스터 장착 발이 들어가도록 맞춘다.
        static const FMount Mounts[] = {// Doug의 뒤 데크 중앙에 장착하고 배기가 꼬리지느러미 앞에서 끝나게 한다.
                                        {TEXT("SM_MW_Doug_Body"), {-46.0, 0.0, 35.4}, 0.099, 2.5},
                                        {TEXT("SM_MW_Fred_Body"), {-37.6, 0.0, 1.4}, 0.155},
                                        {TEXT("SM_MW_Mammoth_Body"), {-43.7, 0.0, 43.9}, 0.115},
                                        {TEXT("SM_MW_Smiet_Body"), {-14.1, 0.0, 34.2}, 0.142},
                                        {TEXT("SM_MW_Ufology_Body"), {-44.0, 0.0, 8.2}, 0.161}};
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
        // 원본 조립체가 +Y를 향하므로 장착 후 폭은 원본 X 크기에서 구한다.
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

// 기본 외형과 음원을 준비하되 원본 시퀀스는 CDO 시점에 강제로 로드하지 않는다.
void UMWItemComponent::LoadBoosterVisualAssets()
{
    static const ConstructorHelpers::FClassFinder<AActor> Model(
        TEXT("/Game/Gameplay/Items/Art/Blueprints/BP_MWVisual_Booster_3d816410"));
    // 복구 시퀀스의 Spawnable은 CDO 생성 중 아직 준비되지 않은 요소 레지스트리를 요구하므로 로드를 늦춘다.
    BoosterAttach = TSoftObjectPtr<ULevelSequence>(FSoftObjectPath(
        TEXT("/Game/Gameplay/Items/Art/Animations/LS_MW_Booster_Attach_17505c5d.LS_MW_Booster_Attach_17505c5d")));
    BoosterFireStart = TSoftObjectPtr<ULevelSequence>(FSoftObjectPath(TEXT(
        "/Game/Gameplay/Items/Art/Animations/LS_MW_Booster_Fire_Start_a6dbc896.LS_MW_Booster_Fire_Start_a6dbc896")));
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

// 원래 차량 장착표 또는 BP의 명시적 override로 로컬 부스터 외형을 붙인다.
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
    if (bOverrideBoosterMount &&
        (BoosterMountTransform.ContainsNaN() || BoosterMountTransform.GetScale3D().GetMin() <= UE_SMALL_NUMBER ||
         !FMath::IsFinite(BoosterExhaustLengthScale) || BoosterExhaustLengthScale <= UE_SMALL_NUMBER))
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: BP의 부스터 장착 변환 또는 배기 길이가 유효하지 않습니다."),
               *Vehicle->GetName());
        return false;
    }

    FActorSpawnParameters Params;
    Params.Owner = Vehicle;
    Params.ObjectFlags |= RF_Transient;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    BoosterVisual = GetWorld()->SpawnActor<AActor>(BoosterVisualClass, FTransform(Vehicle->GetActorLocation()), Params);
    if (!BoosterVisual)
    {
        return false;
    }
    BoosterVisual->SetReplicates(false);
    BoosterVisual->SetActorEnableCollision(false);
    const FBoosterMount Mount = bOverrideBoosterMount ? FBoosterMount{BoosterMountTransform, BoosterExhaustLengthScale}
                                                      : GetBoosterMount(*Vehicle, *BoosterVisual);
    BoosterVisual->SetActorTransform(Mount.Transform * Vehicle->GetActorTransform());
    BoosterVisual->AttachToComponent(Vehicle->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
    if (!FindVisualNode(*BoosterVisual, BoosterNozzleTag))
    {
        // 이 표시는 렌더링 메시가 없어 복구 임포트에서 빠졌으므로 별도로 장착 기준을 적용한다.
        if (UStaticMeshComponent* Body = Cast<UStaticMeshComponent>(FindVisualNode(*BoosterVisual, BoosterBodyTag)))
        {
            USceneComponent* Nozzle = NewObject<USceneComponent>(BoosterVisual, TEXT("BoosterNozzle"), RF_Transient);
            Nozzle->ComponentTags.Add(BoosterNozzleTag);
            Nozzle->SetupAttachment(Body);
            // 원본 뒤 테두리는 Y=-87.9, 안쪽 링 시작은 Y=-83.8이므로 장착 위치 계산에 반영한다.
            Nozzle->SetRelativeLocation(FVector(0.0, -84.0, 0.0));
            Nozzle->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
            // 배기 시작을 노즐 구멍에 맞추고 노즐 축을 따라 길이를 조정한다.
            Nozzle->SetRelativeScale3D(FVector(Mount.ExhaustLengthScale, 4.10, 4.10));
            Nozzle->RegisterComponent();
        }
    }
    return true;
}

// 복구 시퀀스의 루트와 부품 바인딩을 현재 장착 외형으로 연결한다.
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
    ULevelSequencePlayer* Player =
        ULevelSequencePlayer::CreateLevelSequencePlayer(this, Sequence, Settings, SequenceActor);
    if (!Player || !SequenceActor)
    {
        return;
    }

    BoosterSequenceActor = SequenceActor;
    // Actor의 생성 트랙도 비어 있지 않으므로 트랙 개수로 Actor와 부품을 구분하면 안 된다.
    // 원본의 미리보기 Actor 바인딩은 장착 Actor에, 부품 바인딩은 애니메이션 대상에 연결한다.
    UMovieScene* MovieScene = Sequence->GetMovieScene();
    for (const FMovieSceneBinding& Binding : MovieScene->GetBindings())
    {
        UObject* Object = MovieScene->FindSpawnable(Binding.GetObjectGuid())
                              ? static_cast<UObject*>(BoosterVisual.Get())
                              : AnimatedNode;
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

// 현재 시퀀스 재생과 바인딩 참조를 정리한다.
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

// 시퀀스가 바꾼 외형 자세를 보관한 기준으로 되돌린다.
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

// 노즐 기준으로 배기 외형과 사용 음향을 시작한다. 서버 속도를 직접 변경하지 않는다.
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

// 배기 표시와 재생 음향을 함께 정리한다.
void UMWItemComponent::StopBoosterJet()
{
    if (IsValid(BoosterJet))
    {
        BoosterJet->DestroyComponent();
    }
    BoosterJet = nullptr;
}

// 기존 부스터 사용 음원을 이어 재생한다. 이번 주석 정리에서 음량·재생 정책은 바꾸지 않는다.
void UMWItemComponent::PlayBoosterFireLoop()
{
    if (DisplayedFiringEnd > GetServerTime())
    {
        PlayBoosterAnimation(BoosterFireLoop, true);
    }
}

// 표시 타이머·시퀀스·배기·음향·생성 액터를 해제해 이전 차량 참조가 남지 않게 한다.
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

// 서버 종료 시각과 외형 상태로 장착·사용·분리 연출을 맞춘다.
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
