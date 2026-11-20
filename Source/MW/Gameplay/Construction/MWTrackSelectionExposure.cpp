#include "Gameplay/Construction/MWTrackSelectionExposure.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Gameplay/Construction/MWTrackSelectionBoard.h"
#include "System/GameState/MWGameplayGameState.h"

DEFINE_LOG_CATEGORY_STATIC(LogMWTrackSelectionExposure, Log, All);

bool UMWTrackSelectionExposure::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	AActor* Target = CameraOwner->GetViewTarget();
	if (const auto* Board = Cast<AMWTrackSelectionBoard>(Target))
	{
		const auto* State = Board->GetWorld()->GetGameState<AMWGameplayGameState>();
		const FGameplayTag Theme = State ? State->GetRoomSettings().MapThemeId : FGameplayTag();
		if (Theme != ActiveTheme)
		{
			ActiveTheme = Theme;
			const auto* Entry = ThemeRanges.FindByPredicate([Theme](const FMWTrackSelectionExposureRange& Range)
			{
				return Theme.IsValid() && Range.Theme == Theme;
			});
			bHasRange = Entry && Entry->EV100.IsValid();
			if (bHasRange)
			{
				ActiveRange = Entry->EV100;
			}
		}
		Weight = bHasRange ? 1.f : 0.f;
	}
	else if (Weight > 0.f)
	{
		// 배치 단계도 같은 카메라를 쓴다. 실제로 뷰 타깃을 넘겼을 때 기존 노출이 드러나게 한다.
		const float PreviousWeight = Weight;
		Weight = ReleaseSeconds > 0.f ? FMath::Max(0.f, Weight - DeltaTime / ReleaseSeconds) : 0.f;
		if (PreviousWeight == 1.f || Weight == 0.f)
		{
			UE_LOG(LogMWTrackSelectionExposure, Verbose, TEXT("Target=%s BlendWeight=%.3f"), *GetNameSafe(Target), Weight);
		}
	}

	if (Weight > 0.f)
	{
		FPostProcessSettings Settings;
		Settings.bOverride_AutoExposureMinBrightness = true;
		Settings.bOverride_AutoExposureMaxBrightness = true;
		Settings.AutoExposureMinBrightness = ActiveRange.Min;
		Settings.AutoExposureMaxBrightness = ActiveRange.Max;
		if (Weight == 1.f && ActiveRange.Min == ActiveRange.Max)
		{
			// 고정 EV도 자동 노출 방식에서는 GPU의 이전 노출값을 읽는다.
			// 선택판의 고정 범위는 수동 노출로 평가하고, 시점을 넘기면 원래 방식으로 복귀한다.
			Settings.bOverride_AutoExposureMethod = true;
			Settings.AutoExposureMethod = AEM_Manual;
			Settings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
			Settings.AutoExposureApplyPhysicalCameraExposure = false;
			Settings.bOverride_AutoExposureBias = true;
			Settings.AutoExposureBias = -ActiveRange.Min;
		}
		CameraOwner->AddCachedPPBlend(Settings, Weight, VTBlendOrder_Override);
	}

	return false;
}
