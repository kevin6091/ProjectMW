#pragma once

#include "NativeGameplayTags.h"

/** 에셋과 입력 기능을 같은 이름으로 찾기 위해 프로젝트 전체에서 함께 쓰는 태그입니다. */
namespace MWGameplayTags
{
	// Labels
	MW_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(AssetLabel_Preload);
	MW_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(AssetLabel_Vehicles);
	
	// Input Data
	MW_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Asset_Input_Driving);
	MW_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Asset_Input_PlaceConstruction);
	MW_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Asset_Input_SelectConstruction);
	
	// VehicleAsset ID
	MW_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Asset_Vehicle_SMIET);
	MW_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Asset_Vehicle_MAMMOTH);
	MW_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Asset_Vehicle_UFOLOGY);
	MW_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Asset_Vehicle_DOUG);
	MW_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Asset_Vehicle_FRED);
	
	// MapTheme
	MW_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Asset_MapTheme_DriveDefault);

	
	
	// Input_Driving
	MW_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Accel);
	MW_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Reverse);
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
