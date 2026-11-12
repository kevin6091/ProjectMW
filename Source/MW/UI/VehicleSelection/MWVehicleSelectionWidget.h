#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Types/SlateEnums.h"
#include "Layout/Margin.h"
#include "UI/VehicleSelection/MWVehicleUITypes.h"
#include "MWVehicleSelectionWidget.generated.h"
class AMWLobbyGameState;
class AMWPlayerState;
class UButton;
class UCanvasPanel;
class UGameViewportClient;
class SWidget;
class SWindow;
class UHorizontalBox;
class UTextBlock;
class UTexture2D;
class UUniformGridPanel;
class UWrapBox;
class UMWVehicleCardWidget;
class UMWVehicleDefinitionData;
class UMWVehiclePlayerEntryWidget;
class UMWVehiclePlayerMarkerWidget;
class UWidgetComponent;
struct FMWCursorVisualLayout;

// 외형 검토용 개발 데이터다. 실제 차량 ID·Pawn·게임 등록을 만들지 않는다.
USTRUCT(BlueprintType)
struct FMWVehiclePreviewCard
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<UTexture2D> Thumbnail;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bUnavailable = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText UnavailableReason;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMWVehicleSelectionRequested, FGameplayTag, VehicleAssetID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMWVehicleSelectionBackRequested);

UCLASS(Abstract, Config = Game)
class MW_API UMWVehicleSelectionWidget : public UUserWidget
{
    GENERATED_BODY()

public:

    // 카탈로그의 실제 ID로 기존 카드 순서를 유지하고 비밀 차량은 별도 표시 모델로 보관한다.
    UFUNCTION(BlueprintCallable, Category = "MW|Vehicle UI")
    void RefreshVehicles();
    // 실패하거나 끝난 로컬 대기 요청을 해제한다. 이것만으로 서버 승인을 확정하지 않는다.
    UFUNCTION(BlueprintCallable, Category = "MW|Vehicle UI")
    void FinishSelectionRequest(const FText& Message);

    UFUNCTION(BlueprintPure, Category = "MW|Vehicle UI")
    int32 GetVisibleVehicleCount() const
    {
        return Cards.Num();
    }

    // 화면에서 고른 모델의 실제 차량 ID를 조회하며 서버 확정을 대신하지 않는다.
    UFUNCTION(BlueprintPure, Category = "MW|Vehicle UI")
    FGameplayTag GetSelectedVehicleAssetId() const;

    // 현재 선택 카드의 사용자용 이름만 조회한다.
    UFUNCTION(BlueprintPure, Category = "MW|Vehicle UI")
    FText GetSelectedDisplayName() const;

    // 외형 Preview의 완료 조건을 확인한다. 서버의 실제 단계 판정과 구분한다.
    UFUNCTION(BlueprintPure, Category = "MW|Vehicle UI")
    bool IsPreviewLocked() const;

    // 명시한 개발 시나리오로만 바꾸고 실제 방·차량 ID에는 요청하지 않는다.
    UFUNCTION(BlueprintCallable, Category = "MW|Vehicle UI|Preview")
    void SelectPreviewScenario(int32 Index);

    // 외형 Preview에서 로컬 참가자로 표시할 슬롯을 고른다.
    UFUNCTION(BlueprintCallable, Category = "MW|Vehicle UI|Preview")
    void SelectPreviewLocalSlot(int32 SlotIndex);

    // 개발 검토용 조작부만 표시하거나 숨긴다.
    UFUNCTION(BlueprintCallable, Category = "MW|Vehicle UI|Preview")
    void SetPreviewControlsVisible(bool bVisible);

    // 기존 외형 Preview 표시 배율만 조정하고 실제 서버 선택 상태는 변경하지 않는다.
    void SetWorldPreviewMarkerScales(float PinnedScale, float CardScale);

    // 화면별 자리 배정 순서 대신 공유 MemberId로 같은 플레이어 색을 고른다.
    FLinearColor GetParticipantTintForMember(int32 MemberId) const;

    UFUNCTION(BlueprintPure, Category = "MW|Vehicle UI|Preview")
    int32 GetActivePreviewScenarioIndex() const
    {
        return ActivePreviewScenarioIndex;
    }

    UFUNCTION(BlueprintPure, Category = "MW|Vehicle UI")
    int32 GetDisplayedSlotCount() const
    {
        return ParticipantViews.Num();
    }

    UFUNCTION(BlueprintPure, Category = "MW|Vehicle UI")
    TArray<FMWVehiclePlayerPresentation> GetParticipantPresentation() const
    {
        return ParticipantViews;
    }

    // 실제 방과 Preview 데이터 원본을 구분해 참가자 표시를 갱신한다.
    UFUNCTION(BlueprintCallable, Category = "MW|Vehicle UI")
    void RefreshPlayers();
	// HUD가 사용하는 화면 좌표/표시 조회. Widget은 RPC나 서버 상태를 변경하지 않는다.
	bool SampleSharedCursor(const UWidgetComponent* Surface, FVector2D& OutNormalizedPosition) const;
	bool ProjectSharedCursor(const UWidgetComponent* Surface, FVector2D Position, FVector2D& OutViewportPosition) const;
	UUserWidget* CreateSharedCursorVisual() const;
	bool ConfigureSharedCursorVisual(UUserWidget& Visual, const AMWPlayerState* Player, FMWCursorVisualLayout& OutLayout) const;

UPROPERTY(BlueprintAssignable, Category = "MW|Vehicle UI")
FMWVehicleSelectionRequested OnSelectionRequested;

UPROPERTY(BlueprintAssignable, Category = "MW|Vehicle UI")
FMWVehicleSelectionRequested OnSelectionCancelRequested;

UPROPERTY(BlueprintAssignable, Category = "MW|Vehicle UI")
FMWVehicleSelectionBackRequested OnBackRequested;

UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Data")
FGameplayTag VehicleAssetRoot;

UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Data")
TMap<FGameplayTag, TSoftObjectPtr<UTexture2D>> VehicleThumbnails;

UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Data")
FGameplayTag SecretVehicleAssetId;

UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Designer")
int32 ColumnCount = 0;

UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Designer")
TSubclassOf<UMWVehicleCardWidget> CardWidgetClass;

UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Designer")
TSubclassOf<UMWVehiclePlayerEntryWidget> PlayerEntryWidgetClass;

UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Designer")
TEnumAsByte<EHorizontalAlignment> CardHorizontalAlignment = HAlign_Fill;

UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Designer")
TEnumAsByte<EVerticalAlignment> CardVerticalAlignment = VAlign_Fill;

UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Designer")
FMargin PlayerEntryPadding;

UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Designer")
TArray<FLinearColor> ParticipantTints;

UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Designer")
FLinearColor FallbackParticipantTint;

UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Designer")
FLinearColor WaitingSlotTint;

UPROPERTY(Config, EditDefaultsOnly, Category = "MW|Vehicle UI|Cursor", meta = (ClampMin = "0.0", ClampMax = "1.0"))
float RemoteCursorScale = 1.f;

UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Preview")
bool bPreviewOnly = false;

UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Preview")
bool bShowPreviewControls = true;

UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Preview")
TArray<FMWVehiclePreviewCard> PreviewCards;

UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Preview")
TArray<FMWVehiclePreviewScenario> PreviewScenarios;

UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Preview")
int32 DefaultPreviewScenarioIndex = 0;

UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Preview")
FText PreviewPlayerSelectingText;

UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Preview")
FText PreviewPlayerLockedText;

UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Text")
FText EmptyCatalogText;

UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Text")
FText PreviewText;

UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Text")
FText PreviewLockedText;

UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Text")
FText SelectionCompleteText;

UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Text")
FText WaitingForPlayersText;

UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Text")
FText ConfirmVehicleText;

UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Text")
FText MissingServerContractText;

UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Text")
FText ChooseVehicleText;

UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Text")
FText RequestPendingText;

UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Text")
FText InvalidVehicleText;

UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Text")
FText RegisteredCountFormat;

UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Text")
FText WaitingPlayerText;

UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Text")
FText UnselectedVehicleText;

UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Text")
FText MissingServerSelectionText;

UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Text")
FText ParticipantMarkerFormat;

UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Text")
FText PreviewLocalSlotFormat;

UPROPERTY(EditDefaultsOnly, Category = "MW|Vehicle UI|Text")
FText PreviewCapacityFormat;

protected:
// 기존 카드·참가자·커서 이벤트를 연결하고 실제 방과 외형 Preview의 시작 상태를 구분한다.
virtual void NativeConstruct() override;

// 소프트웨어 커서·버튼·참가자 상태 연결을 해제해 다음 화면의 입력을 덮어쓰지 않는다.
virtual void NativeDestruct() override;

// 자식 버튼보다 먼저 게시판 입력을 확인해 실제 우클릭 취소 경로를 놓치지 않는다.
virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

// 현재 실제 마우스 이벤트의 위치를 기존 커서 좌표 갱신에 전달한다.
virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

// 선택 화면에 들어온 포인터의 유효 상태를 반영하고 커서 표시를 갱신한다.
virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

// 게시판 히트 상태를 비우되 Viewport 커서 수명은 이 선택 화면 정책을 따른다.
virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

UPROPERTY(meta = (BindWidgetOptional))
TObjectPtr<UCanvasPanel> CursorLayer;

UPROPERTY(meta = (BindWidgetOptional))
TObjectPtr<UMWVehiclePlayerMarkerWidget> LocalPlayerCursor;

UPROPERTY(meta = (BindWidget))
TObjectPtr<UUniformGridPanel> Grid_Vehicles;

UPROPERTY(meta = (BindWidgetOptional))
TObjectPtr<UWrapBox> Wrap_Vehicles;

UPROPERTY(meta = (BindWidget))
TObjectPtr<UHorizontalBox> Box_Players;

UPROPERTY(meta = (BindWidget))
TObjectPtr<UButton> Button_Confirm;

UPROPERTY(meta = (BindWidgetOptional))
TObjectPtr<UButton> Button_SecretVehicle;

UPROPERTY(meta = (BindWidget))
TObjectPtr<UButton> Button_Reset;

UPROPERTY(meta = (BindWidget))
TObjectPtr<UButton> Button_Back;

UPROPERTY(meta = (BindWidget))
TObjectPtr<UButton> Button_Refresh;

UPROPERTY(meta = (BindWidget))
TObjectPtr<UTextBlock> Text_Status;

UPROPERTY(meta = (BindWidgetOptional))
TObjectPtr<UTextBlock> Text_SelectionDetail;

UPROPERTY(meta = (BindWidgetOptional))
TObjectPtr<UWidget> Legend_Cancel;

UPROPERTY(meta = (BindWidget))
TObjectPtr<UTextBlock> Text_Error;

UPROPERTY(meta = (BindWidget))
TObjectPtr<UTextBlock> Text_RegisteredCount;

UPROPERTY(meta = (BindWidget))
TObjectPtr<UWidget> PreviewNotice;

UPROPERTY(meta = (BindWidgetOptional))
TObjectPtr<UButton> Button_NextPreviewScenario;

UPROPERTY(meta = (BindWidgetOptional))
TObjectPtr<UButton> Button_NextPreviewLocalPlayer;

UPROPERTY(meta = (BindWidgetOptional))
TObjectPtr<UTextBlock> Text_PreviewCapacity;

UPROPERTY(meta = (BindWidgetOptional))
TObjectPtr<UTextBlock> Text_PreviewScenario;

UPROPERTY(meta = (BindWidgetOptional))
TObjectPtr<UTextBlock> Text_PreviewLocalPlayer;

UPROPERTY(meta = (BindWidgetOptional))
TObjectPtr<UWidget> PreviewControls;

private:

UPROPERTY(Transient)
TArray<TObjectPtr<UMWVehicleCardWidget>> Cards;
// 보이는 카드 줄을 늘리지 않고 기존 선택 모델로 비밀 차량을 처리한다.
UPROPERTY(Transient)
TObjectPtr<UMWVehicleCardWidget> SecretVehicleCard;

UPROPERTY(Transient)
TArray<TObjectPtr<UMWVehiclePlayerEntryWidget>> PlayerEntries;

UPROPERTY(Transient)
TArray<FMWVehiclePlayerPresentation> ParticipantViews;

UPROPERTY(Transient)
FMWVehiclePreviewScenario ActivePreview;
TWeakObjectPtr<UMWVehicleCardWidget> SelectedCard;
	TWeakObjectPtr<AMWLobbyGameState> BoundGameState;
    // 실제 PlayerArray가 바뀌어도 화면 배치용 자리는 유지한다. 서버 참가자 원본을 대체하지 않는다.
    TMap<TWeakObjectPtr<AMWPlayerState>, int32> PlayerSlots;
    int32 ActivePreviewScenarioIndex = INDEX_NONE;
    enum class ESelectionRequest : uint8
    {
        None,
        Confirm,
        Cancel
    };
    ESelectionRequest PendingRequest = ESelectionRequest::None;

    bool IsRequestPending() const
    {
        return PendingRequest != ESelectionRequest::None;
    }

    // 기존 카드 이벤트와 참조를 해제한 뒤 목록을 다시 만들 수 있게 비운다.
    void ClearCards();

    // 작성된 행·그리드 배치에 카드를 넣고 기존 클릭·호버 알림을 연결한다.
    void AddCard(UMWVehicleCardWidget* Card);

    // 카탈로그 ID와 실제 차량 정의를 기존 카드 클래스에 연결한다.
    UMWVehicleCardWidget* CreateVehicleCard(FGameplayTag Id, UMWVehicleDefinitionData& Definition);

    // 비밀 차량 모델도 포함해 실제 ID에 대응하는 기존 카드만 찾는다.
    UMWVehicleCardWidget* FindVehicleCard(FGameplayTag Id) const;

    // 현재 카드 안의 표시 요소에 설정된 Preview 배율을 적용한다.
    void ApplyCardMarkerScale(UMWVehicleCardWidget* Card) const;

    // 실제 요청 대기·선택 잠금 상태로 입력을 조절하고 불필요한 Preview 안내를 숨긴다.
    void RefreshControls();
	bool IsSelectionLocked() const;

    // 개발용 참가자의 표시 완료 여부만 계산한다.
    bool AreAllPreviewParticipantsConfirmed() const;

    // 개발 표시 조작부의 현재 시나리오·슬롯 상태를 갱신한다.
    void RefreshPreviewControls();

    // 개발용 선택을 현재 카드 표시 모델로 맞춘다. 실제 서버 선택은 만들지 않는다.
    void RefreshPreviewSelection();

    // 외형 검토용 참가자만 구성한다. 실제 서버 차량 선택으로 등록하지 않는다.
    void BuildPreviewParticipants();
	void BuildLobbyParticipants(AMWLobbyGameState* State);

// 기존 하단 참가자 슬롯과 이동 커서를 갱신한다. 카드 위 고정 마커는 새로 만들지 않는다.
void RenderParticipants();

// 현재 참가자 표시와 포인터 유효성으로 로컬 커서 외형을 갱신한다.
void RefreshLocalCursor();

// 월드 위젯의 실제 Geometry로 입력 좌표를 바꿔 커서 그림과 클릭 위치를 맞춘다.
void UpdateLocalCursor(const FPointerEvent& MouseEvent);

// 이전 게시판 히트 좌표를 무효화해 새 화면에서 오래된 클릭 지점을 쓰지 않게 한다.
void ResetPointerHit();

// 실제 Slate 창의 커서 처리를 연결해 월드 위젯에서도 클릭 위치를 맞춘다.
void BindCursorWindow();

// 기존 커서 설정을 보관하고 선택 화면 수명 동안 같은 외형과 클릭 끝점을 사용한다.
void RegisterSoftwareCursor();

// 아직 이 화면이 설치한 커서일 때만 이전 설정을 복원해 다음 화면을 덮어쓰지 않는다.
void ReleaseSoftwareCursor();

// 실제 Designer 마커 크기와 프로젝트 DPI를 사용해 화면 커서 크기를 얻는다.
FVector2D GetSoftwareCursorSize() const;

UPROPERTY(Transient)
TObjectPtr<UMWVehiclePlayerMarkerWidget> SoftwareCursor;
TWeakObjectPtr<UGameViewportClient> CursorViewport;
TSharedPtr<SWidget> SoftwareCursorSlate;
TSharedPtr<SWidget> PreviousSoftwareCursor;
bool bPreviouslyUsedSoftwareCursors = false;

// 실제 방은 서버 취소 응답을 기다리고 Preview만 로컬 표시를 해제한다.
bool CancelLocalSelection();

// 현재 단계와 소유자의 서버 확정 ID를 확인해 취소 가능한 대상만 반환한다.
FGameplayTag GetCancelableVehicleId() const;

// 로컬 선택 강조와 카드 참조만 해제한다. 서버 취소 승인은 별도 요청 경로를 따른다.
void ClearLocalSelectionDisplay();
TWeakPtr<SWindow> CursorWindow;
FDelegateHandle WindowDeactivatedHandle;
bool bCursorInUI = false;
float CardMarkerScale = 1.f;

// 개발용 참가자 목록에서 지정된 로컬 슬롯을 찾는다. 실제 서버 참가자를 만들지 않는다.
FMWVehiclePreviewParticipant* FindLocalPreviewParticipant();

// 개발용 참가자 목록에서 지정된 로컬 슬롯을 찾는다. 실제 서버 참가자를 만들지 않는다.
const FMWVehiclePreviewParticipant* FindLocalPreviewParticipant() const;

// 개발용 참가자의 외형·선택 상태만 표시 값으로 변환한다.
FMWVehiclePlayerPresentation MakePreviewPresentation(const FMWVehiclePreviewParticipant& Participant) const;

// 빈 하단 슬롯에 사용할 대기 문구와 표시 색을 구성한다.
FMWVehiclePlayerPresentation MakeWaitingPresentation() const;

// 기존 팔레트 범위 안에서 표시 색을 조회하고 없는 경우 대체 색을 사용한다.
FLinearColor GetParticipantTint(int32 SlotIndex) const;

// 기존 개발 목록 범위에서 다음 외형 시나리오를 고른다.
UFUNCTION()
void HandleNextPreviewScenario();

// 개발 참가자 표시의 로컬 슬롯만 순환한다.
UFUNCTION()
void HandleNextPreviewLocalPlayer();

// 유효한 카드만 선택하고 실제 방에서는 기존 서버 승인 요청으로 즉시 전달한다.
UFUNCTION()
void HandleCardChosen(UMWVehicleCardWidget* Card);

// 기존 비밀 차량 카드 모델을 같은 선택 요청 경로에 전달한다.
UFUNCTION()
void HandleSecretVehicleChosen();

// 외형 Preview의 호버 미리보기만 갱신하고 확정 참가자의 표시를 덮어쓰지 않는다.
UFUNCTION()
void HandleCardHover(UMWVehicleCardWidget* Card, bool bHovered);

// Preview의 로컬 완료와 실제 서버 확정 요청을 구분한다.
UFUNCTION()
void HandleConfirm();

// 기존 선택 취소 경로를 재사용해 표시와 요청 대기 처리를 일치시킨다.
UFUNCTION()
void HandleReset();

// 실제 요청 대기 중에는 중복 뒤로 요청을 보내지 않고 기존 화면 종료 알림을 사용한다.
UFUNCTION()
void HandleBack();
}
;
