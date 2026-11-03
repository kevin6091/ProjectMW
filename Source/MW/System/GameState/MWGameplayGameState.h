

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "System/MWRoomTypes.h"
#include "MWGameplayGameState.generated.h"

class UMWTrackConstructionComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMWGameplayStateChanged);

// 기존 GameState에서 테마 진입 상태와 카운트다운 저장 필드만 분리했습니다.
UCLASS()
class MW_API AMWGameplayGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AMWGameplayGameState();
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	UFUNCTION(BlueprintPure, Category = "MW|Gameplay")
	FMWRoomSettings GetRoomSettings() const { return RoomSettings; }
	
	UFUNCTION(BlueprintPure, Category = "MW|Gameplay")
	EMWRoomPhase GetRoomPhase() const { return RoomPhase; }

	void ApplyRoomSettingsFromServer(const FMWRoomSettings& Settings);
	void SetRoomPhaseFromServer(EMWRoomPhase Phase);
	
	UPROPERTY(BlueprintAssignable, Category = "MW|Gameplay")
	FMWGameplayStateChanged OnGameplayStateChanged;
	
	// Construction
	UFUNCTION(BlueprintPure, Category = "MW|Construction")
	UMWTrackConstructionComponent* GetConstructionComponent() const { return TrackConstructionComponent; }

private:
	UPROPERTY(ReplicatedUsing = OnRep_RoomState)
	FMWRoomSettings RoomSettings;
	
	UPROPERTY(ReplicatedUsing = OnRep_RoomState)
	EMWRoomPhase RoomPhase = EMWRoomPhase::PreparingConstruction;
	
	// 기존 필드의 이전입니다. 실제 카운트다운 시작/종료 로직을 추가한 것은 아닙니다.
	UPROPERTY(ReplicatedUsing = OnRep_RoomState)
	double CountdownEndServerTime = 0.0;
	
	UFUNCTION()
	void OnRep_RoomState();
	void NotifyRoomStateChanged();
	
	UPROPERTY(VisibleAnywhere, Category = "MW|Construction")
	UMWTrackConstructionComponent* TrackConstructionComponent;
};
