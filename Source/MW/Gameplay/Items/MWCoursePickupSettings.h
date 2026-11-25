#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MWCoursePickupSettings.generated.h"

class UStaticMesh;

USTRUCT()
struct FMWPickupVisualPart
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Presentation")
    TSoftObjectPtr<UStaticMesh> Mesh;

    UPROPERTY(EditAnywhere, Category = "Presentation")
    FVector Offset = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, Category = "Presentation")
    FRotator Rotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, Category = "Presentation", meta = (ClampMin = "0.01"))
    float Scale = 1.f;
};

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "MW Course Pickups"))
class UMWCoursePickupSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UPROPERTY(Config, EditAnywhere, Category = "Placement")
    bool bEnabled = false;

    // 출발 슬롯의 맨 앞과 시작 트랙의 출구 사이에서 첫 줄의 상대 위치를 정한다.
    UPROPERTY(Config, EditAnywhere, Category = "Placement", meta = (ClampMin = "0", ClampMax = "1"))
    double FirstRowFraction = 0.5;

    UPROPERTY(Config, EditAnywhere, Category = "Placement", meta = (ClampMin = "0", Units = "cm"))
    double FirstRowMinClearance = 0;

    // 안전한 도로 후보를 이 거리 간격으로 배치한다. 0은 반복 배치를 끈다.
    UPROPERTY(Config, EditAnywhere, Category = "Placement", meta = (ClampMin = "0", Units = "cm"))
    double RowSpacing = 0;

    UPROPERTY(Config, EditAnywhere, Category = "Placement", meta = (ClampMin = "1"))
    int32 MaxRowsPerPiece = 1;

    UPROPERTY(Config, EditAnywhere, Category = "Placement", meta = (DisplayName = "Lane Offsets (cm)"))
    TArray<double> LaneOffsets;

    UPROPERTY(Config, EditAnywhere, Category = "Placement", meta = (ClampMin = "0", Units = "cm"))
    double SeamClearance = 0;

    UPROPERTY(Config, EditAnywhere, Category = "Placement", meta = (ClampMin = "0", Units = "cm"))
    double SurfaceSearchHeight = 0;

    UPROPERTY(Config, EditAnywhere, Category = "Placement", meta = (ClampMin = "0", ClampMax = "1"))
    double MinSurfaceUpDot = 1;

    UPROPERTY(Config, EditAnywhere, Category = "Placement", meta = (ClampMin = "0", Units = "cm"))
    double BoxOriginHeight = 0;

    // 필요하면 복잡한 조각을 클래스 단위로 제외할 수 있다.
    UPROPERTY(Config, EditAnywhere, Category = "Placement")
    TArray<TSoftClassPtr<AActor>> ExcludedTracks;

    UPROPERTY(Config, EditAnywhere, Category = "Presentation")
    FMWPickupVisualPart Body;

    UPROPERTY(Config, EditAnywhere, Category = "Presentation")
    FMWPickupVisualPart Lid;

    UPROPERTY(Config, EditAnywhere, Category = "Presentation")
    FMWPickupVisualPart Handle;
};
