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

bool UMWEnvironmentGameplay::DoesSupportWorldType(EWorldType::Type Type) const
{
    return Type == EWorldType::Game || Type == EWorldType::PIE;
}

void UMWEnvironmentGameplay::OnWorldBeginPlay(UWorld& World)
{
    Super::OnWorldBeginPlay(World);
    if (!Enabled || World.GetNetMode() == NM_DedicatedServer) return;
    RefreshState();
    // Handles late GameState replication and local controller replacement. No actor scan/tick.
    World.GetTimerManager().SetTimer(ReadinessTimer, this, &ThisClass::RefreshState, 0.25f, true);
}

void UMWEnvironmentGameplay::RefreshState()
{
    UWorld* World = GetWorld();
    if (!World || World->bIsTearingDown) return;
    auto* Current = World->GetGameState<AMWGameplayGameState>();
    if (State != Current)
    {
        if (State) State->OnGameplayStateChanged.RemoveDynamic(this, &ThisClass::RefreshState);
        StopEnvironment();
        State = Current;
        if (State) State->OnGameplayStateChanged.AddUniqueDynamic(this, &ThisClass::RefreshState);
    }
    if (!State) { Status = TEXT("WaitingForGameplayState"); return; }
    const EMWRoomPhase Phase = State->GetRoomPhase();
    const bool InMatch = Phase == EMWRoomPhase::PreparingConstruction || Phase == EMWRoomPhase::TrackPlacement ||
        Phase == EMWRoomPhase::ElementPlacement || Phase == EMWRoomPhase::PreparingRace ||
        Phase == EMWRoomPhase::Countdown || Phase == EMWRoomPhase::Racing;
    const FName ThemeId = State->GetRoomSettings().MapThemeId.GetTagName();
    if (!InMatch || ThemeId.IsNone())
    {
        StopEnvironment(); Status = TEXT("WaitingForMatchSettings"); return;
    }
    APlayerController* Local = nullptr;
    for (auto It = World->GetPlayerControllerIterator(); It; ++It)
        if (It->Get() && It->Get()->IsLocalController()) { Local = It->Get(); break; }
    if (!Local) { Status = TEXT("WaitingForLocalController"); return; }
    const auto* Binding = Bindings.FindByPredicate([ThemeId](const auto& B) { return B.MapThemeId == ThemeId; });
    if (!Binding)
    {
        StopEnvironment(); Status = TEXT("UnmappedTheme:") + ThemeId.ToString(); return;
    }
    if (Generator && ActiveTheme == ThemeId)
    {
        Generator->ViewPlayerController = Local;
        Status = TEXT("Active:") + ThemeId.ToString(); return;
    }
    StopEnvironment();
    UMWEnvironmentTheme* Theme = Binding->Environment.LoadSynchronous();
    if (!Theme)
    {
        Status = TEXT("MissingThemeAsset:") + Binding->Environment.ToString();
        UE_LOG(LogTemp, Error, TEXT("[MWEnvironmentGameplay] %s"), *Status); return;
    }
    // Deferred spawn supplies all inputs before the existing generator's BeginPlay.
    Generator = World->SpawnActorDeferred<AMWEnvironmentGenerator>(AMWEnvironmentGenerator::StaticClass(),
        FTransform::Identity, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!Generator) { Status = TEXT("SpawnFailed"); return; }
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
        // Only global environment actors, not vehicle/track lights or level assets.
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (!It->IsA<ADirectionalLight>() && !It->IsA<ASkyLight>() &&
                !It->IsA<ASkyAtmosphere>() && !It->IsA<AExponentialHeightFog>()) continue;
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
    UE_LOG(LogTemp, Display, TEXT("[MWEnvironmentGameplay] Theme=%s Asset=%s Seed=%d(local configured) World=%s Controller=%s"),
        *ThemeId.ToString(), *Theme->GetPathName(), LocalEnvironmentSeed, *World->GetName(), *Local->GetName());
}

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
        if (auto* Component = Entry.Key.Get()) Component->SetVisibility(Entry.Value);
    LevelLightingVisibility.Reset();
    for (const auto& Entry : LevelBackdropVisibility)
        if (auto* Actor = Entry.Key.Get()) Actor->SetActorHiddenInGame(Entry.Value);
    LevelBackdropVisibility.Reset();
}

void UMWEnvironmentGameplay::Deinitialize()
{
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ReadinessTimer);
    if (State) State->OnGameplayStateChanged.RemoveDynamic(this, &ThisClass::RefreshState);
    State = nullptr;
    StopEnvironment();
    UE_LOG(LogTemp, Display, TEXT("[MWEnvironmentGameplay] Deinitialize timer/delegate/generator cleared"));
    Super::Deinitialize();
}
