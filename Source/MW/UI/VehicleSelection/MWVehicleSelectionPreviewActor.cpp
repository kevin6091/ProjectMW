// 기존 차량 선택 위젯을 월드 게시판에 표시하는 개발 호스트다. 서버 판정과 구분한다.

#include "UI/VehicleSelection/MWVehicleSelectionPreviewActor.h"

#include "Components/WidgetComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UI/VehicleSelection/MWVehicleSelectionWidget.h"

AMWVehicleSelectionPreviewActor::AMWVehicleSelectionPreviewActor()
{
    PrimaryActorTick.bCanEverTick = false;
    PreviewSurface = CreateDefaultSubobject<UWidgetComponent>(TEXT("PreviewSurface"));
    SetRootComponent(PreviewSurface);
}

// 설정된 개발 표시 시작 정책을 따르고 기존 차량 위젯의 월드 표면을 준비한다.
void AMWVehicleSelectionPreviewActor::BeginPlay()
{
    Super::BeginPlay();
    PreviewSurface->SetVisibility(false);
    PreviewSurface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (bOpenOnBeginPlay)
        OpenPreview();
}

// 기존 개발 표시 위젯을 준비해 월드 표면에 연결한다. Pawn이나 서버 경기를 만들지 않는다.
bool AMWVehicleSelectionPreviewActor::OpenPreview()
{
    if (IsValid(PreviewWidget))
        return false;
    if (!GetWorld() || GetWorld()->WorldType != EWorldType::PIE || !PreviewWidgetClass ||
        PreviewSurface->GetWidgetSpace() != EWidgetSpace::World || !PreviewSurface->GetReceiveHardwareInput())
        return false;
    APlayerController* Controller = GetWorld()->GetFirstPlayerController();
    if (!IsValid(Controller) || !Controller->IsLocalController())
        return false;
    PreviewWidget = CreateWidget<UMWVehicleSelectionWidget>(Controller, PreviewWidgetClass);
    if (!PreviewWidget)
        return false;
    PreviewController = Controller;
    bPreviousCursorVisible = Controller->bShowMouseCursor;
    PreviousViewTarget = Controller->GetViewTarget();
    if (IsValid(PreviewViewTarget))
        Controller->SetViewTarget(PreviewViewTarget);
    PreviewWidget->OnBackRequested.AddUniqueDynamic(this, &ThisClass::ClosePreview);
    PreviewSurface->SetOwnerPlayer(Controller->GetLocalPlayer());
    PreviewSurface->SetWidget(PreviewWidget);
    PreviewSurface->SetVisibility(true);
    PreviewSurface->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Controller->bShowMouseCursor = true;
    FInputModeGameAndUI Mode;
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    Mode.SetHideCursorDuringCapture(false);
    Controller->SetInputMode(Mode);
    return true;
}

// 위젯·입력 표시를 정리해 개발 화면을 닫은 뒤 참조가 남지 않게 한다.
void AMWVehicleSelectionPreviewActor::ClosePreview()
{
    if (!PreviewWidget && !PreviewController.IsValid())
        return;
    if (IsValid(PreviewWidget))
        PreviewWidget->OnBackRequested.RemoveDynamic(this, &ThisClass::ClosePreview);
    if (IsValid(PreviewSurface))
    {
        PreviewSurface->SetWidget(nullptr);
        PreviewSurface->SetVisibility(false);
        PreviewSurface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    PreviewWidget = nullptr;
    if (APlayerController* Controller = PreviewController.Get())
    {
        Controller->bShowMouseCursor = bPreviousCursorVisible;
        if (AActor* ViewTarget = PreviousViewTarget.Get())
            Controller->SetViewTarget(ViewTarget);
        Controller->SetInputMode(FInputModeGameOnly());
    }
    PreviewController.Reset();
    PreviousViewTarget.Reset();
    if (GetWorld() && !GetWorld()->bIsTearingDown)
        OnPreviewClosed.Broadcast();
}

// 개발 위젯을 닫아 입력과 화면 참조를 정리한 뒤 액터 수명을 종료한다.
void AMWVehicleSelectionPreviewActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClosePreview();
    Super::EndPlay(EndPlayReason);
}
