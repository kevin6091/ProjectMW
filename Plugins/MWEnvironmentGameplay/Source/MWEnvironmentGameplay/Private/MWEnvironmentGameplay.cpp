// 기존 경기 테마·단계를 완료된 Environment 생성기에 연결하고 월드 종료 때 정리한다.

#include "MWEnvironmentGameplay.h"
#include "UI/Gameplay/MWArrivalScreen.h"
#include "Materials/MaterialInterface.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "MWEnvironmentPreload.h"
#include "MWEnvironmentRestore.h"
#include "System/GameState/MWGameplayGameState.h"
#include "System/GameState/MWLobbyGameState.h"
#include "System/MWAssetManager.h"
#include "Engine/GameInstance.h"
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
    World.GetGameInstance()->GetSubsystem<UMWArrivalScreen>()->CollectEnvironment.AddUObject(this, &ThisClass::CollectFirstView);
    RefreshState();
    // 늦은 GameState 복제와 로컬 컨트롤러 교체를 주기적으로 확인한다. 매 프레임 액터를 검색하지 않는다.
    World.GetTimerManager().SetTimer(ReadinessTimer, this, &ThisClass::RefreshState, 0.25f, true);
}

const FMWEnvironmentBinding* UMWEnvironmentGameplay::FindBinding(FName ThemeId) const
{
    return Bindings.FindByPredicate([ThemeId](const FMWEnvironmentBinding& Binding)
    {
        return Binding.MapThemeId == ThemeId;
    });
}

void UMWEnvironmentGameplay::RefreshLobbyTheme()
{
    const AMWLobbyGameState* Lobby = LobbyState.Get();
    UGameInstance* Game = GetWorld()->GetGameInstance();
    auto* Preload = Game ? Game->GetSubsystem<UMWEnvironmentPreload>() : nullptr;
    if (!Lobby || !Preload)
    {
        return;
    }
    const EMWRoomPhase Phase = Lobby->GetRoomPhase();
    const FGameplayTag ThemeId = Lobby->GetRoomSettings().MapThemeId;
    const auto* Binding = FindBinding(ThemeId.GetTagName());
    if (!Binding || Phase == EMWRoomPhase::Failed)
    {
        Preload->ClearSelection();
        return;
    }
    const FSoftObjectPath MapPath = UMWAssetManager::Get().FindAssetPath(ThemeId);
    Preload->SelectTheme(ThemeId.GetTagName(), MapPath.GetLongPackageFName(), Binding->Environment, Binding->FirstViewMaterials,
                         Phase == EMWRoomPhase::Traveling);
}

// 기존 복제 테마·단계와 로컬 컨트롤러를 확인해 완료본 배경 생성기를 연결한다.
void UMWEnvironmentGameplay::RefreshState()
{
    UWorld* World = GetWorld();
    if (!World || World->bIsTearingDown || World->GetNetMode() == NM_DedicatedServer)
        return;
    auto* Preload = UGameInstance::GetSubsystem<UMWEnvironmentPreload>(World->GetGameInstance());
    const FName ArrivalTheme = ReplaceLevelLighting && Preload ? Preload->FindLoadedArrivalTheme(*World) : NAME_None;
    if (auto* Lobby = World->GetGameState<AMWLobbyGameState>(); Lobby && ArrivalTheme.IsNone())
    {
        if (!bPreloadLobbyTheme)
        {
            return;
        }
        if (LobbyState.Get() != Lobby)
        {
            if (LobbyState.IsValid())
            {
                LobbyState->OnLobbyChanged.RemoveDynamic(this, &ThisClass::RefreshLobbyTheme);
            }
            LobbyState = Lobby;
            Lobby->OnLobbyChanged.AddUniqueDynamic(this, &ThisClass::RefreshLobbyTheme);
            RefreshLobbyTheme();
        }
        return;
    }
    auto* Current = World->GetGameState<AMWGameplayGameState>();
    if (State != Current)
    {
        if (State)
            State->OnGameplayStateChanged.RemoveDynamic(this, &ThisClass::RefreshState);
        State = Current;
        if (State)
            State->OnGameplayStateChanged.AddUniqueDynamic(this, &ThisClass::RefreshState);
    }
    const EMWRoomPhase Phase = State ? State->GetRoomPhase() : EMWRoomPhase::Traveling;
    const bool InMatch = Phase == EMWRoomPhase::PreparingConstruction || Phase == EMWRoomPhase::TrackPlacement ||
                         Phase == EMWRoomPhase::ElementPlacement || Phase == EMWRoomPhase::PreparingRace ||
                         Phase == EMWRoomPhase::Countdown || Phase == EMWRoomPhase::Racing;
    const FName MatchTheme = InMatch ? State->GetRoomSettings().MapThemeId.GetTagName() : NAME_None;
    // 복제된 경기 설정을 우선하고, 아직 받기 전에는 해당 목적지의 로비 테마를 먼저 표시한다.
    const FName ThemeId = MatchTheme.IsNone() && Phase != EMWRoomPhase::Failed ? ArrivalTheme : MatchTheme;
    if (ThemeId.IsNone())
    {
        StopEnvironment();
        Status = State ? TEXT("WaitingForMatchSettings") : TEXT("WaitingForGameplayState");
        return;
    }
    APlayerController* Local = nullptr;
    for (auto It = World->GetPlayerControllerIterator(); It; ++It)
        if (It->Get() && It->Get()->IsLocalController())
        {
            Local = It->Get();
            break;
        }
    if (!Local && ArrivalTheme.IsNone())
    {
        Status = TEXT("WaitingForLocalController");
        return;
    }
    const auto* Binding = FindBinding(ThemeId);
    if (!Binding)
    {
        StopEnvironment();
        Status = TEXT("UnmappedTheme:") + ThemeId.ToString();
        return;
    }
    if (Generator && ActiveTheme == ThemeId)
    {
        Generator->ViewPlayerController = Local;
        if (!MatchTheme.IsNone() && Preload && !ArrivalTheme.IsNone())
        {
            Preload->ClearSelection();
        }
        Status = TEXT("Active:") + ThemeId.ToString();
        return;
    }
    StopEnvironment();
    UMWEnvironmentTheme* Theme = MatchTheme.IsNone() ? Binding->Environment.Get() : Binding->Environment.LoadSynchronous();
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
    TArray<FSoftObjectPath> EntryPaths;
    for (const auto& Material : Binding->FirstViewMaterials) EntryPaths.AddUnique(Material.ToSoftObjectPath());
    if (!EntryPaths.IsEmpty())
        FirstViewMaterialHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(EntryPaths);
    // 생성기가 테마를 참조한 뒤 로비에서 보관한 로드 핸들을 놓는다.
    if (Preload && !MatchTheme.IsNone())
    {
        Preload->ClearSelection();
    }
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
           *ThemeId.ToString(), *Theme->GetPathName(), LocalEnvironmentSeed, *World->GetName(), *GetNameSafe(Local));
}

// 이번 월드와 테마가 실제 카메라에서 보일 준비가 됐는지 로딩 화면에 전달한다.
void UMWEnvironmentGameplay::CollectFirstView(UWorld& World, TArray<UMaterialInterface*>& Materials, FString& Pending)
{
    if (GetWorld() != &World) return;
    const auto* Current = World.GetGameState<AMWGameplayGameState>();
    if (!Current) { Pending = TEXT("environment world"); return; }
    const FName Theme = Current->GetRoomSettings().MapThemeId.GetTagName();
    const auto* Binding = FindBinding(Theme);
    if (!Binding) return;
    if (ActiveTheme != Theme || !IsValid(Generator) || !Generator->Theme)
    {
        Pending = Theme.ToString();
        return;
    }
    const auto* Viewer = World.GetGameInstance()->GetFirstLocalPlayerController(&World);
    // 차량이 스폰되기 전 대기판부터 보여 주는 경로는 컨트롤러 시점을 첫 장면으로 준비한다.
    if (Viewer && Current->GetRoomPhase() == EMWRoomPhase::PreparingRace && !Viewer->GetPawn())
        Generator->AllowInitialControllerView();
    if (!Viewer || !Generator->IsViewPrepared(Viewer->GetViewTarget()))
    {
        Pending = TEXT("environment visible tiles");
        return;
    }
    if (Generator->Theme->Sky) Materials.AddUnique(Generator->Theme->Sky);
    for (UMaterialInterface* Floor : Generator->Theme->Floors) Materials.AddUnique(Floor);
    for (const auto& Material : Binding->FirstViewMaterials)
    {
        if (!Material.IsValid()) { Pending = Material.ToString(); return; }
        Materials.AddUnique(Material.Get());
    }
}

// 현재 생성기를 정리하고 잠시 숨겼던 레벨 배경·조명의 표시 상태를 복원한다.
void UMWEnvironmentGameplay::StopEnvironment()
{
    if (IsValid(Generator))
    {
        Generator->ClearEnvironment();
        Generator->Destroy();
    }
    FirstViewMaterialHandle.Reset();
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
    if (auto* Screen = UGameInstance::GetSubsystem<UMWArrivalScreen>(GetWorld()->GetGameInstance()))
        Screen->CollectEnvironment.RemoveAll(this);
    if (auto* Lobby = LobbyState.Get())
    {
        Lobby->OnLobbyChanged.RemoveDynamic(this, &ThisClass::RefreshLobbyTheme);
        // 실제 출발 중에는 목적지 월드가 사용할 선로딩 핸들을 게임 인스턴스에 남긴다.
        if (Lobby->GetRoomPhase() != EMWRoomPhase::Traveling)
        {
            if (auto* Preload = UGameInstance::GetSubsystem<UMWEnvironmentPreload>(GetWorld()->GetGameInstance()))
            {
                Preload->ClearSelection();
            }
        }
    }
    if (GetWorld())
        GetWorld()->GetTimerManager().ClearTimer(ReadinessTimer);
    if (State)
        State->OnGameplayStateChanged.RemoveDynamic(this, &ThisClass::RefreshState);
    State = nullptr;
    StopEnvironment();
    UE_LOG(LogTemp, Display, TEXT("[MWEnvironmentGameplay] Deinitialize timer/delegate/generator cleared"));
    Super::Deinitialize();
}
