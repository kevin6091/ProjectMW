// 같은 커서 외형에 참가자별 색·닉네임과 로컬 여부를 적용한다.

#include "UI/VehicleSelection/MWVehiclePlayerMarkerWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"

// 참가자별 색·닉네임·로컬 여부로 같은 커서 외형을 갱신한다.
void UMWVehiclePlayerMarkerWidget::SetPresentation(const FMWVehiclePlayerPresentation& Player, bool bShowCursorName)
{
    if (!IsValid(Image_Badge) || !IsValid(Text_Player) || !IsValid(Text_State))
        return;
    SetRenderOpacity(Player.bConfirmed ? ConfirmedOpacity : Player.bHoverPreview ? HoverOpacity : SelectingOpacity);
    Image_Badge->SetColorAndOpacity(Player.PlayerTint);
    if (IsValid(Image_PointerArrow))
        Image_PointerArrow->SetColorAndOpacity(Player.PlayerTint);
    Text_Player->SetText(Player.MarkerText);
    Text_State->SetText(Player.bConfirmed ? ConfirmedText : Player.bHoverPreview ? HoverText : SelectingText);
    SetToolTipText(Player.PlayerName);
    if (IsValid(CursorPlayerName))
    {
        const bool bShowName = bShowCursorName && !Player.PlayerName.IsEmpty();
        if (bShowName && !CursorPlayerName->GetText().IdenticalTo(Player.PlayerName))
        {
            CursorPlayerName->SetText(Player.PlayerName);
        }
        CursorPlayerName->SetVisibility(bShowName ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
    if (IsValid(LocalIndicator))
        LocalIndicator->SetVisibility(Player.bLocal ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
