#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "UI/MWTitleRootWidget.h"
#include "UCMTitlePreview.h"
#include "MWUCMTitleFlow.generated.h"

class AMWUCMTitleFlow;
class ACameraActor;
class UMWVehicleSelectionWidget;
class AMWVehicleSelectionPreviewActor;
class SBorder;
class UGameViewportClient;
class UMWVehiclePlayerMarkerWidget;
struct FWorldContext;
enum class EMWSessionState : uint8;

USTRUCT(BlueprintType)
struct FMWUCMWorldRoute
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Source;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FUCMSourceCameraSample> Forward;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FUCMSourceCameraSample> Reverse;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FTransform WidgetPose;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float VerticalFOV = 45;
};

/** Local presentation lifetime only. Authentication and sessions remain in MWMultiplayerSubsystem. */
UCLASS(Config=Game)
class MWUCMFRONTEND_API UMWUCMIntroSession : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    bool bIntroStarted = false;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    void SetRoomPresentationWorld(UWorld* World);
    void RevealReturnedWorld(APlayerController* PC, AActor* ViewTarget, UUserWidget* Root);
    bool FinishReturnPresentation(UWorld* World);
    bool IsRoomReturnCovered() const { return ReturnCoverPhase != EReturnCoverPhase::Hidden; }
    bool IsReturnWorldReady(const UWorld* World) const;
    void SetPreparationFailure(const UWorld* World, const TCHAR* Reason);
    void InstallFrontendCursor();
private:
    UPROPERTY(Config) TSoftClassPtr<UMWVehiclePlayerMarkerWidget> PointerWidgetClass;
    UPROPERTY(Transient) TObjectPtr<UMWVehiclePlayerMarkerWidget> FrontendCursor;
    TSharedPtr<SWidget> FrontendCursorSlate;
    TArray<TSharedPtr<SWidget>> PreviousCursors;
    TWeakObjectPtr<UGameViewportClient> CursorViewport;
    bool bPreviousSoftwareCursors = false;
    UFUNCTION() void HandleRoomPresentationState(EMWSessionState PreviousState, EMWSessionState NewState);
    UFUNCTION() void HandleRoomReturnError(const FString& Error);
    bool BeginRoomReturnCover(UWorld* World);
    bool TickReturnCover(float DeltaTime);
    bool DeferRoomReturnTravel(FName Map, FSimpleDelegate Travel);
    void CoverPainted();
    void CoverMapBoundary(const FWorldContext& Context, const FString& MapName);
    void OnReturnMapLoaded(UWorld* World);
    void EnsureReturnCover();
    void ShowReturnStatus(const FText& Message, bool bRetry);
    void ClearReturnStatus();
    void FailReturn(const TCHAR* Reason, const FText& Message, bool bRetry = false);
    FReply RetryReturn();
    FReply ExitAfterReturnFailure();
    bool IsReadyViewCurrent() const;
    void SetReturnCoverAlpha(float Alpha);
    void RemoveReturnCover();
    TSharedPtr<SBorder> ReturnCover;
    TWeakObjectPtr<UGameViewportClient> CoverViewport;
    TWeakObjectPtr<UWorld> RoomPresentationWorld;
    TWeakObjectPtr<UWorld> ReturnSourceWorld;
    TWeakObjectPtr<UWorld> ReturnTargetWorld;
    TWeakObjectPtr<APlayerController> ReturnController;
    TWeakObjectPtr<AActor> ReturnViewTarget;
    TWeakObjectPtr<UUserWidget> ReturnMenuRoot;
    FName ExpectedReturnMap;
    FString PreparationFailure;
    FText StatusMessage;
    bool bStatusRetry = false;
    FSimpleDelegate PendingReturnTravel;
    FTSTicker::FDelegateHandle CoverTickHandle;
    FDelegateHandle PreLoadMapHandle;
    FDelegateHandle PostLoadMapHandle;
    double CoverChangedAt = 0;
    uint64 OpaquePaintFrame = MAX_uint64;
    float CoverAlpha = 0.f;
    float RevealFromAlpha = 1.f;
    enum class EReturnCoverPhase : uint8 { Hidden, FadingOut, WaitingForPaint, WaitingForSession, Traveling, WorldReady, FadingIn, AwaitingInput, Failed };
    void SetReturnPhase(EReturnCoverPhase Phase);
    EReturnCoverPhase ReturnCoverPhase = EReturnCoverPhase::Hidden;
};

/** Reuses the existing WorldDev widget tree and all of its room/login handlers. */
UCLASS(Abstract)
class MWUCMFRONTEND_API UMWUCMTitleRootWidget : public UMWTitleRootWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(Transient) TObjectPtr<AMWUCMTitleFlow> Flow;
    UPROPERTY(EditDefaultsOnly, Category="MW Integration") TSubclassOf<UUserWidget> MainMenuWidgetClass;
    void ActivateSourceMenu(int32 Index);
protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual bool CanPresentEntryMenu(bool bLoggedIn) override;
    virtual void OnEntryPresentationUpdated(bool bLoggedIn, bool bMenuRequested) override;
    virtual UWidget* ResolveEntryPresentation(UWidget* Screen, bool bMenuRequested) override;
private:
    UFUNCTION() void HandleQuit();
};

/** Owns the connection to UCM's existing sequence and camera state machine, never room/session state. */
UCLASS(Blueprintable)
class MWUCMFRONTEND_API AMWUCMTitleFlow : public AUCMTitleFlowPreview
{
    GENERATED_BODY()
public:
    AMWUCMTitleFlow();
    UPROPERTY(EditAnywhere, Category="MW Integration") TSubclassOf<UMWUCMTitleRootWidget> RootWidgetClass;
    bool IsMenuReady(bool bLoggedIn);
    void ApplyEntryPresentation(bool bLoggedIn, bool bMenuRequested);
    UPROPERTY(EditAnywhere, Category="MW Integration") TArray<FMWUCMWorldRoute> WorldRoutes;
    UPROPERTY(EditAnywhere, Category="MW Integration") TSubclassOf<UMWVehicleSelectionWidget> VehicleWidgetClass;
    UPROPERTY(EditInstanceOnly, Category="MW Integration") TObjectPtr<AMWVehicleSelectionPreviewActor> VehiclePreviewActor;
    UPROPERTY(VisibleAnywhere, Category="MW Integration") TObjectPtr<UUCMWorldWidgetComponent> MWWorldSurface;
    UPROPERTY(BlueprintReadOnly, Category="MW Integration") FName ActiveRegion = TEXT("Main");
    UPROPERTY(BlueprintReadOnly, Category="MW Integration") bool bWorldRouteActive = false;
    void PresentEntryWidget(UWidget* Screen, bool bMenuRequested);
    // Presentation entry point for the existing vehicle UI. No readiness, selection RPC or phase approval is invented.
    UFUNCTION(BlueprintCallable, Category="MW Integration") bool ShowVehicleSelection();
    UFUNCTION(BlueprintCallable, Category="MW Integration") void CloseVehicleSelection();
protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY(Transient) TObjectPtr<UMWUCMTitleRootWidget> RootWidget;
    FDelegateHandle PresentationHandle;
    FDelegateHandle MenuActionHandle;
    FTimerHandle InitializeHandle;
    void InitializeRoot();
    bool bStarted = false;
    bool bRefreshing = false;
    bool bMainPresentationRequested = false;
    float ReturnCameraStartedAt = 0.f;
    void RefreshRoot();
    void HandleSourceMenu(FName Action);
    UPROPERTY(Transient) TObjectPtr<ACameraActor> WorldRouteCamera;
    UPROPERTY(Transient) TObjectPtr<UUserWidget> PresentedWidget;
    UPROPERTY(Transient) TObjectPtr<UUserWidget> LobbyWidget;
    UPROPERTY(Transient) TObjectPtr<UMWVehicleSelectionWidget> VehicleWidget;
    FName RunningRoute;
    bool bRouteReverse = false;
    bool bWorldRoutePresentationReady = false;
    bool bWorldRouteBlendCompleteLogged = false;
    float WorldRouteTime = 0;
    FTransform WorldRouteOutgoing;
    float WorldRouteOutgoingFOV = 90;
    const FMWUCMWorldRoute* FindRoute(FName Name) const;
    bool StartWorldRoute(FName Name, bool bReverse, FName Destination);
    void UpdateWorldRoute(float DeltaTime);
    void SetWorldWidget(UUserWidget* Widget, FName Anchor);
    void UseWorldMouse();
    void StartFidelity();
    void UpdateLobbyPresentation();
};
