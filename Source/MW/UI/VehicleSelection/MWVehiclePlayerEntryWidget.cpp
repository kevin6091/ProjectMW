#include "UI/VehicleSelection/MWVehiclePlayerEntryWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"

void UMWVehiclePlayerEntryWidget::SetPresentation(const FMWVehiclePlayerPresentation& Player)
{
	if (!IsValid(Text_PlayerName) || !IsValid(Text_VehicleName) ||
		!IsValid(Text_SelectionState) || !IsValid(Image_Vehicle)) return;
	Text_PlayerName->SetText(Player.PlayerName);
	Text_VehicleName->SetText(Player.VehicleName);
	Text_SelectionState->SetText(Player.StatusText);
	Text_SelectionState->SetToolTipText(Player.StatusText);
	if (IsValid(Text_PlayerMarker)) Text_PlayerMarker->SetText(Player.MarkerText);
	SetToolTipText(Player.PlayerName);
	if (IsValid(Image_DisplayBackground)) Image_DisplayBackground->SetColorAndOpacity(
		Player.bConfirmed && !Player.Thumbnail.IsNull() ? ConfirmedDisplayTint : NeutralDisplayTint);
	if (IsValid(Image_IdentityBadge))
	{
		Image_IdentityBadge->SetColorAndOpacity(Player.PlayerTint);
		Image_IdentityBadge->SetVisibility(Player.bOccupied ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	Image_Vehicle->SetBrushFromSoftTexture(Player.Thumbnail, true);
	Image_Vehicle->SetVisibility(Player.Thumbnail.IsNull() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	const bool bHasSelectionFrame = IsValid(LocalFrame);
	if (bHasSelectionFrame)
	{
		LocalFrame->SetBrushTintColor(FSlateColor(Player.PlayerTint));
	}
	if (IsValid(LocalIndicator))
	{
		if (bHasSelectionFrame)
		{
			// The authored legacy indicator is transparent. Visibility now controls the frame.
			LocalIndicator->SetRenderOpacity(1.f);
		}
		const bool bShowFrame = bHasSelectionFrame ? Player.bOccupied && Player.bConfirmed : Player.bLocal;
		LocalIndicator->SetVisibility(bShowFrame ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (IsValid(SelectionDetails)) SelectionDetails->SetVisibility(Player.bOccupied
		? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
}
