#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MWParticipantPalette.generated.h"

// 실제 경기의 참가자 표시 색을 한 곳에서 읽는다. Widget이나 서버 상태를 소유하지 않는다.
UCLASS(Config = Game, DefaultConfig)
class MW_API UMWParticipantPalette : public UObject
{
    GENERATED_BODY()

public:
    // 공유 MemberId는 1부터 시작한다. 슬롯 수를 넘으면 기존처럼 팔레트를 순환한다.
    UFUNCTION(BlueprintPure, Category = "MW|UI|Participant")
    FLinearColor GetColorForMember(int32 MemberId) const;

    // 0부터 시작하는 표시 슬롯 조회다. 범위 밖의 슬롯은 대체 색을 사용한다.
    FLinearColor GetColorForSlot(int32 SlotIndex) const;

private:
    UPROPERTY(Config)
    TArray<FLinearColor> Colors;

    UPROPERTY(Config)
    FLinearColor FallbackColor = FLinearColor::White;
};
