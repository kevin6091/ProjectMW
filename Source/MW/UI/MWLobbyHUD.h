#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GameplayTagContainer.h"
#include "UObject/WeakInterfacePtr.h"
#include "MWLobbyHUD.generated.h"

class AMWLobbyGameState;
class AMWLobbyPlayerController;
class AMWPlayerState;
class UMWRoomLobbyWidget;
class UMWVehicleSelectionWidget;
class UWidgetComponent;
class IMWLobbyPresentation;
class UMWRemoteCursorOverlay;

// 기존 Controller/BP에 있던 로비 화면 생성과 상태 구독만 이전한 로컬 표시 담당자입니다.
UCLASS()
class MW_API AMWLobbyHUD : public AHUD
{
	GENERATED_BODY()

public:
	AMWLobbyHUD();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	void RefreshLobbyScreen();

private:
	UPROPERTY(EditDefaultsOnly, Category = "MW|Lobby UI")
	TSubclassOf<UMWRoomLobbyWidget> RoomLobbyWidgetClass;
	UPROPERTY(Transient)
	TObjectPtr<UMWRoomLobbyWidget> RoomLobbyWidget;
	UPROPERTY(EditDefaultsOnly, Category = "MW|Lobby UI")
	TSubclassOf<UMWVehicleSelectionWidget> VehicleSelectionWidgetClass;
	UPROPERTY(Transient)
	TObjectPtr<UMWVehicleSelectionWidget> VehicleSelectionWidget;
	UPROPERTY(Transient)
	TObjectPtr<UMWRemoteCursorOverlay> CursorOverlay;
	void UpdateCursorSharing(float DeltaSeconds);
	TWeakInterfacePtr<IMWLobbyPresentation> LobbyPresentation;
	TWeakObjectPtr<UWidgetComponent> VehicleSelectionSurface;
	TWeakObjectPtr<AActor> PreviousViewTarget;
	FTimerHandle VehicleCameraTimer;
	void OpenVehicleSelection();
	void PresentVehicleSelection();
	void CloseVehicleSelection();
	UFUNCTION()
	void HandleVehicleSelectionRequested(FGameplayTag VehicleAssetID);

	// 비소유 참조를 사용해 맵 이동으로 파괴될 상태 객체를 붙잡지 않습니다.
	TWeakObjectPtr<AMWLobbyGameState> BoundGameState;
	TWeakObjectPtr<AMWLobbyPlayerController> BoundController;
	TArray<TWeakObjectPtr<AMWPlayerState>> BoundPlayers;
	FTimerHandle BindingRetryTimer;
	void RefreshBindings();
	void ClearBindings();
	UFUNCTION()
	void HandleLobbyChanged();
	UFUNCTION()
	void HandlePlayerStateChanged();
	UFUNCTION()
	void HandleActionErrorChanged();
};
