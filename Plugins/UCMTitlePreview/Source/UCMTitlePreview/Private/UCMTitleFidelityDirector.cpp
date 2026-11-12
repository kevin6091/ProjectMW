// 원본 타이틀 부품 움직임과 교통 재생을 같은 연출 수명으로 관리한다.

#include "UCMTitleFidelityDirector.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "LevelSequencePlayer.h"
#include "NiagaraComponent.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraSystemInstanceController.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UCMMenuTraffic.h"
#include "UCMTitlePreview.h"

AUCMTitleFidelityDirector::AUCMTitleFidelityDirector()
{
    MenuTrafficClass = TSoftClassPtr<AActor>(
        FSoftObjectPath(TEXT("/Game/Shared/SceneVisuals/Traffic/BP_UCMMenuTraffic.BP_UCMMenuTraffic_C")));
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SourceVisualRoot"));
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PrePhysics;
    bReplicates = false;
}

// 복구한 원본 시간·축·주기로 타이틀 배경 부품의 움직임을 재생한다.
void AUCMTitleFidelityDirector::ApplyMotion(float Seconds)
{
    for (const auto& Motion : Motions)
    {
        if (!Motion.Target)
        {
            continue;
        }

        // 원본 월드 축과 회전 중심을 유지하며 누적 회전 대신 시작 자세에서 계산한다.
        const FQuat RotationDelta(Motion.AxisWorld.GetSafeNormal(),
                                  FMath::DegreesToRadians(Motion.InitialDegrees + Motion.SpeedDegrees * Seconds));
        FTransform Pose = Motion.StaticWorld;
        Pose.SetLocation(Motion.PivotWorld + RotationDelta.RotateVector(Pose.GetLocation() - Motion.PivotWorld));
        Pose.SetRotation(RotationDelta * Pose.GetRotation());
        Motion.Target->SetActorTransform(Pose, false, nullptr, ETeleportType::None);

        if (auto* Root = Motion.Target->GetRootComponent())
        {
            Root->ComponentVelocity =
                FVector::CrossProduct(Motion.AxisWorld.GetSafeNormal(), Pose.GetLocation() - Motion.PivotWorld) *
                FMath::DegreesToRadians(Motion.SpeedDegrees);
        }
    }
}

// 메뉴 재진입 시 원본 기준 자세와 교통 재생 상태를 다시 준비한다.
void AUCMTitleFidelityDirector::ResetFidelityRuntime()
{
    if (GetNetMode() != NM_DedicatedServer)
    {
        // 타이틀이나 인트로를 다시 표시할 때도 배경 교통 제어기는 하나만 재사용한다.
        if (!IsValid(MenuTraffic))
        {
            if (UClass* TrafficClass = MenuTrafficClass.LoadSynchronous())
            {
                FActorSpawnParameters Params;
                Params.Owner = this;
                Params.OverrideLevel = GetLevel();
                MenuTraffic = GetWorld()->SpawnActor<AActor>(TrafficClass, FTransform::Identity, Params);
            }
        }
        if (auto* Traffic = Cast<AUCMMenuTraffic>(MenuTraffic))
        {
            Traffic->StartTraffic();
        }
    }

    StartedAt = GetWorld()->GetTimeSeconds();
    Resets++;
    ApplyMotion(0);
    for (int32 Index = 0; Index < SourceParticles.Num(); Index++)
    {
        UNiagaraComponent* Component = SourceParticles[Index].Get();
        if (!Component)
        {
            continue;
        }
        Component->AddTickPrerequisiteActor(this);
        Component->SetRandomSeedOffset(104729 + Index);
        Component->SetCustomTimeDilation(SourceSimulationSpeeds.IsValidIndex(Index) ? SourceSimulationSpeeds[Index]
                                                                                    : 1.f);
        Component->Activate(true);
    }

    for (auto Component : BrandingRenderers)
    {
        if (Component)
        {
            Component->SetVisibility(ShowOriginalBranding, false);
        }
    }
    UE_LOG(LogTemp, Display, TEXT("UCM_FIDELITY START branding=%d particles=%d motions=%d"), ShowOriginalBranding,
           SourceParticles.Num(), Motions.Num());
}

// 원본 부품 기준 자세와 로컬 교통 재생을 이 월드에서 준비한다.
void AUCMTitleFidelityDirector::BeginPlay()
{
    Super::BeginPlay();
    if (bStartAutomatically)
    {
        ResetFidelityRuntime();
    }
    else
    {
        SetActorTickEnabled(false);
        for (auto Component : SourceParticles)
        {
            if (Component)
            {
                Component->DeactivateImmediate();
            }
        }
    }
}

// 이 연출의 경과 시간으로 배경 움직임을 갱신한다. 경기 규칙이나 차량 입력은 수정하지 않는다.
void AUCMTitleFidelityDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    float SequenceTime = -1;
    for (TActorIterator<AUCMTitleFlowPreview> It(GetWorld()); It; ++It)
    {
        if (It->SequencePlayer)
        {
            SequenceTime = It->SequencePlayer->GetCurrentTime().AsSeconds();
            break;
        }
    }

    // 원본 시퀀스가 되감긴 경우에만 소품·파티클의 재생 기준 시각을 다시 맞춘다.
    if (SequenceTime >= 0 && LastSequenceTime >= 0 && SequenceTime + 0.1f < LastSequenceTime)
    {
        ResetFidelityRuntime();
    }
    LastSequenceTime = SequenceTime;
    ApplyMotion(GetWorld()->GetTimeSeconds() - StartedAt);
}

// 현재 월드의 교통·부품 재생 참조를 해제한다.
void AUCMTitleFidelityDirector::EndPlay(const EEndPlayReason::Type Reason)
{
    if (IsValid(MenuTraffic))
    {
        MenuTraffic->Destroy();
    }
    MenuTraffic = nullptr;

    for (auto Component : SourceParticles)
    {
        if (Component)
        {
            Component->DeactivateImmediate();
        }
    }
    Super::EndPlay(Reason);
}

// 연출의 현재 재생 상태를 검증용 문자열로 반환한다.
FString AUCMTitleFidelityDirector::GetFidelityEvidence() const
{
    auto Evidence = MakeShared<FJsonObject>();
    Evidence->SetBoolField(TEXT("show_original_branding"), ShowOriginalBranding);
    Evidence->SetNumberField(TEXT("elapsed"), GetWorld()->GetTimeSeconds() - StartedAt);
    Evidence->SetNumberField(TEXT("resets"), Resets);

    TArray<TSharedPtr<FJsonValue>> ParticleRows;
    for (auto Component : SourceParticles)
    {
        auto Row = MakeShared<FJsonObject>();
        if (Component)
        {
            Row->SetStringField(TEXT("component"), Component->GetPathName());
            Row->SetStringField(TEXT("asset"),
                                Component->GetAsset() ? Component->GetAsset()->GetPathName() : TEXT("NONE"));
            Row->SetBoolField(TEXT("active"), Component->IsActive());
            Row->SetNumberField(TEXT("simulation_speed"), Component->GetCustomTimeDilation());

            int32 ParticleCount = 0;
            if (auto Controller = Component->GetSystemInstanceController())
            {
                if (auto Instance = Controller->GetSystemInstance_Unsafe())
                {
                    // 명시적인 검증 조회에서만 완료된 파티클 수를 읽는다. Tick의 재생 경로에서는 호출하지 않는다.
                    Instance->WaitForConcurrentTickAndFinalize();
                    for (const auto& Emitter : Instance->GetEmitters())
                    {
                        ParticleCount += Emitter->GetNumParticles();
                    }
                }
            }
            Row->SetNumberField(TEXT("particles"), ParticleCount);
            FVector Location = Component->GetComponentLocation();
            Row->SetArrayField(TEXT("location"),
                               {MakeShared<FJsonValueNumber>(Location.X), MakeShared<FJsonValueNumber>(Location.Y),
                                MakeShared<FJsonValueNumber>(Location.Z)});
        }
        ParticleRows.Add(MakeShared<FJsonValueObject>(Row));
    }
    Evidence->SetArrayField(TEXT("particles"), ParticleRows);

    TArray<TSharedPtr<FJsonValue>> BrandingRows;
    for (auto Component : BrandingRenderers)
    {
        auto Row = MakeShared<FJsonObject>();
        if (Component)
        {
            Row->SetStringField(TEXT("component"), Component->GetPathName());
            Row->SetBoolField(TEXT("visible"), Component->IsVisible());
        }
        BrandingRows.Add(MakeShared<FJsonValueObject>(Row));
    }
    Evidence->SetArrayField(TEXT("branding"), BrandingRows);

    FString Serialized;
    FJsonSerializer::Serialize(Evidence, TJsonWriterFactory<>::Create(&Serialized));
    return Serialized;
}
