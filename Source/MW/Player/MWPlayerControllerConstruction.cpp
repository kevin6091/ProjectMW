#include "Player/MWPlayerController.h"
#include "Gameplay/Construction/MWTrackConstructionComponent.h"
#include "Gameplay/Construction/MWPlayerConstructionComponent.h"
#include "Gameplay/Construction/MWTrackOfferPreview.h"
#include "Gameplay/Construction/MWTrackSelectionBoard.h"
#include "UI/MWTrackSelectionWidget.h"
#include "UI/MWRoomLobbyWidget.h"
#include "Data/MWInputData.h"
#include "Data/MWTrackPieceDefinition.h"
#include "System/MWGameState.h"
#include "System/MWPlayerState.h"
#include "System/MWGameMode.h"
#include "System/MWTrackConstructionTestGameMode.h"
#include "System/MWAssetManager.h"
#include "MWGameplayTags.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "InputAction.h"

namespace
{
    // 서버의 판정 코드는 유지하고, 사용자에게 보여줄 문구는 로컬 표현 계층에서 결정한다.
    FText SelectionRejectionText(EMWTrackSelectionRejectReason Reason)
    {
        switch (Reason)
        {
        case EMWTrackSelectionRejectReason::NotOpen: return FText::FromString(TEXT("트랙 선택이 종료되었습니다."));
        case EMWTrackSelectionRejectReason::StaleSession:
        case EMWTrackSelectionRejectReason::StaleOffers: return FText::FromString(TEXT("후보가 갱신되었습니다. 다시 선택해 주세요."));
        case EMWTrackSelectionRejectReason::AlreadyConfirmed: return FText::FromString(TEXT("이미 트랙을 선택했습니다."));
        case EMWTrackSelectionRejectReason::NotParticipant: return FText::FromString(TEXT("이번 트랙 선택에 참여할 수 없습니다."));
        case EMWTrackSelectionRejectReason::InvalidOffer: return FText::FromString(TEXT("선택할 수 없는 트랙입니다."));
        default: return FText::FromString(TEXT("선택을 처리하지 못했습니다. 다시 시도해 주세요."));
        }
    }
}

void AMWPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    // 상태 복제보다 입력 컴포넌트가 늦게 준비되었어도 선택 입력 설치를 다시 시도한다.
    if (HasActorBegunPlay()) RefreshConstructionPresentation();
}

void AMWPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    // 커서 검사는 조작 중인 로컬 화면에서만 한다. 확정 응답 대기 중에는 Hover를 고정한다.
    if (IsLocalController() && bSelectionInputInstalled && !bConfirmPending)
        UpdateLocalTrackHover(FindHoveredTrackOffer());
}

void AMWPlayerController::RegisterTrackSelectionBoard(AMWTrackSelectionBoard* Board)
{
    if (!IsLocalController() || bConstructionShuttingDown) return;
    if (SelectionBoard.IsValid() && SelectionBoard.Get() != Board)
    {
        UE_LOG(LogTemp, Warning, TEXT("Only one track selection board is supported per world."));
        return;
    }
    SelectionBoard = Board;
    RefreshConstructionPresentation();
}

void AMWPlayerController::UnregisterTrackSelectionBoard(AMWTrackSelectionBoard* Board)
{
    if (SelectionBoard.Get() == Board)
    {
        StopConstructionPresentation();
        SelectionBoard.Reset();
    }
}

void AMWPlayerController::UnbindConstructionSources()
{
    // 다른 구독자의 이벤트는 건드리지 않고 이 Controller가 등록한 핸들러만 제거한다.
    if (BoundConstruction.IsValid())
        BoundConstruction->OnSelectionSessionChanged.RemoveDynamic(this, &ThisClass::HandleConstructionSessionChanged);
    for (const auto& Weak : BoundPlayerConstruction)
        if (Weak.IsValid()) Weak->OnSelectionChanged.RemoveDynamic(this, &ThisClass::HandlePlayerConstructionChanged);
    BoundPlayerConstruction.Reset();
    BoundConstruction.Reset();
}

void AMWPlayerController::RebindConstructionSources()
{
    if (!IsLocalController() || bConstructionShuttingDown) return;
    AMWGameState* GS = GetWorld()->GetGameState<AMWGameState>();
    UMWTrackConstructionComponent* Construction = GS ? GS->GetTrackConstructionComponent() : nullptr;
    if (BoundConstruction.Get() != Construction)
    {
        if (BoundConstruction.IsValid())
            BoundConstruction->OnSelectionSessionChanged.RemoveDynamic(this, &ThisClass::HandleConstructionSessionChanged);
        BoundConstruction = Construction;
        if (Construction)
            Construction->OnSelectionSessionChanged.AddUniqueDynamic(this, &ThisClass::HandleConstructionSessionChanged);
    }
    // 입장·퇴장으로 PlayerArray가 바뀔 수 있으므로 개인 상태 구독 목록을 다시 만든다.
    for (const auto& Weak : BoundPlayerConstruction)
        if (Weak.IsValid()) Weak->OnSelectionChanged.RemoveDynamic(this, &ThisClass::HandlePlayerConstructionChanged);
    BoundPlayerConstruction.Reset();
    if (!GS) return;
    for (APlayerState* Base : GS->PlayerArray)
    {
        AMWPlayerState* PS = Cast<AMWPlayerState>(Base);
        UMWPlayerConstructionComponent* Personal = PS ? PS->GetPlayerConstructionComponent() : nullptr;
        if (!Personal) continue;
        Personal->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandlePlayerConstructionChanged);
        BoundPlayerConstruction.Add(Personal);
    }
}

void AMWPlayerController::HandleConstructionSessionChanged() { RefreshConstructionPresentation(); }
void AMWPlayerController::HandlePlayerConstructionChanged() { RefreshConstructionPresentation(); }

void AMWPlayerController::RefreshConstructionPresentation()
{
    if (!IsLocalController() || !HasActorBegunPlay() || bConstructionShuttingDown || bRefreshingConstruction) return;
    // 화면 생성·이벤트 처리 중 같은 갱신 함수로 재진입하는 것을 막는다.
    TGuardValue<bool> Guard(bRefreshingConstruction, true);
    if (!BoundConstruction.IsValid()) return;
    const FMWTrackSelectionSession Session = BoundConstruction->GetSelectionSession();
    if (!Session.bIsOpen) { StopConstructionPresentation(); return; }
    // 새 선택의 상태가 도착하면 이전 후보·입력·대기 요청부터 정리한다.
    if (DisplaySessionId != 0 && (DisplaySessionId != Session.SelectionSessionId || DisplayOffersRevision != Session.OffersRevision))
        StopConstructionPresentation();

    AMWPlayerState* OwnPS = GetPlayerState<AMWPlayerState>();
    const UMWPlayerConstructionComponent* Own = OwnPS ? OwnPS->GetPlayerConstructionComponent() : nullptr;
    if (!Own) return;
    const FMWPlayerTrackSelection Selection = Own->GetSelection();
    // 서로 다른 Actor의 복제 순서는 보장하지 않는다. 공용·개인 상태가 일치할 때까지 기다린다.
    if (Selection.SelectionSessionId != Session.SelectionSessionId || Selection.OffersRevision != Session.OffersRevision) return;
    if (!SelectionBoard.IsValid())
    {
        // Board의 BeginPlay 등록보다 Controller 준비가 늦은 경우 현재 레벨에서도 찾아본다.
        for (TActorIterator<AMWTrackSelectionBoard> It(GetWorld()); It; ++It)
        {
            SelectionBoard = *It;
            break;
        }
    }
    if (!SelectionBoard.IsValid()) return;
    // 후보 Actor와 Widget은 화면 진입 시 한 번만 만든다. Hover 갱신마다 재생성하지 않는다.
    if (!bSelectionPresentationActive)
    {
        FText Error;
        if (!SelectionBoard->BuildOffers(Session, Error))
        {
            UE_LOG(LogTemp, Error, TEXT("%s"), *Error.ToString());
            return;
        }
        DisplaySessionId = Session.SelectionSessionId;
        DisplayOffersRevision = Session.OffersRevision;
        LocalHoveredOfferId = INDEX_NONE;
        LastSentHoveredOfferId = INDEX_NONE;
        AcknowledgedSelectedOfferId = INDEX_NONE;
        SelectionStatus = FText::GetEmpty();
        bSelectionPresentationActive = true;
        bLobbyPresentationEnabled = false;
        TArray<UUserWidget*> LobbyWidgets;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, LobbyWidgets, UMWRoomLobbyWidget::StaticClass(), false);
        for (UUserWidget* Widget : LobbyWidgets)
            if (Widget->GetOwningPlayer() == this) Widget->SetVisibility(ESlateVisibility::Collapsed);
        // 화면 종료 시 복귀할 대상을 보관한 뒤 선택판 카메라로 전환한다.
        PreviousSelectionViewTarget = GetViewTarget();
        SetViewTargetWithBlend(SelectionBoard.Get(), 0.25f);
        SelectionWidget = CreateWidget<UMWTrackSelectionWidget>(this, UMWTrackSelectionWidget::StaticClass());
        if (SelectionWidget) SelectionWidget->AddToViewport(20);
    }
    // 승인 RPC와 개인 상태 복제 중 어느 쪽이 먼저 도착해도 재선택 입력을 잠근다.
    if (Selection.bConfirmed || AcknowledgedSelectedOfferId != INDEX_NONE)
        DisableSelectionInput();
    else if (!EnableSelectionInput())
        SelectionStatus = FText::FromString(TEXT("선택 입력을 준비할 수 없습니다. 입력 설정을 확인해 주세요."));
    SelectionBoard->RefreshMarkers(this, Session, LocalHoveredOfferId);
    RefreshSelectionText(Session);
}

bool AMWPlayerController::EnableSelectionInput()
{
    if (bSelectionInputInstalled) return true;
    UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
    ULocalPlayer* LP = GetLocalPlayer();
    UEnhancedInputLocalPlayerSubsystem* Inputs = LP ? LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
    if (!EIC || !Inputs) return false;
    if (!SelectionInputData)
        SelectionInputData = UMWAssetManager::Get().LoadAssetSynchronous<UMWInputData>(MWGameplayTags::Asset_Input_SelectConstruction);
    if (!SelectionInputData) return false;
    const UInputMappingContext* Context = SelectionInputData->GetMappingContext();
    const UInputAction* Select = SelectionInputData->FindInputActionByTag(MWGameplayTags::Input_Select);
    if (!Context || !Select || Select->ValueType != EInputActionValueType::Boolean) return false;
    // 액션의 시작만 확정 입력으로 받는다. 눌러 둔 동안 반복 확정하지 않는다.
    SelectionBindingHandle = EIC->BindAction(Select, ETriggerEvent::Started, this, &ThisClass::HandleTrackSelectPressed).GetHandle();
    SelectionInputComponent = EIC;
    FModifyContextOptions Options;
    // 이전 화면에서 누르고 있던 버튼이 선택판 진입 즉시 확정으로 이어지는 것을 막는다.
    Options.bIgnoreAllPressedKeysUntilRelease = true;
    Inputs->AddMappingContext(Context, 1, Options);
    FInputModeGameAndUI Mode;
    Mode.SetHideCursorDuringCapture(false);
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Mode);
    bShowMouseCursor = true;
    bSelectionInputInstalled = true;
    return true;
}

void AMWPlayerController::DisableSelectionInput()
{
    if (!bSelectionInputInstalled) return;
    // 전체 입력을 지우지 않고 선택 기능에서 추가한 바인딩과 Mapping Context만 제거한다.
    if (SelectionInputComponent.IsValid()) SelectionInputComponent->RemoveBindingByHandle(SelectionBindingHandle);
    if (ULocalPlayer* LP = GetLocalPlayer())
        if (UEnhancedInputLocalPlayerSubsystem* Inputs = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
            if (SelectionInputData && SelectionInputData->GetMappingContext())
                Inputs->RemoveMappingContext(SelectionInputData->GetMappingContext());
    SelectionInputComponent.Reset();
    bSelectionInputInstalled = false;
}

void AMWPlayerController::StopConstructionPresentation()
{
    // 화면용 자원과 대기 요청만 정리한다. 서버 PlayerState의 확정 결과는 지우지 않는다.
    DisableSelectionInput();
    if (bSelectionPresentationActive && SelectionBoard.IsValid()) SelectionBoard->ClearOffers();
    if (SelectionWidget) SelectionWidget->RemoveFromParent();
    SelectionWidget = nullptr;
    if (bSelectionPresentationActive && PreviousSelectionViewTarget.IsValid() && !bConstructionShuttingDown)
        SetViewTargetWithBlend(PreviousSelectionViewTarget.Get(), 0.f);
    PreviousSelectionViewTarget.Reset();
    bSelectionPresentationActive = false;
    bConfirmPending = false;
    PendingSelectionRequestId = 0;
    PendingSelectedOfferId = INDEX_NONE;
    AcknowledgedSelectedOfferId = INDEX_NONE;
    LocalHoveredOfferId = INDEX_NONE;
    LastSentHoveredOfferId = INDEX_NONE;
    DisplaySessionId = DisplayOffersRevision = 0;
    // 다음 화면은 외부 진행 코드가 결정한다. 로비를 자동 재표시하지 않는다.
}

int32 AMWPlayerController::FindHoveredTrackOffer() const
{
    // 선택용 Visibility Box에 맞은 Preview만 허용한다. 다른 Board의 Actor는 무시한다.
    FHitResult Hit;
    if (!GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), false, Hit)) return INDEX_NONE;
    const AMWTrackOfferPreview* Preview = Cast<AMWTrackOfferPreview>(Hit.GetActor());
    return Preview && Preview->GetBoard() == SelectionBoard.Get() ? Preview->GetOfferId() : INDEX_NONE;
}

bool AMWPlayerController::IsDisplayedTrackOffer(int32 OfferId, bool bAllowNone) const
{
    if (!BoundConstruction.IsValid()) return false;
    const FMWTrackSelectionSession Session = BoundConstruction->GetSelectionSession();
    // 로컬 검사도 현재 화면과 공용 상태를 대조한다. 최종 승인은 서버가 다시 검사한다.
    return Session.bIsOpen && Session.SelectionSessionId == DisplaySessionId && Session.OffersRevision == DisplayOffersRevision
        && ((bAllowNone && OfferId == INDEX_NONE) || Session.Offers.ContainsByPredicate(
            [OfferId](const FMWTrackOffer& Offer) { return Offer.OfferId == OfferId; }));
}

void AMWPlayerController::UpdateLocalTrackHover(int32 OfferId)
{
    if (!IsDisplayedTrackOffer(OfferId, true)) return;
    if (LocalHoveredOfferId != OfferId)
    {
        // 자기 강조는 왕복 지연 없이 반영하고, 다른 사람에게는 서버가 승인한 상태를 공유한다.
        LocalHoveredOfferId = OfferId;
        if (SelectionBoard.IsValid()) SelectionBoard->RefreshMarkers(this, BoundConstruction->GetSelectionSession(), OfferId);
    }
    // 변경된 후보만 최대 약 10Hz로 보낸다. 마지막 변경이 제한에 걸려도 다음 Tick에서 다시 확인한다.
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now - LastHoverSendTime < 0.1 || LastSentHoveredOfferId == OfferId) return;
    LastHoverSendTime = Now;
    LastSentHoveredOfferId = OfferId;
    ServerUpdateTrackHover(DisplaySessionId, DisplayOffersRevision, OfferId);
}

// 이전 프레임의 Hover가 아니라 입력 순간의 커서 위치를 다시 검사해 확정한다.
void AMWPlayerController::HandleTrackSelectPressed() { ConfirmLocalTrackSelection(FindHoveredTrackOffer()); }

void AMWPlayerController::ConfirmLocalTrackSelection(int32 OfferId)
{
    if (!IsLocalController() || !bSelectionInputInstalled || bConfirmPending
        || !IsDisplayedTrackOffer(OfferId, false) || NextSelectionRequestId == MAX_int32) return;
    // 요청 식별자를 보관해 응답을 대조하고, 응답을 받을 때까지 추가 확정 입력을 막는다.
    bConfirmPending = true;
    PendingSelectionRequestId = ++NextSelectionRequestId;
    PendingSelectedOfferId = OfferId;
    SelectionStatus = FText::FromString(TEXT("선택을 확인하고 있습니다..."));
    RefreshSelectionText(BoundConstruction->GetSelectionSession());
    ServerConfirmTrackSelection(DisplaySessionId, DisplayOffersRevision, OfferId, PendingSelectionRequestId);
}

void AMWPlayerController::ServerUpdateTrackHover_Implementation(int32 SessionId, int32 Revision, int32 OfferId)
{
    // 요청자는 클라이언트가 넘긴 PlayerState가 아니라 이 RPC를 소유한 Controller로 결정한다.
    AMWGameState* GS = GetWorld()->GetGameState<AMWGameState>();
    if (GS && GS->GetTrackConstructionComponent())
        GS->GetTrackConstructionComponent()->UpdateHover(this, SessionId, Revision, OfferId);
}

void AMWPlayerController::ServerConfirmTrackSelection_Implementation(int32 SessionId, int32 Revision, int32 OfferId, int32 RequestId)
{
    AMWGameState* GS = GetWorld()->GetGameState<AMWGameState>();
    UMWTrackConstructionComponent* Construction = GS ? GS->GetTrackConstructionComponent() : nullptr;
    FMWTrackSelectionReply Reply;
    if (Construction) Reply = Construction->ConfirmSelection(this, SessionId, Revision, OfferId, RequestId);
    else
    {
        Reply.SelectionSessionId = SessionId;
        Reply.OffersRevision = Revision;
        Reply.OfferId = OfferId;
        Reply.RequestId = RequestId;
        Reply.Reason = EMWTrackSelectionRejectReason::Unavailable;
    }
    // 개인 상태는 모든 참가자에게 복제하고, 이 요청의 성공·실패는 요청한 소유자에게 돌려준다.
    ClientTrackSelectionResult(Reply);
}

void AMWPlayerController::ClientTrackSelectionResult_Implementation(const FMWTrackSelectionReply& Reply)
{
    // 이전 화면에서 늦게 온 응답이나 다른 요청의 응답이 현재 화면을 바꾸지 않게 한다.
    if (!bSelectionPresentationActive || !bConfirmPending || Reply.SelectionSessionId != DisplaySessionId
        || Reply.OffersRevision != DisplayOffersRevision || Reply.RequestId != PendingSelectionRequestId
        || Reply.OfferId != PendingSelectedOfferId) return;
    bConfirmPending = false;
    PendingSelectionRequestId = 0;
    PendingSelectedOfferId = INDEX_NONE;
    if (Reply.bAccepted)
    {
        // 개인 OnRep가 아직 오지 않았어도 승인 사실을 기록해 입력 잠금을 유지한다.
        AcknowledgedSelectedOfferId = Reply.OfferId;
        SelectionStatus = FText::FromString(TEXT("선택했습니다. 다른 플레이어의 선택을 기다립니다."));
    }
    else SelectionStatus = SelectionRejectionText(Reply.Reason);
    OnTrackSelectionRequestFinished(Reply);
    RefreshConstructionPresentation();
}

void AMWPlayerController::RefreshSelectionText(const FMWTrackSelectionSession& Session)
{
    if (!SelectionWidget) return;
    const AMWGameState* GS = GetWorld()->GetGameState<AMWGameState>();
    if (!GS) return;
    TArray<AMWPlayerState*> Players;
    for (APlayerState* Base : GS->PlayerArray)
        if (AMWPlayerState* PS = Cast<AMWPlayerState>(Base)) Players.Add(PS);
    // 각 컴퓨터의 PlayerArray 순서 대신 같은 참가자 번호 순서로 현황을 보여준다.
    Players.Sort([](const AMWPlayerState& A, const AMWPlayerState& B) { return A.GetLobbyMemberId() < B.GetLobbyMemberId(); });
    FString Lines;
    for (const AMWPlayerState* PS : Players)
    {
        const FMWPlayerTrackSelection S = PS->GetPlayerConstructionComponent()->GetSelection();
        // 아직 이번 선택의 개인 상태가 도착하지 않은 사람은 이전 트랙 이름을 보여주지 않는다.
        const bool bSame = S.SelectionSessionId == Session.SelectionSessionId && S.OffersRevision == Session.OffersRevision;
        FString State = bSame ? (S.bConfirmed ? TEXT("선택 완료") : TEXT("선택 중")) : TEXT("참여 준비 중");
        const int32 OfferId = S.bConfirmed ? S.SelectedOfferId : S.HoveredOfferId;
        if (bSame)
        {
            const FMWTrackOffer* Offer = Session.Offers.FindByPredicate(
                [OfferId](const FMWTrackOffer& Candidate) { return Candidate.OfferId == OfferId; });
            const UMWTrackPieceDefinition* Definition = Offer
                ? UMWAssetManager::Get().FindLoadedAsset<UMWTrackPieceDefinition>(Offer->DefinitionAssetId) : nullptr;
            if (Definition) State = Definition->DisplayName.ToString() + TEXT(" · ") + State;
        }
        Lines += FString::Printf(TEXT("ID %02d · %s · %s\n"), PS->GetLobbyMemberId(), *PS->GetPlayerName(), *State);
    }
    const FText Status = SelectionStatus.IsEmpty()
        ? FText::FromString(TEXT("트랙 위에 마우스를 올려 확인하고, 왼쪽 버튼으로 선택하세요.")) : SelectionStatus;
    SelectionWidget->SetSelectionText(FText::FromString(Lines), Status);
}

void AMWPlayerController::BeginTrackSelection()
{
    // 정식 게임의 진행 API와 제작 맵의 재시작 API를 개발 콘솔에서 호출하는 연결점이다.
#if !UE_BUILD_SHIPPING
    if (!IsLocalController() || !HasAuthority()) return;
    if (AMWGameMode* GM = Cast<AMWGameMode>(GetWorld()->GetAuthGameMode()))
    {
        FText Error;
        if (!GM->BeginTrackSelection(Error)) UE_LOG(LogTemp, Warning, TEXT("%s"), *Error.ToString());
    }
    else if (AMWTrackConstructionTestGameMode* Test = Cast<AMWTrackConstructionTestGameMode>(GetWorld()->GetAuthGameMode()))
        Test->RestartSelection();
#endif
}

void AMWPlayerController::EndTrackSelection()
{
#if !UE_BUILD_SHIPPING
    if (!IsLocalController() || !HasAuthority()) return;
    AMWGameState* GS = GetWorld()->GetGameState<AMWGameState>();
    if (GS && GS->GetTrackConstructionComponent()) GS->GetTrackConstructionComponent()->EndSelection();
#endif
}
