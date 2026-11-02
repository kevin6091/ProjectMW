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
	// 레벨에 놓인 선택판이 로컬 Controller에 자신을 등록한다.
	void RegisterTrackSelectionBoard(AMWTrackSelectionBoard* Board);
	void UnregisterTrackSelectionBoard(AMWTrackSelectionBoard* Board);

	// 화면에 표시 중인 후보의 확정을 요청한다. 실제 승인 권한은 서버 컴포넌트에 있다.
	UFUNCTION(BlueprintCallable, Category="MW|Construction")
	void ConfirmLocalTrackSelection(int32 OfferId);

	// 로컬 서버의 비Shipping 실행에서만 처리하는 개발 콘솔 명령.
	UFUNCTION(Exec)
	void BeginTrackSelection();

	UFUNCTION(Exec)
	void EndTrackSelection();

	UFUNCTION(BlueprintCallable, Category = "MW|Lobby")
	void RefreshLobbyScreen();

protected:
	// 선택 전용 테스트 Controller BP는 로비 Widget을 생성하지 않도록 끈다.
	UPROPERTY(EditDefaultsOnly, Category="MW|Lobby")
	bool bCreateLobbyScreen = true;

	UFUNCTION(BlueprintImplementableEvent, Category="MW|Construction")
	void OnTrackSelectionRequestFinished(const FMWTrackSelectionReply& Reply);

	UFUNCTION(BlueprintImplementableEvent, Category = "MW|Lobby")
	void CreateLobbyScreen();

	UFUNCTION(BlueprintImplementableEvent, Category = "MW|Lobby")
	void OnLobbyScreenRefresh();

private:
	// 선택 기능 구현은 MWPlayerControllerConstruction.cpp에 모아 둔다.
	// 상태 이벤트를 구독한 뒤 현재 상태도 읽어, 초기 복제와 Actor 생성 순서에 대응한다.
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
	// 클라이언트가 소유한 Controller를 요청 경로로 사용한다. Requester는 서버에서 this로 결정한다.
	UFUNCTION(Server, Reliable)
	void ServerUpdateTrackHover(int32 SessionId, int32 Revision, int32 OfferId);
	UFUNCTION(Server, Reliable)
	void ServerConfirmTrackSelection(int32 SessionId, int32 Revision, int32 OfferId, int32 RequestId);
	// 공용 상태 복제와 별개로 이 요청을 보낸 소유자에게만 성공·실패를 알려준다.
	UFUNCTION(Client, Reliable)
	void ClientTrackSelectionResult(const FMWTrackSelectionReply& Reply);

	// 구독 대상과 레벨 Actor는 Weak 참조로 보관해 맵 종료 시 수명을 붙잡지 않는다.
	TWeakObjectPtr<UMWTrackConstructionComponent> BoundConstruction;
	TArray<TWeakObjectPtr<UMWPlayerConstructionComponent>> BoundPlayerConstruction;
	TWeakObjectPtr<AMWTrackSelectionBoard> SelectionBoard;
	TWeakObjectPtr<AActor> PreviousSelectionViewTarget;
	TWeakObjectPtr<UEnhancedInputComponent> SelectionInputComponent;
	// 로컬 표현에 사용하는 UObject는 GC가 추적하도록 UPROPERTY로 보관한다.
	UPROPERTY(Transient) TObjectPtr<UMWInputData> SelectionInputData;
	UPROPERTY(Transient) TObjectPtr<UMWTrackSelectionWidget> SelectionWidget;
	// 선택 기능이 설치한 입력 바인딩만 해제하기 위한 핸들.
	uint32 SelectionBindingHandle = 0;
	bool bSelectionInputInstalled = false;
	bool bSelectionPresentationActive = false;
	bool bRefreshingConstruction = false;
	bool bConstructionShuttingDown = false;
	bool bLobbyPresentationEnabled = true;
	bool bConfirmPending = false; // 확정 응답을 기다리는 동안 중복 입력을 막는다.
	// 최신 복제값 대신 사용자가 실제로 본 세션·버전을 요청에 넣는다.
	int32 DisplaySessionId = 0;
	int32 DisplayOffersRevision = 0;
	int32 LocalHoveredOfferId = INDEX_NONE; // 자기 화면은 서버 왕복 없이 즉시 강조한다.
	int32 LastSentHoveredOfferId = INDEX_NONE;
	// 승인 RPC가 개인 상태 복제보다 먼저 와도 재선택을 잠그기 위한 로컬 기록.
	int32 AcknowledgedSelectedOfferId = INDEX_NONE;
	int32 NextSelectionRequestId = 0;
	// 응답의 세션·버전·요청·후보가 모두 일치해야 대기 중인 요청의 결과로 처리한다.
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
