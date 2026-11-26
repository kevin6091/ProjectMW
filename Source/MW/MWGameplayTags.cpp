#include "MWGameplayTags.h"

namespace MWGameplayTags
{
	// 선언한 에셋 ID와 분류 태그의 실제 이름을 등록해, 설정 데이터와 C++에서 같은 태그를 사용합니다.
	UE_DEFINE_GAMEPLAY_TAG(AssetLabel_Preload, "AssetLabel.Preload");
	UE_DEFINE_GAMEPLAY_TAG(AssetLabel_Vehicles, "AssetLabel.Vehicles");
	
	UE_DEFINE_GAMEPLAY_TAG(Asset_Input_Driving, "Asset.Input.Driving");
	
	UE_DEFINE_GAMEPLAY_TAG(Asset_Input_PlaceConstruction, "Asset.Input.PlaceConstruction");
	UE_DEFINE_GAMEPLAY_TAG(Asset_Input_SelectConstruction, "Asset.Input.SelectConstruction");
	
	UE_DEFINE_GAMEPLAY_TAG(Asset_Track_RuntimeCatalog, "Asset.Track.RuntimeCatalog");
	UE_DEFINE_GAMEPLAY_TAG(Asset_Item, "Asset.Item");
	UE_DEFINE_GAMEPLAY_TAG(Asset_Item_Booster, "Asset.Item.Booster");
	UE_DEFINE_GAMEPLAY_TAG(Asset_Item_Hammer, "Asset.Item.Hammer");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(Asset_Item_MachineGun, "Asset.Item.MachineGun");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(Asset_Item_Shotgun, "Asset.Item.Shotgun");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(Asset_Item_MortarLauncher, "Asset.Item.MortarLauncher");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(Asset_Item_MineLayer, "Asset.Item.MineLayer");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(Asset_Item_StickyBlobLauncher, "Asset.Item.StickyBlobLauncher");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(Asset_Item_GrowPotion, "Asset.Item.GrowPotion");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(Asset_Item_TeslaCoil, "Asset.Item.TeslaCoil");

	// VehicleAsset ID
	UE_DEFINE_GAMEPLAY_TAG(Asset_Vehicle_SMIET, "Asset.Vehicle.SMIET");
	UE_DEFINE_GAMEPLAY_TAG(Asset_Vehicle_MAMMOTH, "Asset.Vehicle.MAMMOTH");
	UE_DEFINE_GAMEPLAY_TAG(Asset_Vehicle_UFOLOGY, "Asset.Vehicle.UFOLOGY");
	UE_DEFINE_GAMEPLAY_TAG(Asset_Vehicle_DOUG, "Asset.Vehicle.DOUG");
	UE_DEFINE_GAMEPLAY_TAG(Asset_Vehicle_FRED, "Asset.Vehicle.FRED");
	
	// MapTheme
	UE_DEFINE_GAMEPLAY_TAG(Asset_MapTheme_DriveDefault, "Asset.MapTheme.DriveDefault");

	// 실제 키 대신 입력의 목적을 나타냅니다. 키와 액션의 연결은 모드별 MWInputData에서 정합니다.
	// Input_Driving
	UE_DEFINE_GAMEPLAY_TAG(Input_Accel, "Input.Action.Drive.Accel");
	UE_DEFINE_GAMEPLAY_TAG(Input_Reverse, "Input.Action.Drive.Reverse");
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
