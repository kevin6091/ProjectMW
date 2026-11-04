#pragma once
#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "System/MWRoomTypes.h"
#include "UI/MWMenuTypes.h"
#include "MWGameInstance.generated.h"

class UMWConnectionSubsystem;
struct FMWConnectionResult;
enum class EMWConnectionAction : uint8;
DECLARE_DELEGATE_TwoParams(FMWStartPrepared, bool, const FString&);

/** The only project adapter that consumes the independent connection module. */
UCLASS()
class MW_API UMWGameInstance : public UGameInstance
{
    GENERATED_BODY()
public:
    virtual void Init() override;
    virtual void Shutdown() override;
    UFUNCTION(BlueprintCallable, Category="MW|Connection") void RequestLogin();
    UFUNCTION(BlueprintPure, Category="MW|Connection") bool IsLoggedIn() const;
    UFUNCTION(BlueprintPure, Category="MW|Connection") bool IsLoginRequired() const;
    UFUNCTION(BlueprintPure, Category="MW|Connection") bool IsLoginInProgress() const;
    UFUNCTION(BlueprintPure, Category="MW|Connection") bool CanHostRoom() const;
    UFUNCTION(BlueprintPure, Category="MW|Connection") bool CanBrowseRooms() const;
    UFUNCTION(BlueprintPure, Category="MW|Connection") EMWSessionState GetSessionState() const;
    UFUNCTION(BlueprintPure, Category="MW|Connection") bool IsSessionBusy() const;
    UFUNCTION(BlueprintPure, Category="MW|Connection") FString GetLastSessionError() const;
    UFUNCTION(BlueprintCallable, Category="MW|Connection") void RequestCreateRoom(const FMWRoomSettings& Settings);
    UFUNCTION(BlueprintCallable, Category="MW|Connection") void RequestRefreshRooms();
    UFUNCTION(BlueprintCallable, Category="MW|Connection") void RequestJoinRoom(const FString& RoomHandle, int32 ListRevision);
    UFUNCTION(BlueprintCallable, Category="MW|Connection") void RequestJoinAddress(const FString& Address);
    UFUNCTION(BlueprintCallable, Category="MW|Connection") void RequestLeaveRoom();
    UFUNCTION(BlueprintCallable, Category="MW|Connection") void RetrySessionCleanup();
    UFUNCTION(BlueprintPure, Category="MW|Connection") TArray<FMWRoomInfo> GetAvailableRooms() const;
    UFUNCTION(BlueprintPure, Category="MW|Connection") int32 GetRoomListRevision() const { return RoomListRevision; }
    UFUNCTION(BlueprintCallable, Category="MW|Room") bool ValidateRoomSettings(const FMWRoomSettings& Input, FMWRoomSettings& Validated, FString& Error) const;
    UFUNCTION(BlueprintPure, Category="MW|Room") TArray<FName> GetAvailableMapThemeIds() const;

    bool GetRoomSettingsForWorld(const UWorld* World, FMWRoomSettings& Settings) const;
    bool IsLobbyWorld(const UWorld* World) const;
    bool IsGameplayWorld(const UWorld* World) const;
    FName ResolveMapTheme(FGameplayTag Theme, FString& Error) const;
    bool PrepareForGameStart(const TArray<int32>& Players, FMWStartPrepared Completion, FString& Error);
    const TArray<int32>& GetStartingPlayers() const { return StartingPlayers; }

    UPROPERTY(BlueprintAssignable) FMWLoginFinished OnLoginFinished;
    UPROPERTY(BlueprintAssignable) FMWSessionStateChanged OnSessionStateChanged;
    UPROPERTY(BlueprintAssignable) FMWSessionError OnSessionError;
    UPROPERTY(BlueprintAssignable) FMWCreateRoomFinished OnCreateRoomFinished;
    UPROPERTY(BlueprintAssignable) FMWRoomsUpdated OnRoomsUpdated;
    UPROPERTY(BlueprintAssignable) FMWJoinRoomFinished OnJoinRoomFinished;
private:
    TWeakObjectPtr<UMWConnectionSubsystem> Connection;
    FDelegateHandle SnapshotHandle, OperationHandle, WorldHandle;
    UPROPERTY(Transient) FMWRoomSettings CurrentRoomSettings;
    TArray<int32> StartingPlayers;
    FMWStartPrepared StartCompletion;
    EMWSessionState PreviousState = EMWSessionState::None;
    FString LastError, LastProviderError;
    int32 RoomListRevision = 0;
    void HandleSnapshot();
    void HandleOperation(EMWConnectionAction Action, const FMWConnectionResult& Result);
    void HandleWorld(UWorld* World);
    void ReportError(const FString& Error);
    void CompleteStart(bool Success, const FString& Error);
};
