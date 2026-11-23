// 월드의 실제 트랙을 복구 카탈로그에 연결하고 차량별 진행·복귀 정보를 관리한다.

#include "Gameplay/Track/MWTrackRuntimeSubsystem.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Gameplay/Construction/MWTrackPreview.h"
#include "Gameplay/Construction/MWTrackSnap.h"
#include "Gameplay/Track/MWTrackRuntimeComponent.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "System/GameMode/MWGameplayGameMode.h"
#include "System/GameState/MWGameplayGameState.h"
#include "System/MWPlayerState.h"
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

// 일반 게임과 PIE 월드만 대상으로 해 에디터 미리보기에서 런타임 기능이 켜지지 않게 한다.
bool UMWTrackRuntimeSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

// 기존 배치 트랙을 연결하고 이후 생성 이벤트도 등록해 늦게 도착한 조각을 놓치지 않는다.
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
    Super::Deinitialize();
}

// 늦게 복제되거나 생성된 트랙에도 기존 카탈로그 연결을 적용한다.
void UMWTrackRuntimeSubsystem::OnActorSpawned(AActor* Actor)
{
    if (!Pieces.Contains(Actor->GetClass()->GetFName()))
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
    // 선택판과 고스트에는 다음 틱에도 경기용 충돌·트리거를 만들지 않는다.
    if (!IsValid(Actor) || MWTrackPreview::IsDisplayOnly(*Actor) || !Actor->GetRootComponent() ||
        Actor->FindComponentByClass<UMWTrackRuntimeComponent>())
    {
        return;
    }
    const FMWTrackPieceRuntimeData* const* Data = Pieces.Find(Actor->GetClass()->GetFName());
    if (!Data)
    {
        return;
    }

    const bool bLift = (*Data)->Motions.ContainsByPredicate(
        [](const FMWTrackMotionData& Motion)
        {
            return Motion.Type == EMWTrackMotion::Lift;
        });
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

// 복제로 생긴 컴포넌트도 현재 카탈로그의 같은 복구 설정으로 초기화한다.
void UMWTrackRuntimeSubsystem::ConfigureRuntimeComponent(UMWTrackRuntimeComponent& Component)
{
    const AActor* Actor = Component.GetOwner();
    if (!Catalog || !IsValid(Actor))
    {
        return;
    }
    // 클라이언트의 월드 초기화보다 먼저 호출될 수 있으므로 아직 완성되지 않은 조회 표에 의존하지 않는다.
    const FMWTrackPieceRuntimeData* Data = Catalog->Pieces.FindByPredicate(
        [Actor](const FMWTrackPieceRuntimeData& Entry)
        {
            return Entry.VisualClass == Actor->GetClass()->GetFName();
        });
    if (Data)
    {
        Component.SetPreview(Actor->ActorHasTag(PreviewTag));
        Component.Configure(*Data);
    }
}

// 배치 미리보기 상태를 실제 트랙 런타임에 전달한다.
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
    const auto* Data = Catalog ? Catalog->Pieces.FindByPredicate([&Track](const FMWTrackPieceRuntimeData& Entry)
        { return Entry.VisualClass == Track.GetClass()->GetFName(); }) : nullptr;
    return Data && Data->Shapes.ContainsByPredicate([](const FMWTrackShapeData& Shape) { return Shape.Trigger == EMWTrackTrigger::Checkpoint; });
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
