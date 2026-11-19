#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SpringArmComponent.h"
#include "MWVehicleSpringArmComponent.generated.h"

// 기존 SpringArm 설정을 사용하면서 Camera Lag의 프레임 시간 의존성을 줄인다.
UCLASS(ClassGroup=(Camera), meta=(BlueprintSpawnableComponent))
class MW_API UMWVehicleSpringArmComponent : public USpringArmComponent
{
	GENERATED_BODY()

public:
	void ResetCameraLag();

protected:
	virtual void UpdateDesiredArmLocation(bool bDoTrace, bool bDoLocationLag, bool bDoRotationLag, float DeltaTime) override;

private:
	FQuat PreviousTargetRotation = FQuat::Identity;
	bool bHasCameraHistory = false;
};
