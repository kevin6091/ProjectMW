#pragma once

#include "UObject/Interface.h"
#include "MWLobbyPresentation.generated.h"

class UMWRoomLobbyWidget;
class UMWVehicleSelectionWidget;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UMWLobbyPresentation : public UInterface
{
	GENERATED_BODY()
};

class MW_API IMWLobbyPresentation
{
	GENERATED_BODY()

public:
	// HUD는 화면과 요청을 소유하고, 맵의 연출 액터는 화면을 표시할 위치와 타이밍을 정합니다.
	virtual void SetLobbyWidgets(UMWRoomLobbyWidget* Lobby, UMWVehicleSelectionWidget* Selection) = 0;
};
