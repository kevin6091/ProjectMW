#include "Frontend/MWRoadButtonWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

void UMWRoadButtonWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	SetLabel(LabelText);
}

void UMWRoadButtonWidget::SetLabel(const FText& Text)
{
	LabelText = Text;
	if (Label) { Label->SetText(Text); }
}

void UMWRoadButtonWidget::FocusButton(APlayerController* Controller)
{
	if (ActionButton && Controller) { ActionButton->SetUserFocus(Controller); }
}

void UMWRoadButtonWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ActionButton->OnClicked.AddUniqueDynamic(this, &ThisClass::Click);
	ActionButton->OnHovered.AddUniqueDynamic(this, &ThisClass::Hover);
	ActionButton->OnUnhovered.AddUniqueDynamic(this, &ThisClass::Unhover);
	ActionButton->OnPressed.AddUniqueDynamic(this, &ThisClass::Press);
	ActionButton->OnReleased.AddUniqueDynamic(this, &ThisClass::Release);
}

void UMWRoadButtonWidget::NativeDestruct()
{
	ActionButton->OnClicked.RemoveDynamic(this, &ThisClass::Click);
	ActionButton->OnHovered.RemoveDynamic(this, &ThisClass::Hover);
	ActionButton->OnUnhovered.RemoveDynamic(this, &ThisClass::Unhover);
	ActionButton->OnPressed.RemoveDynamic(this, &ThisClass::Press);
	ActionButton->OnReleased.RemoveDynamic(this, &ThisClass::Release);
	Super::NativeDestruct();
}

void UMWRoadButtonWidget::Click() { OnClicked.Broadcast(); }
void UMWRoadButtonWidget::Hover() { Label->SetColorAndOpacity(FSlateColor(FLinearColor::White)); SetRenderTransformAngle(-1.f); }
void UMWRoadButtonWidget::Unhover() { Label->SetColorAndOpacity(FSlateColor(FLinearColor::FromSRGBColor(FColor(55, 52, 53)))); SetRenderTransformAngle(0.f); }
void UMWRoadButtonWidget::Press() { SetRenderScale(FVector2D(.97f)); }
void UMWRoadButtonWidget::Release() { SetRenderScale(FVector2D(1.f)); }
