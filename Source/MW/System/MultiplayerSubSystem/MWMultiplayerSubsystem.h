#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "System/MultiplayerSubSystem/MWMultiplayerTypes.h"
#include "Engine/EngineBaseTypes.h"
#include "TimerManager.h"

#include "MWMultiplayerSubsystem.generated.h"

class UNetDriver;
class UWorld;
class AMWPlayerController;
class FOnlineSessionSearch;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMWSessionStateChanged, EMWSessionState, PreviousState, EMWSessionState, NewState);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMWCreateRoomFinished, bool, bSuccess, const FString&, ErrorMessage);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMWSessionError, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMWRoomsUpdated, bool, bSuccess, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMWJoinRoomFinished, bool, bSuccess, const FString&, ErrorMessage);

// Subsystem이 Widget에 전달할 로그인 결과입니다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMWLoginFinished, bool, bSuccess, const FString&, ErrorMessage);

UCLASS(BlueprintType)
class MW_API UMWMultiplayerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// 로그인 작업을 요청.
	// 최종 결과는 OnLoginFinished로 전달.
	UFUNCTION(BlueprintCallable, Category = "MW|Multiplayer|Login")
	void RequestLogin();

	// 온라인 인터페이스의 현재 로그인 상태를 조회.
	UFUNCTION(BlueprintPure, Category = "MW|Multiplayer|Login")
	bool IsLoggedIn() const;

	UFUNCTION(BlueprintPure, Category = "MW|Multiplayer|Login")
	bool IsLoginInProgress() const
	{
		return bLoginInProgress;
	}

	UPROPERTY(BlueprintAssignable, Category = "MW|Multiplayer|Login")
	FMWLoginFinished OnLoginFinished;

	UFUNCTION(BlueprintPure, Category = "MW|Multiplayer|Session")
	EMWSessionState GetSessionState() const
	{
		return SessionState;
	}

	UFUNCTION(BlueprintPure, Category = "MW|Multiplayer|Session")
	bool IsSessionBusy() const;

	UPROPERTY(BlueprintAssignable, Category = "MW|Multiplayer|Session")
	FMWSessionStateChanged OnSessionStateChanged;

	// 입력을 검사하고, 성공하면 정리된 설정을 반환합니다.
	// 이 함수 자체는 Subsystem의 방 설정을 변경하지 않습니다.
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "MW|Multiplayer|Room")
	bool ValidateRoomSettings(const FMWRoomSettings& InputSettings, FMWRoomSettings& OutValidatedSettings, FString& OutErrorMessage) const;

	UFUNCTION(BlueprintCallable, Category = "MW|Multiplayer|Session")
	void RequestCreateRoom(const FMWRoomSettings& Settings);

	UFUNCTION(BlueprintCallable, Category = "MW|Multiplayer|Rooms")
	void RequestRefreshRooms();

	UFUNCTION(BlueprintPure, Category = "MW|Multiplayer|Rooms")
	TArray<FMWRoomInfo> GetAvailableRooms() const { return AvailableRooms; }

	UFUNCTION(BlueprintPure, Category = "MW|Multiplayer|Rooms")
	int32 GetRoomListRevision() const { return RoomListRevision; }

	UFUNCTION(BlueprintCallable, Category = "MW|Multiplayer|Rooms")
	void RequestJoinRoom(const FString& SessionId, int32 ListRevision);

	UPROPERTY(BlueprintAssignable, Category = "MW|Multiplayer|Rooms")
	FMWRoomsUpdated OnRoomsUpdated;

	UPROPERTY(BlueprintAssignable, Category = "MW|Multiplayer|Rooms")
	FMWJoinRoomFinished OnJoinRoomFinished;

	UFUNCTION(BlueprintCallable, Category = "MW|Multiplayer|Session")
	void RequestLeaveRoom();

	UFUNCTION(BlueprintCallable, Category = "MW|Multiplayer|Session")
	void RetrySessionCleanup();

	UFUNCTION(BlueprintPure, Category = "MW|Multiplayer|Session")
	FString GetLastSessionError() const { return LastSessionError; }

	UPROPERTY(BlueprintAssignable, Category = "MW|Multiplayer|Session")
	FMWCreateRoomFinished OnCreateRoomFinished;

	UPROPERTY(BlueprintAssignable, Category = "MW|Multiplayer|Session")
	FMWSessionError OnSessionError;

	// 아래 함수들은 서버 Framework가 호출할 일반 C++ 함수입니다.
	bool GetApprovedRoomSettingsForWorld(const UWorld* World, FMWRoomSettings& OutSettings) const;

	void ExpectHostRegistration(UWorld* World);
	void NotifyListenWorldReady(UWorld* World);
	void NotifyClientLobbyReady(AMWPlayerController* Controller);
	FString GetActiveRoomSessionId() const;

	UFUNCTION(BlueprintCallable, Category = "MW|Multiplayer|Room")
	TArray<FName> GetAvailableMapThemeIds() const;

private:
	// 현재 World에 맞는 EOS IdentityInterface를 구함.
	IOnlineIdentityPtr ResolveIdentity(FString& OutError) const;

	IOnlineSessionPtr ResolveSession(FString& OutError) const;

	// 온라인 인터페이스가 호출하는 완료 콜백.
	void HandleLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error, uint64 RequestId);

	// 등록했던 엔진 Delegate를 해제.
	void ClearLoginDelegate();

	// 상태 정리 후 UI에 최종 결과를 전달.
	void FinishLogin(bool bSuccess, const FString& ErrorMessage);

	void SetSessionState(EMWSessionState NewState);

	bool IsCurrentSessionRequest(uint64 RequestId) const;

	// 테마 태그와 카탈로그의 레벨 참조를 검사합니다. 실제 레벨은 로딩하지 않습니다.
	// 실패하면 NAME_None과 구체적인 오류를 반환합니다.
	FName ResolveMapPackageName(FGameplayTag MapThemeId, FString& OutError) const;

private:
	// 이번 구현에서는 실행 인스턴스당 로컬 사용자 한 명.
	static constexpr int32 LoginLocalUserNum = 0;

	UPROPERTY(Transient)
	bool bLoginInProgress = false;
	bool bDeinitializing = false;

	// 오래된 요청의 콜백을 구분.
	uint64 LoginRequestId = 0;

	// Delegate를 등록한 바로 그 인터페이스를 보관.
	IOnlineIdentityPtr LoginIdentity;

	FDelegateHandle LoginCompleteHandle;

	UPROPERTY(Transient)
	EMWSessionState SessionState = EMWSessionState::None;

	uint64 SessionRequestId = 0;

	static constexpr int32 MaxRoomNameLength = 32;
	static constexpr int32 MinRoomPlayers = 2;
	static constexpr int32 MaxRoomPlayers = 4;

	// 실제 생성 요청을 받아들인 뒤 보관할 설정입니다.
	UPROPERTY(Transient)
	FMWRoomSettings PendingRoomSettings;

private:
	enum class EMWCreateRoomStage : uint8
	{
		None,
		CreatingSession,
		LoadingWorld,
		Publishing
	};

	enum class EMWCleanupCause : uint8
	{
		None,
		CreationFailure,
		JoinFailure,
		RoomListFailure,
		Leave
	};

	FName ResolveTitleMapPackageName(FString& OutError) const;
	bool IsOwnWorld(const UWorld* World) const;
	bool HasPendingSessionWork() const;

	void HandleHostPlayersRegistered(FName SessionName, const TArray<FUniqueNetIdRef>& Players, bool bWasSuccessful, uint64 RequestId);

	void ClearHostRegistrationDelegate();

	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful, uint64 RequestId);

	void HandleUpdateSessionComplete(FName SessionName, bool bWasSuccessful, uint64 RequestId);

	void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful, uint64 RequestId);
	void HandleRoomsComplete(bool bWasSuccessful, uint64 RequestId);
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result, uint64 RequestId);
	void ClearRoomListDelegate();
	void ClearJoinDelegate();
	void FailJoin(const FString& Error);
	bool DescribeRoom(const FOnlineSessionSearchResult& Result, FMWRoomInfo& OutRoom) const;
	void InvalidateRoomList();

	void HandlePostLoadMap(UWorld* World);

	void HandleTravelFailure(UWorld* World, ETravelFailure::Type Failure, const FString& Error);

	void HandleNetworkFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Failure, const FString& Error);

	void ClearSessionApiDelegates();
	void ArmSessionTimeout();
	void ClearSessionTimeout();
	void HandleSessionTimeout(uint64 RequestId);

	void EmitSessionError(const FString& Error);
	void FailCreation(const FString& Error);
	void CloseServerAdmission();
	void StartCleanup();
	void BeginReturnToTitle();
	void FinishCleanup();

	IOnlineSessionPtr SessionApi;
	FUniqueNetIdPtr HostUserId;

	FDelegateHandle CreateSessionHandle;
	FDelegateHandle UpdateSessionHandle;
	FDelegateHandle DestroySessionHandle;
	FDelegateHandle RegisterPlayersHandle;
	FDelegateHandle RoomListHandle;
	FDelegateHandle JoinSessionHandle;
	TSharedPtr<FOnlineSessionSearch> RoomSearch;
	UPROPERTY(Transient) TArray<FMWRoomInfo> AvailableRooms;
	int32 RoomListRevision = 0;
	FString PendingJoinSessionId;
	FGameplayTag PendingJoinThemeId;
	FName PendingJoinMapPackage;
	bool bJoinConnecting = false;
	bool bClientTravelPending = false;
	bool bJoinResultPending = false;

	FDelegateHandle PostLoadMapHandle;
	FDelegateHandle TravelFailureHandle;
	FDelegateHandle NetworkFailureHandle;

	FTimerHandle SessionTimeoutHandle;

	FName PendingMapPackageName;
	FName ReturnMapPackageName;
	TWeakObjectPtr<UWorld> HostWorld;

	EMWCreateRoomStage CreateRoomStage = EMWCreateRoomStage::None;
	EMWCleanupCause CleanupCause = EMWCleanupCause::None;

	FString LastSessionError;

	bool bCreateResultPending = false;
	bool bReturningToTitle = false;
	bool bCleanupUncertain = false;
	bool bWorldLoadPending = false;
	bool bHostRegistrationExpected = false;
	bool bHostRegistrationConfirmed = false;

	// 개발용 시작값입니다. 서비스/맵별 실측 후 조정합니다.
	static constexpr float SessionOperationTimeoutSeconds = 60.0f;
};
