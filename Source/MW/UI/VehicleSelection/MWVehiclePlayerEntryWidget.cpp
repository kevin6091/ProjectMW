// 선택 화면 하단 참가자의 이름·차량·확정 색 테두리를 표시한다.

#include "UI/VehicleSelection/MWVehiclePlayerEntryWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"

// 서버 상태로 만든 참가자 정보를 표시하고 확정 슬롯 테두리에 플레이어 색을 반영한다.
void UMWVehiclePlayerEntryWidget::SetPresentation(const FMWVehiclePlayerPresentation& Player)
{
    if (!IsValid(Text_PlayerName) || !IsValid(Text_VehicleName) || !IsValid(Text_SelectionState) ||
        !IsValid(Image_Vehicle))
        return;
    Text_PlayerName->SetText(Player.PlayerName);
    Text_VehicleName->SetText(Player.VehicleName);
    Text_SelectionState->SetText(Player.StatusText);
    Text_SelectionState->SetToolTipText(Player.StatusText);
    if (IsValid(Text_PlayerMarker))
        Text_PlayerMarker->SetText(Player.MarkerText);
    SetToolTipText(Player.PlayerName);
    if (IsValid(Image_DisplayBackground))
        Image_DisplayBackground->SetColorAndOpacity(
            Player.bConfirmed && !Player.Thumbnail.IsNull() ? ConfirmedDisplayTint : NeutralDisplayTint);
    if (IsValid(Image_IdentityBadge))
    {
        Image_IdentityBadge->SetColorAndOpacity(Player.PlayerTint);
        Image_IdentityBadge->SetVisibility(Player.bOccupied ? ESlateVisibility::HitTestInvisible
                                                            : ESlateVisibility::Collapsed);
    }
    Image_Vehicle->SetBrushFromSoftTexture(Player.Thumbnail, true);
    Image_Vehicle->SetVisibility(Player.Thumbnail.IsNull() ? ESlateVisibility::Collapsed
                                                           : ESlateVisibility::HitTestInvisible);
    const bool bHasSelectionFrame = IsValid(LocalFrame);
    if (bHasSelectionFrame)
    {
        LocalFrame->SetBrushTintColor(FSlateColor(Player.PlayerTint));
    }
    if (IsValid(LocalIndicator))
    {
        if (bHasSelectionFrame)
        {
            // 기존 표시 이미지가 투명하므로 테두리의 표시 여부로 확정 상태를 구분한다.
            LocalIndicator->SetRenderOpacity(1.f);
        }
        const bool bShowFrame = bHasSelectionFrame ? Player.bOccupied && Player.bConfirmed : Player.bLocal;
        LocalIndicator->SetVisibility(bShowFrame ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
    if (IsValid(SelectionDetails))
        SelectionDetails->SetVisibility(Player.bOccupied ? ESlateVisibility::HitTestInvisible
                                                         : ESlateVisibility::Hidden);
}
