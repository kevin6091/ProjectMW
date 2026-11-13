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

// 원본 Prefab 자식 하나의 메시·재질·상대 변환과 출처를 함께 보관한다.
USTRUCT(BlueprintType)
struct FMWEnvPart
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<UStaticMesh> Mesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<TObjectPtr<UMaterialInterface>> Materials;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FTransform RelativeTransform;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString SourceRef;
};

// 원본 장식의 자식 부품과 배치·크기·기울기·겹침 제한을 묶는다.
USTRUCT(BlueprintType)
struct FMWEnvPrefab
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString SourceRef;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName PrefabName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FMWEnvPart> Parts;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FVector RootScale = FVector::OneVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FRotator RootRotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FVector2D ScaleRange = FVector2D(1);

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FVector2D HeightRangeUnity = FVector2D::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float RotationRange = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float LeanAngle = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool HasPlacementController = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool FixedRotation = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool AvoidStartArea = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool DontOverlap = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FTransform OverlapBox;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FVector OverlapExtent = FVector::ZeroVector;
};

// 같은 종류의 배경 장식 후보와 밀도·활성 설정을 보관한다.
USTRUCT(BlueprintType)
struct FMWEnvFeature
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName FeatureName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float Density = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool EnabledOnPC = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FMWEnvPrefab> Prefabs;
};

// 테마별 원본 자원·하늘·바닥·조명·후처리 설정을 저장하는 에셋이다.
UCLASS(BlueprintType)
class MWENVIRONMENTRESTORE_API UMWEnvironmentTheme : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString SourceRef;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine = true))
    FString OriginalSettings;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FMWEnvFeature> Features;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<UMaterialInterface> Sky;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<TObjectPtr<UMaterialInterface>> Floors;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool RandomFloor = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FLinearColor SunColor = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FLinearColor AmbientColor = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FLinearColor FogColor = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FVector2D SunAngle = FVector2D(20, 50);

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FVector2D BuildingFogStart = FVector2D(450, 650);

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FVector2D BuildingFogEnd = FVector2D(1000, 1750);

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float ShadowStrength = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float VignetteIntensity = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float BloomIntensity = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float BloomThreshold = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FVector4 ShadowGain = FVector4(1, 1, 1, 1);

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FVector4 MidtoneGain = FVector4(1, 1, 1, 1);

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FVector4 HighlightGain = FVector4(1, 1, 1, 1);
};

// 테마 배치를 타일로 계산하고 로컬 카메라 시야에 필요한 렌더러를 제한적으로 재사용한다.
UCLASS()
class MWENVIRONMENTRESTORE_API AMWEnvironmentGenerator : public AActor
{
    GENERATED_BODY()
public:
    AMWEnvironmentGenerator();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Environment")
    TObjectPtr<UMWEnvironmentTheme> Theme;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Environment")
    int32 Seed = 42;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Environment")
    TObjectPtr<ACameraActor> ViewCamera;
    // 선택적으로 지정하는 로컬 소유 컨트롤러다. 원격 플레이어나 서버 카메라를 따라가지 않는다.
    UPROPERTY(Transient, BlueprintReadOnly, Category = "Environment")
    TWeakObjectPtr<APlayerController> ViewPlayerController;
    // 원본 Epic level2 경기 카메라의 시야·원거리 클립은 모두 Unity 단위 2000이다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Environment", meta = (ClampMin = "100"))
    float ViewDistanceUnity = 2000;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Environment", meta = (ClampMin = "100"))
    float FarDistanceUnity = 2000;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Environment")
    bool AutoUpdate = true;

    // 0은 비교용 기존 경로, 1은 단계별 제거, 2는 제한된 렌더러 캐시 재사용까지 적용한다.
    UPROPERTY(EditAnywhere, Category = "Environment", meta = (ClampMin = "0", ClampMax = "2"))
    int32 RendererReuseMode = 2;

    // 테마 데이터와 생성 순서를 준비하고 하늘·바닥·조명을 구성한다.
    UFUNCTION(BlueprintCallable, Category = "Environment")
    void InitializeEnvironment();

    // 실제 로컬 카메라의 시야와 작업 예산을 기준으로 배경 표시 범위를 갱신한다.
    UFUNCTION(BlueprintCallable, Category = "Environment")
    void UpdateVisibility();

    // 생성한 렌더러·타일·캐시를 해제해 다른 테마의 상태가 남지 않게 한다.
    UFUNCTION(BlueprintCallable, Category = "Environment")
    void ClearEnvironment();

    // 현재 생성·표시·재사용 상태를 진단 문자열로 반환한다. 배치 상태를 변경하지 않는다.
    UFUNCTION(BlueprintPure, Category = "Environment")
    FString GetEnvironmentDebug() const;

protected:
    // 지정된 테마 입력이 준비된 뒤 배경을 초기화하고 설정된 가시성 갱신을 시작한다.
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

    UPROPERTY()
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> Sky;

    UPROPERTY()
    TObjectPtr<UDirectionalLightComponent> Sun;

    UPROPERTY()
    TObjectPtr<USkyLightComponent> Ambient;

    UPROPERTY()
    TObjectPtr<UExponentialHeightFogComponent> Fog;

    UPROPERTY()
    TObjectPtr<UPostProcessComponent> PostProcess;

    UPROPERTY()
    TObjectPtr<UStaticMesh> Plane;

    FTimerHandle VisibilityTimer;

    TMap<FIntPoint, FTile> Tiles;

    TMap<FIntPoint, TArray<FBox>> Footprints;

    TArray<FIntPoint> RegistrationOrder;

    TMap<int32, TWeakObjectPtr<UInstancedStaticMeshComponent>> FloorRenderers;

    int32 PreparedFloorCount = 0;

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

    bool InitialViewPending = true;

    float LastCameraFov = 90;

    float LastCameraAspect = 16.0f / 9.0f;

    // 설정된 시드로 원본 호환 난수 상태를 초기화한다. 서버 시드를 새로 배포하지 않는다.
    void ResetRandom();

    // 복구한 감산 난수 순서를 유지한다. 호출 횟수가 바뀌면 이후 배치도 달라진다.
    double NextRandom();

    // 같은 난수 스트림에서 지정한 범위의 값을 얻어 배치 순서를 유지한다.
    double Range(double Low, double High);

    // 타일을 처음 등록할 때만 바닥 인덱스와 등록 순서를 확정한다.
    void RegisterTile(FIntPoint Key);

    // 타일 키를 현재 배경 격자의 월드 중심으로 바꾼다.
    FVector TileCenter(FIntPoint Key) const;

    // 원본 배치 설정과 난수 소비 순서로 타일 배치를 계산한다. 표시 생성과 분리한다.
    void GenerateTile(FIntPoint Key, FTile& Tile);

    // 계산된 타일 배치를 실제 렌더러로 만들거나 제한된 캐시에서 재사용한다.
    void BuildTileRenderers(FIntPoint Key, FTile& Tile);

    // 재사용 정책에 맞춰 타일 표시를 숨기거나 해제해 렌더러 수를 제한한다.
    void ReleaseTileRenderers(FTile& Tile);

    bool UsesPrewarm() const
    {
        return AutoUpdate && RendererReuseMode == 2;
    }

    // 타일별 반복 생성 대신 재질별 바닥 인스턴스 묶음을 준비한다.
    void PrepareFloorRenderers();

    // 같은 메시의 배경 장식을 인스턴스로 묶고 로컬 표시용 컴포넌트로 등록한다.
    UInstancedStaticMeshComponent* CreateRenderer(UStaticMesh* Mesh);
};
