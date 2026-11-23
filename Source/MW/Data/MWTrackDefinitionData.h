#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Gameplay/Track/MWTrackRuntimeTypes.h"
#include "MWTrackDefinitionData.generated.h"

class USoundBase;

// 표시부터 설치 후 주행까지 트랙 한 종류가 공유하는 정의다.
USTRUCT(BlueprintType)
struct FMWTrackDefinition
{
	GENERATED_BODY()

	// 추첨 목록에서는 소프트 참조만 사용하고, 뽑힌 BP를 실제로 로드한다.
	UPROPERTY(EditAnywhere, Category = "Track")
	TSoftClassPtr<AActor> TrackClass;

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

	UPROPERTY(EditAnywhere, Category = "Track")
	FText DisplayName;
};

// 공용 에셋 목록에서 한 번 조회하는 전체 트랙 정의 카탈로그다.
UCLASS(BlueprintType)
class MW_API UMWTrackDefinitionData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Track", meta = (DisplayName = "Track Definitions", TitleProperty = "DisplayName"))
	TArray<FMWTrackDefinition> Pieces;
};
