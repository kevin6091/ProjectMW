#pragma once

#include "UObject/Interface.h"
#include "MWLobbyPresentation.generated.h"

class UMWRoomLobbyWidget;
class UMWVehicleSelectionWidget;
class UWidgetComponent;

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
	// 실제 표시 표면만 반환한다. 네트워크/입력/선택 규칙은 HUD와 서버가 담당한다.
	virtual UWidgetComponent* GetVehicleSelectionSurface() const { return nullptr; }
	// 출발 연출을 제공하는 맵은 화면을 가린 뒤 완료를 알립니다.
	virtual bool BeginVehicleDeparture(FSimpleDelegate OnCovered) { return false; }
};
