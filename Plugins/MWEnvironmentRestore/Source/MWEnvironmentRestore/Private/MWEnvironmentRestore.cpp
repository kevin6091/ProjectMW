#include "MWEnvironmentRestore.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "ConvexVolume.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "SceneView.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyLightComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Crc.h"
#include "Serialization/JsonSerializer.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
constexpr double CmPerUnityUnit = 100;
constexpr double TileUnity = 500;
constexpr double TileCm = TileUnity * CmPerUnityUnit;
constexpr double TileBoundsRadiusCm = TileCm * 0.72;
constexpr float MaxSunShadowDistanceCm = 120000.0f;
constexpr int32 MaxTiles = 1024;
constexpr int32 BuildBudget = 3;
constexpr int32 InitialBuildBudget = 40;
constexpr int32 MaxPrewarmTiles = 16;
constexpr double PrewarmMarginCm = TileCm * 0.4;
constexpr double TurnPrewarmDistanceCm = 110000.0;
constexpr int32 MaxPlacementAttempts = 100;
constexpr int32 RandomMax = MAX_int32;
const FIntPoint Neighbors[] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
}

AMWEnvironmentGenerator::AMWEnvironmentGenerator()
{
    PrimaryActorTick.bCanEverTick = false;
    SetReplicates(false);
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("EnvironmentRoot"));
    SetRootComponent(SceneRoot);
    Sky = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SourceSky"));
    Sky->SetupAttachment(SceneRoot);
    Sky->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Sky->SetCastShadow(false);
    Sky->SetVisibility(false);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere"));
    Sky->SetStaticMesh(Sphere.Object);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> FloorMesh(TEXT("/Engine/BasicShapes/Plane"));
    Plane = FloorMesh.Object;
    Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("ThemeSun"));
    Sun->SetupAttachment(SceneRoot);
    Sun->SetIntensity(3.5f);
    Ambient = CreateDefaultSubobject<USkyLightComponent>(TEXT("ThemeAmbient"));
    Ambient->SetupAttachment(SceneRoot);
    Ambient->Mobility = EComponentMobility::Movable;
    Ambient->Intensity = 0.75f;
    Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("ThemeFog"));
    Fog->SetupAttachment(SceneRoot);
    PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("ThemePostProcess"));
    PostProcess->SetupAttachment(SceneRoot);
    PostProcess->bUnbound = true;
    PostProcess->bEnabled = false;
}

void AMWEnvironmentGenerator::BeginPlay()
{
    Super::BeginPlay();
    InitializeEnvironment();
}

void AMWEnvironmentGenerator::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearEnvironment();
    Super::EndPlay(EndPlayReason);
}

void AMWEnvironmentGenerator::ResetRandom()
{
    // Legacy System.Random subtractive generator. UE visibility/order and all other
    // unobserved source RNG consumers still affect parity with Unity's map stream.
    const int32 AbsoluteSeed = Seed == MIN_int32 ? MAX_int32 : FMath::Abs(Seed);
    int32 Previous = 161803398 - AbsoluteSeed;
    Samples[55] = Previous;
    int32 Current = 1;
    for (int32 I = 1; I < 55; ++I)
    {
        const int32 Index = 21 * I % 55;
        Samples[Index] = Current;
        Current = static_cast<int32>(static_cast<uint32>(Previous) - static_cast<uint32>(Current));
        if (Current < 0)
        {
            Current += RandomMax;
        }
        Previous = Samples[Index];
    }
    for (int32 Pass = 0; Pass < 4; ++Pass)
    {
        for (int32 I = 1; I < 56; ++I)
        {
            Samples[I] = static_cast<int32>(static_cast<uint32>(Samples[I]) -
                static_cast<uint32>(Samples[1 + (I + 30) % 55]));
            if (Samples[I] < 0)
            {
                Samples[I] += RandomMax;
            }
        }
    }
    RandomIndex = 0;
    RandomIndex2 = 21;
    RandomDraws = 0;
}

double AMWEnvironmentGenerator::NextRandom()
{
    ++RandomDraws;
    RandomIndex = RandomIndex % 55 + 1;
    RandomIndex2 = RandomIndex2 % 55 + 1;
    int32 Value = Samples[RandomIndex] - Samples[RandomIndex2];
    if (Value == RandomMax)
    {
        --Value;
    }
    if (Value < 0)
    {
        Value += RandomMax;
    }
    Samples[RandomIndex] = Value;
    return static_cast<double>(Value) / RandomMax;
}

double AMWEnvironmentGenerator::Range(double Low, double High)
{
    return FMath::Lerp(Low, High, NextRandom());
}

void AMWEnvironmentGenerator::InitializeEnvironment()
{
    ClearEnvironment();
    if (!Theme || GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    // Epic GenerateEnvironmentRoutine 0x5FE00A resets the pending floor to zero.
    NextFloorMaterialIndex = 0;
    ResetRandom();
    // InitialiseTileGeneration 0x5F2DF0..0x5F2E2E: X is the inner loop.
    // This changes floor assignment, not the subsequent feature visibility/order.
    for (int32 Y = -5; Y <= 5; ++Y)
    {
        for (int32 X = -5; X <= 5; ++X)
        {
            RegisterTile(FIntPoint(X, Y));
        }
    }

    Sky->SetMaterial(0, Theme->Sky);
    Sky->SetVisibility(Theme->Sky != nullptr);
    Sky->SetWorldScale3D(FVector(FarDistanceUnity * CmPerUnityUnit / 20));
    Sun->SetVisibility(true);
    Ambient->SetVisibility(true);
    Fog->SetVisibility(true);
    Sun->SetLightColor(Theme->SunColor);
    Sun->SetRelativeRotation(FRotator(-(Theme->SunAngle.X + Theme->SunAngle.Y) * 0.5, -35, 0));
    // VSM uses the movable sun range to cull distant non-Nanite casters.
    Sun->SetDynamicShadowDistanceMovableLight(FMath::Min(ViewDistanceUnity * float(CmPerUnityUnit), MaxSunShadowDistanceCm));
    Sun->SetShadowAmount(Theme->ShadowStrength);
    Ambient->SetLightColor(Theme->AmbientColor);
    Ambient->SetLowerHemisphereColor(Theme->AmbientColor * 0.22f);
    Ambient->SetRealTimeCapture(true);
    Fog->SetFogInscatteringColor(Theme->FogColor);
    Fog->SetFogDensity(0.012f);
    Fog->SetStartDistance((Theme->BuildingFogStart.X + Theme->BuildingFogStart.Y) * 0.5 * CmPerUnityUnit);
    // UE EndDistance stops integration; it is not Unity's linear fog end.
    // Retain the existing source mean/density approximation without tuning color.
    Fog->SetEndDistance((Theme->BuildingFogEnd.X + Theme->BuildingFogEnd.Y) * 0.5 * CmPerUnityUnit);
    // Cutoff excludes the authored sky sphere (radius 2.5 * Far), never FogEnd.
    // At the source 2000-unit view/far, 4000 exceeds all active floor corners
    // (< 2000 + 2 * 360), while the sky sphere is at 5000 units.
    Fog->SetFogCutoffDistance(FMath::Max(ViewDistanceUnity, FarDistanceUnity) * CmPerUnityUnit * 2);
    PostProcess->bEnabled = true;
    FPostProcessSettings& Settings = PostProcess->Settings;
    Settings.bOverride_VignetteIntensity = true;
    Settings.VignetteIntensity = Theme->VignetteIntensity;
    Settings.bOverride_BloomIntensity = true;
    Settings.BloomIntensity = Theme->BloomIntensity;
    Settings.bOverride_BloomThreshold = true;
    Settings.BloomThreshold = Theme->BloomThreshold;
    Settings.bOverride_ColorGainShadows = true;
    Settings.ColorGainShadows = Theme->ShadowGain;
    Settings.bOverride_ColorGainMidtones = true;
    Settings.ColorGainMidtones = Theme->MidtoneGain;
    Settings.bOverride_ColorGainHighlights = true;
    Settings.ColorGainHighlights = Theme->HighlightGain;
    if (AutoUpdate)
    {
        const bool PrewarmEnabled = UsesPrewarm();
        if (PrewarmEnabled)
        {
            PrepareFloorRenderers();
            UpdateVisibility();
        }
        const float FirstDelay = PrewarmEnabled && InitialViewPending ? 0.0f : 0.12f;
        GetWorldTimerManager().SetTimer(VisibilityTimer, this, &AMWEnvironmentGenerator::UpdateVisibility, 0.12f, true, FirstDelay);
    }
}

void AMWEnvironmentGenerator::ClearEnvironment()
{
    if (GetWorld())
    {
        GetWorldTimerManager().ClearTimer(VisibilityTimer);
    }
    for (auto& Pair : Tiles)
    {
        ReleaseTileRenderers(Pair.Value);
    }
    for (const auto& Pair : FloorRenderers)
    {
        if (UInstancedStaticMeshComponent* Floor = Pair.Value.Get())
        {
            Floor->DestroyComponent();
        }
    }
    FloorRenderers.Reset();
    PreparedFloorCount = 0;
    Tiles.Reset();
    Footprints.Reset();
    ActiveTiles.Reset();
    InactiveRenderers.Reset();
    Timings = {};
    RegistrationOrder.Reset();
    Reactivations = 0;
    InitialViewPending = true;
    PostProcess->bEnabled = false;
    Sky->SetVisibility(false);
    Sun->SetVisibility(false);
    Ambient->SetVisibility(false);
    Fog->SetVisibility(false);
}

void AMWEnvironmentGenerator::RegisterTile(FIntPoint Key)
{
    if (Tiles.Num() >= MaxTiles || Tiles.Contains(Key))
    {
        return;
    }
    FTile& Tile = Tiles.Add(Key);
    if (!Theme->Floors.IsEmpty())
    {
        // Tile.ctor 0x5FC367..0x5FC4B9 applies the PREVIOUS index, then advances.
        // Random floors still consume exactly one draw, including one-material themes.
        Tile.FloorIndex = NextFloorMaterialIndex;
        NextFloorMaterialIndex = Theme->RandomFloor
            ? FMath::FloorToInt(NextRandom() * Theme->Floors.Num())
            : (NextFloorMaterialIndex + 1) % Theme->Floors.Num();
    }
    RegistrationOrder.Add(Key);
}

FVector AMWEnvironmentGenerator::TileCenter(FIntPoint Key) const
{
    return GetActorLocation() + FVector(-Key.X * TileCm, Key.Y * TileCm, 0);
}

UInstancedStaticMeshComponent* AMWEnvironmentGenerator::CreateRenderer(UStaticMesh* Mesh)
{
    const double Started = FPlatformTime::Seconds();
    auto* Renderer = NewObject<UInstancedStaticMeshComponent>(this);
    Renderer->SetupAttachment(SceneRoot);
    Renderer->SetMobility(EComponentMobility::Movable);
    Renderer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Renderer->SetGenerateOverlapEvents(false);
    Renderer->SetStaticMesh(Mesh);
    Timings.Prepare += FPlatformTime::Seconds() - Started;
    if (RendererReuseMode == 0)
    {
        const double RegisterStart = FPlatformTime::Seconds();
        Renderer->RegisterComponent();
        Timings.Register += FPlatformTime::Seconds() - RegisterStart;
    }
    return Renderer;
}

void AMWEnvironmentGenerator::PrepareFloorRenderers()
{
    if (!Plane || PreparedFloorCount == RegistrationOrder.Num())
    {
        return;
    }

    // The registered floor is inexpensive and ready from every camera angle.
    // Append each tile once to its material batch and let UE cull the instances.
    TMap<int32, TArray<FTransform>> Batches;
    for (; PreparedFloorCount < RegistrationOrder.Num(); ++PreparedFloorCount)
    {
        const FIntPoint Key = RegistrationOrder[PreparedFloorCount];
        const int32 FloorIndex = Tiles.FindChecked(Key).FloorIndex;
        if (Theme->Floors.IsValidIndex(FloorIndex))
        {
            Batches.FindOrAdd(FloorIndex).Emplace(FQuat::Identity, TileCenter(Key), FVector(TileCm / 100, TileCm / 100, 1));
        }
    }

    for (const auto& Pair : Batches)
    {
        auto& Slot = FloorRenderers.FindOrAdd(Pair.Key);
        UInstancedStaticMeshComponent* Floor = Slot.Get();
        if (!Floor)
        {
            Floor = CreateRenderer(Plane);
            Floor->SetMaterial(0, Theme->Floors[Pair.Key]);
            Floor->SetCastShadow(false);
            Floor->SetCullDistances(0, FMath::CeilToInt(ViewDistanceUnity * CmPerUnityUnit + TileBoundsRadiusCm + PrewarmMarginCm));
            Floor->RegisterComponent();
            Slot = Floor;
        }
        Floor->AddInstances(Pair.Value, false, true, false);
    }
}

void AMWEnvironmentGenerator::GenerateTile(FIntPoint Key, FTile& Tile)
{
    const FVector Center = TileCenter(Key);
    for (int32 FeatureIndex = 0; FeatureIndex < Theme->Features.Num(); ++FeatureIndex)
    {
        const FMWEnvFeature& Feature = Theme->Features[FeatureIndex];
        if (!Feature.EnabledOnPC || Feature.Prefabs.IsEmpty())
        {
            continue;
        }
        const double Expected = TileUnity * TileUnity / 1000000 * Feature.Density;
        const int32 Whole = FMath::FloorToInt(Expected);
        const int32 Count = Whole + (NextRandom() < Expected - Whole);
        for (int32 Candidate = 0; Candidate < Count; ++Candidate)
        {
            const int32 PrefabIndex = FMath::FloorToInt(NextRandom() * Feature.Prefabs.Num());
            const FMWEnvPrefab& Prefab = Feature.Prefabs[PrefabIndex];
            FVector Scale = Prefab.RootScale;
            FQuat Rotation = Prefab.RootRotation.Quaternion();
            double Height = 0;
            if (Prefab.HasPlacementController)
            {
                // Epic EnvironmentPropController.Initialise (0x3E2070)
                // overwrites the authored root TRS, preserving child transforms.
                Scale = FVector(Range(Prefab.ScaleRange.X, Prefab.ScaleRange.Y));
                double Yaw;
                if (Prefab.FixedRotation)
                {
                    const int32 Steps = FMath::Max(1, FMath::RoundToInt(360.0 /
                        FMath::Clamp(Prefab.RotationRange, 1.0f, 360.0f)));
                    Yaw = FMath::FloorToDouble(NextRandom() * Steps) * 360.0 / Steps;
                }
                else
                {
                    Yaw = Range(0, Prefab.RotationRange);
                }
                Height = Range(Prefab.HeightRangeUnity.X, Prefab.HeightRangeUnity.Y) * CmPerUnityUnit;
                const double LeanX = FMath::DegreesToRadians(Range(-Prefab.LeanAngle, Prefab.LeanAngle));
                const double LeanZ = FMath::DegreesToRadians(Range(-Prefab.LeanAngle, Prefab.LeanAngle));
                // Unity Z-X-Y Euler order. Basis: (x, y, z) -> (-x, z, y).
                Rotation = FQuat(FVector::UpVector, FMath::DegreesToRadians(Yaw))
                    * FQuat(-FVector::ForwardVector, LeanX) * FQuat(FVector::RightVector, LeanZ);
            }
            for (int32 Attempt = 0; Attempt < MaxPlacementAttempts; ++Attempt)
            {
                const FVector Location = Center + FVector(Range(-TileCm / 2, TileCm / 2),
                    Range(-TileCm / 2, TileCm / 2), Height);
                if (Prefab.AvoidStartArea && FVector::DistSquared2D(Location, GetActorLocation()) < FMath::Square(10000.0))
                {
                    continue;
                }
                const FTransform Transform(Rotation, Location, Scale);
                if (Prefab.DontOverlap && !Prefab.OverlapExtent.IsNearlyZero())
                {
                    const FBox Box = FBox(-Prefab.OverlapExtent, Prefab.OverlapExtent)
                        .TransformBy(Prefab.OverlapBox * Transform);
                    bool Blocked = false;
                    for (int32 X = Key.X - 1; X <= Key.X + 1 && !Blocked; ++X)
                    {
                        for (int32 Y = Key.Y - 1; Y <= Key.Y + 1 && !Blocked; ++Y)
                        {
                            if (const TArray<FBox>* Other = Footprints.Find(FIntPoint(X, Y)))
                            {
                                Blocked = Other->ContainsByPredicate([&Box](const FBox& B) { return B.Intersect(Box); });
                            }
                        }
                    }
                    if (Blocked)
                    {
                        continue;
                    }
                    Footprints.FindOrAdd(Key).Add(Box);
                }
                Tile.Placements.Add({FeatureIndex, PrefabIndex, Transform});
                break;
            }
        }
    }

    Tile.Generated = true;
}

void AMWEnvironmentGenerator::BuildTileRenderers(FIntPoint Key, FTile& Tile)
{
    ++Timings.Built;
    auto Populate = [this](UInstancedStaticMeshComponent* Renderer, const TArray<FTransform>& Instances)
    {
        if (RendererReuseMode != 0)
        {
            const double RegisterStart = FPlatformTime::Seconds();
            Renderer->RegisterComponent();
            Timings.Register += FPlatformTime::Seconds() - RegisterStart;
        }
        const double AddStart = FPlatformTime::Seconds();
        if (RendererReuseMode == 0)
        {
            for (const FTransform& Transform : Instances)
            {
                Renderer->AddInstance(Transform, true);
            }
        }
        else
        {
            Renderer->AddInstances(Instances, false, true, false);
        }
        Timings.Add += FPlatformTime::Seconds() - AddStart;
    };
    const FVector Center = TileCenter(Key);
    if (!UsesPrewarm() && Plane && Theme->Floors.IsValidIndex(Tile.FloorIndex))
    {
        auto* Floor = CreateRenderer(Plane);
        Tile.Renderers.Add(Floor);
        Floor->SetMaterial(0, Theme->Floors[Tile.FloorIndex]);
        Populate(Floor, {FTransform(FQuat::Identity, Center, FVector(TileCm / 100, TileCm / 100, 1))});
    }

    struct FBatch
    {
        UInstancedStaticMeshComponent* Renderer = nullptr;
        TArray<FTransform> Instances;
    };
    TMap<const FMWEnvPart*, int32> BatchIndices;
    TArray<FBatch> Batches;
    for (const FPlacement& Item : Tile.Placements)
    {
        const FMWEnvPrefab& Prefab = Theme->Features[Item.Feature].Prefabs[Item.Prefab];
        for (const FMWEnvPart& Part : Prefab.Parts)
        {
            if (!Part.Mesh)
            {
                continue;
            }
            int32* Existing = BatchIndices.Find(&Part);
            const int32 Index = Existing ? *Existing : Batches.AddDefaulted();
            FBatch& Batch = Batches[Index];
            if (!Existing)
            {
                BatchIndices.Add(&Part, Index);
                Batch.Renderer = CreateRenderer(Part.Mesh);
                Tile.Renderers.Add(Batch.Renderer);
                for (int32 Slot = 0; Slot < Part.Materials.Num(); ++Slot)
                {
                    if (Part.Materials[Slot])
                    {
                        Batch.Renderer->SetMaterial(Slot, Part.Materials[Slot]);
                    }
                }
            }
            const FTransform Transform = Part.RelativeTransform * Item.Transform;
            if (RendererReuseMode == 0)
            {
                const double AddStart = FPlatformTime::Seconds();
                Batch.Renderer->AddInstance(Transform, true);
                Timings.Add += FPlatformTime::Seconds() - AddStart;
            }
            else
            {
                Batch.Instances.Add(Transform);
            }
        }
    }
    if (RendererReuseMode != 0)
    {
        for (const FBatch& Batch : Batches)
        {
            Populate(Batch.Renderer, Batch.Instances);
        }
    }
}

void AMWEnvironmentGenerator::ReleaseTileRenderers(FTile& Tile)
{
    const double Started = FPlatformTime::Seconds();
    for (const auto& Entry : Tile.Renderers)
    {
        if (UInstancedStaticMeshComponent* Renderer = Entry.Get())
        {
            Renderer->DestroyComponent();
            ++Timings.Destroyed;
        }
    }
    Tile.Renderers.Reset();
    Tile.Active = false;
    Timings.Destroy += FPlatformTime::Seconds() - Started;
}

void AMWEnvironmentGenerator::UpdateVisibility()
{
    Timings = {};
    if (Tiles.IsEmpty())
    {
        return;
    }
    const bool PrewarmEnabled = UsesPrewarm();
    FVector Eye;
    FRotator Angle;
    float HorizontalFov = 90;
    float Aspect = 16.0f / 9.0f;
    FConvexVolume PlayerFrustum;
    UsingPlayerProjection = false;
    if (ViewCamera)
    {
        const UCameraComponent* Camera = ViewCamera->GetCameraComponent();
        Eye = Camera->GetComponentLocation();
        Angle = Camera->GetComponentRotation();
        HorizontalFov = Camera->FieldOfView;
    }
    else if (APlayerController* PC = ViewPlayerController.IsValid()
        ? ViewPlayerController.Get() : GetWorld()->GetFirstPlayerController())
    {
        if (!PC->IsLocalController() || (PrewarmEnabled && InitialViewPending && PC->GetViewTarget() == PC)) return;
        PC->GetPlayerViewPoint(Eye, Angle);
        if (PC->PlayerCameraManager)
        {
            HorizontalFov = PC->PlayerCameraManager->GetFOVAngle();
            Aspect = PC->PlayerCameraManager->GetCameraCacheView().AspectRatio;
        }
        if (const ULocalPlayer* Player = PC->GetLocalPlayer())
        {
            FSceneViewProjectionData Projection;
            if (Player->ViewportClient && Player->ViewportClient->Viewport &&
                Player->GetProjectionData(Player->ViewportClient->Viewport, Projection))
            {
                GetViewFrustumBounds(PlayerFrustum, Projection.ComputeViewProjectionMatrix(), true);
                UsingPlayerProjection = true;
                const FIntRect Rect = Projection.GetConstrainedViewRect();
                if (Rect.Height() > 0) Aspect = float(Rect.Width()) / Rect.Height();
            }
        }
    }
    else
    {
        return;
    }

    LastCameraFov = HorizontalFov;
    LastCameraAspect = FMath::Max(Aspect, 0.01f);
    Sky->SetWorldLocation(Eye);
    const FVector RelativeEye = Eye - GetActorLocation();
    const FIntPoint CameraTile(FMath::RoundToInt(-RelativeEye.X / TileCm), FMath::RoundToInt(RelativeEye.Y / TileCm));
    const double TileRadius = TileBoundsRadiusCm;
    const double ViewDistanceCm = ViewDistanceUnity * CmPerUnityUnit;
    const double Far = FMath::Max(FarDistanceUnity, ViewDistanceUnity) * CmPerUnityUnit;
    const double PrewarmRange = ViewDistanceCm + PrewarmMarginCm;
    const double ScanDistance = PrewarmEnabled ? FMath::Max(Far, PrewarmRange) : Far;
    const int32 Radius = FMath::CeilToInt((ScanDistance + TileRadius) / TileCm);
    const FVector Forward = Angle.Vector();
    const FRotationMatrix View(Angle);
    const FVector Right = View.GetScaledAxis(EAxis::Y);
    const FVector Up = View.GetScaledAxis(EAxis::Z);
    const double TanX = FMath::Tan(FMath::DegreesToRadians(HorizontalFov * 0.5));
    const double TanY = TanX / LastCameraAspect;
    TArray<FIntPoint> Wanted;
    TArray<FIntPoint> Prewarm;
    TArray<FIntPoint> Expand;
    for (int32 X = CameraTile.X - Radius; X <= CameraTile.X + Radius; ++X)
    {
        for (int32 Y = CameraTile.Y - Radius; Y <= CameraTile.Y + Radius; ++Y)
        {
            const FIntPoint Key(X, Y);
            if (!Tiles.Contains(Key))
            {
                continue;
            }
            const FVector Center = TileCenter(Key);
            const FVector Delta = Center - Eye;
            const double Distance = FMath::Max(0.0, Delta.Length() - TileRadius);
            const auto IntersectsView = [&](double BoundsRadius)
            {
                if (UsingPlayerProjection)
                {
                    return PlayerFrustum.IntersectSphere(Center, BoundsRadius);
                }
                const double Depth = FVector::DotProduct(Delta, Forward);
                return Depth >= -BoundsRadius
                    && FMath::Abs(FVector::DotProduct(Delta, Right)) <= FMath::Max(Depth, 0) * TanX + BoundsRadius
                    && FMath::Abs(FVector::DotProduct(Delta, Up)) <= FMath::Max(Depth, 0) * TanY + BoundsRadius;
            };
            const bool InView = IntersectsView(TileRadius);
            const bool Visible = Distance <= Far && InView;
            if (Visible || Distance <= 100 * CmPerUnityUnit)
            {
                Expand.Add(Key);
            }
            if (Visible && Distance < ViewDistanceCm)
            {
                Wanted.Add(Key);
            }
            else if (PrewarmEnabled && !InitialViewPending && Distance < PrewarmRange
                && (Distance < TurnPrewarmDistanceCm || ActiveTiles.Contains(Key) || IntersectsView(TileRadius + PrewarmMarginCm)))
            {
                Prewarm.Add(Key);
            }
        }
    }

    for (FIntPoint Key : Expand)
    {
        for (FIntPoint Offset : Neighbors)
        {
            RegisterTile(Key + Offset);
        }
    }
    if (PrewarmEnabled)
    {
        PrepareFloorRenderers();
    }

    const auto Nearer = [&](FIntPoint A, FIntPoint B)
    {
        return FVector::DistSquared(TileCenter(A), Eye) < FVector::DistSquared(TileCenter(B), Eye);
    };
    Wanted.StableSort(Nearer);
    Prewarm.StableSort(Nearer);
    Prewarm.SetNum(FMath::Min(Prewarm.Num(), MaxPrewarmTiles), EAllowShrinking::No);
    const bool PrimeFirstView = PrewarmEnabled && InitialViewPending && !Wanted.IsEmpty();
    if (PrimeFirstView)
    {
        InitialViewPending = false;
    }
    // Build the current view first. Prewarmed renderers stay enabled so a turn
    // can reveal them immediately through the engine's own frustum culling.
    Wanted.Append(Prewarm);
    const TSet<FIntPoint> Desired(Wanted);
    for (auto It = ActiveTiles.CreateIterator(); It; ++It)
    {
        if (!Desired.Contains(*It))
        {
            FTile& Tile = Tiles.FindChecked(*It);
            if (RendererReuseMode == 0)
            {
                ReleaseTileRenderers(Tile);
            }
            else if (!Tile.Renderers.IsEmpty())
            {
                const double Started = FPlatformTime::Seconds();
                for (const auto& Entry : Tile.Renderers)
                {
                    if (auto* Renderer = Entry.Get())
                    {
                        Renderer->SetVisibility(false);
                    }
                }
                Timings.Hide += FPlatformTime::Seconds() - Started;
                InactiveRenderers.Add(*It);
            }
            Tile.Active = false;
            It.RemoveCurrent();
        }
    }

    // Keep layout and renderer budgets independent during cold re-entry.
    int32 LayoutBudget = PrimeFirstView ? InitialBuildBudget : BuildBudget;
    int32 RenderBudget = LayoutBudget;
    for (FIntPoint Key : Wanted)
    {
        FTile& Tile = Tiles.FindChecked(Key);
        if (!Tile.Generated && LayoutBudget > 0)
        {
            --LayoutBudget;
            const double Started = FPlatformTime::Seconds();
            GenerateTile(Key, Tile);
            Timings.Layout += FPlatformTime::Seconds() - Started;
        }
        if (Tile.Active || !Tile.Generated)
        {
            continue;
        }
        if (!Tile.Renderers.IsEmpty())
        {
            const double Started = FPlatformTime::Seconds();
            InactiveRenderers.RemoveSingle(Key);
            for (const auto& Entry : Tile.Renderers)
            {
                if (auto* Renderer = Entry.Get())
                {
                    Renderer->SetVisibility(true);
                }
            }
            ++Timings.WarmHits;
            ++Reactivations;
            Timings.Warm += FPlatformTime::Seconds() - Started;
            Tile.Active = true;
            ActiveTiles.Add(Key);
            continue;
        }
        if (RenderBudget == 0)
        {
            continue;
        }
        --RenderBudget;
        if (Tile.RenderedOnce)
        {
            ++Reactivations;
        }
        BuildTileRenderers(Key, Tile);
        Tile.RenderedOnce = true;
        Tile.Active = true;
        ActiveTiles.Add(Key);
    }

    // Keep eviction off the rebuild update unless the retirement queue is under pressure.
    const int32 WarmLimit = RendererReuseMode == 2 ? 16 : 0;
    const bool bRetirementPressure = InactiveRenderers.Num() > WarmLimit + 16;
    const int32 ReleaseBudget = RendererReuseMode == 2
        ? (Timings.Built == 0 || bRetirementPressure ? 1 : 0)
        : BuildBudget;
    const int32 Evictions = FMath::Clamp(InactiveRenderers.Num() - WarmLimit, 0, ReleaseBudget);
    for (int32 Index = 0; Index < Evictions; ++Index)
    {
        ReleaseTileRenderers(Tiles.FindChecked(InactiveRenderers[Index]));
    }
    if (Evictions > 0)
    {
        InactiveRenderers.RemoveAt(0, Evictions, EAllowShrinking::No);
    }
}

FString AMWEnvironmentGenerator::GetEnvironmentDebug() const
{
    auto Root = MakeShared<FJsonObject>();
    Root->SetNumberField(TEXT("seed"), Seed);
    Root->SetNumberField(TEXT("random_draws"), RandomDraws);
    Root->SetNumberField(TEXT("next_floor_index"), NextFloorMaterialIndex);
    uint32 RandomHash = FCrc::MemCrc32(Samples, sizeof(Samples));
    RandomHash = FCrc::MemCrc32(&RandomIndex, sizeof(RandomIndex), RandomHash);
    RandomHash = FCrc::MemCrc32(&RandomIndex2, sizeof(RandomIndex2), RandomHash);
    Root->SetStringField(TEXT("random_state_crc"), FString::Printf(TEXT("%08x"), RandomHash));
    TArray<TSharedPtr<FJsonValue>> Floors;
    for (FIntPoint Key : RegistrationOrder)
    {
        TArray<TSharedPtr<FJsonValue>> Values;
        Values.Add(MakeShared<FJsonValueNumber>(Key.X));
        Values.Add(MakeShared<FJsonValueNumber>(Key.Y));
        Values.Add(MakeShared<FJsonValueNumber>(Tiles.FindChecked(Key).FloorIndex));
        Floors.Add(MakeShared<FJsonValueArray>(Values));
    }
    Root->SetArrayField(TEXT("registered_floors"), Floors);
    Root->SetBoolField(TEXT("player_projection"), UsingPlayerProjection);
    Root->SetNumberField(TEXT("camera_fov"), LastCameraFov);
    Root->SetNumberField(TEXT("camera_aspect"), LastCameraAspect);
    Root->SetNumberField(TEXT("registered"), Tiles.Num());
    Root->SetNumberField(TEXT("floor_batches"), FloorRenderers.Num());
    Root->SetNumberField(TEXT("active"), ActiveTiles.Num());
    Root->SetNumberField(TEXT("reactivations"), Reactivations);
    Root->SetNumberField(TEXT("tile_cm"), TileCm);
    Root->SetNumberField(TEXT("inactive_renderers"), InactiveRenderers.Num());
    Root->SetNumberField(TEXT("hide_ms"), Timings.Hide * 1000);
    Root->SetNumberField(TEXT("destroy_ms"), Timings.Destroy * 1000);
    Root->SetNumberField(TEXT("prepare_ms"), Timings.Prepare * 1000);
    Root->SetNumberField(TEXT("register_ms"), Timings.Register * 1000);
    Root->SetNumberField(TEXT("add_ms"), Timings.Add * 1000);
    Root->SetNumberField(TEXT("layout_ms"), Timings.Layout * 1000);
    Root->SetNumberField(TEXT("warm_ms"), Timings.Warm * 1000);
    Root->SetNumberField(TEXT("destroyed_components"), Timings.Destroyed);
    Root->SetNumberField(TEXT("built_tiles"), Timings.Built);
    Root->SetNumberField(TEXT("warm_hits"), Timings.WarmHits);
    Root->SetBoolField(TEXT("timer_active"), GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(VisibilityTimer));
    TArray<TSharedPtr<FJsonValue>> Generated;
    for (FIntPoint Key : RegistrationOrder)
    {
        const FTile& Tile = Tiles.FindChecked(Key);
        if (!Tile.Generated)
        {
            continue;
        }
        auto Entry = MakeShared<FJsonObject>();
        Entry->SetNumberField(TEXT("x"), Key.X);
        Entry->SetNumberField(TEXT("y"), Key.Y);
        Entry->SetBoolField(TEXT("active"), Tile.Active);
        Entry->SetNumberField(TEXT("placements"), Tile.Placements.Num());
        Entry->SetNumberField(TEXT("batches"), Tile.Renderers.Num());
        Entry->SetNumberField(TEXT("floor"), Tile.FloorIndex);
        uint32 Hash = 0;
        for (const FPlacement& Placement : Tile.Placements)
        {
            const FString Value = FString::Printf(TEXT("%d/%d/%s"), Placement.Feature, Placement.Prefab,
                *Placement.Transform.ToString());
            Hash = FCrc::StrCrc32(*Value, Hash);
        }
        Entry->SetStringField(TEXT("placement_crc"), FString::Printf(TEXT("%08x"), Hash));
        Generated.Add(MakeShared<FJsonValueObject>(Entry));
    }
    Root->SetArrayField(TEXT("generated"), Generated);
    FString Result;
    FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Result));
    return Result;
}
