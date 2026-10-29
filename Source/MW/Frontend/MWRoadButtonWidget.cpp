#include "Frontend/MWRoadButtonWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"

void UMWRoadButtonWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	SetLabel(LabelText);
	if (Label) { FSlateFontInfo Font = Label->GetFont(); Font.Size = LabelFontSize; Label->SetFont(Font); }
	if (USizeBox* Frame = Cast<USizeBox>(GetWidgetFromName(TEXT("ArrowFrame"))))
	{
		Frame->SetWidthOverride(IconSize); Frame->SetHeightOverride(IconSize);
	}
}

void UMWRoadButtonWidget::SetLabel(const FText& Text)
{
	LabelText = Text;
	if (Label) { Label->SetText(Text); }
}

void UMWRoadButtonWidget::SetIconAngle(float Angle)
{
	if (Arrow) { Arrow->SetRenderTransformAngle(Angle); }
}

void UMWRoadButtonWidget::FocusButton(APlayerController* Controller)
{
	if (ActionButton && Controller) { ActionButton->SetUserFocus(Controller); }
}

void UMWRoadButtonWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!ActionButton) { return; }
	ActionButton->OnClicked.AddUniqueDynamic(this, &ThisClass::Click);
	ActionButton->OnHovered.AddUniqueDynamic(this, &ThisClass::Hover);
	ActionButton->OnUnhovered.AddUniqueDynamic(this, &ThisClass::Unhover);
	ActionButton->OnPressed.AddUniqueDynamic(this, &ThisClass::Press);
	ActionButton->OnReleased.AddUniqueDynamic(this, &ThisClass::Release);
}

void UMWRoadButtonWidget::NativeDestruct()
{
	// Blueprint reinstancing can clear BindWidget fields before Slate releases the old instance.
	if (IsValid(ActionButton))
	{
		ActionButton->OnClicked.RemoveDynamic(this, &ThisClass::Click);
		ActionButton->OnHovered.RemoveDynamic(this, &ThisClass::Hover);
		ActionButton->OnUnhovered.RemoveDynamic(this, &ThisClass::Unhover);
		ActionButton->OnPressed.RemoveDynamic(this, &ThisClass::Press);
		ActionButton->OnReleased.RemoveDynamic(this, &ThisClass::Release);
	}
	Super::NativeDestruct();
}

void UMWRoadButtonWidget::Click() { OnClicked.Broadcast(); }
void UMWRoadButtonWidget::Hover() { if (Arrow) { Arrow->SetColorAndOpacity(FLinearColor::White); } if (Label) { Label->SetColorAndOpacity(FSlateColor(FLinearColor::White)); } SetRenderTransformAngle(-1.f); }
void UMWRoadButtonWidget::Unhover() { if (Arrow) { Arrow->SetColorAndOpacity(FLinearColor::FromSRGBColor(FColor(55,52,53))); } if (Label) { Label->SetColorAndOpacity(FSlateColor(FLinearColor::FromSRGBColor(FColor(55, 52, 53)))); } SetRenderTransformAngle(0.f); }
void UMWRoadButtonWidget::Press() { SetRenderScale(FVector2D(.97f)); }
void UMWRoadButtonWidget::Release() { SetRenderScale(FVector2D(1.f)); }
