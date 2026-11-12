#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MWTrackRuntimeData.generated.h"

class AActor;
class UStaticMesh;
class USoundBase;

UENUM(BlueprintType)
enum class EMWTrackShape : uint8
{
    Mesh,
    Box,
    Capsule,
    Sphere
};

UENUM(BlueprintType)
enum class EMWTrackTrigger : uint8
{
    None,
    Boost,
    Roller,
    LiftButton,
    LiftRide,
    Checkpoint,
    Finish,
    Start
};

UENUM(BlueprintType)
enum class EMWTrackMotion : uint8
{
    Lift,
    Roller,
    Barrier,
    TrafficGate
};

// 트랙 루트 기준 충돌·트리거 형상과 연결할 움직임 정보를 보관한다.
USTRUCT(BlueprintType)
struct FMWTrackShapeData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Track")
    FTransform Local;

    UPROPERTY(EditAnywhere, Category = "Track")
    FVector Extent = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, Category = "Track")
    TObjectPtr<UStaticMesh> Mesh;

    UPROPERTY(EditAnywhere, Category = "Track")
    EMWTrackShape Shape = EMWTrackShape::Mesh;

    UPROPERTY(EditAnywhere, Category = "Track")
    EMWTrackTrigger Trigger = EMWTrackTrigger::None;

    UPROPERTY(EditAnywhere, Category = "Track")
    int32 MotionIndex = INDEX_NONE;

    UPROPERTY(EditAnywhere, Category = "Track")
    float Amount = 0;

    UPROPERTY(EditAnywhere, Category = "Track")
    float Duration = 0;
};

// 복구 부품의 회전축·기준점·세기·속도와 적용 렌더러 태그를 보관한다.
USTRUCT(BlueprintType)
struct FMWTrackMotionData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Track")
    FTransform Pivot;

    UPROPERTY(EditAnywhere, Category = "Track")
    TArray<FName> RendererTags;

    UPROPERTY(EditAnywhere, Category = "Track")
    EMWTrackMotion Type = EMWTrackMotion::Lift;

    UPROPERTY(EditAnywhere, Category = "Track")
    FVector Axis = FVector::UpVector;

    UPROPERTY(EditAnywhere, Category = "Track")
    float Amount = 0;

    UPROPERTY(EditAnywhere, Category = "Track")
    float Rate = 0;

    UPROPERTY(EditAnywhere, Category = "Track")
    int32 TrafficLaneIndex = INDEX_NONE;

    UPROPERTY(EditAnywhere, Category = "Track")
    bool bAtLaneEnd = false;
};

// 원본 기차 부품의 메시와 트랙 기준 상대 변환을 보관한다.
USTRUCT(BlueprintType)
struct FMWTrackTrainPart
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Track")
    TObjectPtr<UStaticMesh> Mesh;

    UPROPERTY(EditAnywhere, Category = "Track")
    FTransform Local;
};

// 교통 차선의 기준 변환·길이·속도·생성 간격과 슬롯 수를 보관한다.
USTRUCT(BlueprintType)
struct FMWTrackTrafficLaneData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Track")
    FTransform Start;

    UPROPERTY(EditAnywhere, Category = "Track")
    float Length = 0;

    UPROPERTY(EditAnywhere, Category = "Track")
    float Radius = 0;

    UPROPERTY(EditAnywhere, Category = "Track")
    float Speed = 0;

    UPROPERTY(EditAnywhere, Category = "Track")
    float SpawnDelay = 0;

    UPROPERTY(EditAnywhere, Category = "Track")
    int32 Slots = 1;

    UPROPERTY(EditAnywhere, Category = "Track")
    int32 Direction = 1;
};

// 교통 외형 클래스와 원본 선택 가중치의 대응이다.
USTRUCT(BlueprintType)
struct FMWTrackTrafficModelData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Track")
    TSubclassOf<AActor> VisualClass;

    UPROPERTY(EditAnywhere, Category = "Track")
    int32 Weight = 0;
};

// 트랙 하나의 충돌·기믹·복귀 위치·레일·교통 의존성을 함께 보관한다.
USTRUCT(BlueprintType)
struct FMWTrackPieceRuntimeData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Track")
    FName VisualClass;

    UPROPERTY(EditAnywhere, Category = "Track")
    TArray<FMWTrackShapeData> Shapes;

    UPROPERTY(EditAnywhere, Category = "Track")
    TArray<FMWTrackMotionData> Motions;

    UPROPERTY(EditAnywhere, Category = "Track")
    TArray<FTransform> RespawnPoints;

    UPROPERTY(EditAnywhere, Category = "Track")
    TArray<FTransform> Rails;

    UPROPERTY(EditAnywhere, Category = "Track")
    TArray<FMWTrackTrainPart> TrainParts;

    UPROPERTY(EditAnywhere, Category = "Track")
    float TrainInterval = 0;

    UPROPERTY(EditAnywhere, Category = "Track")
    TObjectPtr<USoundBase> TrainBell;

    UPROPERTY(EditAnywhere, Category = "Track")
    TArray<FMWTrackTrafficLaneData> TrafficLanes;

    UPROPERTY(EditAnywhere, Category = "Track")
    TArray<FMWTrackTrafficModelData> TrafficModels;

    UPROPERTY(EditAnywhere, Category = "Track")
    float LiftDelay = 0;

    UPROPERTY(EditAnywhere, Category = "Track")
    float LiftReturnDelay = 0;
};

// 복구 트랙 외형 클래스와 실행 설정의 카탈로그다.
UCLASS(BlueprintType)
class MW_API UMWTrackRuntimeData : public UDataAsset
{
    GENERATED_BODY()

public:

    UPROPERTY(EditAnywhere, Category = "Track")
    TArray<FMWTrackPieceRuntimeData> Pieces;
};
