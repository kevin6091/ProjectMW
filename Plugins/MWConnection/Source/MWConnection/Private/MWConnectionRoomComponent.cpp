#include "MWConnectionRoomComponent.h"
#include "MWConnectionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

UMWConnectionRoomComponent::UMWConnectionRoomComponent()
{
    SetIsReplicatedByDefault(true);
}

void UMWConnectionRoomComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UMWConnectionRoomComponent, SessionId);
    DOREPLIFETIME(UMWConnectionRoomComponent, RoomName);
    DOREPLIFETIME(UMWConnectionRoomComponent, MaxPlayers);
    DOREPLIFETIME(UMWConnectionRoomComponent, Properties);
    DOREPLIFETIME(UMWConnectionRoomComponent, RegisteredUsers);
}

void UMWConnectionRoomComponent::SetRoom(const FString& Session, const FMWConnectionRoomOptions& Options, const TArray<FString>& Users)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    SessionId = Session;
    RoomName = Options.RoomName;
    MaxPlayers = Options.MaxPlayers;
    Properties.Reset();
    for (const auto& Item : Options.Properties)
    {
        FMWConnectionProperty& Entry = Properties.AddDefaulted_GetRef();
        Entry.Key = Item.Key;
        Entry.Value = Item.Value;
    }
    RegisteredUsers = Users;
    GetOwner()->ForceNetUpdate();
}

FMWConnectionRoomOptions UMWConnectionRoomComponent::GetRoom() const
{
    FMWConnectionRoomOptions Result;
    Result.RoomName = RoomName;
    Result.MaxPlayers = MaxPlayers;
    for (const auto& Item : Properties) Result.Properties.Add(Item.Key, Item.Value);
    return Result;
}

void UMWConnectionRoomComponent::OnRep_Info()
{
    if (UWorld* World = GetWorld())
        if (UGameInstance* GI = World->GetGameInstance())
            if (UMWConnectionSubsystem* Service = GI->GetSubsystem<UMWConnectionSubsystem>())
                Service->HandleRoomInfoChanged();
}
