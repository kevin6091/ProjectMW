#include "UI/MWVehicleCardWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Data/MWVehicleDefinitionData.h"
#include "UI/MWVehiclePlayerMarkerWidget.h"

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
	if (IsValid(Grid_PlayerMarkers)) Grid_PlayerMarkers->ClearChildren();
	Super::NativeDestruct();
}

void UMWVehicleCardWidget::SetParticipants(const TArray<FMWVehiclePlayerPresentation>& Participants)
{
	bHasLocalParticipant = false;
	for (const FMWVehiclePlayerPresentation& Player : Participants)
	{
		if (!Player.bLocal) continue;
		bHasLocalParticipant = true;
		LocalParticipantTint = Player.PlayerTint;
		break;
	}
	if (!IsValid(Grid_PlayerMarkers) || !MarkerWidgetClass || MarkerColumnCount <= 0 || !GetOwningPlayer()) return;
	if (PlayerMarkers.Num() != Participants.Num())
	{
		Grid_PlayerMarkers->ClearChildren();
		PlayerMarkers.Reset();
		for (int32 Index = 0; Index < Participants.Num(); ++Index)
		{
			UMWVehiclePlayerMarkerWidget* Marker = CreateWidget<UMWVehiclePlayerMarkerWidget>(GetOwningPlayer(), MarkerWidgetClass);
			if (!Marker) continue;
			Grid_PlayerMarkers->AddChildToUniformGrid(Marker, Index / MarkerColumnCount, Index % MarkerColumnCount);
			PlayerMarkers.Add(Marker);
		}
	}
	for (int32 Index = 0; Index < PlayerMarkers.Num(); ++Index) PlayerMarkers[Index]->SetPresentation(Participants[Index]);
	Refresh();
}

void UMWVehicleCardWidget::SetVehicle(FGameplayTag AssetId, UMWVehicleDefinitionData* InDefinition,
	TSoftObjectPtr<UTexture2D> InThumbnail, bool bInAvailable, const FText& InReason)
{
	const FText Name = IsValid(InDefinition) && !InDefinition->GetDisplayName().IsEmpty()
		? InDefinition->GetDisplayName() : FText::FromName(AssetId.GetTagName());
	SetPreview(Name, InThumbnail, bInAvailable && AssetId.IsValid() && IsValid(InDefinition), InReason);
	VehicleAssetID = AssetId;
}

void UMWVehicleCardWidget::SetPreview(const FText& Name, TSoftObjectPtr<UTexture2D> InThumbnail,
	bool bInAvailable, const FText& InReason)
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

void UMWVehicleCardWidget::SetSelected(bool bInSelected)
{
	bSelected = bInSelected;
	Refresh();
}

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

void UMWVehicleCardWidget::Refresh()
{
	if (!IsValid(Button_Vehicle) || !IsValid(Image_Frame) || !IsValid(Image_Outline) ||
		!IsValid(Image_Vehicle) || !IsValid(Text_Name) || !IsValid(Text_Unavailable) ||
		!IsValid(SelectionMarker) || !IsValid(MissingImageNotice)) return;

	Button_Vehicle->SetIsEnabled(bAvailable);
	// A confirmed preview remains readable while all pointer/keyboard hit testing is blocked.
	Button_Vehicle->SetVisibility(bInteractionLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Visible);
	Text_Name->SetText(DisplayName);
	Text_Name->SetToolTipText(DisplayName);
	Button_Vehicle->SetToolTipText(DisplayName);
	Text_Unavailable->SetText(UnavailableReason);
	Text_Unavailable->SetVisibility(bAvailable ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	Image_Vehicle->SetBrushFromSoftTexture(Thumbnail, true);
	Image_Vehicle->SetRenderOpacity(bAvailable ? 1.f : UnavailableImageOpacity);
	Image_Vehicle->SetVisibility(Thumbnail.IsNull() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	MissingImageNotice->SetVisibility(Thumbnail.IsNull() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	SelectionMarker->SetVisibility(bShowStandaloneSelectionMarker && bSelected && PlayerMarkers.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	const FLinearColor Tint = !bAvailable ? UnavailableTint : bSelected
		? (bHasLocalParticipant ? LocalParticipantTint : SelectedTint) : bHovered ? HoverTint : NormalTint;
	Image_Outline->SetColorAndOpacity(Tint);
	Image_Frame->SetColorAndOpacity(bPressed ? PressedFrameTint : FrameTint);
}

void UMWVehicleCardWidget::HandleChosen()
{
	if (bAvailable && !bInteractionLocked) OnChosen.Broadcast(this);
}

void UMWVehicleCardWidget::HandleHovered()
{
	bHovered = true;
	Refresh();
	OnHoverChanged.Broadcast(this, true);
}

void UMWVehicleCardWidget::HandleUnhovered()
{
	bHovered = false;
	Refresh();
	OnHoverChanged.Broadcast(this, false);
}

void UMWVehicleCardWidget::HandlePressed()
{
	if (!bAvailable || bInteractionLocked) return;
	bPressed = true;
	Refresh();
}

void UMWVehicleCardWidget::HandleReleased()
{
	bPressed = false;
	Refresh();
}
