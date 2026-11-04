#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "System/MWRoomTypes.h"
#include "UI/MWMenuTypes.h"
#include "MWMultiplayerSubsystem.generated.h"
class UMWGameInstance;
/** Legacy Blueprint compatibility only. Owns no online interfaces, session state or world flow. */
UCLASS(BlueprintType)
class MW_API UMWMultiplayerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    UFUNCTION(BlueprintCallable) void RequestLogin();
    UFUNCTION(BlueprintPure) bool IsLoggedIn() const;
    UFUNCTION(BlueprintPure) bool IsLoginInProgress() const;
    UFUNCTION(BlueprintPure) EMWSessionState GetSessionState() const;
    UFUNCTION(BlueprintPure) bool IsSessionBusy() const;
    UFUNCTION(BlueprintCallable, BlueprintPure=false) bool ValidateRoomSettings(const FMWRoomSettings& InputSettings, FMWRoomSettings& OutValidatedSettings, FString& OutErrorMessage) const;
    UFUNCTION(BlueprintCallable) void RequestCreateRoom(const FMWRoomSettings& Settings);
    UFUNCTION(BlueprintCallable) void RequestRefreshRooms();
    UFUNCTION(BlueprintPure) TArray<FMWRoomInfo> GetAvailableRooms() const;
    UFUNCTION(BlueprintPure) int32 GetRoomListRevision() const;
    UFUNCTION(BlueprintCallable) void RequestJoinRoom(const FString& SessionId, int32 ListRevision);
    UFUNCTION(BlueprintCallable) void RequestLeaveRoom();
    UFUNCTION(BlueprintCallable) void RetrySessionCleanup();
    UFUNCTION(BlueprintPure) FString GetLastSessionError() const;
    UFUNCTION(BlueprintCallable) TArray<FName> GetAvailableMapThemeIds() const;
    UPROPERTY(BlueprintAssignable) FMWLoginFinished OnLoginFinished;
    UPROPERTY(BlueprintAssignable) FMWSessionStateChanged OnSessionStateChanged;
    UPROPERTY(BlueprintAssignable) FMWSessionError OnSessionError;
    UPROPERTY(BlueprintAssignable) FMWCreateRoomFinished OnCreateRoomFinished;
    UPROPERTY(BlueprintAssignable) FMWRoomsUpdated OnRoomsUpdated;
    UPROPERTY(BlueprintAssignable) FMWJoinRoomFinished OnJoinRoomFinished;
private:
    UMWGameInstance* App() const;
    UFUNCTION() void ForwardLoginFinished(bool bSuccess, const FString& ErrorMessage);
    UFUNCTION() void ForwardSessionStateChanged(EMWSessionState PreviousState, EMWSessionState NewState);
    UFUNCTION() void ForwardSessionError(const FString& ErrorMessage);
    UFUNCTION() void ForwardCreateRoomFinished(bool bSuccess, const FString& ErrorMessage);
    UFUNCTION() void ForwardRoomsUpdated(bool bSuccess, const FString& ErrorMessage);
    UFUNCTION() void ForwardJoinRoomFinished(bool bSuccess, const FString& ErrorMessage);
};
