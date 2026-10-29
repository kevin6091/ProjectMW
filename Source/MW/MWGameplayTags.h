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
	MW_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Asset_Input_PlaceConstruction);
	MW_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Asset_Input_SelectConstruction);
	
	/** 기본 차량의 생성 클래스와 주행 입력 설정을 찾는 ID입니다. */
	MW_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Asset_Vehicle_Default);
	
	// MapTheme
	MW_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Asset_MapTheme_DevDefault);

	
	
	// Input_Driving
	MW_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Accel);
	MW_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Brake);
	MW_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Steer);
	MW_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Drift);
	MW_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_UseItem);
	
	// Input_SelectConstruction
	MW_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Select);
	MW_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_SelectMouseMove);
	
	// Input_PlaceConstruction
	MW_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Accept);
	MW_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_PlaceMouseMove);
	MW_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_ZoomInOut);
	MW_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Turn);
	
}
