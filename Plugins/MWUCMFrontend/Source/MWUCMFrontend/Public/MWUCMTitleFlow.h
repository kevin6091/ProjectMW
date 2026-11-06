#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "UI/MWTitleRootWidget.h"
#include "UI/MWLobbyPresentation.h"
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
    bool GetReturnView(const UWorld* World, FTransform& Pose, float& FOV) const;
    bool FinishReturnPresentation(APlayerController* PC, AActor* ViewTarget, UUserWidget* Root);
    bool IsRoomReturnPending() const { return ReturnCoverPhase != EReturnCoverPhase::Hidden; }
    bool IsReturnWorldReady(const UWorld* World) const;
    bool ShouldReturnToRoomBrowser() const { return bReturnToRoomBrowser; }
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
    void BeginRoomReturn(UWorld* World, bool bWasParticipant);
    bool TickReturnCover(float DeltaTime);
    void CoverMapBoundary(const FWorldContext& Context, const FString& MapName);
    void OnReturnMapLoaded(UWorld* World);
    void EnsureReturnCover();
    void ShowReturnStatus(const FText& Message, bool bRetry);
    void ClearReturnStatus();
    void FailReturn(const TCHAR* Reason, const FText& Message, bool bRetry = false);
    FReply RetryReturn();
    FReply ExitAfterReturnFailure();
    void RemoveReturnCover();
    TSharedPtr<SBorder> ReturnCover;
    TWeakObjectPtr<UGameViewportClient> CoverViewport;
    TWeakObjectPtr<UWorld> RoomPresentationWorld;
    TWeakObjectPtr<UWorld> ReturnSourceWorld;
    TWeakObjectPtr<UWorld> ReturnTargetWorld;
    FTransform ReturnViewPose;
    float ReturnViewFOV = 0.f;
    bool bReturnToRoomBrowser = false;
    FName ExpectedReturnMap;
    FString PreparationFailure;
    FText StatusMessage;
    bool bStatusRetry = false;
    FTSTicker::FDelegateHandle CoverTickHandle;
    FDelegateHandle PreLoadMapHandle;
    FDelegateHandle PostLoadMapHandle;
    double CoverChangedAt = 0;
    enum class EReturnCoverPhase : uint8 { Hidden, WaitingForSession, Traveling, WorldReady, Failed };
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
    void RestoreCreateRoomForm() { OpenCreateRoomForm(); }
    void RestoreRoomBrowser() { OpenRoomBrowser(); }
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
class MWUCMFRONTEND_API AMWUCMTitleFlow : public AUCMTitleFlowPreview, public IMWLobbyPresentation
{
    GENERATED_BODY()
public:
    AMWUCMTitleFlow();
    UPROPERTY(EditAnywhere, Category="MW Integration") TSubclassOf<UMWUCMTitleRootWidget> RootWidgetClass;
    bool IsMenuReady(bool bLoggedIn);
    UFUNCTION(BlueprintCallable, Category="MW Integration") bool SkipIntro();
    void ApplyEntryPresentation(bool bLoggedIn, bool bMenuRequested);
    UPROPERTY(EditAnywhere, Category="MW Integration") TArray<FMWUCMWorldRoute> WorldRoutes;
    UPROPERTY(EditAnywhere, Category="MW Integration") TSubclassOf<UMWVehicleSelectionWidget> VehicleWidgetClass;
    UPROPERTY(EditInstanceOnly, Category="MW Integration") TObjectPtr<AMWVehicleSelectionPreviewActor> VehiclePreviewActor;
    UPROPERTY(VisibleAnywhere, Category="MW Integration") TObjectPtr<UUCMWorldWidgetComponent> MWWorldSurface;
    UPROPERTY(BlueprintReadOnly, Category="MW Integration") FName ActiveRegion = TEXT("Main");
    UPROPERTY(BlueprintReadOnly, Category="MW Integration") bool bWorldRouteActive = false;
    void PresentEntryWidget(UWidget* Screen, bool bMenuRequested);
    bool IsEntryPanelReady() const { return ActiveRegion != TEXT("RoomPanel") || bWorldRoutePresentationReady || !bWorldRouteActive; }
    virtual void SetLobbyWidgets(UMWRoomLobbyWidget* Lobby, UMWVehicleSelectionWidget* Selection) override;
    virtual UWidgetComponent* GetVehicleSelectionSurface() const override;
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
    TSharedPtr<SWidget> IntroSkipOverlay;
    uint64 IntroSkippedFrame = MAX_uint64;
    void ShowIntroSkip();
    void HideIntroSkip();
    bool bStarted = false;
    bool bRefreshing = false;
    bool StartRoomReturn(const UMWUCMIntroSession& Session);
    void RefreshRoot();
    void HandleSourceMenu(FName Action);
    UPROPERTY(Transient) TObjectPtr<ACameraActor> WorldRouteCamera;
    UPROPERTY(Transient) TObjectPtr<UUserWidget> PresentedWidget;
    UPROPERTY(Transient) TObjectPtr<UMWRoomLobbyWidget> LobbyWidget;
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
