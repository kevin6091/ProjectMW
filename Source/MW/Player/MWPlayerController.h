#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "System/MultiplayerSubSystem/MWMultiplayerTypes.h"
#include "Gameplay/Construction/MWTrackSelectionTypes.h"
#include "MWPlayerController.generated.h"

class AMWGameState;
class UMWMultiplayerSubsystem;
class UMWTrackConstructionComponent;
class UMWPlayerConstructionComponent;
class AMWTrackSelectionBoard;
class UMWTrackSelectionWidget;
class UMWInputData;
class UEnhancedInputComponent;

UCLASS()
class MW_API AMWPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnRep_PlayerState() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;
	void RegisterTrackSelectionBoard(AMWTrackSelectionBoard* Board);
	void UnregisterTrackSelectionBoard(AMWTrackSelectionBoard* Board);

	UFUNCTION(BlueprintCallable, Category="MW|Construction")
	void ConfirmLocalTrackSelection(int32 OfferId);

	UFUNCTION(Exec)
	void BeginTrackSelection();

	UFUNCTION(Exec)
	void EndTrackSelection();

	UFUNCTION(BlueprintCallable, Category = "MW|Lobby")
	void RefreshLobbyScreen();

protected:
	UPROPERTY(EditDefaultsOnly, Category="MW|Lobby")
	bool bCreateLobbyScreen = true;

	UFUNCTION(BlueprintImplementableEvent, Category="MW|Construction")
	void OnTrackSelectionRequestFinished(const FMWTrackSelectionReply& Reply);

	UFUNCTION(BlueprintImplementableEvent, Category = "MW|Lobby")
	void CreateLobbyScreen();

	UFUNCTION(BlueprintImplementableEvent, Category = "MW|Lobby")
	void OnLobbyScreenRefresh();

private:
	void RebindConstructionSources();
	void UnbindConstructionSources();
	void RefreshConstructionPresentation();
	void StopConstructionPresentation();
	bool EnableSelectionInput();
	void DisableSelectionInput();
	int32 FindHoveredTrackOffer() const;
	void UpdateLocalTrackHover(int32 OfferId);
	void HandleTrackSelectPressed();
	bool IsDisplayedTrackOffer(int32 OfferId, bool bAllowNone) const;
	void RefreshSelectionText(const FMWTrackSelectionSession& Session);

	UFUNCTION() void HandleConstructionSessionChanged();
	UFUNCTION() void HandlePlayerConstructionChanged();
	UFUNCTION(Server, Reliable)
	void ServerUpdateTrackHover(int32 SessionId, int32 Revision, int32 OfferId);
	UFUNCTION(Server, Reliable)
	void ServerConfirmTrackSelection(int32 SessionId, int32 Revision, int32 OfferId, int32 RequestId);
	UFUNCTION(Client, Reliable)
	void ClientTrackSelectionResult(const FMWTrackSelectionReply& Reply);

	TWeakObjectPtr<UMWTrackConstructionComponent> BoundConstruction;
	TArray<TWeakObjectPtr<UMWPlayerConstructionComponent>> BoundPlayerConstruction;
	TWeakObjectPtr<AMWTrackSelectionBoard> SelectionBoard;
	TWeakObjectPtr<AActor> PreviousSelectionViewTarget;
	TWeakObjectPtr<UEnhancedInputComponent> SelectionInputComponent;
	UPROPERTY(Transient) TObjectPtr<UMWInputData> SelectionInputData;
	UPROPERTY(Transient) TObjectPtr<UMWTrackSelectionWidget> SelectionWidget;
	uint32 SelectionBindingHandle = 0;
	bool bSelectionInputInstalled = false;
	bool bSelectionPresentationActive = false;
	bool bRefreshingConstruction = false;
	bool bConstructionShuttingDown = false;
	bool bLobbyPresentationEnabled = true;
	bool bConfirmPending = false;
	int32 DisplaySessionId = 0;
	int32 DisplayOffersRevision = 0;
	int32 LocalHoveredOfferId = INDEX_NONE;
	int32 LastSentHoveredOfferId = INDEX_NONE;
	int32 AcknowledgedSelectedOfferId = INDEX_NONE;
	int32 NextSelectionRequestId = 0;
	int32 PendingSelectionRequestId = 0;
	int32 PendingSelectedOfferId = INDEX_NONE;
	double LastHoverSendTime = 0.0;
	FText SelectionStatus;

	TWeakObjectPtr<AMWGameState> BoundGameState;
	TWeakObjectPtr<UMWMultiplayerSubsystem> BoundSubsystem;

	UFUNCTION()
	void HandleLobbyChanged();

	UFUNCTION()
	void HandleSessionError(const FString& ErrorMessage);

	UFUNCTION()
	void HandleSessionStateChanged(
		EMWSessionState PreviousState, EMWSessionState NewState);
};
