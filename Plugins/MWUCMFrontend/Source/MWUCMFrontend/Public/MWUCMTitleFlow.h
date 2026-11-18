#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "UI/Title/MWTitleRootWidget.h"
#include "UI/Lobby/MWLobbyPresentation.h"
#include "UCMTitlePreview.h"
#include "MWUCMTitleFlow.generated.h"

class AMWUCMTitleFlow;
class ACameraActor;
class UMWVehicleSelectionWidget;
class AMWVehicleSelectionPreviewActor;
class SBorder;
class UButton;
class UGameViewportClient;
class UMWVehiclePlayerMarkerWidget;
struct FWorldContext;
enum class EMWSessionState : uint8;

// 원본에서 복구한 카메라 샘플과 해당 화면의 월드 배치를 함께 보관한다.
USTRUCT(BlueprintType)
struct FMWUCMWorldRoute
{
    GENERATED_BODY()
    
    // 경로를 찾을 때 쓰는 키다. 로비·차량 선택 등 화면 이동 요청과 대응한다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) 
    FName Name;
    
    // 원본 자료의 출처를 기록한다. 경로 재생 로그에서 복구 근거를 추적할 때 사용한다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) 
    FString Source;
    
    // 화면으로 진입할 때 재생하는 정방향 카메라 샘플이다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) 
    TArray<FUCMSourceCameraSample> Forward;
    
    // 이전 화면으로 돌아올 때 재생하는 역방향 카메라 샘플이다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) 
    TArray<FUCMSourceCameraSample> Reverse;
    
    // 해당 경로에 연결된 위젯 표면을 놓을 월드 위치·회전·크기다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) 
    FTransform WidgetPose;
    
    // 원본 수직 시야각이다. 재생할 때 화면 비율에 맞는 수평 시야각으로 변환한다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) 
    float VerticalFOV = 45;
};

/**
 * 맵이 바뀌어도 인트로 재생 이력과 메뉴 복귀 표시 상태를 유지한다.
 * 인증·세션 처리는 기존 MWGameInstance와 MWConnection에 맡기고 화면 준비만 확인한다.
 */
UCLASS(Config=Game)
class MWUCMFRONTEND_API UMWUCMIntroSession : public UGameInstanceSubsystem
{
    GENERATED_BODY()
    
public:
    // 게임 실행 중 최초 인트로를 시작한 이력이다. 맵을 다시 열어도 반복 재생하지 않게 한다.
    bool bIntroStarted = false;
    
    // 기존 세션 상태와 맵 로드 이벤트를 구독하고 복귀 상태 점검을 등록한다.
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    
    // 게임 인스턴스 종료 시 구독·가림막을 해제하고 교체 전 커서 설정을 복원한다.
    virtual void Deinitialize() override;
    
    // 복귀를 시작할 수 있는 실제 로비 월드를 기록한다. 같은 게임 인스턴스인지 확인한다.
    void SetRoomPresentationWorld(UWorld* World);
    
    // 새 복귀 월드가 준비됐을 때만 저장한 로비 카메라 위치와 시야각을 전달한다.
    bool GetReturnView(const UWorld* World, FTransform& Pose, float& FOV) const;
    
    // 현재 월드·로컬 컨트롤러·실제 시점·루트 위젯을 확인한 뒤 복귀 가림막을 제거한다.
    bool FinishReturnPresentation(APlayerController* PC, AActor* ViewTarget, UUserWidget* Root);
    
    // 실패 안내 상태도 포함해 복귀 처리가 남아 있는지 확인한다.
    bool IsRoomReturnPending() const
    {
        return ReturnCoverPhase != EReturnCoverPhase::Hidden;
    }
    
    // 예상한 새 월드가 로드됐는지 확인한다. 메뉴 표시 준비까지 끝났다는 뜻은 아니다.
    bool IsReturnWorldReady(const UWorld* World) const;
    
    // 참가자로 나왔다면 방 검색 화면으로 복귀할지 표시 쪽에서 판단할 수 있게 한다.
    bool ShouldReturnToRoomBrowser() const
    {
        return bReturnToRoomBrowser;
    }
    
    // 카메라나 위젯 준비가 막힌 이유를 기록한다. 준비되지 않은 화면을 강제로 노출하지 않는다.
    void SetPreparationFailure(const UWorld* World, const TCHAR* Reason);
    
    // 차량 선택과 같은 커서 외형을 프런트엔드에 설치하고 기존 설정은 종료 시 복구한다.
    void InstallFrontendCursor();
    
private:
    // 설정 파일에서 지정한 커서 클래스다. 설치 시 필요한 에셋을 불러온다.
    UPROPERTY(Config) 
    TSoftClassPtr<UMWVehiclePlayerMarkerWidget> PointerWidgetClass;
    
    // 맵 이동 중에도 유지할 커서 위젯을 UE가 추적하는 참조로 보관한다.
    UPROPERTY(Transient) 
    TObjectPtr<UMWVehiclePlayerMarkerWidget> FrontendCursor;
    
    TSharedPtr<SWidget> FrontendCursorSlate;
    
    // 기존 소프트웨어 커서를 보관해 다른 화면의 설정을 영구히 덮어쓰지 않게 한다.
    TArray<TSharedPtr<SWidget>> PreviousCursors;
    
    TWeakObjectPtr<UGameViewportClient> CursorViewport;
    
    bool bPreviousSoftwareCursors = false;
    
    // 기존 연결 상태가 방 종료로 바뀌면 복귀 표시를 시작한다. 세션 종료 자체를 구현하지 않는다.
    UFUNCTION() 
    void HandleRoomPresentationState(EMWSessionState PreviousState, EMWSessionState NewState);
    
    // 기존 연결 서비스의 정리 오류를 현재 복구 안내에 반영한다.
    // 기존 연결 상태가 방 종료로 바뀌면 복귀 표시를 시작한다. 세션 종료 자체를 구현하지 않는다.
    UFUNCTION() 
    void HandleRoomReturnError(const FString& Error);
    
    // 복귀 출발 월드를 검증하고 진행 중인 중복 요청은 다시 시작하지 않는다.
    void BeginRoomReturn(UWorld* World, bool bWasParticipant);
    
    // 엔진 틱에서 응답·이동·표시 준비 지연을 확인한다. 게임 스레드 정지를 대신 처리하는 기능은 아니다.
    bool TickReturnCover(float DeltaTime);
    
    // 이 게임 인스턴스의 이동 시작만 받아 예상 맵과 마지막 로비 시점을 기록한다.
    void CoverMapBoundary(const FWorldContext& Context, const FString& MapName);
    
    // 로드된 월드가 이전 월드와 다르고 예상 맵인지 확인한 뒤 준비 상태로 넘긴다.
    void OnReturnMapLoaded(UWorld* World);
    
    // 현재 게임 인스턴스의 Viewport에 가림막을 한 번만 연결한다.
    void EnsureReturnCover();
    
    // 실패·대기 사유와 가능한 재시도·종료 동작을 표시하고 키보드 포커스를 안내로 옮긴다.
    void ShowReturnStatus(const FText& Message, bool bRetry);
    
    // 기존 안내 내용을 지우고 필요하지 않은 상태 패널을 숨긴다.
    void ClearReturnStatus();
    
    // 실패 이유를 남기고 재시도 가능 여부에 맞춰 안내를 표시한다.
    void FailReturn(const TCHAR* Reason, const FText& Message, bool bRetry = false);
    
    // 이전 로비 월드가 아직 유효할 때만 기존 세션 정리 요청을 다시 보낸다.
    FReply RetryReturn();
    
    // 현재 유효한 로컬 컨트롤러 또는 Viewport를 통해 명시적으로 실행을 종료한다.
    FReply ExitAfterReturnFailure();
    
    // 화면 가림막과 이전·새 월드 참조를 해제하고 커서 표시를 복구한다.
    void RemoveReturnCover();
    
    TSharedPtr<SBorder> ReturnCover;
    
    TWeakObjectPtr<UGameViewportClient> CoverViewport;
    
    // 아래 월드들은 맵 교체로 사라질 수 있으므로 약한 참조로 보관하고 사용 전에 검증한다.
    TWeakObjectPtr<UWorld> RoomPresentationWorld;
    
    TWeakObjectPtr<UWorld> ReturnSourceWorld;
    
    TWeakObjectPtr<UWorld> ReturnTargetWorld;
    
    // 복귀 카메라가 갑자기 튀지 않도록 마지막으로 보이던 로비 시점을 보관한다.
    FTransform ReturnViewPose;
    
    float ReturnViewFOV = 0.f;
    
    bool bReturnToRoomBrowser = false;
    
    // 같은 이름의 다른 월드나 예상하지 않은 맵을 복귀 성공으로 처리하지 않기 위한 경로다.
    FName ExpectedReturnMap;
    
    // 현재 준비를 막는 사유와 사용자 안내 상태다. 실패 로그와 복구 화면에 사용한다.
    FString PreparationFailure;
    
    FText StatusMessage;
    
    bool bStatusRetry = false;
    
    // 종료 시 등록을 해제할 틱·맵 이벤트 손잡이들이다.
    FTSTicker::FDelegateHandle CoverTickHandle;
    
    FDelegateHandle PreLoadMapHandle;
    
    FDelegateHandle PostLoadMapHandle;
    
    // 월드 시간이 아닌 실제 경과 시간으로 현재 복귀 단계의 지연을 확인한다.
    double CoverChangedAt = 0;
    
    // 월드 로드 완료와 카메라·메뉴 준비 완료를 구분한다. 시간 초과만으로 성공 처리하지 않는다.
    enum class EReturnCoverPhase : uint8
    {
        Hidden,             // 복귀 처리가 없는 평상시 상태.
        WaitingForSession,  // 기존 연결 서비스의 방 종료 응답과 이동 시작을 기다린다.
        Traveling,          // 예상 타이틀 맵이 새 월드로 로드되기를 기다린다.
        WorldReady,         // 맵 로드는 끝났고 실제 카메라와 메뉴의 준비를 확인한다.
        Failed              // 실패 안내와 가능한 복구 동작을 표시한다.
    };
    
    // 복귀 단계가 바뀐 이유를 추적할 수 있도록 이전·다음 상태와 실제 시간을 기록한다.
    void SetReturnPhase(EReturnCoverPhase Phase);
    
    EReturnCoverPhase ReturnCoverPhase = EReturnCoverPhase::Hidden;
};

/**
 * 기존 WorldDev 위젯의 로그인·방 생성·검색 기능을 그대로 사용한다.
 * 표시 시점과 메뉴 선택만 UCM 연출 액터에 연결하는 루트 위젯이다.
 */
UCLASS(Abstract)
class MWUCMFRONTEND_API UMWUCMTitleRootWidget : public UMWTitleRootWidget
{
    GENERATED_BODY()
    
public:
    // 이 루트 위젯의 표시와 카메라를 담당하는 현재 월드의 연출 액터다.
    UPROPERTY(Transient) 
    TObjectPtr<AMWUCMTitleFlow> Flow;
    
    // 에디터에서 지정하는 메인 메뉴 위젯 클래스 설정이다.
    UPROPERTY(EditDefaultsOnly, Category="MW Integration") 
    TSubclassOf<UUserWidget> MainMenuWidgetClass;
    
    // UCM 표지판 선택을 기존 MW의 방 생성·검색·종료 버튼 처리로 전달한다.
    void ActivateSourceMenu(int32 Index);
    
    // 복귀 후 기존 방 생성 폼을 다시 연다. 폼 기능을 별도로 재구현하지 않는다.
    void RestoreCreateRoomForm()
    {
        OpenCreateRoomForm();
    }
    
    // 참가자 복귀에 기존 방 검색 화면을 재사용한다.
    void RestoreRoomBrowser()
    {
        OpenRoomBrowser();
    }
    
protected:
    // 기존 루트의 생성 수명을 그대로 따르고 화면 기능을 중복 생성하지 않는다.
    virtual void NativeConstruct() override;
    
    // 기존 루트가 등록한 위젯·요청 연결의 종료 처리를 유지한다.
    virtual void NativeDestruct() override;
    
    // MW 메인 화면의 중복 표시를 막고 실제 표지판 표시는 UCM 연출에 맡긴다.
    virtual bool CanPresentEntryMenu(bool bLoggedIn) override;
    
    // 기존 루트가 고른 로그인·메뉴 상태를 현재 월드 연출에 전달한다.
    virtual void OnEntryPresentationUpdated(bool bLoggedIn, bool bMenuRequested) override;
    
    // 카메라 이동 중에는 대체 영역을 사용하고 준비된 기존 화면을 표시한다.
    virtual UWidget* ResolveEntryPresentation(UWidget* Screen, bool bMenuRequested) override;
    
private:
    // 기존 소유 플레이어의 정상 게임 종료 경로를 사용한다.
    UFUNCTION()
    void HandleQuit();
};

/**
 * 기존 MW 화면을 UCM 인트로·타이틀·카메라 경로와 연결한다.
 * 위젯의 월드 배치와 입력·연출 시점을 맡으며 방 상태나 서버 선택 결과는 결정하지 않는다.
 */
UCLASS(Blueprintable)
class MWUCMFRONTEND_API AMWUCMTitleFlow : public AUCMTitleFlowPreview, public IMWLobbyPresentation
{
    GENERATED_BODY()
    
public:
    AMWUCMTitleFlow();
    
    // 레벨에서 사용할 기존 MW 연동 루트 클래스다. 지정하지 않으면 루트를 만들 수 없다.
    UPROPERTY(EditAnywhere, Category="MW Integration")
    TSubclassOf<UMWUCMTitleRootWidget> RootWidgetClass;
    
    // 로그인 이후 인트로를 최초 한 번 시작하거나 복귀 경로를 선택하고 메뉴 준비 여부를 반환한다.
    bool IsMenuReady(bool bLoggedIn);
    
    // 실제 인트로가 재생 중일 때만 넘긴다. 스킵 입력이 같은 프레임의 메뉴 실행으로 이어지지 않게 한다.
    UFUNCTION(BlueprintCallable, Category="MW Integration")
    bool SkipIntro();
    
    // 로그인·현재 영역·복귀 상태에 맞춰 메뉴 표시, 입력 모드, 음악 활성 상태를 갱신한다.
    void ApplyEntryPresentation(bool bLoggedIn, bool bMenuRequested);
    
    // 복구한 원본 경로 목록이다. 화면별 이동은 이 데이터의 이름으로 찾는다.
    UPROPERTY(EditAnywhere, Category="MW Integration") 
    TArray<FMWUCMWorldRoute> WorldRoutes;
    
    // 기존 차량 선택 위젯 클래스다. 선택 확정과 취소 요청 처리는 기존 HUD·서버가 담당한다.
    UPROPERTY(EditAnywhere, Category="MW Integration") 
    TSubclassOf<UMWVehicleSelectionWidget> VehicleWidgetClass;
    
    // 이 레벨에 배치된 차량 선택 게시판이다. 이름에 Preview가 있어도 실제 표시 표면에 사용한다.
    UPROPERTY(EditInstanceOnly, Category="MW Integration") 
    TObjectPtr<AMWVehicleSelectionPreviewActor> VehiclePreviewActor;
    
    // 로비 등 기존 MW 위젯을 월드에 표시하는 표면이다.
    UPROPERTY(VisibleAnywhere, Category="MW Integration") 
    TObjectPtr<UUCMWorldWidgetComponent> MWWorldSurface;
    
    // 카메라 이동의 목적 영역을 기록한다. 이름이 바뀌었다고 이동이 끝난 것은 아니다.
    UPROPERTY(BlueprintReadOnly, Category="MW Integration") 
    // 카메라 이동의 목적 영역을 기록한다. 이름이 바뀌었다고 이동이 끝난 것은 아니다.
    FName ActiveRegion = TEXT("Main");
    
    // 현재 원본 경로를 재생 중인지 나타낸다.
    UPROPERTY(BlueprintReadOnly, Category="MW Integration") 
    bool bWorldRouteActive = false;
    
    // 기존 생성·검색 창을 유지하고 해당 배경의 원본 진입·복귀 카메라만 연결한다.
    void PresentEntryWidget(UWidget* Screen, bool bMenuRequested);
    
    // 생성·검색 폼은 표시가 허용될 때까지 기다린다. 경로 재생 종료와 표시 준비를 구분한다.
    bool IsEntryPanelReady() const
    {
        return ActiveRegion != TEXT("RoomPanel") || bWorldRoutePresentationReady || !bWorldRouteActive;
    }
    
    // HUD가 만든 기존 위젯을 받아 이 월드의 게시판과 카메라에 연결한다.
    virtual void SetLobbyWidgets(UMWRoomLobbyWidget* Lobby, UMWVehicleSelectionWidget* Selection) override;
    
    // 공유 커서가 현재 선택판의 실제 좌표를 계산할 수 있도록 표시 표면을 반환한다.
    virtual UWidgetComponent* GetVehicleSelectionSurface() const override;
    
    // 출발 경로를 시작하고 화면을 가린 뒤 완료를 알린다. 서버 이동 승인 자체는 하지 않는다.
    virtual bool BeginVehicleDeparture(FSimpleDelegate OnCovered) override;
    
    // 기존 차량 선택 UI의 표시만 담당한다. 선택 승인과 게임 단계 변경은 기존 HUD·서버 경로를 따른다.
    UFUNCTION(BlueprintCallable, Category="MW Integration") 
    bool ShowVehicleSelection();
    
    // 차량 게시판 표시와 관련 입력을 정리하고 기존 월드 화면으로 돌아간다.
    // 실제 인트로가 재생 중일 때만 넘긴다. 스킵 입력이 같은 프레임의 메뉴 실행으로 이어지지 않게 한다.
    UFUNCTION(BlueprintCallable, Category="MW Integration") 
    void CloseVehicleSelection();
    
protected:
    // 기존 연출과 MW 위젯 이벤트를 연결하고 로컬 컨트롤러 준비 뒤 루트를 초기화한다.
    virtual void BeginPlay() override;

    // 스킵 입력을 먼저 처리한 뒤 기존 경로·출발·로비 표시를 현재 단계에 맞춰 갱신한다.
    virtual void Tick(float DeltaTime) override;
    // 이 월드에서 만든 카메라·위젯·타이머·이벤트 구독을 종료한다.
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    
private:
    // 현재 월드에서 생성한 루트 위젯이다. 종료 때 화면과 Flow 연결을 해제한다.
    UPROPERTY(Transient)
    TObjectPtr<UMWUCMTitleRootWidget> RootWidget;
    
    FDelegateHandle PresentationHandle;
    
    FDelegateHandle MenuActionHandle;
    
    // 로컬 컨트롤러 준비를 기다리는 다음 틱 초기화 예약이다. 종료 시 취소한다.
    FTimerHandle InitializeHandle;
    
    // 현재 월드와 로컬 컨트롤러를 확인하고 Standalone 진입과 실제 네트워크 로비 표시를 구분한다.
    void InitializeRoot();
    
    TSharedPtr<SWidget> IntroSkipOverlay;
    
    // 스킵한 프레임을 기억해 같은 입력이 메뉴까지 실행하는 것을 막는다.
    uint64 IntroSkippedFrame = MAX_uint64;
    
    // 현재 인트로의 클릭 안내를 한 번만 붙이고 약한 액터 참조로 스킵을 전달한다.
    void ShowIntroSkip();
    
    // 이 Flow가 붙인 인트로 스킵 화면을 제거하고 Slate 참조를 해제한다.
    void HideIntroSkip();
    
    bool bStarted = false;
    
    // 표시 변경 알림이 다시 루트 갱신을 호출하는 재진입을 막는다.
    bool bRefreshing = false;
    
    // 복귀 세션이 저장한 시점에서 로비 역방향 경로를 시작한다.
    bool StartRoomReturn(const UMWUCMIntroSession& Session);
    
    // 표시 이벤트가 다시 루트 갱신을 부르는 재진입을 막고 기존 화면 선택을 갱신한다.
    void RefreshRoot();
    
    // 원본 메뉴 동작 이름을 기존 MW 버튼 인덱스로 연결한다.
    void HandleSourceMenu(FName Action);
    
    // 메뉴 가시성·입력 허용과 음악의 메인 메뉴 상태를 함께 갱신한다.
    void SetMainMenuPresentation(bool bVisible, bool bInputEnabled);
    
    // 입력이 허용된 메뉴 선택 변경에만 기존 강조 효과음을 재생한다.
    void OnSourceMenuSelectionChanged() override;
    
    // 경로 샘플로 움직이는 전용 카메라다. 레벨 종료 때 정리한다.
    UPROPERTY(Transient) 
    TObjectPtr<ACameraActor> WorldRouteCamera;
    
    // 현재 월드 표면에 연결한 기존 화면 참조들이다.
    UPROPERTY(Transient) 
    TObjectPtr<UUserWidget> PresentedWidget;
    
    // 레벨에서 사용할 기존 MW 연동 루트 클래스다. 지정하지 않으면 루트를 만들 수 없다.
    UPROPERTY(Transient) 
    TObjectPtr<UMWRoomLobbyWidget> LobbyWidget;
    
    // 레벨에서 사용할 기존 MW 연동 루트 클래스다. 지정하지 않으면 루트를 만들 수 없다.
    UPROPERTY(Transient) 
    TObjectPtr<UMWVehicleSelectionWidget> VehicleWidget;
    
    // 아래 값들은 현재 경로의 방향·재생 시간·표시 준비와 출발 시점을 보관한다.
    FName RunningRoute;
    
    bool bRouteReverse = false;
    
    // 전체 이동 종료와 별개로 메뉴 또는 패널을 보여 줘도 되는 상태다.
    bool bWorldRoutePresentationReady = false;
    
    bool bWorldRouteBlendCompleteLogged = false;
    
    float WorldRouteTime = 0;
    
    // 새 경로로 부드럽게 연결하기 위해 이동 직전 카메라 시점과 시야각을 보관한다.
    FTransform WorldRouteOutgoing;
    
    float WorldRouteOutgoingFOV = 90;
    
    // 원본 경로를 이름으로 조회한다. 반환 포인터는 WorldRoutes 내부 데이터를 가리킨다.
    const FMWUCMWorldRoute* FindRoute(FName Name) const;
    
    // 경로·카메라를 검증한 뒤 이동을 시작하고 준비 전 위젯 입력을 막는다.
    bool StartWorldRoute(FName Name, bool bReverse, FName Destination);
    
    // 원본 샘플 사이의 위치·회전·시야각을 보간하고 화면 표시 가능 시점을 갱신한다.
    void UpdateWorldRoute(float DeltaTime);
    
    // 기존 출발 경로의 화면 가림을 진행하고 준비 뒤 완료 콜백을 한 번 전달한다.
    void UpdateVehicleDeparture();
    
    // 출발 가림막과 완료 콜백을 해제해 이전 월드 요청이 남지 않게 한다.
    void ClearVehicleDeparture();
    
    // HUD가 소유 컨트롤러에 전달한 완료 요청이다. Flow가 직접 서버 이동을 실행하지 않는다.
    FSimpleDelegate OnVehicleDepartureCovered;
    
    TSharedPtr<SBorder> VehicleDepartureCover;
    
    TWeakObjectPtr<UGameViewportClient> VehicleDepartureViewport;
    
    bool bVehicleDepartureCovered = false;
    
    // 복구 경로의 위젯 배치 정보에 맞춰 기존 화면을 월드 표면에 붙인다.
    void SetWorldWidget(UUserWidget* Widget, FName Anchor);
    
    // 월드 UI에서 커서를 보이게 하고 게임·UI 입력을 함께 받을 모드로 맞춘다.
    void UseWorldMouse();
    
    // 이 월드의 기존 타이틀 배경 동작과 교통 연출을 시작한다.
    void StartFidelity();
    
    // 현재 로비·차량 위젯과 연출 영역을 맞추고 호스트 종료 확인 연결을 유지한다.
    void UpdateLobbyPresentation();
    
    // 기존 호스트 종료 버튼 연결을 잠시 확인 창으로 바꾸고 원래 콜백은 보관한다.
    void BindHostLeaveConfirmation();
    
    // 확인 창 연결을 제거하고 보관한 기존 종료 버튼 콜백을 돌려놓는다.
    void UnbindHostLeaveConfirmation();
    
    // 호스트 종료 버튼이 유효할 때 확인 창을 한 번만 띄우고 이전 키보드 포커스를 보관한다.
    // 실제 인트로가 재생 중일 때만 넘긴다. 스킵 입력이 같은 프레임의 메뉴 실행으로 이어지지 않게 한다.
    UFUNCTION() 
    void ShowHostLeaveConfirmation();
    
    // 확인을 받은 경우에만 보관한 기존 방 종료 처리를 실행한다.
    void ConfirmHostLeave();
    
    // 확인 창을 제거하고 열기 전의 키보드 포커스를 복구한다.
    void CloseHostLeaveConfirmation();
    
    // 기존 로비 위젯이 소유한 종료 콜백이다. 세션 종료를 별도 구현하지 않기 위해 재사용한다.
    FScriptDelegate OriginalHostLeave;
    
    TWeakObjectPtr<UButton> HostLeaveButton;
    
    TWeakObjectPtr<UGameViewportClient> HostLeaveViewport;
    
    TSharedPtr<SWidget> HostLeaveConfirmation;
    
    // 닫힌 위젯을 살려 두지 않으면서 확인 창 이전 포커스로 돌아갈 수 있도록 보관한다.
    TWeakPtr<SWidget> PreviousHostLeaveFocus;
};
