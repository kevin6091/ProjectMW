#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/SoftObjectPtr.h"
#include "MWTrackPresentationSettings.generated.h"

class UMaterialInterface;

// 선택판과 배치 고스트에서만 읽는 표시 설정이다. 서버 스냅·지면 검사는 같은 값을 계속 사용한다.
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="MW Track Presentation"))
class MW_API UMWTrackPresentationSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category="Selection", meta=(ClampMin="0"))
	double CandidateDegreesPerSecond = 0.0;

	// 투명도는 이 재질의 SurfaceOpacity에서 편집한다. 원본 트랙 재질에는 적용하지 않는다.
	UPROPERTY(Config, EditAnywhere, Category="Selection")
	TSoftObjectPtr<UMaterialInterface> CandidateSurfaceMaterial;

	UPROPERTY(Config, EditAnywhere, Category="Selection")
	FLinearColor CandidateTint = FLinearColor::White;

	UPROPERTY(Config, EditAnywhere, Category="Selection")
	FLinearColor HoveredCandidateTint = FLinearColor::White;

	UPROPERTY(Config, EditAnywhere, Category="Selection", meta=(ClampMin="0", Units="s"))
	double CandidateHoverSeconds = 0.0;

	UPROPERTY(Config, EditAnywhere, Category="Selection", meta=(ClampMin="1"))
	double CandidateHoverPeakScale = 1.0;

	UPROPERTY(Config, EditAnywhere, Category="Placement", meta=(ClampMin="0", Units="s"))
	double BlendSeconds = 0.0;

	UPROPERTY(Config, EditAnywhere, Category="Placement", meta=(ClampMin="0", Units="s"))
	double SnapSettleSeconds = 0.0;

	UPROPERTY(Config, EditAnywhere, Category="Placement", meta=(ClampMin="0", Units="cm"))
	double SnapOvershoot = 0.0;

	UPROPERTY(Config, EditAnywhere, Category="Placement", meta=(ClampMin="1"))
	double SnapPeakScale = 1.0;

	UPROPERTY(Config, EditAnywhere, Category="Placement", meta=(ClampMin="0", Units="deg"))
	float HeldSwayMaxDegrees = 0.f;

	UPROPERTY(Config, EditAnywhere, Category="Placement", meta=(ClampMin="0"))
	double HeldSwayDegreesPerWidthPerSecond = 0.0;
};
