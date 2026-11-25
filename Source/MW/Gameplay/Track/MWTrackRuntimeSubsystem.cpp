// 월드의 실제 트랙을 복구 카탈로그에 연결하고 차량별 진행·복귀 정보를 관리한다.

#include "Gameplay/Track/MWTrackRuntimeSubsystem.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Gameplay/Construction/MWTrackPreview.h"
#include "Gameplay/Construction/MWTrackSnap.h"
#include "Gameplay/Track/MWTrackRuntimeComponent.h"
#include "Gameplay/Track/MWTrackRuntimeSettings.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "System/GameMode/MWGameplayGameMode.h"
#include "System/GameState/MWGameplayGameState.h"
#include "System/MWPlayerState.h"
#include "LogChannel.h"
#include "MWGameplayTags.h"
#include "System/MWAssetManager.h"
#include "TimerManager.h"

void UMWTrackRuntimeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    // 복제된 트랙 컴포넌트가 초기화될 때 사용할 수 있도록 월드 시작 전에 카탈로그를 준비한다.
    Catalog = UMWAssetManager::Get().LoadAssetSynchronous<UMWTrackDefinitionData>(MWGameplayTags::Asset_Track_RuntimeCatalog);
    if (!Catalog)
    {
        UE_LOG(LogDefault, Warning, TEXT("[MWTrackRuntime] 공용 에셋 목록에서 트랙 카탈로그를 불러올 수 없습니다."));
        return;
    }

    // 월드 시작 전에 조회 표를 만들어 원격에서 복제된 컴포넌트도 같은 행을 찾게 한다.
    Pieces.Reserve(Catalog->Pieces.Num());
    for (const FMWTrackDefinition& Piece : Catalog->Pieces)
    {
        if (!Piece.TrackClass.IsNull())
        {
            Pieces.Add(Piece.TrackClass.ToSoftObjectPath(), &Piece);
        }
    }
}

// 일반 게임과 PIE 월드만 대상으로 해 에디터 미리보기에서 런타임 기능이 켜지지 않게 한다.
bool UMWTrackRuntimeSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TArray<TSoftClassPtr<AActor>> UMWTrackRuntimeSubsystem::GetSelectionPool() const
{
    TArray<TSoftClassPtr<AActor>> Result;
    const auto* Settings = GetDefault<UMWTrackRuntimeSettings>();
    if (!Catalog)
    {
        return Result;
    }

    // 추첨 전에 전체 BP를 로드하지 않도록 카탈로그의 소프트 참조만 모은다.
    Result.Reserve(Catalog->Pieces.Num());
    for (const FMWTrackDefinition& Piece : Catalog->Pieces)
    {
        const auto& TrackClass = Piece.TrackClass;
        if (!TrackClass.IsNull() && TrackClass != Settings->StartTrackClass && TrackClass != Settings->FinishTrackClass
            && !Settings->CourseOnlyTracks.Contains(TrackClass))
        {
            Result.AddUnique(TrackClass);
        }
    }
    return Result;
}

FText UMWTrackRuntimeSubsystem::GetTrackDisplayName(const TSoftClassPtr<AActor>& TrackClass) const
{
    const FMWTrackDefinition* Definition = FindTrackDefinition(TrackClass);
    return Definition ? Definition->DisplayName : FText::GetEmpty();
}

const FMWTrackDefinition* UMWTrackRuntimeSubsystem::FindTrackDefinition(const TSoftClassPtr<AActor>& TrackClass) const
{
    return Pieces.FindRef(TrackClass.ToSoftObjectPath());
}

// 기존 배치 트랙을 연결하고 이후 생성 이벤트도 등록해 늦게 도착한 조각을 놓치지 않는다.
void UMWTrackRuntimeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    if (!Catalog)
    {
        return;
    }

    ActorSpawnHandle = InWorld.AddOnActorSpawnedHandler(
        FOnActorSpawned::FDelegate::CreateUObject(this, &UMWTrackRuntimeSubsystem::OnActorSpawned));
    for (TActorIterator<AActor> Actor(&InWorld); Actor; ++Actor)
    {
        AttachTrack(*Actor, true);
    }
}

// 액터 생성 구독과 진행·카탈로그 참조를 이 월드에서 해제한다.
void UMWTrackRuntimeSubsystem::Deinitialize()
{
    if (ActorSpawnHandle.IsValid())
    {
        GetWorld()->RemoveOnActorSpawnedHandler(ActorSpawnHandle);
    }
    for (const TWeakObjectPtr<AActor>& Preview : PreviewActors)
    {
        if (AActor* Actor = Preview.Get())
        {
            Actor->OnEndPlay.RemoveDynamic(this, &ThisClass::OnPreviewActorEndPlay);
        }
    }
    PreviewActors.Reset();
    Pieces.Reset();
    Catalog = nullptr;
    Super::Deinitialize();
}

// 늦게 복제되거나 생성된 트랙에도 기존 카탈로그 연결을 적용한다.
void UMWTrackRuntimeSubsystem::OnActorSpawned(AActor* Actor)
{
    if (!Pieces.Contains(FSoftObjectPath(Actor->GetClass())))
    {
        return;
    }
    GetWorld()->GetTimerManager().SetTimerForNextTick(
        FTimerDelegate::CreateWeakLambda(this,
                                         [this, Pending = TWeakObjectPtr<AActor>(Actor)]
                                         {
                                             AttachTrack(Pending.Get());
                                         }));
}

// 카탈로그에 대응하는 트랙에만 런타임을 연결하고 이미 있는 컴포넌트는 중복 생성하지 않는다.
void UMWTrackRuntimeSubsystem::AttachTrack(AActor* Actor, bool bInitialWorldActor)
{
    // 전시용 액터와 서버가 설치를 보류 중인 조각에는 경기용 충돌·트리거를 만들지 않는다.
    if (!IsValid(Actor) || MWTrackPreview::IsDisplayOnly(*Actor) || PreviewActors.Contains(Actor) || !Actor->GetRootComponent() ||
        Actor->FindComponentByClass<UMWTrackRuntimeComponent>())
    {
        return;
    }
    const FMWTrackDefinition* const* Data = Pieces.Find(FSoftObjectPath(Actor->GetClass()));
    if (!Data)
    {
        return;
    }

    const bool bLift = (*Data)->Motions.ContainsByPredicate(
        [](const FMWTrackMotionData& Motion)
        {
            return Motion.Type == EMWTrackMotion::Lift;
        });
    if (GetWorld()->GetNetMode() == NM_Client && bLift)
    {
        // 서버가 같은 컴포넌트를 복제하므로 클라이언트에서 별도 생성해 이중 이동시키지 않는다.
        return;
    }
    UMWTrackRuntimeComponent* Runtime = NewObject<UMWTrackRuntimeComponent>(Actor);
    Actor->AddInstanceComponent(Runtime);
    Runtime->Configure(**Data);
    Runtime->SetIsReplicated(bLift);
    Runtime->RegisterComponent();
    // 레벨에 저장된 승강기도 액터 채널이 있어야 작동 시각이 복제된다.
    if (bLift && bInitialWorldActor && GetWorld()->GetNetMode() != NM_Client)
    {
        // 탑승자가 트랙 원점에서 멀어도 발판의 복제가 끊기지 않도록 기존 확정 트랙과 같은 가시성 규칙을 쓴다.
        Actor->bAlwaysRelevant = true;
        Actor->SetReplicates(true);
    }
}

// 복제로 생긴 컴포넌트도 현재 카탈로그의 같은 복구 설정으로 초기화한다.
void UMWTrackRuntimeSubsystem::ConfigureRuntimeComponent(UMWTrackRuntimeComponent& Component)
{
    const AActor* Actor = Component.GetOwner();
    if (!IsValid(Actor))
    {
        return;
    }
    const FMWTrackDefinition* const* Data = Pieces.Find(FSoftObjectPath(Actor->GetClass()));
    if (Data)
    {
        Component.SetPreview(PreviewActors.Contains(Actor));
        Component.Configure(**Data);
    }
}

// 배치 미리보기 상태를 실제 트랙 런타임에 전달한다.
void UMWTrackRuntimeSubsystem::SetTrackPreview(AActor& Actor, bool bPreview)
{
    if (bPreview)
    {
        PreviewActors.Add(&Actor);
        Actor.OnEndPlay.AddUniqueDynamic(this, &ThisClass::OnPreviewActorEndPlay);
    }
    else if (PreviewActors.Remove(&Actor) > 0)
    {
        Actor.OnEndPlay.RemoveDynamic(this, &ThisClass::OnPreviewActorEndPlay);
        // 설치를 보류한 동안 생성하지 않았던 런타임을 확정 시점에 연결한다.
        AttachTrack(&Actor);
    }
    if (UMWTrackRuntimeComponent* Runtime = Actor.FindComponentByClass<UMWTrackRuntimeComponent>())
    {
        Runtime->SetPreview(bPreview);
    }
}

bool UMWTrackRuntimeSubsystem::PrepareCourseFinish(AActor& Track, const FVector& HalfExtent)
{
    if (!Track.HasAuthority() || Track.GetWorld() != GetWorld())
    {
        return false;
    }
    // 종료 캡이 이번 틱에 막 게시됐다면 지연된 생성 콜백을 기다리지 않고 연결한다.
    AttachTrack(&Track);
    auto* Runtime = Track.FindComponentByClass<UMWTrackRuntimeComponent>();
    FTransform PortFrame;
    return Runtime && MWTrackSnap::ReadFirstPort(Track, PortFrame)
        && Runtime->EnsureCourseFinishTrigger(PortFrame, HalfExtent);
}

bool UMWTrackRuntimeSubsystem::PrepareCourseCheckpoint(AActor& Track, const FTransform& WorldFrame, const FVector& HalfExtent)
{
    if (!Track.HasAuthority() || Track.GetWorld() != GetWorld()) return false;
    AttachTrack(&Track);
    auto* Runtime = Track.FindComponentByClass<UMWTrackRuntimeComponent>();
    // 도로의 열린 끝을 종료 캡에서 보던 반대 프레임으로 바꾼다.
    const FTransform Facing((WorldFrame.GetRotation() * FQuat(FVector::UpVector, UE_PI)).GetNormalized(), WorldFrame.GetLocation());
    return Runtime && Runtime->EnsureCourseCheckpointTrigger(Facing.GetRelativeTransform(Track.GetActorTransform()), HalfExtent);
}

bool UMWTrackRuntimeSubsystem::IsCheckpointTrack(const AActor& Track) const
{
    if (const auto* Runtime = Track.FindComponentByClass<UMWTrackRuntimeComponent>(); Runtime && Runtime->HasCheckpointTrigger()) return true;
    const FMWTrackDefinition* const* Data = Pieces.Find(FSoftObjectPath(Track.GetClass()));
    return Data && (*Data)->Shapes.ContainsByPredicate([](const FMWTrackShapeData& Shape) { return Shape.Trigger == EMWTrackTrigger::Checkpoint; });
}

void UMWTrackRuntimeSubsystem::BeginPlayerRace(AMWVehiclePawn& Vehicle, int32 RoundEpoch)
{
    if (!Vehicle.HasAuthority() || Vehicle.GetWorld() != GetWorld() || RoundEpoch <= 0)
    {
        return;
    }
    FRacer& Racer = Racers.FindOrAdd(&Vehicle);
    Racer = FRacer{};
    Racer.RoundEpoch = RoundEpoch;
    Racer.StartTime = GetWorld()->GetTimeSeconds();
    Racer.StartTransform = Vehicle.GetActorTransform();
}

void UMWTrackRuntimeSubsystem::OnPreviewActorEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason)
{
    PreviewActors.Remove(Actor);
}

// 서버가 현재 진행과 중복 통과를 검사해 출발·체크포인트·완주 상태를 기록한다.
void UMWTrackRuntimeSubsystem::ProcessRaceTrigger(AMWVehiclePawn& Vehicle, AActor& Track, EMWTrackTrigger Trigger,
                                                  const TArray<FTransform>& RespawnPoints)
{
    if (!Vehicle.HasAuthority() || Vehicle.GetWorld() != GetWorld() || Track.GetWorld() != GetWorld())
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

    auto* GameMode = GetWorld()->GetAuthGameMode<AMWGameplayGameMode>();
    const auto* GameState = GetWorld()->GetGameState<AMWGameplayGameState>();
    if (GameState)
    {
        const auto* Player = Vehicle.GetPlayerState<AMWPlayerState>();
        if (!GameMode || !Player || !Vehicle.CanDrive() || Player->GetRoundState().RoundEpoch != GameState->GetProgress().RoundEpoch)
        {
            return;
        }
    }
    FRacer& Racer = Racers.FindOrAdd(&Vehicle);
    if (GameState && Racer.RoundEpoch != GameState->GetProgress().RoundEpoch)
    {
        return;
    }
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
        if (GameState && !GameMode->TryPassPlayerCheckpoint(Vehicle, Track, Racer.RoundEpoch)) return;
        FRacer* Checkpoint = Racers.Find(&Vehicle);
        if (!Checkpoint) return;
        Checkpoint->Passed.Add(&Track);
        ++Checkpoint->Progress.Checkpoints;
        Checkpoint->RespawnTransforms.Reset(RespawnPoints.Num());
        for (const FTransform& Point : RespawnPoints)
        {
            Checkpoint->RespawnTransforms.Add(Point * Track.GetActorTransform());
        }
        return;
    }
    if (Trigger == EMWTrackTrigger::Finish)
    {
        const float Elapsed = static_cast<float>(Now - Racer.StartTime);
        if (GameState && !GameMode->TryFinishPlayerRace(Vehicle, Track, Racer.RoundEpoch))
        {
            return;
        }
        // 승인 알림에서 다른 진행 기록이 바뀔 수 있으므로 TMap의 참조를 다시 찾는다.
        if (FRacer* Finished = Racers.Find(&Vehicle))
        {
            Finished->Progress.bFinished = true;
            Finished->Progress.ElapsedSeconds = Elapsed;
        }
    }
}

// 현재 차량에 기록된 진행 값을 반환한다. 조회로 점수를 확정하지 않는다.
FMWTrackRaceProgress UMWTrackRuntimeSubsystem::GetTrackRaceProgress(AMWVehiclePawn* Vehicle) const
{
    if (const FRacer* Racer = Racers.Find(Vehicle))
    {
        return Racer->Progress;
    }
    return {};
}

// 서버에서 기록한 유효 복귀 Transform만 사용해 기존 차량을 복귀시킨다.
bool UMWTrackRuntimeSubsystem::RespawnAtTrackCheckpoint(AMWVehiclePawn* Vehicle)
{
    if (!IsValid(Vehicle) || !Vehicle->HasAuthority())
    {
        return false;
    }
    if (GetWorld()->GetGameState<AMWGameplayGameState>() && !Vehicle->CanDrive())
    {
        return false;
    }
    if (const auto* GS = GetWorld()->GetGameState<AMWGameplayGameState>())
    {
        auto* Mode = GetWorld()->GetAuthGameMode<AMWGameplayGameMode>();
        const bool bSuddenDeath = GS->GetProgress().RaceMode == EMWRaceMode::SuddenDeath;
        if (!Mode || !Mode->HandlePlayerRaceFailure(*Vehicle, GS->GetProgress().RoundEpoch)) return false;
        // 이미 영구 탈락으로 처리했다. 호출자가 표면 충격이나 복귀를 추가하지 않게 성공으로 소비한다.
        if (bSuddenDeath) return true;
    }
    const FRacer* Racer = Racers.Find(Vehicle);
    if (!Racer || Racer->Progress.bFinished)
    {
        return false;
    }
    if (Racer->RespawnTransforms.IsEmpty() && Racer->RoundEpoch > 0)
    {
        return Vehicle->RespawnAtCheckpoint(Racer->StartTransform.GetLocation(), Racer->StartTransform.Rotator());
    }
    for (const FTransform& Point : Racer->RespawnTransforms)
    {
        if (Vehicle->RespawnAtCheckpoint(
                Point.GetLocation(),
                FRotationMatrix::MakeFromXZ(Point.GetUnitAxis(EAxis::Y), Point.GetUnitAxis(EAxis::Z)).Rotator()))
        {
            return true;
        }
    }
    return false;
}
