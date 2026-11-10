#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTagContainer.h"
#include "MWGameplayTags.h"
#include "MWPlayerState.generated.h"

class UMWPlayerCursorComponent;
class UMWPlayerConstructionComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMWPlayerStateChanged);

UCLASS()
class MW_API AMWPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AMWPlayerState();
	virtual void CopyProperties(APlayerState* PlayerState) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnRep_PlayerName() override;
	virtual void OnRep_UniqueId() override;

	UFUNCTION(BlueprintPure, Category = "MW|Lobby")
	bool IsRoomHost() const { return bRoomHost; }

	UFUNCTION(BlueprintPure, Category = "MW|Lobby")
	bool IsReady() const { return bReady; }

	UFUNCTION(BlueprintPure, Category = "MW|Lobby")
	int32 GetLobbyMemberId() const { return LobbyMemberId; }

	void SetLobbyFlagsFromServer(bool bInHost, bool bInReady);
	void SetLobbyMemberIdFromServer(int32 Id);

	UFUNCTION(BlueprintPure, Category = "MW|Vehicle")
	FGameplayTag GetVehicleAssetID() const { return VehicleAssetID; }

	// 서버가 승인한 선택 결과를 저장합니다. Server RPC는 아닙니다.
	bool ConfirmVehicleAssetIDFromServer(FGameplayTag InVehicleAssetID);
	bool ClearVehicleAssetIDFromServer(FGameplayTag ExpectedVehicleID);
	
	// 공용 개인 상태는 특정 로비 GameState나 Widget을 직접 호출하지 않습니다.
	UPROPERTY(BlueprintAssignable, Category = "MW|Player")
	FMWPlayerStateChanged OnPlayerStateChanged;
	
	// Construction
	UFUNCTION(BlueprintPure, Category = "MW|Construction")
	UMWPlayerConstructionComponent* GetPlayerConstructionComponent() const { return PlayerConstructionComponent; }

	UFUNCTION(BlueprintPure, Category="MW|Cursor")
	UMWPlayerCursorComponent* GetCursorComponent() const { return CursorComponent; }

private:
	// 화면 수명이 끝나면 폐기하는 표시 상태다. CopyProperties로 맵 간 인계하지 않는다.
	UPROPERTY(VisibleAnywhere, Category="MW|Cursor")
	TObjectPtr<UMWPlayerCursorComponent> CursorComponent;

	UPROPERTY(ReplicatedUsing = OnRep_LobbyFlags)
	bool bRoomHost = false;

	UPROPERTY(ReplicatedUsing = OnRep_LobbyFlags)
	bool bReady = false;
	
	UPROPERTY(ReplicatedUsing = OnRep_LobbyFlags)
	int32 LobbyMemberId = INDEX_NONE;
	UFUNCTION()
	void OnRep_LobbyFlags();
	
	UPROPERTY(ReplicatedUsing = OnRep_VehicleAssetID)
	FGameplayTag VehicleAssetID = FGameplayTag::EmptyTag;
	
	UFUNCTION()
	void OnRep_VehicleAssetID();
	void NotifyPlayerStateChanged();
	
	UPROPERTY(VisibleAnywhere, Category = "MW|Construction")
	UMWPlayerConstructionComponent* PlayerConstructionComponent;
};
