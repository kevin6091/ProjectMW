#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MWConnectionTypes.h"
#include "MWConnectionRoomComponent.generated.h"

USTRUCT()
struct FMWConnectionProperty
{
    GENERATED_BODY()
    UPROPERTY() FString Key;
    UPROPERTY() FString Value;
};

/** Infrastructure handshake attached to any AGameStateBase. No project class is required. */
UCLASS()
class UMWConnectionRoomComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UMWConnectionRoomComponent();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    void SetRoom(const FString& Session, const FMWConnectionRoomOptions& Options, const TArray<FString>& Users);
    FMWConnectionRoomOptions GetRoom() const;
    const FString& GetSessionId() const { return SessionId; }
    const TArray<FString>& GetRegisteredUsers() const { return RegisteredUsers; }
private:
    UPROPERTY(ReplicatedUsing=OnRep_Info) FString SessionId;
    UPROPERTY(ReplicatedUsing=OnRep_Info) FString RoomName;
    UPROPERTY(ReplicatedUsing=OnRep_Info) int32 MaxPlayers = 0;
    UPROPERTY(ReplicatedUsing=OnRep_Info) TArray<FMWConnectionProperty> Properties;
    UPROPERTY(ReplicatedUsing=OnRep_Info) TArray<FString> RegisteredUsers;
    UFUNCTION() void OnRep_Info();
};
