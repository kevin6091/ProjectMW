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

void AMWVehicleSelectionPreviewActor::BeginPlay()
{
	Super::BeginPlay();
	PreviewSurface->SetVisibility(false);
	PreviewSurface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (bOpenOnBeginPlay) OpenPreview();
}

bool AMWVehicleSelectionPreviewActor::OpenPreview()
{
	if (IsValid(PreviewWidget)) return false;
	if (!GetWorld() || GetWorld()->WorldType != EWorldType::PIE || !PreviewWidgetClass ||
		PreviewSurface->GetWidgetSpace() != EWidgetSpace::World || !PreviewSurface->GetReceiveHardwareInput()) return false;
	APlayerController* Controller = GetWorld()->GetFirstPlayerController();
	if (!IsValid(Controller) || !Controller->IsLocalController()) return false;
	PreviewWidget = CreateWidget<UMWVehicleSelectionWidget>(Controller, PreviewWidgetClass);
	if (!PreviewWidget) return false;
	PreviewController = Controller;
	bPreviousCursorVisible = Controller->bShowMouseCursor;
	PreviousViewTarget = Controller->GetViewTarget();
	if (IsValid(PreviewViewTarget)) Controller->SetViewTarget(PreviewViewTarget);
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

void AMWVehicleSelectionPreviewActor::ClosePreview()
{
	if (!PreviewWidget && !PreviewController.IsValid()) return;
	if (IsValid(PreviewWidget)) PreviewWidget->OnBackRequested.RemoveDynamic(this, &ThisClass::ClosePreview);
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
		if (AActor* ViewTarget = PreviousViewTarget.Get()) Controller->SetViewTarget(ViewTarget);
		Controller->SetInputMode(FInputModeGameOnly());
	}
	PreviewController.Reset();
	PreviousViewTarget.Reset();
	if (GetWorld() && !GetWorld()->bIsTearingDown) OnPreviewClosed.Broadcast();
}

void AMWVehicleSelectionPreviewActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClosePreview();
	Super::EndPlay(EndPlayReason);
}
