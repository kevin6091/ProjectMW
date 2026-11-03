

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MWGameplayPlayerController.generated.h"

// 테마 월드에서 기존 GameOnly 입력 전환만 담당합니다. 차량/건설 입력은 추가하지 않습니다.
UCLASS()
class MW_API AMWGameplayPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void OnRep_PlayerState() override;
	virtual void PostSeamlessTravel() override;

private:
	void ApplyGameplayInputMode();
};
