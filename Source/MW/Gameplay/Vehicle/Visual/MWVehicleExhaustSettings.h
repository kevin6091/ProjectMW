#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MWVehicleExhaustSettings.generated.h"

class UNiagaraSystem;

// 배기 위치는 차량 박스의 절반 크기를 기준으로 하고 효과의 크기·최소 이동 속도는 설정에서 조정한다.
UCLASS(Config = Game, DefaultConfig)
class UMWVehicleExhaustSettings : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(Config)
    TSoftObjectPtr<UNiagaraSystem> System;

    UPROPERTY(Config)
    FVector RearAnchor = FVector(-1.1, 0.0, 0.25);

    UPROPERTY(Config)
    float SideOffset = 0.26f;

    UPROPERTY(Config)
    float EffectScale = 0.32f;

    UPROPERTY(Config)
    float WakeSpeed = 140.0f;
};
