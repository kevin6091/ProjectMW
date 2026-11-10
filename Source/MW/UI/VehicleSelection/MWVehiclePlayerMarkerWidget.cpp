#include "UI/VehicleSelection/MWVehiclePlayerMarkerWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"

void UMWVehiclePlayerMarkerWidget::SetPresentation(const FMWVehiclePlayerPresentation& Player)
{
	if (!IsValid(Image_Badge) || !IsValid(Text_Player) || !IsValid(Text_State)) return;
	SetRenderOpacity(Player.bConfirmed ? ConfirmedOpacity : Player.bHoverPreview ? HoverOpacity : SelectingOpacity);
	Image_Badge->SetColorAndOpacity(Player.PlayerTint);
	if (IsValid(Image_PointerArrow)) Image_PointerArrow->SetColorAndOpacity(Player.PlayerTint);
	Text_Player->SetText(Player.MarkerText);
	Text_State->SetText(Player.bConfirmed ? ConfirmedText : Player.bHoverPreview ? HoverText : SelectingText);
	SetToolTipText(Player.PlayerName);
	if (IsValid(LocalIndicator)) LocalIndicator->SetVisibility(Player.bLocal
		? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
