#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MWFrontendConnection.generated.h"

class AGameStateBase;
class APlayerState;

UENUM(BlueprintType)
enum class EMWFrontendOperation : uint8
{
	Idle,
	Creating,
	Searching,
	Joining,
	Leaving,
	SettingReady,
	Starting
};

USTRUCT(BlueprintType)
struct MW_API FMWFrontendTheme
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
};

USTRUCT(BlueprintType)
struct MW_API FMWCreateRoomRequest
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString RoomName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MaxPlayers = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName MapThemeId;

	bool Validate(const TArray<FMWFrontendTheme>& Themes, FText& OutError) const;
};

USTRUCT(BlueprintType)
struct MW_API FMWRoomSearchResult
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString SessionId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText RoomName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText ThemeName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 PlayerCount = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MaxPlayers = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bJoinable = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText UnavailableReason;

	bool IsJoinable() const;
};

/** Display projection of the team's replicated PlayerState, never a second Ready authority. */
USTRUCT(BlueprintType)
struct MW_API FMWLobbyParticipantView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite) TObjectPtr<APlayerState> PlayerState = nullptr;
	UPROPERTY(BlueprintReadWrite) bool bReady = false;
	UPROPERTY(BlueprintReadWrite) bool bHost = false;
};

USTRUCT(BlueprintType)
struct MW_API FMWLobbyView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite) TObjectPtr<AGameStateBase> GameState = nullptr;
	UPROPERTY(BlueprintReadWrite) TObjectPtr<APlayerState> LocalPlayerState = nullptr;
	UPROPERTY(BlueprintReadWrite) FString SessionId;
	UPROPERTY(BlueprintReadWrite) FText RoomName;
	UPROPERTY(BlueprintReadWrite) FText ThemeName;
	UPROPERTY(BlueprintReadWrite) int32 MaxPlayers = 0;
	UPROPERTY(BlueprintReadWrite) bool bLobbyPhase = false;
	UPROPERTY(BlueprintReadWrite) bool bConnectionReady = false;
	UPROPERTY(BlueprintReadWrite) bool bCanStart = false;
	UPROPERTY(BlueprintReadWrite) TArray<FMWLobbyParticipantView> Participants;

	bool IsReadyFor(const UWorld* World, const APlayerState* OwningPlayerState) const;
	bool IsConnectedTo(const UWorld* World, const APlayerState* OwningPlayerState) const;
	const FMWLobbyParticipantView* FindLocalParticipant() const;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMWFrontendConnectionChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMWFrontendConnectionError, const FText&, Message);

/**
 * Integration seam until the session owner's public API exists. The implementation forwards to
 * that service and the owning Controller; it owns no duplicate session request IDs or replication.
 * No instance is installed automatically and the default implementation cannot report success.
 */
UCLASS(Blueprintable, Abstract)
class MW_API UMWFrontendConnection : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category="MW|Frontend") EMWFrontendOperation GetOperation() const;
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category="MW|Frontend") TArray<FMWFrontendTheme> GetThemes() const;
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category="MW|Frontend") TArray<FMWRoomSearchResult> GetRooms() const;
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category="MW|Frontend") FMWLobbyView GetLobby() const;
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MW|Frontend") bool CreateRoom(const FMWCreateRoomRequest& Request);
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MW|Frontend") bool SearchRooms();
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MW|Frontend") bool JoinRoom(const FString& SessionId);
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MW|Frontend") bool LeaveRoom();
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MW|Frontend") bool SetReady(bool bReady);
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MW|Frontend") bool StartMatch();
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MW|Frontend") bool CancelOperation();

	UPROPERTY(BlueprintAssignable, Category="MW|Frontend") FMWFrontendConnectionChanged OnChanged;
	UPROPERTY(BlueprintAssignable, Category="MW|Frontend") FMWFrontendConnectionError OnError;
};
