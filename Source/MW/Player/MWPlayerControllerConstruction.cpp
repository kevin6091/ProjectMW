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
    if (HasActorBegunPlay()) RefreshConstructionPresentation();
}

void AMWPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
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
    TGuardValue<bool> Guard(bRefreshingConstruction, true);
    if (!BoundConstruction.IsValid()) return;
    const FMWTrackSelectionSession Session = BoundConstruction->GetSelectionSession();
    if (!Session.bIsOpen) { StopConstructionPresentation(); return; }
    if (DisplaySessionId != 0 && (DisplaySessionId != Session.SelectionSessionId || DisplayOffersRevision != Session.OffersRevision))
        StopConstructionPresentation();

    AMWPlayerState* OwnPS = GetPlayerState<AMWPlayerState>();
    const UMWPlayerConstructionComponent* Own = OwnPS ? OwnPS->GetPlayerConstructionComponent() : nullptr;
    if (!Own) return;
    const FMWPlayerTrackSelection Selection = Own->GetSelection();
    if (Selection.SelectionSessionId != Session.SelectionSessionId || Selection.OffersRevision != Session.OffersRevision) return;
    if (!SelectionBoard.IsValid())
    {
        for (TActorIterator<AMWTrackSelectionBoard> It(GetWorld()); It; ++It)
        {
            SelectionBoard = *It;
            break;
        }
    }
    if (!SelectionBoard.IsValid()) return;
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
        PreviousSelectionViewTarget = GetViewTarget();
        SetViewTargetWithBlend(SelectionBoard.Get(), 0.25f);
        SelectionWidget = CreateWidget<UMWTrackSelectionWidget>(this, UMWTrackSelectionWidget::StaticClass());
        if (SelectionWidget) SelectionWidget->AddToViewport(20);
    }
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
    SelectionBindingHandle = EIC->BindAction(Select, ETriggerEvent::Started, this, &ThisClass::HandleTrackSelectPressed).GetHandle();
    SelectionInputComponent = EIC;
    FModifyContextOptions Options;
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
    FHitResult Hit;
    if (!GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), false, Hit)) return INDEX_NONE;
    const AMWTrackOfferPreview* Preview = Cast<AMWTrackOfferPreview>(Hit.GetActor());
    return Preview && Preview->GetBoard() == SelectionBoard.Get() ? Preview->GetOfferId() : INDEX_NONE;
}

bool AMWPlayerController::IsDisplayedTrackOffer(int32 OfferId, bool bAllowNone) const
{
    if (!BoundConstruction.IsValid()) return false;
    const FMWTrackSelectionSession Session = BoundConstruction->GetSelectionSession();
    return Session.bIsOpen && Session.SelectionSessionId == DisplaySessionId && Session.OffersRevision == DisplayOffersRevision
        && ((bAllowNone && OfferId == INDEX_NONE) || Session.Offers.ContainsByPredicate(
            [OfferId](const FMWTrackOffer& Offer) { return Offer.OfferId == OfferId; }));
}

void AMWPlayerController::UpdateLocalTrackHover(int32 OfferId)
{
    if (!IsDisplayedTrackOffer(OfferId, true)) return;
    if (LocalHoveredOfferId != OfferId)
    {
        LocalHoveredOfferId = OfferId;
        if (SelectionBoard.IsValid()) SelectionBoard->RefreshMarkers(this, BoundConstruction->GetSelectionSession(), OfferId);
    }
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now - LastHoverSendTime < 0.1 || LastSentHoveredOfferId == OfferId) return;
    LastHoverSendTime = Now;
    LastSentHoveredOfferId = OfferId;
    ServerUpdateTrackHover(DisplaySessionId, DisplayOffersRevision, OfferId);
}

void AMWPlayerController::HandleTrackSelectPressed() { ConfirmLocalTrackSelection(FindHoveredTrackOffer()); }

void AMWPlayerController::ConfirmLocalTrackSelection(int32 OfferId)
{
    if (!IsLocalController() || !bSelectionInputInstalled || bConfirmPending
        || !IsDisplayedTrackOffer(OfferId, false) || NextSelectionRequestId == MAX_int32) return;
    bConfirmPending = true;
    PendingSelectionRequestId = ++NextSelectionRequestId;
    PendingSelectedOfferId = OfferId;
    SelectionStatus = FText::FromString(TEXT("선택을 확인하고 있습니다..."));
    RefreshSelectionText(BoundConstruction->GetSelectionSession());
    ServerConfirmTrackSelection(DisplaySessionId, DisplayOffersRevision, OfferId, PendingSelectionRequestId);
}

void AMWPlayerController::ServerUpdateTrackHover_Implementation(int32 SessionId, int32 Revision, int32 OfferId)
{
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
    ClientTrackSelectionResult(Reply);
}

void AMWPlayerController::ClientTrackSelectionResult_Implementation(const FMWTrackSelectionReply& Reply)
{
    if (!bSelectionPresentationActive || !bConfirmPending || Reply.SelectionSessionId != DisplaySessionId
        || Reply.OffersRevision != DisplayOffersRevision || Reply.RequestId != PendingSelectionRequestId
        || Reply.OfferId != PendingSelectedOfferId) return;
    bConfirmPending = false;
    PendingSelectionRequestId = 0;
    PendingSelectedOfferId = INDEX_NONE;
    if (Reply.bAccepted)
    {
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
    Players.Sort([](const AMWPlayerState& A, const AMWPlayerState& B) { return A.GetLobbyMemberId() < B.GetLobbyMemberId(); });
    FString Lines;
    for (const AMWPlayerState* PS : Players)
    {
        const FMWPlayerTrackSelection S = PS->GetPlayerConstructionComponent()->GetSelection();
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
