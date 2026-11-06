#include "UI/Cursor/MWCursorProjection.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "UnrealClient.h"

bool MWCursorProjection::SampleNormalized(const UUserWidget& Widget, const UWidget& ReferenceLayer,
	const UWidgetComponent* Surface, APlayerController& PC, FVector2D& OutPosition)
{
	const ULocalPlayer* LP = PC.GetLocalPlayer();
	const FViewport* Viewport = LP && LP->ViewportClient ? LP->ViewportClient->Viewport : nullptr;
	if (!PC.IsLocalController() || !Viewport || !Viewport->IsForegroundWindow() || !PC.bShowMouseCursor) return false;
	float MouseX, MouseY;
	int32 Width, Height;
	PC.GetViewportSize(Width, Height);
	if (!PC.GetMousePosition(MouseX, MouseY) || MouseX < 0 || MouseY < 0 || MouseX >= Width || MouseY >= Height) return false;
	const FGeometry& LayerGeometry = ReferenceLayer.GetCachedGeometry();
	const FVector2D LayerSize = LayerGeometry.GetLocalSize();
	if (LayerSize.X <= 0 || LayerSize.Y <= 0) return false;
	FVector2D Absolute;
	if (Surface)
	{
		if (!Surface->IsVisible() || Surface->GetUserWidgetObject() != &Widget
			|| Surface->GetGeometryMode() != EWidgetGeometryMode::Plane) return false;
		FVector Origin, Direction;
		if (!PC.DeprojectMousePositionToWorld(Origin, Direction)) return false;
		const FVector Normal = Surface->GetForwardVector();
		const double Denominator = FVector::DotProduct(Direction, Normal);
		if (FMath::Abs(Denominator) < KINDA_SMALL_NUMBER) return false;
		const double Distance = FVector::DotProduct(Surface->GetComponentLocation() - Origin, Normal) / Denominator;
		if (Distance < 0 || Distance > 100000.0) return false;
		FVector2D DrawPosition;
		Surface->GetLocalHitLocation(Origin + Direction * Distance, DrawPosition);
		const FVector2D DrawSize = Surface->GetCurrentDrawSize();
		if (DrawSize.X <= 0 || DrawSize.Y <= 0) return false;
		const FGeometry& RootGeometry = Widget.GetCachedGeometry();
		Absolute = RootGeometry.LocalToAbsolute(DrawPosition / DrawSize * RootGeometry.GetLocalSize());
	}
	else
	{
		if (!Widget.IsInViewport() || !FSlateApplication::IsInitialized()) return false;
		Absolute = FSlateApplication::Get().GetCursorPos();
	}
	OutPosition = LayerGeometry.AbsoluteToLocal(Absolute) / LayerSize;
	return !OutPosition.ContainsNaN() && OutPosition.X >= 0 && OutPosition.X <= 1 && OutPosition.Y >= 0 && OutPosition.Y <= 1;
}

FVector MWCursorProjection::WidgetPointToWorld(const UWidgetComponent& Surface, const FVector2D& DrawPosition)
{
	const FVector2D Point = DrawPosition - Surface.GetCurrentDrawSize() * Surface.GetPivot();
	// UWidgetComponent::GetLocalHitLocation의 역변환. 표면의 X축이 법선이다.
	return Surface.GetComponentTransform().TransformPosition(FVector(0.0, -Point.X, -Point.Y));
}

bool MWCursorProjection::ToViewport(const UUserWidget& Widget, const UWidget& ReferenceLayer,
	const UWidgetComponent* Surface, const FVector2D& NormalizedPosition, APlayerController& PC, FVector2D& OutPosition)
{
	if (NormalizedPosition.ContainsNaN() || NormalizedPosition.X < 0 || NormalizedPosition.X > 1
		|| NormalizedPosition.Y < 0 || NormalizedPosition.Y > 1) return false;
	const FGeometry& LayerGeometry = ReferenceLayer.GetCachedGeometry();
	const FVector2D LayerSize = LayerGeometry.GetLocalSize();
	if (LayerSize.X <= 0 || LayerSize.Y <= 0) return false;
	const FVector2D Absolute = LayerGeometry.LocalToAbsolute(NormalizedPosition * LayerSize);
	if (Surface)
	{
		if (!Surface->IsVisible() || Surface->GetUserWidgetObject() != &Widget
			|| Surface->GetGeometryMode() != EWidgetGeometryMode::Plane) return false;
		const FGeometry& RootGeometry = Widget.GetCachedGeometry();
		const FVector2D RootSize = RootGeometry.GetLocalSize();
		const FVector2D DrawSize = Surface->GetCurrentDrawSize();
		if (RootSize.X <= 0 || RootSize.Y <= 0 || DrawSize.X <= 0 || DrawSize.Y <= 0) return false;
		// World UI의 Absolute는 가상 Slate 창 좌표다. 직접 Viewport 좌표로 쓰지 않는다.
		const FVector2D DrawPosition = RootGeometry.AbsoluteToLocal(Absolute) / RootSize * DrawSize;
		return UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(&PC,
			WidgetPointToWorld(*Surface, DrawPosition), OutPosition, false);
	}
	if (!Widget.IsInViewport()) return false;
	const FGeometry ViewportGeometry = UWidgetLayoutLibrary::GetViewportWidgetGeometry(&Widget);
	// GetViewportWidgetGeometry는 DPI가 적용된 게임 화면의 논리 좌표계를 반환한다.
	OutPosition = ViewportGeometry.AbsoluteToLocal(Absolute);
	return !OutPosition.ContainsNaN();
}
