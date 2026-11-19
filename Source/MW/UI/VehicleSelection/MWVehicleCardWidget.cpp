#include "UI/VehicleSelection/MWVehicleCardWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Data/MWVehicleDefinitionData.h"
#include "UI/VehicleSelection/MWVehiclePlayerMarkerWidget.h"

// 기존 카드 버튼의 클릭·호버·누름 이벤트를 한 번 연결하고 현재 외형을 반영한다.
void UMWVehicleCardWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (IsValid(Button_Vehicle))
	{
		Button_Vehicle->SetClickMethod(EButtonClickMethod::PreciseClick);
		Button_Vehicle->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleChosen);
		Button_Vehicle->OnHovered.AddUniqueDynamic(this, &ThisClass::HandleHovered);
		Button_Vehicle->OnUnhovered.AddUniqueDynamic(this, &ThisClass::HandleUnhovered);
		Button_Vehicle->OnPressed.AddUniqueDynamic(this, &ThisClass::HandlePressed);
		Button_Vehicle->OnReleased.AddUniqueDynamic(this, &ThisClass::HandleReleased);
	}
	Refresh();
}

// 이 카드가 연결한 버튼 이벤트와 표시 참조를 해제한다.
void UMWVehicleCardWidget::NativeDestruct()
{
    if (IsValid(Button_Vehicle))
    {
        Button_Vehicle->OnClicked.RemoveDynamic(this, &ThisClass::HandleChosen);
        Button_Vehicle->OnHovered.RemoveDynamic(this, &ThisClass::HandleHovered);
        Button_Vehicle->OnUnhovered.RemoveDynamic(this, &ThisClass::HandleUnhovered);
        Button_Vehicle->OnPressed.RemoveDynamic(this, &ThisClass::HandlePressed);
        Button_Vehicle->OnReleased.RemoveDynamic(this, &ThisClass::HandleReleased);
    }
    bHovered = false;
    bPressed = false;
    PlayerMarkers.Reset();
    if (IsValid(Grid_PlayerMarkers))
        Grid_PlayerMarkers->ClearChildren();
    Super::NativeDestruct();
}

// 기존 카드의 참가자 표시 데이터를 갱신한다. 차량 ID나 서버 승인은 변경하지 않는다.
void UMWVehicleCardWidget::SetParticipants(const TArray<FMWVehiclePlayerPresentation>& Participants)
{
    bHasLocalParticipant = false;
    for (const FMWVehiclePlayerPresentation& Player : Participants)
    {
        if (!Player.bLocal)
            continue;
        bHasLocalParticipant = true;
        LocalParticipantTint = Player.PlayerTint;
        break;
    }
    if (!IsValid(Grid_PlayerMarkers) || !MarkerWidgetClass || MarkerColumnCount <= 0 || !GetOwningPlayer())
        return;
    if (PlayerMarkers.Num() != Participants.Num())
    {
        Grid_PlayerMarkers->ClearChildren();
        PlayerMarkers.Reset();
        for (int32 Index = 0; Index < Participants.Num(); ++Index)
        {
            UMWVehiclePlayerMarkerWidget* Marker =
                CreateWidget<UMWVehiclePlayerMarkerWidget>(GetOwningPlayer(), MarkerWidgetClass);
            if (!Marker)
                continue;
            Grid_PlayerMarkers->AddChildToUniformGrid(Marker, Index / MarkerColumnCount, Index % MarkerColumnCount);
            PlayerMarkers.Add(Marker);
        }
    }
    for (int32 Index = 0; Index < PlayerMarkers.Num(); ++Index)
        PlayerMarkers[Index]->SetPresentation(Participants[Index]);
    Refresh();
}

// 실제 차량 정의와 카탈로그 ID를 카드 표시 모델에 연결한다.
void UMWVehicleCardWidget::SetVehicle(FGameplayTag AssetId, UMWVehicleDefinitionData* InDefinition,
                                      TSoftObjectPtr<UTexture2D> InThumbnail, bool bInAvailable, const FText& InReason)
{
    const FText Name = IsValid(InDefinition) && !InDefinition->GetDisplayName().IsEmpty()
                           ? InDefinition->GetDisplayName()
                           : FText::FromName(AssetId.GetTagName());
    SetPreview(Name, InThumbnail, bInAvailable && AssetId.IsValid() && IsValid(InDefinition), InReason);
    VehicleAssetID = AssetId;
}

// 외형 검토용 이름·이미지를 사용하며 실제 게임 차량으로 등록하지 않는다.
void UMWVehicleCardWidget::SetPreview(const FText& Name, TSoftObjectPtr<UTexture2D> InThumbnail, bool bInAvailable,
                                      const FText& InReason)
{
    VehicleAssetID = FGameplayTag::EmptyTag;
    DisplayName = Name;
    Thumbnail = InThumbnail;
    bAvailable = bInAvailable;
    UnavailableReason = InReason;
    bSelected = false;
    bInteractionLocked = false;
    Refresh();
}

// 화면에서 고른 카드 표시만 바꾸고 실제 서버 확정 값은 건드리지 않는다.
void UMWVehicleCardWidget::SetSelected(bool bInSelected)
{
    bSelected = bInSelected;
    Refresh();
}

// 대기·완료 상태에서 입력만 잠그고 카드 내용은 읽을 수 있게 둔다.
void UMWVehicleCardWidget::SetInteractionLocked(bool bLocked)
{
    bInteractionLocked = bLocked;
    if (bLocked)
    {
        bHovered = false;
        bPressed = false;
    }
    Refresh();
}

// 실제 선택·잠금·사용 가능 상태에 맞춰 카드 외형과 입력을 함께 갱신한다.
void UMWVehicleCardWidget::Refresh()
{
    if (!IsValid(Button_Vehicle) || !IsValid(Image_Frame) || !IsValid(Image_Outline) || !IsValid(Image_Vehicle) ||
        !IsValid(Text_Name) || !IsValid(Text_Unavailable) || !IsValid(SelectionMarker) || !IsValid(MissingImageNotice))
        return;

    Button_Vehicle->SetIsEnabled(bAvailable);
    // 선택 입력이 잠겨도 확정한 Preview의 내용은 읽을 수 있게 유지한다.
    Button_Vehicle->SetVisibility(bInteractionLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Visible);
    Text_Name->SetText(DisplayName);
    Text_Name->SetToolTipText(DisplayName);
    Button_Vehicle->SetToolTipText(DisplayName);
    Text_Unavailable->SetText(UnavailableReason);
    Text_Unavailable->SetVisibility(bAvailable ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    Image_Vehicle->SetBrushFromSoftTexture(Thumbnail, true);
    Image_Vehicle->SetRenderOpacity(bAvailable ? 1.f : UnavailableImageOpacity);
    Image_Vehicle->SetVisibility(Thumbnail.IsNull() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    MissingImageNotice->SetVisibility(Thumbnail.IsNull() ? ESlateVisibility::HitTestInvisible
                                                         : ESlateVisibility::Collapsed);
    SelectionMarker->SetVisibility(ESlateVisibility::Collapsed);
    const FLinearColor Tint = !bAvailable ? UnavailableTint
                              : bSelected ? (bHasLocalParticipant ? LocalParticipantTint : SelectedTint)
                              : bHovered  ? HoverTint
                                          : NormalTint;
    Image_Outline->SetColorAndOpacity(Tint);
    Image_Frame->SetColorAndOpacity(bPressed ? PressedFrameTint : FrameTint);
}

// 사용 가능한 카드의 선택 의도를 위젯 소유자에게 알린다.
void UMWVehicleCardWidget::HandleChosen()
{
    if (bAvailable && !bInteractionLocked)
        OnChosen.Broadcast(this);
}

// 호버 상태를 갱신하고 기존 선택 화면에 해당 카드의 포커스 의도를 전달한다.
void UMWVehicleCardWidget::HandleHovered()
{
    bHovered = true;
    Refresh();
    OnHoverChanged.Broadcast(this, true);
}

// 호버가 끝난 카드 표시를 되돌려 이전 강조가 남지 않게 한다.
void UMWVehicleCardWidget::HandleUnhovered()
{
    bHovered = false;
    Refresh();
    OnHoverChanged.Broadcast(this, false);
}

// 누르는 동안의 표시 상태를 갱신하고 실제 실행은 클릭 처리에 맡긴다.
void UMWVehicleCardWidget::HandlePressed()
{
    if (!bAvailable || bInteractionLocked)
        return;
    bPressed = true;
    Refresh();
}

// 누름 표시를 해제하고 현재 선택·호버 기준으로 외형을 갱신한다.
void UMWVehicleCardWidget::HandleReleased()
{
    bPressed = false;
    Refresh();
}
