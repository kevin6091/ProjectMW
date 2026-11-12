#include "UCMTitleFidelityDirector.h"
#include "UCMMenuTraffic.h"
#include "Engine/World.h"
#include "UCMTitlePreview.h"
#include "NiagaraComponent.h"
#include "NiagaraSystemInstanceController.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraSystem.h"
#include "Components/SceneComponent.h"
#include "EngineUtils.h"
#include "LevelSequencePlayer.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
AUCMTitleFidelityDirector::AUCMTitleFidelityDirector()
{
    MenuTrafficClass = TSoftClassPtr<AActor>(FSoftObjectPath(
        TEXT("/Game/Shared/SceneVisuals/Traffic/BP_UCMMenuTraffic.BP_UCMMenuTraffic_C")));
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SourceVisualRoot"));
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PrePhysics;
    bReplicates = false;
}
void AUCMTitleFidelityDirector::ApplyMotion(float Seconds){for(const auto& M:Motions){if(!M.Target)continue;FQuat D(M.AxisWorld.GetSafeNormal(),FMath::DegreesToRadians(M.InitialDegrees+M.SpeedDegrees*Seconds));FTransform P=M.StaticWorld;P.SetLocation(M.PivotWorld+D.RotateVector(P.GetLocation()-M.PivotWorld));P.SetRotation(D*P.GetRotation());M.Target->SetActorTransform(P,false,nullptr,ETeleportType::None);if(auto* Root=M.Target->GetRootComponent())Root->ComponentVelocity=FVector::CrossProduct(M.AxisWorld.GetSafeNormal(),P.GetLocation()-M.PivotWorld)*FMath::DegreesToRadians(M.SpeedDegrees);}}
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
    for (int32 I = 0; I < SourceParticles.Num(); I++)
    {
        UNiagaraComponent* C = SourceParticles[I].Get();
        if (!C) continue;
        C->AddTickPrerequisiteActor(this);
        C->SetRandomSeedOffset(104729 + I);
        C->SetCustomTimeDilation(SourceSimulationSpeeds.IsValidIndex(I) ? SourceSimulationSpeeds[I] : 1.f);
        C->Activate(true);
    }
    for (auto C : BrandingRenderers)
    {
        if (C) C->SetVisibility(ShowOriginalBranding, false);
    }
    UE_LOG(LogTemp, Display, TEXT("UCM_FIDELITY START branding=%d particles=%d motions=%d"),
        ShowOriginalBranding, SourceParticles.Num(), Motions.Num());
}
void AUCMTitleFidelityDirector::BeginPlay(){Super::BeginPlay();if(bStartAutomatically)ResetFidelityRuntime();else{SetActorTickEnabled(false);for(auto C:SourceParticles)if(C)C->DeactivateImmediate();}}
void AUCMTitleFidelityDirector::Tick(float DeltaSeconds){Super::Tick(DeltaSeconds);float Seq=-1;for(TActorIterator<AUCMTitleFlowPreview> It(GetWorld());It;++It)if(It->SequencePlayer){Seq=It->SequencePlayer->GetCurrentTime().AsSeconds();break;}if(Seq>=0&&LastSequenceTime>=0&&Seq+0.1f<LastSequenceTime)ResetFidelityRuntime();LastSequenceTime=Seq;ApplyMotion(GetWorld()->GetTimeSeconds()-StartedAt);}
void AUCMTitleFidelityDirector::EndPlay(const EEndPlayReason::Type Reason)
{
    if (IsValid(MenuTraffic)) MenuTraffic->Destroy();
    MenuTraffic = nullptr;
    for (auto C : SourceParticles)
    {
        if (C) C->DeactivateImmediate();
    }
    Super::EndPlay(Reason);
}
FString AUCMTitleFidelityDirector::GetFidelityEvidence()const{auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("show_original_branding"),ShowOriginalBranding);O->SetNumberField(TEXT("elapsed"),GetWorld()->GetTimeSeconds()-StartedAt);O->SetNumberField(TEXT("resets"),Resets);TArray<TSharedPtr<FJsonValue>> P;for(auto C:SourceParticles){auto V=MakeShared<FJsonObject>();if(C){V->SetStringField(TEXT("component"),C->GetPathName());V->SetStringField(TEXT("asset"),C->GetAsset()?C->GetAsset()->GetPathName():TEXT("NONE"));V->SetBoolField(TEXT("active"),C->IsActive());V->SetNumberField(TEXT("simulation_speed"),C->GetCustomTimeDilation());int32 Count=0;if(auto Control=C->GetSystemInstanceController()){if(auto Instance=Control->GetSystemInstance_Unsafe()){Instance->WaitForConcurrentTickAndFinalize();for(const auto& E:Instance->GetEmitters())Count+=E->GetNumParticles();}}V->SetNumberField(TEXT("particles"),Count);FVector L=C->GetComponentLocation();V->SetArrayField(TEXT("location"),{MakeShared<FJsonValueNumber>(L.X),MakeShared<FJsonValueNumber>(L.Y),MakeShared<FJsonValueNumber>(L.Z)});}P.Add(MakeShared<FJsonValueObject>(V));}O->SetArrayField(TEXT("particles"),P);TArray<TSharedPtr<FJsonValue>> B;for(auto C:BrandingRenderers){auto V=MakeShared<FJsonObject>();if(C){V->SetStringField(TEXT("component"),C->GetPathName());V->SetBoolField(TEXT("visible"),C->IsVisible());}B.Add(MakeShared<FJsonValueObject>(V));}O->SetArrayField(TEXT("branding"),B);FString S;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&S));return S;}
