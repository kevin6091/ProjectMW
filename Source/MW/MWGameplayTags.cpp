#include "MWGameplayTags.h"

namespace MWGameplayTags
{
	// 선언한 에셋 ID와 분류 태그의 실제 이름을 등록해, 설정 데이터와 C++에서 같은 태그를 사용합니다.
	UE_DEFINE_GAMEPLAY_TAG(AssetLabel_Preload, "AssetLabel.Preload");
	
	UE_DEFINE_GAMEPLAY_TAG(Asset_Input_Driving, "Asset.Input.Driving");
	
	UE_DEFINE_GAMEPLAY_TAG(Asset_Input_PlaceConstruction, "Asset.Input.PlaceConstruction");
	UE_DEFINE_GAMEPLAY_TAG(Asset_Input_SelectConstruction, "Asset.Input.SelectConstruction");
	
	UE_DEFINE_GAMEPLAY_TAG(Asset_Vehicle_Default, "Asset.Vehicle.Default");
	
	UE_DEFINE_GAMEPLAY_TAG(Asset_MapTheme_DevDefault, "Asset.MapTheme.DevDefault");

	// 실제 키 대신 입력의 목적을 나타냅니다. 키와 액션의 연결은 모드별 MWInputData에서 정합니다.
	// Input_Driving
	UE_DEFINE_GAMEPLAY_TAG(Input_Accel, "Input.Action.Drive.Accel");
	UE_DEFINE_GAMEPLAY_TAG(Input_Brake, "Input.Action.Drive.Brake");
	UE_DEFINE_GAMEPLAY_TAG(Input_Drift, "Input.Action.Drive.Drift");
	UE_DEFINE_GAMEPLAY_TAG(Input_Steer, "Input.Action.Drive.Steer");
	UE_DEFINE_GAMEPLAY_TAG(Input_UseItem, "Input.Action.Drive.UseItem");
	
	// Input_SelectConstruction
	UE_DEFINE_GAMEPLAY_TAG(Input_Select, "Input.Action.SelectConstruction.Select");
	UE_DEFINE_GAMEPLAY_TAG(Input_SelectMouseMove, "Input.Action.SelectConstruction.MouseMove");
	
	// Input_PlaceConstruction
	UE_DEFINE_GAMEPLAY_TAG(Input_Accept, "Input.Action.PlaceConstruction.Accept");
	UE_DEFINE_GAMEPLAY_TAG(Input_PlaceMouseMove, "Input.Action.PlaceConstruction.MouseMove");
	UE_DEFINE_GAMEPLAY_TAG(Input_ZoomInOut, "Input.Action.PlaceConstruction.ZoomInOut");
	UE_DEFINE_GAMEPLAY_TAG(Input_Turn, "Input.Action.PlaceConstruction.Turn");
}
