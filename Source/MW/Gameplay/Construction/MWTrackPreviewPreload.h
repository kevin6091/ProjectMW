#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "MWTrackPreviewPreload.generated.h"

struct FStreamableHandle;

// 로비에서 시작한 후보 리소스 요청을 경기 월드의 선택판이 만들어질 때까지 유지한다.
UCLASS()
class UMWTrackPreviewPreload : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void PlayerControllerChanged(APlayerController* NewController) override;
	virtual void Deinitialize() override;

	void ReleaseCandidates();

private:
	void PrepareMaterials(TArray<FSoftObjectPath> Paths);
	TSharedPtr<FStreamableHandle> LoadHandle;
};
