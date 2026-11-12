#pragma once
#include "CoreMinimal.h"
#include "MWMenuTypes.generated.h"

namespace MWMenu
{
	// 표시명만 변환한다. 카탈로그 조회와 방 설정에 쓰는 원래 테마 식별자는 바꾸지 않는다.
    MW_API FText GetMapThemeDisplayName(FName ThemeId);
}

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
