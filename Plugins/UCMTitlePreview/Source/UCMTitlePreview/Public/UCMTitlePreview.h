#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/WidgetComponent.h"
#include "Components/Button.h"
#include "GameFramework/Actor.h"
#include "UCMTitlePreview.generated.h"
class UFontFace;
class UCanvasPanel;
class ULevelSequence;
class ULevelSequencePlayer;
class ALevelSequenceActor;

// 원본 UI 요소의 좌표·색·글자·동작 ID를 보관한다. 실제 MW 기능을 중복 구현하지 않는다.
USTRUCT(BlueprintType)
struct FUCMSourceUIElement
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    FName ElementName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    FString SourceRef;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    TObjectPtr<UTexture2D> Texture;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    FString Text;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    FVector2D Position = FVector2D::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    FVector2D Size = FVector2D(100, 100);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    FLinearColor Color = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    float FontSize = 70;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    int32 FontOutlineSize = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    FVector2D TextShadowOffset = FVector2D::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    float Rotation = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    bool bEnabled = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    bool bButton = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    FName SourceActionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    FLinearColor HighlightColor = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    int32 SourceJustification = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    bool bSliced = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    float SourcePixelsPerUnit = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    FMargin SliceMargin;
    // 기존 타이틀 진행 안내에만 선택적으로 적용한다. 다른 원본 UI 표시는 유지한다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    bool bContinuePrompt = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FUCMPreviewActivated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUCMSourceAction, FName, Action);
class UUCMSourcedWidget;

// 원본 동작 ID를 소유 위젯의 기존 클릭·호버 알림으로 전달한다.
UCLASS()
class UCMTITLEPREVIEW_API UUCMSourceActionButton : public UButton
{
    GENERATED_BODY()
public:

    UPROPERTY()
    FName SourceAction;

    UPROPERTY()
    TObjectPtr<UUCMSourcedWidget> SourceOwner;

    // 원본 버튼 동작 ID를 소유 위젯의 기존 실행 경로로 전달한다.
    UFUNCTION()
    void HandleSourceClick();

    // 호버한 동작 ID를 전달해 키보드와 같은 메뉴 선택 상태를 사용한다.
    UFUNCTION()
    void HandleSourceHover();
};
/** 원본 매개변수로 UMG 캔버스를 구성한다. 글자 표현은 원본과 완전히 같다고 보장하지 않는 근사다. */
UCLASS(Blueprintable)
class UCMTITLEPREVIEW_API UUCMSourcedWidget : public UUserWidget
{
    GENERATED_BODY()
public:

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    TArray<FUCMSourceUIElement> Elements;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    bool bSourceUITintsSRGB = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    float SourceFontPixelsToPoints = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    FVector2D SourceCanvasSize = FVector2D(3000, 3000);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    TObjectPtr<UFontFace> SourceFontFace;

    UPROPERTY(BlueprintAssignable, Category = "UCM Preview")
    FUCMPreviewActivated Activated;

    // 기존 위젯의 실행 알림만 전달한다. 로그인이나 게임 시작을 직접 승인하지 않는다.
    UFUNCTION(BlueprintCallable, Category = "UCM Preview")
    void ActivatePreviewButton();

    UPROPERTY(BlueprintAssignable, Category = "UCM Source")
    FUCMSourceAction SourceActionActivated;

    UPROPERTY(BlueprintAssignable, Category = "UCM Source")
    FUCMSourceAction SourceActionHovered;

    // 현재 동작 ID와 맞는 원본 요소만 강조해 메뉴 선택을 표시한다.
    UFUNCTION(BlueprintCallable, Category = "UCM Source")
    void SelectSourceAction(FName Action);

    // 복구 동작 ID를 외부 소비자에게 알리고 실제 기능 처리는 기존 MW에 맡긴다.
    void HandleSourceAction(FName Action);

    // 복구 요소와 현재 위젯 연결 상태를 검증용으로 조회한다.
    UFUNCTION(BlueprintCallable, Category = "UCM Source")
    FString GetSourceWidgetEvidence() const;

private:
    TArray<TWeakObjectPtr<UWidget>> SourceVisuals;

    // 복구 자료가 sRGB 색인지 설정에 따라 변환해 원본 UI 색의 기준을 맞춘다.
    FLinearColor SourceTint(FLinearColor C) const;

protected:
    // 원본 요소 데이터로 기존 UMG 캔버스를 구성하고 표시·버튼 이벤트를 연결한다.
    virtual TSharedRef<SWidget> RebuildWidget() override;
};

// 복구 UI를 월드에 표시하는 표면이다. 전용 메뉴 포인터와 일반 하드웨어 입력을 구분한다.
UCLASS()
class UCMTITLEPREVIEW_API UUCMWorldWidgetComponent : public UWidgetComponent
{
    GENERATED_BODY()
public:
    UUCMWorldWidgetComponent();

    // 메뉴만 전용 포인터를 사용하고 다른 월드 위젯은 기존 하드웨어 입력을 유지한다.
    void DisableHardwareInput()
    {
        bReceiveHardwareInput = false;
    }
};

// 복구 시퀀스 바인딩 ID와 현재 월드 액터 이름을 연결한다.
USTRUCT(BlueprintType)
struct FUCMSourceBindingOverride
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    FGuid BindingId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    FString ActorName;
};

// 원본 메뉴 동작과 월드 표지판 배치·목적지의 대응을 보관한다.
USTRUCT(BlueprintType)
struct FUCMSourceMenuEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    FName ActionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    FTransform WorldWidgetTransform;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    FVector2D CanvasSize = FVector2D(400, 90);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    FName SourceMethod;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    FName SourceDestination;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    FString SourceRef;
};

// 원본 경로의 시각과 카메라 자세를 한 샘플로 보관한다.
USTRUCT(BlueprintType)
struct FUCMSourceCameraSample
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    float Time = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    FTransform Pose;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FUCMPreviewGameEntryRequested, FName, SourceMethod, FName,
                                             SourceDestination);
/** 원본 연출 재생 기반이다. 로그인·경기 로드·팀 게임 규칙을 직접 실행하지 않는다. */
UCLASS(Blueprintable)
class UCMTITLEPREVIEW_API AUCMTitleFlowPreview : public AActor
{
    GENERATED_BODY()
public:
    AUCMTitleFlowPreview();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MW Integration")
    bool bStartAutomatically = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MW Integration")
    bool bExternalMenuPresentation = false;
    DECLARE_MULTICAST_DELEGATE(FPresentationChanged);
    FPresentationChanged PresentationChanged;
    DECLARE_MULTICAST_DELEGATE_OneParam(FExternalMenuAction, FName);
    FExternalMenuAction ExternalMenuAction;

    // 외부 MW 연결부가 메뉴 표시와 입력 허용을 함께 제어할 수 있게 한다.
    void SetSourceMenuPresentation(bool bVisible, bool bInputEnabled);

    // 원본 시퀀스의 최종 자세와 타이틀 시점을 복원한다.
    void RestoreTitlePose();

    bool bSuppressPreviewInput = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UCM Preview")
    TObjectPtr<UUCMWorldWidgetComponent> TitleCanvas;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UCM Preview")
    TObjectPtr<UUCMWorldWidgetComponent> MenuCanvas;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    TSubclassOf<UUserWidget> TitleWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    TSubclassOf<UUserWidget> MenuWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    TObjectPtr<ULevelSequence> IntroSequence;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    TObjectPtr<AActor> PreviewCamera;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    TObjectPtr<AActor> MenuCamera;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    TArray<FUCMSourceBindingOverride> SourceBindingOverrides;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    float SourceOpenSplashWait = 17.8500003815f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    FString SourceCompanyLogo;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    FString SourceSplashScreen;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    FString SourceMainMenu;

    UPROPERTY(BlueprintReadOnly, Category = "UCM Preview")
    int32 PreviewStage = 0;

    UPROPERTY(BlueprintReadOnly, Category = "UCM Preview")
    float SequenceFinishedAt = -1;

    UPROPERTY(BlueprintReadOnly, Category = "UCM Preview")
    int32 IgnoredIntroInputs = 0;

    UPROPERTY(BlueprintReadOnly, Category = "UCM Preview")
    int32 GameEntryRequests = 0;

    UPROPERTY(BlueprintReadOnly, Category = "UCM Preview")
    TObjectPtr<ULevelSequencePlayer> SequencePlayer;

    UPROPERTY(BlueprintReadOnly, Category = "UCM Preview")
    TArray<FString> PreviewEvents;

    UPROPERTY(BlueprintAssignable, Category = "UCM Preview")
    FUCMPreviewGameEntryRequested GameEntryRequested;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    bool bEnableSourceFrontendMenus = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    bool bEnableSourceCameraTransition = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    TArray<FUCMSourceCameraSample> SourceTransitionForward;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    TArray<FUCMSourceCameraSample> SourceTransitionReverse;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    float SourceBrainBlendSeconds = .5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    float SourceTransitionVerticalFOV = 45;

    UPROPERTY(BlueprintReadOnly, Category = "UCM Source")
    int32 IgnoredTransitionInputs = 0;

    UPROPERTY(BlueprintReadOnly, Category = "UCM Source")
    float CameraTransitionTime = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UCM Source")
    TArray<FUCMSourceMenuEntry> SourceMenuEntries;

    UPROPERTY(BlueprintReadOnly, Category = "UCM Source")
    int32 SelectedSourceMenu = 0;

    UPROPERTY(BlueprintReadOnly, Category = "UCM Source")
    int32 SuppressedDuplicateInputs = 0;

    UPROPERTY(BlueprintReadOnly, Category = "UCM Source")
    FName LastSourceMenuRequest;

    UPROPERTY(BlueprintAssignable, Category = "UCM Source")
    FUCMPreviewGameEntryRequested MenuActionRequested;

    // 기존 메뉴 목록 범위 안에서 키보드 선택을 이동한다.
    UFUNCTION(BlueprintCallable, Category = "UCM Source")
    void MoveSourceMenu(int32 Direction);

    // 복구한 역방향 전환이 있으면 재사용하고 기존 타이틀 시점으로 돌아간다.
    UFUNCTION(BlueprintCallable, Category = "UCM Source")
    void ReturnToSourceTitle();

    // 실제 표시·입력 단계가 맞을 때만 동작 ID를 선택하고 실행 입력으로 전달한다.
    UFUNCTION()
    void OnSourceMenuAction(FName Action);

    // 마우스 호버도 기존 메뉴 선택 상태로 연결해 입력 방식을 섞어 쓸 수 있게 한다.
    UFUNCTION()
    void OnSourceMenuHover(FName Action);

    // 인트로·카메라 전환 중 입력을 구분하고 준비된 단계에서만 기존 메뉴 실행을 전달한다.
    UFUNCTION(BlueprintCallable, Category = "UCM Preview")
    void AcceptPreviewInput();

    // 일반 종료는 기존 시간 조건을 유지한 공통 완료 처리로 전달한다.
    UFUNCTION(BlueprintCallable, Category = "UCM Preview")
    void IntroFinished();
    // 사용자 스킵도 정상 종료 처리를 공유하며 일반 종료의 시간 조건은 유지한다.
    bool SkipPreviewIntro();

    // 원본 연출의 실행 이력과 현재 상태를 진단용 문자열로 반환한다.
    UFUNCTION(BlueprintCallable, Category = "UCM Preview")
    FString GetPreviewEvidence() const;

    // 복구 인트로 시퀀스와 실제 바인딩으로 재생을 시작한다.
    UFUNCTION(BlueprintCallable, Category = "UCM Preview")
    void ReplayPreview();

protected:
    // 설정된 자동 시작 정책에 따라 원본 연출만 준비한다. MW 로그인 승인을 대신하지 않는다.
    virtual void BeginPlay() override;

    // 인트로·카메라·메뉴의 현재 재생 단계에 맞는 로컬 입력과 표시만 갱신한다.
    virtual void Tick(float DeltaTime) override;

    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

    virtual void OnSourceMenuSelectionChanged()
    {
    }

private:
    // 유효한 메뉴 인덱스만 반영하고 선택 변경 알림을 전달한다.
    bool SetSourceMenuSelection(int32 Index);

    // 스킵 여부를 구분하되 공통 최종 시점과 타이틀 표시를 한 경로에서 적용한다.
    void CompletePreviewIntro(bool bSkipped);

    bool bRestoringTitlePose = false;

    UPROPERTY()
    TObjectPtr<ALevelSequenceActor> SequenceActor;

    UPROPERTY()
    TArray<TObjectPtr<UUCMWorldWidgetComponent>> SourceMenuCanvases;

    TWeakObjectPtr<UUCMWorldWidgetComponent> PressedSourceMenu;

    TWeakObjectPtr<UUCMWorldWidgetComponent> HoveredSourceMenu;

    FVector2D LastSourceMousePosition = FVector2D::ZeroVector;

    bool bHaveSourceMousePosition = false;

    double StartedAt = 0;

    uint64 LastAcceptedInputFrame = MAX_uint64;

    bool bCameraTransitionActive = false;

    bool bCameraTransitionReverse = false;

    FTransform TransitionOutgoing;

    float TransitionOutgoingFOV = 0;

    // 현재 실제 시점을 출발점으로 잡아 복구한 카메라 경로로 부드럽게 연결한다.
    void BeginSourceCameraTransition(bool bReverse);

    // 원본 샘플 시간과 카메라 블렌드를 유지하며 전환을 재생한다.
    void UpdateSourceCameraTransition(float DeltaTime);

    // 현재 선택 ID를 실제 원본 메뉴 캔버스들에 반영한다.
    void RefreshSourceMenu();

    // 표시 중인 메뉴 표면만 검사해 원본 배경 메시가 버튼 입력을 막지 않게 한다.
    void UpdateSourceMenuPointer(APlayerController* PC);

    // 원본 연출 이벤트와 경과 시간을 남기고 화면 갱신 구독자에게 알린다.
    void Record(const FString& Event);
};
