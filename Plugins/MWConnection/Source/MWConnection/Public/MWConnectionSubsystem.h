#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/EngineBaseTypes.h"
#include "MWConnectionTypes.h"
#include "MWConnectionSubsystem.generated.h"

class FMWConnectionImpl;
class UMWConnectionRoomComponent;
class AGameModeBase;
class APlayerController;
class AController;
class UNetDriver;
struct FUniqueNetIdRepl;
struct FActorsInitializedParams;
struct MWCONNECTION_API FMWConnectionImplDeleter
{
    void operator()(FMWConnectionImpl* Pointer) const;
};

DECLARE_EVENT(UMWConnectionSubsystem, FMWConnectionSnapshotChanged);
DECLARE_EVENT_TwoParams(UMWConnectionSubsystem, FMWConnectionOperationCompleted, EMWConnectionAction, const FMWConnectionResult&);
DECLARE_EVENT_OneParam(UMWConnectionSubsystem, FMWConnectionWorldReady, UWorld*);

/** The sole connection entry point. No SDK or project gameplay types cross this header. */
UCLASS()
class MWCONNECTION_API UMWConnectionSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    UMWConnectionSubsystem();
    virtual ~UMWConnectionSubsystem() override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // False: rejected synchronously, no completion event. True: one deferred completion.
    bool RequestLogin(FText& OutError);
    bool RequestHostRoom(const FMWConnectionRoomOptions& Options, FText& OutError);
    bool RequestRefreshRooms(FText& OutError);
    bool RequestJoinRoom(const FString& RoomHandle, FText& OutError);
    bool RequestJoinAddress(const FString& Address, FText& OutError);
    bool RequestSetRoomJoinable(bool bJoinable, FText& OutError);
    bool RequestLeaveRoom(FText& OutError);
    bool CanExecute(EMWConnectionAction Action) const;
    FMWConnectionSnapshot GetSnapshot() const;

    FMWConnectionSnapshotChanged& OnSnapshotChanged() { return SnapshotChanged; }
    FMWConnectionOperationCompleted& OnOperationCompleted() { return OperationCompleted; }
    FMWConnectionWorldReady& OnLocalWorldReady() { return LocalWorldReady; }

private:
    friend class FMWConnectionImpl;
    friend class UMWConnectionRoomComponent;
    TUniquePtr<FMWConnectionImpl, FMWConnectionImplDeleter> Impl;
    FMWConnectionSnapshotChanged SnapshotChanged;
    FMWConnectionOperationCompleted OperationCompleted;
    FMWConnectionWorldReady LocalWorldReady;
    void EmitSnapshot();
    void EmitWorld(UWorld* World);
    void QueueResult(EMWConnectionAction Action, const FMWConnectionResult& Result);
    void HandleMapLoaded(UWorld* World);
    void HandleActorsInitialized(const FActorsInitializedParams& Params);
    void HandleTravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Error);
    void HandleNetworkFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Error);
    void HandlePlayerEntered(AGameModeBase* Mode, APlayerController* Player);
    void HandlePlayerEntering(AGameModeBase* Mode, const FUniqueNetIdRepl& Player, FString& Error);
    void HandlePlayerExited(AGameModeBase* Mode, AController* Player);
    void HandleRoomInfoChanged();
};
