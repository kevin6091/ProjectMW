#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MWVehiclePresentationComponent.generated.h"

// 모든 차량의 이동 계산이 끝난 뒤 외형을 갱신하고, 그 뒤에 카메라가 따라가게 한다.
UCLASS()
class MW_API UMWVehiclePresentationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMWVehiclePresentationComponent();
	void SetLocalPrerequisite(UMWVehiclePresentationComponent* LocalPresentation);
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	TWeakObjectPtr<UMWVehiclePresentationComponent> LocalPrerequisite;
};
