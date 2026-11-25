#include "UI/Cursor/MWCursorProjection.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SceneViewport.h"
#include "UnrealClient.h"

namespace
{
	const FSceneViewport* FindActivePointerViewport(const APlayerController& PC)
	{
		const auto* LP = PC.GetLocalPlayer();
		const auto* Viewport = LP && LP->ViewportClient ? LP->ViewportClient->GetGameViewport() : nullptr;
		return PC.bShowMouseCursor && Viewport && Viewport->IsForegroundWindow() ? Viewport : nullptr;
	}
}

bool MWCursorProjection::HasActiveViewportPointer(const APlayerController& PC)
{
	return FindActivePointerViewport(PC) != nullptr;
}

bool MWCursorProjection::SampleViewportPointer(const APlayerController& PC, FVector2D& OutPosition)
{
	// 로딩 가림 중의 숨은 포인터와 비활성 PIE 창의 포인터는 입력으로 쓰지 않는다.
	const FSceneViewport* Viewport = FindActivePointerViewport(PC);
	if (!Viewport) return false;

	const FIntPoint Size = Viewport->GetSizeXY();
	if (Size.X <= 0 || Size.Y <= 0) return false;
	float X, Y;
	if (PC.GetMousePosition(X, Y))
	{
		OutPosition = FVector2D(X, Y);
	}
	else
	{
		// Slate가 마우스 이동을 아직 뷰포트에 전달하지 않았으면 현재 포인터를 같은 픽셀 공간으로 변환한다.
		const FGeometry& Geometry = Viewport->GetCachedGeometry();
		const FVector2D LocalSize = Geometry.GetLocalSize();
		if (!FSlateApplication::IsInitialized() || LocalSize.X <= 0.0 || LocalSize.Y <= 0.0) return false;
		OutPosition = Geometry.AbsoluteToLocal(FSlateApplication::Get().GetCursorPos()) * FVector2D(Size) / LocalSize;
	}
	return OutPosition.X >= 0.0 && OutPosition.Y >= 0.0 && OutPosition.X < Size.X && OutPosition.Y < Size.Y;
}

bool MWCursorProjection::SampleWorldPlane(const APlayerController& PC, const FVector& Point, const FVector& Normal, FVector& OutPosition)
{
	FVector2D Screen;
	FVector Origin, Direction;
	if (!SampleViewportPointer(PC, Screen) || !PC.DeprojectScreenPositionToWorld(Screen.X, Screen.Y, Origin, Direction)) return false;
	// 광선이 평면과 거의 평행하면 교점 계산 오차가 커지므로 표시할 좌표를 만들지 않는다.
	const double Dot = FVector::DotProduct(Direction, Normal);
	if (FMath::Abs(Dot) < KINDA_SMALL_NUMBER) return false;
	const double Distance = FVector::DotProduct(Point - Origin, Normal) / Dot;
	if (Distance < 0 || Distance > 100000) return false;
	OutPosition = Origin + Direction * Distance;
	return !OutPosition.ContainsNaN() && OutPosition.GetAbsMax() < 1000000;
}

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
