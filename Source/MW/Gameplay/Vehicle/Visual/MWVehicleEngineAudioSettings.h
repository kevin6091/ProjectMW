#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MWVehicleEngineAudioSettings.generated.h"

class USoundWave;

// 차량 전체의 기본 배기음 크기와 감쇠 거리를 경기 설정에서 조정한다.
UCLASS(Config = Game, DefaultConfig)
class UMWVehicleEngineAudioSettings : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(Config)
    TSoftObjectPtr<USoundWave> Loop;

    UPROPERTY(Config)
    FVector2D PitchRange = FVector2D(0.82, 1.70);

    UPROPERTY(Config)
    FVector2D VolumeRange = FVector2D(0.10, 0.38);

    UPROPERTY(Config)
    float ResponseSpeed = 6.0f;

    UPROPERTY(Config)
    float InnerRadius = 950.0f;

    UPROPERTY(Config)
    float FalloffDistance = 2500.0f;
};
