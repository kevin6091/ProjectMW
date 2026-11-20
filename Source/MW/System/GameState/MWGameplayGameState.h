

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
	FMWGameplayProgress GetProgress() const { return Progress; }
	UFUNCTION(BlueprintPure, Category = "MW|Gameplay")
	double GetPhaseSecondsRemaining() const;
	void ApplyProgressFromServer(const FMWGameplayProgress& NewProgress);
	
	UFUNCTION(BlueprintPure, Category = "MW|Gameplay")
	FMWRoomSettings GetRoomSettings() const { return RoomSettings; }
	
	UFUNCTION(BlueprintPure, Category = "MW|Gameplay")
	EMWRoomPhase GetRoomPhase() const { return Progress.Phase; }

	
	void ApplyRoomSettingsFromServer(const FMWRoomSettings& Settings);
	
	UPROPERTY(BlueprintAssignable, Category = "MW|Gameplay")
	FMWGameplayStateChanged OnGameplayStateChanged;
	
	// Construction
	UFUNCTION(BlueprintPure, Category = "MW|Construction")
	UMWTrackConstructionComponent* GetConstructionComponent() const { return TrackConstructionComponent; }

private:
	// RoomSettings - RoomState
	UPROPERTY(ReplicatedUsing = OnRep_RoomState)
	FMWRoomSettings RoomSettings;
	UFUNCTION()
	void OnRep_RoomState();
	
	// Progress
	UPROPERTY(ReplicatedUsing = OnRep_Progress)
	FMWGameplayProgress Progress;
	UFUNCTION()
	void OnRep_Progress();
	
	void NotifyRoomStateChanged();
	
	UPROPERTY(VisibleAnywhere, Category = "MW|Construction")
	UMWTrackConstructionComponent* TrackConstructionComponent;
};
