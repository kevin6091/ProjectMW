#include "Frontend/MWFrontendPlayerController.h"

#include "Frontend/MWFrontendWidget.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/PackageName.h"
#include "LogChannel.h"

AMWFrontendPlayerController::AMWFrontendPlayerController()
{
	// A blank authoring validation copy must be able to create this asset before it exists.
	// Runtime creation still reports an explicit error below if the dependency is absent.
	if (FPackageName::DoesPackageExist(TEXT("/Game/UI/Frontend/WBP_FrontendRoot")))
	{
		static ConstructorHelpers::FClassFinder<UMWFrontendWidget> Widget(TEXT("/Game/UI/Frontend/WBP_FrontendRoot"));
		FrontendClass = Widget.Class;
	}
}

void AMWFrontendPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController() || Frontend) { return; }
	Frontend = CreateWidget<UMWFrontendWidget>(this, FrontendClass);
	if (!Frontend)
	{
		UE_LOG(LogDefault, Error, TEXT("Frontend could not create WBP_FrontendRoot."));
		return;
	}
	Frontend->AddToViewport();
	Frontend->OnPresentationChanged.AddUniqueDynamic(this, &ThisClass::HandlePresentationChanged);
	HandlePresentationChanged(true);
}

void AMWFrontendPlayerController::HandlePresentationChanged(bool bFrontendVisible)
{
	if (!IsLocalController()) { return; }
	if (!bFrontendVisible)
	{
		bShowMouseCursor = false;
		SetInputMode(FInputModeGameOnly());
		return;
	}
	if (!Frontend) { return; }
	bShowMouseCursor = true;
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Frontend->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
}

void AMWFrontendPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Frontend)
	{
		Frontend->OnPresentationChanged.RemoveDynamic(this, &ThisClass::HandlePresentationChanged);
		Frontend->RemoveFromParent();
		Frontend = nullptr;
	}
	Super::EndPlay(Reason);
}

AMWFrontendGameMode::AMWFrontendGameMode()
{
	PlayerControllerClass = AMWFrontendPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
}
