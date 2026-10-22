#pragma once

#include "NativeGameplayTags.h"

/** 에셋과 입력 기능을 같은 이름으로 찾기 위해 프로젝트 전체에서 함께 쓰는 태그입니다. */
namespace MWGameplayTags
{
	/** 게임 시작 때 미리 불러올 에셋을 묶는 분류 태그입니다. */
	MW_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(AssetLabel_Preload);
	/** 주행 모드의 입력 설정 에셋을 찾는 ID입니다. */
	MW_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Asset_Input_Driving);
	/** 건설 모드의 입력 설정 에셋을 찾는 ID입니다. */
	MW_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Asset_Input_Construction);
	/** 기본 차량의 생성 클래스와 주행 입력 설정을 찾는 ID입니다. */
	MW_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Asset_Vehicle_Default);

	/** 차량의 가속과 브레이크 입력을 구분합니다. */
	MW_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Throttle);
	/** 차량의 좌우 조향 입력을 구분합니다. */
	MW_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Steer);
	/** 보유한 아이템의 사용 요청 입력을 구분합니다. */
	MW_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_UseItem);
	/** 건설 화면에서 트랙 같은 후보를 고르는 입력을 구분합니다. */
	MW_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Select);
	/** 선택한 요소의 배치를 확정하도록 요청하는 입력을 구분합니다. */
	MW_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_ConfirmPlacement);
	/** 현재 선택이나 배치 준비를 취소하는 입력을 구분합니다. */
	MW_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Cancel);
	/** 건설 카메라의 화면 확대·축소 입력을 구분합니다. */
	MW_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Zoom);
}
