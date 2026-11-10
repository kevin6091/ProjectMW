#pragma once
#include "CoreMinimal.h"
#include "MWMenuTypes.generated.h"

// Existing menu/BP presentation contract. Never a gameplay permission check.
UENUM(BlueprintType)
enum class EMWSessionState : uint8
{
    None, Creating, Hosting, Searching, Joining, Joined, Leaving, CleanupRequired
};

USTRUCT(BlueprintType)
struct MW_API FMWRoomInfo
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString SessionId;
    UPROPERTY(BlueprintReadOnly) FString RoomName;
    UPROPERTY(BlueprintReadOnly) int32 CurrentPlayers = 0;
    UPROPERTY(BlueprintReadOnly) int32 MaxPlayers = 0;
    UPROPERTY(BlueprintReadOnly) FName MapThemeId;
    UPROPERTY(BlueprintReadOnly) bool bCanJoin = false;
    UPROPERTY(BlueprintReadOnly) FString UnavailableReason;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMWLoginFinished, bool, bSuccess, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMWSessionStateChanged, EMWSessionState, PreviousState, EMWSessionState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMWSessionError, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMWCreateRoomFinished, bool, bSuccess, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMWRoomsUpdated, bool, bSuccess, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMWJoinRoomFinished, bool, bSuccess, const FString&, ErrorMessage);
