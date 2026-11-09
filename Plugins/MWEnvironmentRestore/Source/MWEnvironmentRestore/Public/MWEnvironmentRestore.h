#pragma once

#include "Engine/DataAsset.h"
#include "GameFramework/Actor.h"
#include "MWEnvironmentRestore.generated.h"

class ACameraActor;
class APlayerController;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UPostProcessComponent;
class USceneComponent;
class USkyLightComponent;
class UStaticMesh;
class UStaticMeshComponent;

USTRUCT(BlueprintType)
struct FMWEnvPart
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMesh> Mesh;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<TObjectPtr<UMaterialInterface>> Materials;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FTransform RelativeTransform;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FString SourceRef;
};

USTRUCT(BlueprintType)
struct FMWEnvPrefab
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FString SourceRef;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName PrefabName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FMWEnvPart> Parts;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector RootScale = FVector::OneVector;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FRotator RootRotation = FRotator::ZeroRotator;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector2D ScaleRange = FVector2D(1);
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector2D HeightRangeUnity = FVector2D::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float RotationRange = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float LeanAngle = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool HasPlacementController = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool FixedRotation = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool AvoidStartArea = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool DontOverlap = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FTransform OverlapBox;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector OverlapExtent = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct FMWEnvFeature
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName FeatureName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float Density = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool EnabledOnPC = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FMWEnvPrefab> Prefabs;
};

UCLASS(BlueprintType)
class MWENVIRONMENTRESTORE_API UMWEnvironmentTheme : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FString SourceRef;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(MultiLine=true)) FString OriginalSettings;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FMWEnvFeature> Features;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UMaterialInterface> Sky;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<TObjectPtr<UMaterialInterface>> Floors;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool RandomFloor = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor SunColor = FLinearColor::White;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor AmbientColor = FLinearColor::White;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor FogColor = FLinearColor::White;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector2D SunAngle = FVector2D(20, 50);
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector2D BuildingFogStart = FVector2D(450, 650);
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector2D BuildingFogEnd = FVector2D(1000, 1750);
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float ShadowStrength = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float VignetteIntensity = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float BloomIntensity = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float BloomThreshold = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector4 ShadowGain = FVector4(1, 1, 1, 1);
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector4 MidtoneGain = FVector4(1, 1, 1, 1);
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector4 HighlightGain = FVector4(1, 1, 1, 1);
};

UCLASS()
class MWENVIRONMENTRESTORE_API AMWEnvironmentGenerator : public AActor
{
    GENERATED_BODY()
public:
    AMWEnvironmentGenerator();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment") TObjectPtr<UMWEnvironmentTheme> Theme;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment") int32 Seed = 42;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment") TObjectPtr<ACameraActor> ViewCamera;
    // Optional owning local controller; never follows a remote/server camera.
    UPROPERTY(Transient, BlueprintReadOnly, Category="Environment") TWeakObjectPtr<APlayerController> ViewPlayerController;
    // Epic level2 Race Camera ViewDistance and far clip, both 2000 Unity units.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment", meta=(ClampMin="100")) float ViewDistanceUnity = 2000;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment", meta=(ClampMin="100")) float FarDistanceUnity = 2000;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment") bool AutoUpdate = true;

    // 0 keeps the reference path for profiling. 1 stages removal. 2 also caches a bounded warm set.
    UPROPERTY(EditAnywhere, Category="Environment", meta=(ClampMin="0", ClampMax="2")) int32 RendererReuseMode = 2;
    UFUNCTION(BlueprintCallable, Category="Environment") void InitializeEnvironment();
    UFUNCTION(BlueprintCallable, Category="Environment") void UpdateVisibility();
    UFUNCTION(BlueprintCallable, Category="Environment") void ClearEnvironment();
    UFUNCTION(BlueprintPure, Category="Environment") FString GetEnvironmentDebug() const;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    struct FPlacement
    {
        int32 Feature;
        int32 Prefab;
        FTransform Transform;
    };
    struct FTile
    {
        bool Generated = false;
        bool RenderedOnce = false;
        bool Active = false;
        int32 FloorIndex = 0;
        TArray<FPlacement> Placements;
        TArray<TWeakObjectPtr<UInstancedStaticMeshComponent>> Renderers;
    };

    UPROPERTY() TObjectPtr<USceneComponent> SceneRoot;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Sky;
    UPROPERTY() TObjectPtr<UDirectionalLightComponent> Sun;
    UPROPERTY() TObjectPtr<USkyLightComponent> Ambient;
    UPROPERTY() TObjectPtr<UExponentialHeightFogComponent> Fog;
    UPROPERTY() TObjectPtr<UPostProcessComponent> PostProcess;
    UPROPERTY() TObjectPtr<UStaticMesh> Plane;
    FTimerHandle VisibilityTimer;
    TMap<FIntPoint, FTile> Tiles;
    TMap<FIntPoint, TArray<FBox>> Footprints;
    TArray<FIntPoint> RegistrationOrder;
    TSet<FIntPoint> ActiveTiles;
    TArray<FIntPoint> InactiveRenderers;
    struct FRendererTimings
    {
        double Hide = 0, Destroy = 0, Prepare = 0, Register = 0, Add = 0, Layout = 0, Warm = 0;
        int32 Destroyed = 0, Built = 0, WarmHits = 0;
    } Timings;
    int32 Samples[56] = {};
    int32 RandomIndex = 0;
    int32 RandomIndex2 = 21;
    int32 NextFloorMaterialIndex = 0;
    int32 RandomDraws = 0;
    int32 Reactivations = 0;
    bool UsingPlayerProjection = false;
    float LastCameraFov = 90;
    float LastCameraAspect = 16.0f / 9.0f;

    void ResetRandom();
    double NextRandom();
    double Range(double Low, double High);
    void RegisterTile(FIntPoint Key);
    FVector TileCenter(FIntPoint Key) const;
    void GenerateTile(FIntPoint Key, FTile& Tile);
    void BuildTileRenderers(FIntPoint Key, FTile& Tile);
    void ReleaseTileRenderers(FTile& Tile);
    UInstancedStaticMeshComponent* AddRenderer(FTile& Tile, UStaticMesh* Mesh);
};
