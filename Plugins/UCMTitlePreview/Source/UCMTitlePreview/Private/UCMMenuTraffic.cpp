// 원본 경로·이벤트·시간에 맞춘 타이틀 교통 재생이다. 경기 차량 AI가 아니다.

#include "UCMMenuTraffic.h"
#include "Components/SceneComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieSceneObjectBindingID.h"

AUCMMenuTraffic::AUCMMenuTraffic()
{
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("TrafficOrigin"));
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    bReplicates = false;
}

// 복구 자료의 식별자와 원본 참조 태그로 이 월드의 실제 액터를 찾는다.
AActor* AUCMMenuTraffic::FindSourceActor(const FString& Id) const
{
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        if (It->GetLevel() != GetLevel())
        {
            continue;
        }
        for (const FName Tag : It->Tags)
        {
            if (Tag.ToString().StartsWith(TEXT("UnitySourceRef=unity://")) && Tag.ToString().EndsWith(TEXT("/") + Id))
            {
                return *It;
            }
        }
    }
    return nullptr;
}

// 복구 경로와 재생 대상을 준비한 뒤 메뉴 교통 연출을 시작한다.
bool AUCMMenuTraffic::StartTraffic()
{
    // 타이틀 재표시에서 중복 풀을 만들지 않는다. 기준 액터가 사라졌다면 이전 재생부터 정리한다.
    if (bStarted && IsValid(Crossing))
    {
        return true;
    }
    if (bStarted)
    {
        StopTraffic();
    }
    if (GetNetMode() == NM_DedicatedServer || Vehicles.IsEmpty() || Lanes.IsEmpty() || PoolLimit < 1)
    {
        return false;
    }
    Crossing = FindSourceActor(CrossingSourceId);
    if (!Crossing)
    {
        // 원본 기준 액터를 찾지 못했을 때 임의 좌표로 대체하면 배치가 달라지므로 시작하지 않는다.
        UE_LOG(LogTemp, Warning, TEXT("UCM_TRAFFIC missing source crossing %s; no guessed placement"),
               *CrossingSourceId);
        return false;
    }
    for (const auto& Preset : Vehicles)
    {
        if (!Preset.VehicleClass || !Preset.Sequence || !Preset.OwnerBinding.IsValid() ||
            !FMath::IsFinite(Preset.Duration) || Preset.Duration <= 0 || Preset.Weight <= 0)
        {
            return false;
        }
    }
    for (auto& Gate : Gates)
    {
        if (!Lanes.IsValidIndex(Gate.Lane) || !Gate.Curve)
        {
            return false;
        }
        for (auto& Part : Gate.Parts)
        {
            Part.Target = FindSourceActor(Part.SourceId);
            if (!Part.Target || !Part.Target->GetRootComponent())
            {
                UE_LOG(LogTemp, Warning, TEXT("UCM_TRAFFIC missing gate part %s; startup cancelled"), *Part.SourceId);
                return false;
            }
            Part.OriginalWorld = Part.Target->GetActorTransform();
            Part.OriginalMobility = Part.Target->GetRootComponent()->Mobility;
        }
    }
    for (auto& Gate : Gates)
    {
        for (auto& Part : Gate.Parts)
        {
            Part.Target->GetRootComponent()->SetMobility(EComponentMobility::Movable);
        }
    }
    NextSpawn.Init(GetWorld()->GetTimeSeconds(), Lanes.Num());
    bStarted = true;
    SetActorTickEnabled(true);
    UE_LOG(LogTemp, Display, TEXT("UCM_TRAFFIC START lanes=%d candidates=%d pool_limit=%d"), Lanes.Num(),
           Vehicles.Num(), PoolLimit);
    return true;
}

// 차선 데이터의 생성 규칙에 따라 로컬 외형 재생을 준비한다. 경기 차량 AI가 아니다.
bool AUCMMenuTraffic::SpawnTraffic(int32 Lane)
{
    int32 TotalWeight = 0;
    for (const auto& Preset : Vehicles)
    {
        TotalWeight += Preset.Weight;
    }
    int32 WeightChoice = FMath::RandRange(0, TotalWeight - 1);
    int32 PresetIndex = 0;
    for (; PresetIndex < Vehicles.Num() - 1; ++PresetIndex)
    {
        if (WeightChoice < Vehicles[PresetIndex].Weight)
        {
            break;
        }
        WeightChoice -= Vehicles[PresetIndex].Weight;
    }
    const auto& Preset = Vehicles[PresetIndex];
    int32 SlotIndex = Pool.IndexOfByPredicate(
        [PresetIndex](const FUCMTrafficPlayback& Playback)
        {
            return !Playback.bActive && Playback.Preset == PresetIndex;
        });
    if (SlotIndex == INDEX_NONE)
    {
        if (Pool.Num() < PoolLimit)
        {
            SlotIndex = Pool.AddDefaulted();
        }
        else
        {
            SlotIndex = Pool.IndexOfByPredicate(
                [](const FUCMTrafficPlayback& Playback)
                {
                    return !Playback.bActive;
                });
        }
        if (SlotIndex == INDEX_NONE)
        {
            return false;
        }
    }
    auto& Playback = Pool[SlotIndex];
    if (Playback.Preset != PresetIndex || !IsValid(Playback.Vehicle) || !IsValid(Playback.Player) ||
        !IsValid(Playback.SequenceActor))
    {
        ReleasePlayback(Playback);
        FActorSpawnParameters Params;
        Params.OverrideLevel = GetLevel();
        Params.Owner = this;
        Playback.Vehicle = GetWorld()->SpawnActor<AActor>(Preset.VehicleClass, FTransform::Identity, Params);
        if (!Playback.Vehicle)
        {
            return false;
        }
        Playback.Vehicle->SetActorEnableCollision(false);
        Playback.Vehicle->SetReplicates(false);
        FMovieSceneSequencePlaybackSettings Settings;
        Settings.bPauseAtEnd = true;
        // 배경 차량의 시퀀스가 플레이어의 타이틀 카메라를 바꾸지 못하게 한다.
        Settings.bDisableCameraCuts = true;
        ALevelSequenceActor* SequenceActor = nullptr;
        Playback.Player =
            ULevelSequencePlayer::CreateLevelSequencePlayer(GetWorld(), Preset.Sequence, Settings, SequenceActor);
        Playback.SequenceActor = SequenceActor;
        if (!Playback.Player || !Playback.SequenceActor)
        {
            ReleasePlayback(Playback);
            return false;
        }
        Playback.SequenceActor->SetBinding(
            FMovieSceneObjectBindingID(UE::MovieScene::FRelativeObjectBindingID(Preset.OwnerBinding)),
            {Playback.Vehicle.Get()}, false);
        Playback.Preset = PresetIndex;
    }
    Playback.Lane = Lane;
    Playback.NextEvent = 0;
    Playback.Vehicle->SetActorTransform(Lanes[Lane] * Crossing->GetActorTransform());
    Playback.Vehicle->SetActorHiddenInGame(false);
    Playback.Player->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(0.f, EUpdatePositionMethod::Jump));
    Playback.Player->Play();
    Playback.bActive = true;
    ApplyEvents(Playback, 0);
    NextSpawn[Lane] = GetWorld()->GetTimeSeconds() + Preset.SpawnTime + FMath::FRandRange(SpawnDelayMin, SpawnDelayMax);
    UE_LOG(LogTemp, Display, TEXT("UCM_TRAFFIC SPAWN lane=%d preset=%d next=%.3f"), Lane, PresetIndex, NextSpawn[Lane]);
    return true;
}

// 원본 시간에 도달한 이벤트만 진행시켜 부품 표시와 애니메이션을 맞춘다.
void AUCMMenuTraffic::ApplyEvents(FUCMTrafficPlayback& Playback, float Time)
{
    const auto& Events = Vehicles[Playback.Preset].Events;
    while (Events.IsValidIndex(Playback.NextEvent) && Events[Playback.NextEvent].Time <= Time)
    {
        const auto& Event = Events[Playback.NextEvent++];
        for (auto& Gate : Gates)
        {
            if (Gate.Lane == Playback.Lane && Gate.Gate == Event.Gate)
            {
                Gate.OpenCount = FMath::Max(0, Gate.OpenCount + (Event.bOpen ? 1 : -1));
            }
        }
    }
}

// 진행 중인 교통 외형만 원본 경로에 맞춰 갱신하고 끝난 재생은 정리한다.
void AUCMMenuTraffic::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bStarted)
    {
        return;
    }
    if (!IsValid(Crossing))
    {
        UE_LOG(LogTemp, Warning, TEXT("UCM_TRAFFIC crossing removed; stopping playback and restoring gates"));
        StopTraffic();
        return;
    }
    for (auto& Playback : Pool)
    {
        if (!Playback.bActive)
        {
            continue;
        }
        if (!IsValid(Playback.Vehicle) || !IsValid(Playback.Player) || !IsValid(Playback.SequenceActor))
        {
            UE_LOG(LogTemp, Warning, TEXT("UCM_TRAFFIC invalid playback; releasing outstanding gate requests"));
            // 재생 객체가 먼저 사라져도 남은 닫기 이벤트를 소비해 차단기가 열린 채 남지 않게 한다.
            ApplyEvents(Playback, Vehicles[Playback.Preset].Duration);
            // 풀의 일부만 사라진 경우 살아 있는 차량·시퀀스도 함께 해제해 고아 객체를 남기지 않는다.
            ReleasePlayback(Playback);
            continue;
        }
        const float Time = Playback.Player->GetCurrentTime().AsSeconds();
        const bool bFinished = Time >= Vehicles[Playback.Preset].Duration - 1.f / 60.f ||
                               (!Playback.Player->IsPlaying() && !Playback.Player->IsPaused());
        ApplyEvents(Playback, bFinished ? Vehicles[Playback.Preset].Duration : Time);
        if (bFinished)
        {
            // 정상 종료는 다음 재생에서 재사용한다. 실패 시의 파괴 정리와 구분한다.
            Playback.Player->Stop();
            Playback.Vehicle->SetActorHiddenInGame(true);
            Playback.bActive = false;
        }
    }
    for (int32 Lane = 0; Lane < NextSpawn.Num(); ++Lane)
    {
        if (GetWorld()->GetTimeSeconds() >= NextSpawn[Lane] && !SpawnTraffic(Lane))
        {
            NextSpawn[Lane] = GetWorld()->GetTimeSeconds() + 1;
        }
    }
    for (auto& Gate : Gates)
    {
        const float Previous = Gate.Progress;
        Gate.Progress =
            FMath::Clamp(Previous + DeltaSeconds * (Gate.OpenCount > 0 ? Gate.OpenSpeed : -Gate.CloseSpeed), 0.f, 1.f);
        if (Gate.Progress == Previous)
        {
            continue;
        }
        const float Angle = Gate.Curve->GetFloatValue(Gate.Progress);
        for (auto& Part : Gate.Parts)
        {
            if (!IsValid(Part.Target))
            {
                continue;
            }
            FTransform Local = Part.LocalRest;
            // 원본의 로컬 Z축 회전은 좌표 변환 후 언리얼의 양의 Y축 회전에 해당한다.
            Local.SetRotation(FQuat(FVector::RightVector, FMath::DegreesToRadians(Angle * Part.Direction)));
            Part.Target->SetActorTransform(Local * Part.ParentRelativeToCrossing * Crossing->GetActorTransform());
        }
    }
}

// 검증 화면에서 현재 살아 있는 교통 재생 수를 조회한다.
int32 AUCMMenuTraffic::GetActiveVehicleCount() const
{
    return Pool
        .FilterByPredicate(
            [](const FUCMTrafficPlayback& Playback)
            {
                return Playback.bActive;
            })
        .Num();
}

// 중단된 재생에서도 바인딩과 원래 렌더러 상태를 복구한다.
void AUCMMenuTraffic::ReleasePlayback(FUCMTrafficPlayback& Playback)
{
    // 중도 실패·후보 교체·EndPlay가 같은 순서로 해제되도록 한곳에서 처리한다.
    if (IsValid(Playback.Player))
    {
        Playback.Player->Stop();
    }
    if (IsValid(Playback.SequenceActor))
    {
        Playback.SequenceActor->Destroy();
    }
    if (IsValid(Playback.Vehicle))
    {
        Playback.Vehicle->Destroy();
    }
    Playback = FUCMTrafficPlayback();
}

// 모든 재생과 생성 외형을 정리해 메뉴 재진입에 이전 교통이 남지 않게 한다.
void AUCMMenuTraffic::StopTraffic()
{
    SetActorTickEnabled(false);
    for (auto& Playback : Pool)
    {
        ReleasePlayback(Playback);
    }
    // 런타임에서 바꾼 차단기의 위치와 이동 가능 상태는 종료 시 원래 값으로 되돌린다.
    if (bStarted)
    {
        for (auto& Gate : Gates)
        {
            for (auto& Part : Gate.Parts)
            {
                if (IsValid(Part.Target) && Part.Target->GetRootComponent())
                {
                    Part.Target->SetActorTransform(Part.OriginalWorld);
                    Part.Target->GetRootComponent()->SetMobility(
                        static_cast<EComponentMobility::Type>(Part.OriginalMobility));
                }
            }
        }
    }
    for (auto& Gate : Gates)
    {
        Gate.OpenCount = 0;
        Gate.Progress = 0;
        for (auto& Part : Gate.Parts)
        {
            Part.Target = nullptr;
        }
    }
    Pool.Reset();
    NextSpawn.Reset();
    Crossing = nullptr;
    bStarted = false;
}

// 월드 종료 때도 기존 재생과 복구 상태를 정리한 뒤 부모 수명을 닫는다.
void AUCMMenuTraffic::EndPlay(const EEndPlayReason::Type Reason)
{
    StopTraffic();
    Super::EndPlay(Reason);
}
