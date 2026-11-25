#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MWEnvironmentGameplay.generated.h"

class AMWEnvironmentGenerator;
class AMWGameplayGameState;
class AMWLobbyGameState;
class UMWEnvironmentTheme;
class USceneComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UMaterialBillboardComponent;
class UMaterialInstanceDynamic;
class UMWTrackConstructionComponent;

USTRUCT()
struct FMWEnvironmentBinding
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    FName MapThemeId;

    UPROPERTY(EditAnywhere)
    TSoftObjectPtr<UMWEnvironmentTheme> Environment;

    // 하늘·바닥 외에 해당 테마의 첫 화면에서 준비를 기다릴 재질을 지정한다.
    UPROPERTY(EditAnywhere)
    TArray<TSoftObjectPtr<UMaterialInterface>> FirstViewMaterials;
};

// 프로젝트 연결 계층이다. 기존 복제 상태를 읽어 배경만 표시하며 경기 규칙은 바꾸지 않는다.
UCLASS(Config = Game, DefaultConfig)
class MWENVIRONMENTGAMEPLAY_API UMWEnvironmentGameplay : public UWorldSubsystem
{
    GENERATED_BODY()
public:

    UPROPERTY(Config, EditAnywhere, Category = "Environment")
    bool Enabled = false;

    // 로비 선로딩만 끄고 같은 경기 화면의 스트리밍 비용을 비교할 수 있다.
    UPROPERTY(Config, EditAnywhere, Category = "Environment|Streaming")
    bool bPreloadLobbyTheme = true;

    UPROPERTY(Config, EditAnywhere, Category = "Environment")
    TArray<FMWEnvironmentBinding> Bindings;
    // 기존 방 설정에는 복제되는 경기 시드가 없으므로 로컬 시드 정책을 설정으로 명시한다.
    UPROPERTY(Config, EditAnywhere, Category = "Environment")
    int32 LocalEnvironmentSeed = 42;

    UPROPERTY(Config, EditAnywhere, Category = "Environment")
    bool ReplaceLevelLighting = false;
    // 지정한 개발 배경의 표시만 숨긴다. 충돌과 원본 에셋 데이터는 그대로 둔다.
    UPROPERTY(Config, EditAnywhere, Category = "Environment")
    TArray<FString> HiddenBackdropActorPaths;

    // 설치한 트랙을 받치는 로컬 렌더링 메시다. 미지정 시 지지대를 표시하지 않는다.
    UPROPERTY(Config, EditAnywhere, Category = "Environment|Track")
    TSoftObjectPtr<UStaticMesh> TrackSupportMesh;

    // 설치가 승인된 직후 노면 표시가 커졌다 돌아오는 시간과 최대 확대량이다.
    UPROPERTY(Config, EditAnywhere, Category = "Environment|Track", meta = (ClampMin = "0.0", Units = "s"))
    float TrackPlacementPopSeconds = 0.23f;

    UPROPERTY(Config, EditAnywhere, Category = "Environment|Track", meta = (ClampMin = "0.0", ClampMax = "0.15"))
    float TrackPlacementPopScale = 0.075f;

    // 설치 섬광과 연기를 한 번 재생하는 카메라 방향 머티리얼이다.
    UPROPERTY(Config, EditAnywhere, Category = "Environment|Track")
    TSoftObjectPtr<UMaterialInterface> TrackLaidParticleMaterial;

    // 곡선 안쪽의 빈 공간을 피하면서 설치 효과 사이에 확보할 최소 거리다.
    UPROPERTY(Config, EditAnywhere, Category = "Environment|Track", meta = (ClampMin = "1.0", Units = "cm"))
    float TrackLaidEffectSpacing = 1700.0f;

    // 지지대 폭에 비례하는 효과 판의 반폭이다.
    UPROPERTY(Config, EditAnywhere, Category = "Environment|Track", meta = (ClampMin = "0.01"))
    float TrackLaidParticleSizeFraction = 0.63f;

    // 설치 섬광이 먼저 보인 뒤 골조 첫 층이 드러날 때까지의 시간이다.
    UPROPERTY(Config, EditAnywhere, Category = "Environment|Track", meta = (ClampMin = "0.0", Units = "s"))
    float TrackSupportRevealSeconds = 0.40f;

    // 골조 첫 층부터 바닥에 닿는 마지막 층이 드러날 때까지의 시간이다.
    UPROPERTY(Config, EditAnywhere, Category = "Environment|Track", meta = (ClampMin = "0.0", Units = "s"))
    float TrackSupportAssemblySeconds = 0.22f;

    UFUNCTION(BlueprintPure, Category = "Environment")
    AMWEnvironmentGenerator* GetGenerator() const
    {
        return Generator;
    }

    UFUNCTION(BlueprintPure, Category = "Environment")
    FString GetBindingStatus() const
    {
        return Status;
    }

    // 로컬 경기 월드에서 기존 상태를 읽고 늦은 복제·컨트롤러 준비 점검을 등록한다.
    virtual void OnWorldBeginPlay(UWorld& World) override;

    // 준비 점검 타이머와 실제 구독 상태를 해제한 뒤 배경 수명을 닫는다.
    virtual void Deinitialize() override;

protected:
    virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;

private:

    UPROPERTY(Transient)
    TObjectPtr<AMWEnvironmentGenerator> Generator;

    TSharedPtr<struct FStreamableHandle> FirstViewMaterialHandle;

    UPROPERTY(Transient)
    TObjectPtr<UStaticMesh> ActiveTrackSupportMesh;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> TrackSupportComponents;

    struct FTrackSupportBuild
    {
        int32 PieceId;
        FVector Top;
        FQuat Rotation;
        double Height;
        double ScaleXY;
        double StartedAt;
        double RevealDelay;
        double AssemblyDuration;
        int32 Layers;
        int32 RevealedLayers = 0;
    };

    struct FTrackLaidSprite
    {
        TWeakObjectPtr<UMaterialBillboardComponent> Component;
        TWeakObjectPtr<UMaterialInstanceDynamic> Material;
        FVector Surface;
        float RiseHeight;
        double StartedAt;
    };

    struct FTrackPopSurface
    {
        TWeakObjectPtr<UStaticMeshComponent> Original;
        TWeakObjectPtr<UStaticMeshComponent> Visual;
        FTransform RestTransform;
    };

    struct FTrackPlacementPop
    {
        int32 PieceId = 0;
        FVector ContactPoint = FVector::ZeroVector;
        double StartedAt = 0.0;
        bool bLoggedPeak = false;
        TArray<FTrackPopSurface> Surfaces;
    };

    int32 PlacementPopSessionId = INDEX_NONE;
    bool bSawOpenPlacementForPop = false;
    TSet<int32> PresentedPopPieces;
    TArray<FTrackPlacementPop> TrackPlacementPops;
    FTimerHandle PlacementPopTimer;
    void RefreshTrackPlacementPops();
    void TickTrackPlacementPops();
    void RestoreTrackPopSurfaces(const FTrackPlacementPop& Pop) const;
    void ResetTrackPlacementPops();

    int32 SupportSessionId = INDEX_NONE;
    bool bObservedOpenPlacement = false;
    TSet<int32> ResolvedSupportPieces;
    TArray<FTrackSupportBuild> PendingSupportBuilds;
    TArray<FTrackLaidSprite> TrackLaidSprites;
    FTimerHandle SupportBuildTimer;
    void RefreshTrackSupports();
    void PlayTrackLaidEffects(const AActor& Piece, int32 PieceId, const FVector& CenterSurface,
        double Width, double StartedAt);
    void TickTrackSupportBuilds();
    void ResetTrackSupports();

    UPROPERTY(Transient)
    TObjectPtr<AMWGameplayGameState> State;

    TWeakObjectPtr<AMWLobbyGameState> LobbyState;
    TWeakObjectPtr<UMWTrackConstructionComponent> BoundConstruction;
    const FMWEnvironmentBinding* FindBinding(FName ThemeId) const;

    UFUNCTION()
    void RefreshLobbyTheme();
    void CollectFirstView(UWorld& World, TArray<UMaterialInterface*>& Materials, FString& Pending);

    FTimerHandle ReadinessTimer;

    FName ActiveTheme;

    FString Status = TEXT("Disabled");

    TMap<TWeakObjectPtr<USceneComponent>, bool> LevelLightingVisibility;

    TMap<TWeakObjectPtr<AActor>, bool> LevelBackdropVisibility;

    // 기존 복제 테마·단계와 로컬 컨트롤러를 확인해 완료본 배경 생성기를 연결한다.
    UFUNCTION()
    void RefreshState();

    // 현재 생성기를 정리하고 잠시 숨겼던 레벨 배경·조명의 표시 상태를 복원한다.
    void StopEnvironment();
};
