#pragma once

#include "CoreMinimal.h"
#include "MWConnectionTypes.generated.h"

UENUM(BlueprintType)
enum class EMWConnectionAction : uint8
{
    None, Login, HostRoom, RefreshRooms, JoinRoom, JoinAddress, SetRoomJoinable, LeaveRoom
};

UENUM(BlueprintType)
enum class EMWConnectionState : uint8
{
    Idle, Creating, Searching, Joining, Hosting, Joined, Closing, Unavailable
};

UENUM(BlueprintType)
enum class EMWConnectionError : uint8
{
    None, Busy, InvalidArgument, NotLoggedIn, ServiceUnavailable, RoomUnavailable,
    ConnectionFailed, Timeout, Cancelled, RestartRequired
};

/** MaxPlayers includes the host. Properties are opaque game metadata, never game rules. */
USTRUCT(BlueprintType)
struct MWCONNECTION_API FMWConnectionRoomOptions
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString RoomName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MaxPlayers = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FString, FString> Properties;
};

/** RoomHandle is only an opaque target for RequestJoinRoom. */
USTRUCT(BlueprintType)
struct MWCONNECTION_API FMWConnectionRoomInfo
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString RoomHandle;
    UPROPERTY(BlueprintReadOnly) FString RoomName;
    UPROPERTY(BlueprintReadOnly) int32 CurrentPlayers = 0;
    UPROPERTY(BlueprintReadOnly) int32 MaxPlayers = 0;
    UPROPERTY(BlueprintReadOnly) TMap<FString, FString> Properties;
    UPROPERTY(BlueprintReadOnly) bool bJoinable = false;
    UPROPERTY(BlueprintReadOnly) FString UnavailableReason;
};

USTRUCT(BlueprintType)
struct MWCONNECTION_API FMWConnectionResult
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) bool bSuccess = false;
    UPROPERTY(BlueprintReadOnly) EMWConnectionError ErrorCode = EMWConnectionError::None;
    UPROPERTY(BlueprintReadOnly) FText Message;
};

/** Read-only projection. This is connection status, not a replicated gameplay phase. */
USTRUCT(BlueprintType)
struct MWCONNECTION_API FMWConnectionSnapshot
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) EMWConnectionState State = EMWConnectionState::Idle;
    UPROPERTY(BlueprintReadOnly) EMWConnectionAction ActiveAction = EMWConnectionAction::None;
    UPROPERTY(BlueprintReadOnly) bool bLocalIP = false;
    UPROPERTY(BlueprintReadOnly) bool bLoggedIn = false;
    UPROPERTY(BlueprintReadOnly) bool bBusy = false;
    UPROPERTY(BlueprintReadOnly, meta=(ScriptName="is_in_room")) bool bInRoom = false;
    UPROPERTY(BlueprintReadOnly) bool bIsHost = false;
    UPROPERTY(BlueprintReadOnly) FMWConnectionRoomOptions Room;
    UPROPERTY(BlueprintReadOnly) TArray<FMWConnectionRoomInfo> Rooms;
    UPROPERTY(BlueprintReadOnly) FText ErrorMessage;
};
