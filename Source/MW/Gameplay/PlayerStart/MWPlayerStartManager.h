#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MWPlayerStartManager.generated.h"

class APlayerStart;
class UArrowComponent;

// 테마 레벨에 하나 배치해 경기의 시작 위치를 지정합니다.
UCLASS()
class MW_API AMWPlayerStartManager : public AActor
{
	GENERATED_BODY()

public:
	AMWPlayerStartManager();

	// 서버에서 실제 참가 인원만큼 만들고, 앞왼쪽부터 배정 순서대로 반환합니다.
	bool PreparePlayerStarts(int32 PlayerCount, TArray<APlayerStart*>& OutStarts);

	virtual void Destroyed() override;

private:
	UPROPERTY(VisibleAnywhere, Category="MW|Start Grid")
	TObjectPtr<UArrowComponent> FrontLeft;

	UPROPERTY(VisibleAnywhere, Category="MW|Start Grid")
	TObjectPtr<UArrowComponent> FrontRight;

	UPROPERTY(VisibleAnywhere, Category="MW|Start Grid")
	TObjectPtr<UArrowComponent> RearLeft;

	UPROPERTY(VisibleAnywhere, Category="MW|Start Grid")
	TObjectPtr<UArrowComponent> RearRight;

	UPROPERTY(Transient)
	TArray<TObjectPtr<APlayerStart>> SpawnedStarts;
};
