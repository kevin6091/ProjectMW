// 기존 경기 테마·단계를 완료된 Environment 생성기에 연결하고 월드 종료 때 정리한다.

#include "MWEnvironmentGameplay.h"
#include "MWEnvironmentRestore.h"
#include "System/GameState/MWGameplayGameState.h"
#include "Engine/World.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/SceneComponent.h"
#include "EngineUtils.h"
#include "Engine/Level.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Modules/ModuleManager.h"
#include "TimerManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, MWEnvironmentGameplay)

// 일반 게임과 PIE 월드만 대상으로 해 에디터 미리보기에서 런타임 기능이 켜지지 않게 한다.
bool UMWEnvironmentGameplay::DoesSupportWorldType(EWorldType::Type Type) const
{
    return Type == EWorldType::Game || Type == EWorldType::PIE;
}

// 로컬 경기 월드에서 기존 상태를 읽고 늦은 복제·컨트롤러 준비 점검을 등록한다.
void UMWEnvironmentGameplay::OnWorldBeginPlay(UWorld& World)
{
    Super::OnWorldBeginPlay(World);
    if (!Enabled || World.GetNetMode() == NM_DedicatedServer)
        return;
    RefreshState();
    // 늦은 GameState 복제와 로컬 컨트롤러 교체를 주기적으로 확인한다. 매 프레임 액터를 검색하지 않는다.
    World.GetTimerManager().SetTimer(ReadinessTimer, this, &ThisClass::RefreshState, 0.25f, true);
}

// 기존 복제 테마·단계와 로컬 컨트롤러를 확인해 완료본 배경 생성기를 연결한다.
void UMWEnvironmentGameplay::RefreshState()
{
    UWorld* World = GetWorld();
    if (!World || World->bIsTearingDown)
        return;
    auto* Current = World->GetGameState<AMWGameplayGameState>();
    if (State != Current)
    {
        if (State)
            State->OnGameplayStateChanged.RemoveDynamic(this, &ThisClass::RefreshState);
        StopEnvironment();
        State = Current;
        if (State)
            State->OnGameplayStateChanged.AddUniqueDynamic(this, &ThisClass::RefreshState);
    }
    if (!State)
    {
        Status = TEXT("WaitingForGameplayState");
        return;
    }
    const EMWRoomPhase Phase = State->GetRoomPhase();
    const bool InMatch = Phase == EMWRoomPhase::PreparingConstruction || Phase == EMWRoomPhase::TrackPlacement ||
                         Phase == EMWRoomPhase::ElementPlacement || Phase == EMWRoomPhase::PreparingRace ||
                         Phase == EMWRoomPhase::Countdown || Phase == EMWRoomPhase::Racing;
    const FName ThemeId = State->GetRoomSettings().MapThemeId.GetTagName();
    if (!InMatch || ThemeId.IsNone())
    {
        StopEnvironment();
        Status = TEXT("WaitingForMatchSettings");
        return;
    }
    APlayerController* Local = nullptr;
    for (auto It = World->GetPlayerControllerIterator(); It; ++It)
        if (It->Get() && It->Get()->IsLocalController())
        {
            Local = It->Get();
            break;
        }
    if (!Local)
    {
        Status = TEXT("WaitingForLocalController");
        return;
    }
    const auto* Binding = Bindings.FindByPredicate(
        [ThemeId](const auto& B)
        {
            return B.MapThemeId == ThemeId;
        });
    if (!Binding)
    {
        StopEnvironment();
        Status = TEXT("UnmappedTheme:") + ThemeId.ToString();
        return;
    }
    if (Generator && ActiveTheme == ThemeId)
    {
        Generator->ViewPlayerController = Local;
        Status = TEXT("Active:") + ThemeId.ToString();
        return;
    }
    StopEnvironment();
    UMWEnvironmentTheme* Theme = Binding->Environment.LoadSynchronous();
    if (!Theme)
    {
        Status = TEXT("MissingThemeAsset:") + Binding->Environment.ToString();
        UE_LOG(LogTemp, Error, TEXT("[MWEnvironmentGameplay] %s"), *Status);
        return;
    }
    // 기존 생성기의 BeginPlay가 실행되기 전에 테마·시드·로컬 시점을 지정한다.
    Generator = World->SpawnActorDeferred<AMWEnvironmentGenerator>(AMWEnvironmentGenerator::StaticClass(),
                                                                   FTransform::Identity, nullptr, nullptr,
                                                                   ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!Generator)
    {
        Status = TEXT("SpawnFailed");
        return;
    }
    Generator->Theme = Theme;
    Generator->Seed = LocalEnvironmentSeed;
    Generator->ViewPlayerController = Local;
    UGameplayStatics::FinishSpawningActor(Generator, FTransform::Identity);
    for (AActor* Actor : World->PersistentLevel->Actors)
    {
        if (Actor && HiddenBackdropActorPaths.Contains(UWorld::RemovePIEPrefix(Actor->GetPathName())))
        {
            LevelBackdropVisibility.Add(Actor, Actor->IsHidden());
            Actor->SetActorHiddenInGame(true);
        }
    }
    if (ReplaceLevelLighting)
    {
        // 전역 배경 조명만 대상으로 한다. 차량·트랙 조명이나 레벨 에셋 데이터는 수정하지 않는다.
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (!It->IsA<ADirectionalLight>() && !It->IsA<ASkyLight>() && !It->IsA<ASkyAtmosphere>() &&
                !It->IsA<AExponentialHeightFog>())
                continue;
            TArray<USceneComponent*> Components;
            It->GetComponents(Components);
            for (auto* Component : Components)
            {
                LevelLightingVisibility.Add(Component, Component->IsVisible());
                Component->SetVisibility(false);
            }
        }
    }
    ActiveTheme = ThemeId;
    Status = TEXT("Active:") + ThemeId.ToString();
    UE_LOG(LogTemp, Display,
           TEXT("[MWEnvironmentGameplay] Theme=%s Asset=%s Seed=%d(local configured) World=%s Controller=%s"),
           *ThemeId.ToString(), *Theme->GetPathName(), LocalEnvironmentSeed, *World->GetName(), *Local->GetName());
}

// 현재 생성기를 정리하고 잠시 숨겼던 레벨 배경·조명의 표시 상태를 복원한다.
void UMWEnvironmentGameplay::StopEnvironment()
{
    if (IsValid(Generator))
    {
        Generator->ClearEnvironment();
        Generator->Destroy();
    }
    Generator = nullptr;
    ActiveTheme = NAME_None;
    for (const auto& Entry : LevelLightingVisibility)
        if (auto* Component = Entry.Key.Get())
            Component->SetVisibility(Entry.Value);
    LevelLightingVisibility.Reset();
    for (const auto& Entry : LevelBackdropVisibility)
        if (auto* Actor = Entry.Key.Get())
            Actor->SetActorHiddenInGame(Entry.Value);
    LevelBackdropVisibility.Reset();
}

// 준비 점검 타이머와 실제 구독 상태를 해제한 뒤 배경 수명을 닫는다.
void UMWEnvironmentGameplay::Deinitialize()
{
    if (GetWorld())
        GetWorld()->GetTimerManager().ClearTimer(ReadinessTimer);
    if (State)
        State->OnGameplayStateChanged.RemoveDynamic(this, &ThisClass::RefreshState);
    State = nullptr;
    StopEnvironment();
    UE_LOG(LogTemp, Display, TEXT("[MWEnvironmentGameplay] Deinitialize timer/delegate/generator cleared"));
    Super::Deinitialize();
}
