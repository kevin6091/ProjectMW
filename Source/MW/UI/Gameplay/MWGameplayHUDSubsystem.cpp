#include "UI/Gameplay/MWGameplayHUDSubsystem.h"

#include "Brushes/SlateImageBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/SceneComponent.h"
#include "Data/MWVehicleDefinitionData.h"
#include "Engine/GameViewportClient.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/MWGameplayPlayerController.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Fonts/FontMeasure.h"
#include "Styling/CoreStyle.h"
#include "System/GameState/MWGameplayGameState.h"
#include "System/MWAssetManager.h"
#include "System/MWPlayerState.h"
#include "UI/Gameplay/MWGameplaySign.h"
#include "UI/Gameplay/SMWVehicleWind.h"
#include "UI/VehicleSelection/MWVehicleSelectionWidget.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

DEFINE_LOG_CATEGORY_STATIC(LogMWInGameUI, Log, All);

namespace
{
bool IsItemPickupTestPIE(const UWorld& World)
{
	return World.WorldType == EWorldType::PIE && UWorld::RemovePIEPrefix(World.GetOutermost()->GetName())
		== TEXT("/Game/Developers/user/L_Dev_Drive_ItemPickup");
}

constexpr float ItemSlotSize = 168.f;
constexpr float ItemIconSize = 158.f;
// Each card stays visible a little longer than the previous card.
constexpr double ItemRouletteSwitchTimes[] = { .045, .095, .15, .215, .29, .38, .49, .63, .80, .99 };
constexpr float ItemUseDuration = .20f;

class SMWItemSlotFrame : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMWItemSlotFrame) {}
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args)
	{
		ChildSlot.Padding(5.f)[Args._Content.Widget];
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
		FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override
	{
		const FVector2D Size = Geometry.GetLocalSize();
		FSlateDrawElement::MakeBox(Elements, Layer, Geometry.ToPaintGeometry(Size,
			FSlateLayoutTransform(FVector2D(2.f, 6.f))), &Shadow, ESlateDrawEffect::None, Shadow.GetTint(Style));

		// A polished outer bevel, a dark gasket and the tinted lens are separate surfaces.
		const auto DrawFace = [&](int32 FaceLayer, float Inset, float Radius, FColor Top, FColor Middle, FColor Bottom)
		{
			const FVector2D FaceSize = Size - FVector2D(Inset * 2.f);
			TArray<FSlateGradientStop> Stops;
			Stops.Emplace(FVector2D::ZeroVector, FLinearColor::FromSRGBColor(Top));
			Stops.Emplace(FaceSize * .42, FLinearColor::FromSRGBColor(Middle));
			Stops.Emplace(FaceSize, FLinearColor::FromSRGBColor(Bottom));
			FSlateDrawElement::MakeGradient(Elements, FaceLayer, Geometry.ToPaintGeometry(FaceSize,
				FSlateLayoutTransform(FVector2D(Inset))), MoveTemp(Stops), Orient_Horizontal,
				ESlateDrawEffect::None, FVector4f(Radius, Radius, Radius, Radius));
		};
		DrawFace(Layer + 1, 0.f, 29.f, FColor(190, 197, 173), FColor(85, 109, 114), FColor(44, 36, 54));
		DrawFace(Layer + 2, 3.f, 26.f, FColor(61, 76, 72), FColor(17, 31, 39), FColor(32, 24, 42));
		DrawFace(Layer + 3, 7.f, 22.f, FColor(89, 118, 114, 244), FColor(41, 67, 79, 235), FColor(66, 49, 75, 245));

		// Diagonal reflection stays inside the lens so the glass retains its rounded silhouette.
		const FVector2f Reflection[] = {
			{ .09f, .19f }, { .17f, .08f }, { .69f, .07f },
			{ .39f, .34f }, { .10f, .70f }, { .06f, .39f }
		};
		const uint8 Alpha[] = { 46, 91, 42, 16, 0, 12 };
		TArray<FSlateVertex> Vertices;
		Vertices.Reserve(UE_ARRAY_COUNT(Reflection));
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Reflection); ++Index)
		{
			Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Geometry.GetAccumulatedRenderTransform(),
				Reflection[Index] * FVector2f(Size), FVector2f(.5f, .5f), FColor(255, 235, 190, Alpha[Index])));
		}
		const TArray<SlateIndex> Indices{ 0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 5 };
		const auto Brush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
		const auto Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*Brush);
		FSlateDrawElement::MakeCustomVerts(Elements, Layer + 4, Handle, Vertices, Indices, nullptr, 0, 0);

		return SCompoundWidget::OnPaint(Args, Geometry, CullingRect, Elements, Layer + 5, Style, bParentEnabled);
	}

private:
	FSlateRoundedBoxBrush Shadow{ FLinearColor(.007f, .009f, .015f, .52f), 29.f };
};

class SMWGameplayHUD : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMWGameplayHUD) {}
		SLATE_ARGUMENT(APlayerController*, Controller)
		SLATE_ARGUMENT(bool, bItemPickupTest)
		SLATE_ATTRIBUTE(EMWItemId, HeldItem)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args)
	{
		Controller = Args._Controller;
		bItemPickupTest = Args._bItemPickupTest;
		HeldItem = Args._HeldItem;
		const auto* Settings = GetDefault<UMWGameplayHUDSubsystem>();
		BoosterTexture.Reset(Settings->BoosterIcon.LoadSynchronous());
		if (BoosterTexture)
		{
			const FVector2D TextureSize(BoosterTexture->GetSizeX(), BoosterTexture->GetSizeY());
			const FVector2D IconSize = TextureSize * (ItemIconSize / FMath::Max(1.0, TextureSize.GetMax()));
			BoosterIconOffset = (FVector2D(.5, .5) - Settings->BoosterIconContentCenter) * IconSize;
			UObject* IconResource = BoosterTexture.Get();
			if (UMaterialInterface* Material = Settings->ItemIconMaterial.LoadSynchronous())
			{
				BoosterIconMaterial.Reset(UMaterialInstanceDynamic::Create(Material, GetTransientPackage()));
				if (BoosterIconMaterial)
				{
					BoosterIconMaterial->SetTextureParameterValue(TEXT("ItemIcon"), BoosterTexture.Get());
					IconResource = BoosterIconMaterial.Get();
				}
			}
			BoosterBrush = FSlateImageBrush(IconResource, IconSize);
		}
		for (const TSoftObjectPtr<UTexture2D>& Icon : Settings->ItemRouletteIcons)
		{
			if (UTexture2D* Texture = Icon.LoadSynchronous())
			{
				const FVector2D TextureSize(Texture->GetSizeX(), Texture->GetSizeY());
				const FVector2D IconSize = TextureSize * (ItemIconSize / FMath::Max(1.0, TextureSize.GetMax()));
				RouletteTextures.Emplace(Texture);
				RouletteBrushes.Emplace(FSlateImageBrush(Texture, IconSize));
			}
		}
		ForceVolatile(true);
		const auto* Brush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
		if (const UClass* Selection = GetDefault<UMWGameplayHUDSubsystem>()->VehiclePalette.LoadSynchronous())
			Palette.Reset(Cast<UMWVehicleSelectionWidget>(Selection->GetDefaultObject()));

		const TSharedRef<SWidget> CenteredHUD = MWGameplayUI::FitDesignCanvas(
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SAssignNew(WaitingBoard, SOverlay).Visibility(EVisibility::Collapsed)
					+ SOverlay::Slot().HAlign(HAlign_Center)
					[
						SNew(SBox).WidthOverride(122.f)
						[
							SNew(SBorder).BorderImage(Brush).BorderBackgroundColor(FLinearColor(.12f, .14f, .17f))
							.Padding(FMargin(26.f, 0.f, 12.f, 0.f))
							[
								SNew(SBorder).BorderImage(Brush).BorderBackgroundColor(FLinearColor(.20f, .23f, .27f))
								.Padding(FMargin(28.f, 0.f, 21.f, 0.f))
								[
									SNew(SBorder).BorderImage(Brush).BorderBackgroundColor(FLinearColor(.30f, .34f, .38f))
								]
							]
						]
					]
					+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.f, 36.f, 0.f, 0.f)
					[
						SNew(SBox).WidthOverride(200.f).HeightOverride(192.f)
						[
							SNew(SImage).Image(Art.LogoBrush())
						]
					]
					+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0.f, 80.f, 0.f, 0.f)
					[
						SNew(SMWGameplayPanel)
						.Size(FVector2D(1152.f, 676.f))
						.Fill(FLinearColor::FromSRGBColor(FColor(92, 145, 211)))
						.Border(FLinearColor::FromSRGBColor(FColor(126, 174, 227))).BorderWidth(4.f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(SBox).HeightOverride(106.f).HAlign(HAlign_Center).VAlign(VAlign_Center)
								[
									SNew(STextBlock).Text(NSLOCTEXT("MWGameplayUI", "RaceWaitingTitle", "경기 준비"))
									.Font(Art.Font(64.f)).ColorAndOpacity(FLinearColor::White)
								]
							]
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(SBox).HeightOverride(5.f)
								[SNew(SBorder).BorderImage(Brush).BorderBackgroundColor(FLinearColor(.11f, .19f, .32f))]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(26.f, 4.f)
							[
								SAssignNew(Roster, SGridPanel)
							]
						]
					]
				]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SAssignNew(SelectSign, SMWGameplaySign).Visibility(EVisibility::Collapsed)
					.Label(NSLOCTEXT("MWGameplayUI", "SelectTrack", "트랙 부속품을\n고르세요"))
				]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SAssignNew(BuildSign, SMWGameplaySign).Visibility(EVisibility::Collapsed)
					.Label(NSLOCTEXT("MWGameplayUI", "BuildTrack", "트랙을\n만드세요"))
				]
			);

		ChildSlot
		[
			SNew(SOverlay)
			+ SOverlay::Slot()[SAssignNew(VehicleWind, SMWVehicleWind)]
			+ SOverlay::Slot()[CenteredHUD]
			+ SOverlay::Slot()
			[
				MWGameplayUI::FitDesignCanvas(
					SNew(SOverlay)
					+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(48.f, 0.f, 0.f, 48.f)
					[
						SAssignNew(ItemSlot, SBox).WidthOverride(ItemSlotSize).HeightOverride(ItemSlotSize)
						.Visibility(EVisibility::Collapsed)
						[
							SNew(SMWItemSlotFrame)
							[
								SNew(SOverlay)
								+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
								[
									SNew(SImage).Image_Lambda([this] { return GetItemSlotBrush(); })
									.RenderTransformPivot(FVector2D(.5, .5))
									.RenderTransform_Lambda([this]() -> TOptional<FSlateRenderTransform> {
										const FVector2D Offset = RouletteStep == INDEX_NONE ? BoosterIconOffset : FVector2D::ZeroVector;
										return FSlateRenderTransform(ItemIconScale, Offset * ItemIconScale);
									})
									.ColorAndOpacity_Lambda([this] { return FSlateColor(FLinearColor(1.f, 1.f, 1.f, ItemIconOpacity)); })
									.Visibility_Lambda([this] { return GetItemSlotBrush() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
								]
								+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
								[
									SNew(STextBlock).Font(Art.Font(24.f))
									.ColorAndOpacity_Lambda([this] { return FSlateColor(FLinearColor(.72f, .75f, .80f, ItemIconOpacity)); })
									.Visibility_Lambda([this] {
										return !GetItemSlotBrush() && DisplayedItem == EMWItemId::Booster
											? EVisibility::HitTestInvisible : EVisibility::Collapsed;
									})
									.Text(NSLOCTEXT("MWGameplayUI", "BoosterItem", "부스터"))
								]
							]
						]
					],
					HAlign_Left, VAlign_Bottom)
			]
		];
	}

	virtual void Tick(const FGeometry&, double CurrentTime, float DeltaTime) override
	{
		const auto* PC = Controller.Get();
		const auto* GS = PC ? PC->GetWorld()->GetGameState<AMWGameplayGameState>() : nullptr;
		const auto* Vehicle = PC ? PC->GetPawn<AMWVehiclePawn>() : nullptr;
		const bool bItemPhase = bItemPickupTest || (GS && GS->GetRoomPhase() == EMWRoomPhase::Racing);
		const bool bShowItemSlot = bItemPhase && Vehicle && Vehicle->GetItemComponent();
		ItemSlot->SetVisibility(bShowItemSlot ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
		UpdateItemSlot(bShowItemSlot ? Vehicle : nullptr, CurrentTime);
		VehicleWind->Update(bShowItemSlot ? Vehicle : nullptr, DeltaTime);
		if (bItemPickupTest || !GS) return;

		const bool bHasRoom = GS->GetRoomSettings().MapThemeId.IsValid();
		const EMWRoomPhase Phase = GS->GetRoomPhase();
		if (!LastPhase.IsSet() || Phase != LastPhase.GetValue())
		{
			LastPhase = Phase;
			PhaseStarted = CurrentTime;
		}

		const bool bShowWaiting = bHasRoom && Phase == EMWRoomPhase::PreparingRace && !PC->GetPawn();
		WaitingBoard->SetVisibility(bShowWaiting ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
		if (bShowWaiting && CurrentTime >= NextRosterRefresh)
		{
			RefreshRoster(*GS);
			NextRosterRefresh = CurrentTime + .25;
		}

		SelectSign->SetVisibility(bHasRoom && Phase == EMWRoomPhase::PreparingConstruction
			? EVisibility::HitTestInvisible : EVisibility::Collapsed);
		BuildSign->SetVisibility(bHasRoom && Phase == EMWRoomPhase::TrackPlacement && CurrentTime - PhaseStarted < 2.0
			? EVisibility::HitTestInvisible : EVisibility::Collapsed);
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
		FSlateWindowElementList& DrawElements, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override
	{
		const int32 TopLayer = SCompoundWidget::OnPaint(Args, Geometry, CullingRect, DrawElements, LayerId, Style, bParentEnabled);
		APlayerController* PC = Controller.Get();
		const auto* GS = PC ? PC->GetWorld()->GetGameState<AMWGameplayGameState>() : nullptr;
		if (bItemPickupTest || !GS || !PC->GetPawn() || !FSlateApplication::IsInitialized()) return TopLayer;

		const EMWRoomPhase Phase = GS->GetRoomPhase();
		if (Phase != EMWRoomPhase::PreparingRace && Phase != EMWRoomPhase::Countdown && Phase != EMWRoomPhase::Racing)
			return TopLayer;

		int32 ViewportWidth = 0;
		int32 ViewportHeight = 0;
		PC->GetViewportSize(ViewportWidth, ViewportHeight);
		if (ViewportWidth <= 0 || ViewportHeight <= 0) return TopLayer;
		const FVector2D PixelToOverlay = Geometry.GetLocalSize() / FVector2D(ViewportWidth, ViewportHeight);
		const auto FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		const double NameScale = FMath::Min(Geometry.GetLocalSize().X / 1920.0, Geometry.GetLocalSize().Y / 1080.0);
		FSlateFontInfo NameFont = Art.Font(FMath::Max(11.0, 30.0 * NameScale));
		NameFont.OutlineSettings.OutlineSize = FMath::Max(1, FMath::RoundToInt(3.0 * NameScale));
		NameFont.OutlineSettings.OutlineColor = FLinearColor(.035f, .025f, .02f);
		for (const APlayerState* State : GS->PlayerArray)
		{
			const auto* Vehicle = State ? State->GetPawn<AMWVehiclePawn>() : nullptr;
			if (!IsValid(Vehicle) || !Vehicle->GetRootComponent() || State->GetPlayerName().IsEmpty()) continue;

			const FBoxSphereBounds& Bounds = Vehicle->GetRootComponent()->Bounds;
			const FVector NameAnchor = Bounds.Origin + FVector::UpVector * (Bounds.BoxExtent.Z + 18.f);
			FVector2D Position;
			if (!PC->ProjectWorldLocationToScreen(NameAnchor, Position))
			{
				continue;
			}

			const FText Name = FText::AsCultureInvariant(State->GetPlayerName().ToUpper());
			const FVector2D Size = FontMeasure->Measure(Name, NameFont);
			Position = Position * PixelToOverlay - FVector2D(Size.X * .5, Size.Y + 8.0 * NameScale);
			if (!FSlateRect::DoRectanglesIntersect(FSlateRect(FVector2D::ZeroVector, Geometry.GetLocalSize()),
				FSlateRect(Position, Position + Size))) continue;

			FSlateDrawElement::MakeText(DrawElements, TopLayer + 1,
				Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Position)), Name, NameFont,
				ESlateDrawEffect::None, GetPlayerTint(State));
			if (!bLoggedNameplate)
			{
				UE_LOG(LogMWInGameUI, Verbose, TEXT("Rendered vehicle nameplate in %s"), *GetNameSafe(PC->GetWorld()));
				bLoggedNameplate = true;
			}
		}
		return TopLayer + 1;
	}

private:
	void UpdateItemSlot(const AMWVehiclePawn* Vehicle, double CurrentTime)
	{
		const EMWItemId CurrentItem = Vehicle ? HeldItem.Get(EMWItemId::None) : EMWItemId::None;
		if (!Vehicle || ItemSlotVehicle.Get() != Vehicle)
		{
			ItemSlotVehicle = Vehicle;
			DisplayedItem = TargetItem = CurrentItem;
			RouletteStep = INDEX_NONE;
			ItemTransitionStart.Reset();
			ItemIconScale = ItemIconOpacity = 1.f;
			return;
		}

		if (CurrentItem != TargetItem)
		{
			TargetItem = CurrentItem;
			ItemTransitionStart = CurrentTime;
			RouletteStep = INDEX_NONE;
			if (CurrentItem != EMWItemId::None)
			{
				DisplayedItem = CurrentItem;
				ItemIconScale = ItemIconOpacity = 1.f;
				if (RouletteBrushes.Num() < 2)
				{
					ItemTransitionStart.Reset();
					return;
				}
				UE_LOG(LogMWInGameUI, Verbose, TEXT("Item slot roulette started"));
			}
			else
			{
				// A quick use stops the spin and fades the awarded item.
				UE_LOG(LogMWInGameUI, Verbose, TEXT("Item slot use started"));
			}
		}

		if (!ItemTransitionStart.IsSet()) return;

		const double Elapsed = CurrentTime - ItemTransitionStart.GetValue();
		if (TargetItem != EMWItemId::None)
		{
			UpdateItemRoulette(Elapsed);
			return;
		}

		const float Progress = FMath::Clamp(static_cast<float>(Elapsed / ItemUseDuration), 0.f, 1.f);
		ItemIconScale = FMath::InterpEaseOut(1.f, .72f, Progress, 2.f);
		ItemIconOpacity = 1.f - Progress;
		if (Progress >= 1.f)
		{
			DisplayedItem = EMWItemId::None;
			ItemTransitionStart.Reset();
			UE_LOG(LogMWInGameUI, Verbose, TEXT("Item slot use finished"));
		}
	}

	void UpdateItemRoulette(double Elapsed)
	{
		int32 Step = 0;
		while (Step < UE_ARRAY_COUNT(ItemRouletteSwitchTimes) && Elapsed >= ItemRouletteSwitchTimes[Step])
		{
			++Step;
		}
		if (Step == UE_ARRAY_COUNT(ItemRouletteSwitchTimes))
		{
			RouletteStep = INDEX_NONE;
			ItemTransitionStart.Reset();
			UE_LOG(LogMWInGameUI, Verbose, TEXT("Item slot roulette settled on %s"), *UEnum::GetValueAsString(DisplayedItem));
		}
		else if (Step != RouletteStep)
		{
			RouletteStep = Step;
			UE_LOG(LogMWInGameUI, Verbose, TEXT("Item slot roulette card %d at %.0f ms"), Step + 1, Elapsed * 1000.0);
		}
	}

	const FSlateBrush* GetItemSlotBrush() const
	{
		if (RouletteStep != INDEX_NONE)
		{
			return &RouletteBrushes[RouletteStep % RouletteBrushes.Num()];
		}
		return BoosterTexture && DisplayedItem == EMWItemId::Booster ? &BoosterBrush : nullptr;
	}

	void AddRosterCell(int32 Column, int32 Row, const FText& Text, FLinearColor Tint)
	{
		static constexpr float Widths[] = { 112.f, 452.f, 358.f, 174.f };
		Roster->AddSlot(Column, Row).Padding(FMargin(0.f, 2.f))
		[
			SNew(SBox).WidthOverride(Widths[Column]).HeightOverride(Row == 0 ? 58.f : 55.f)
			[
				SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
				.HAlign(HAlign_Left).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(Text).ColorAndOpacity(Tint).Font(Art.Font(Row == 0 ? 38.f : 32.f))
				]
			]
		];
	}

	void RefreshRoster(const AMWGameplayGameState& GS)
	{
		TArray<const AMWPlayerState*> Players;
		for (const APlayerState* State : GS.PlayerArray)
		{
			if (const auto* Player = Cast<AMWPlayerState>(State); Player && !Player->IsInactive()
				&& Player->GetLobbyMemberId() > 0)
			{
				Players.Add(Player);
			}
		}
		Players.Sort([](const AMWPlayerState& A, const AMWPlayerState& B)
		{
			return A.GetLobbyMemberId() < B.GetLobbyMemberId();
		});

		TArray<FText> Cells;
		TArray<FLinearColor> VehicleTints;
		for (const AMWPlayerState* Player : Players)
		{
			const auto* Vehicle = UMWAssetManager::Get().FindLoadedAsset<UMWVehicleDefinitionData>(Player->GetVehicleAssetID());
			Cells.Add(FText::AsNumber(VehicleTints.Num() + 1));
			Cells.Add(Player->GetPlayerName().IsEmpty() ? NSLOCTEXT("MWGameplayUI", "UnnamedPlayer", "플레이어")
				: FText::AsCultureInvariant(Player->GetPlayerName()));
			Cells.Add(Vehicle ? Vehicle->GetDisplayName() : FText::GetEmpty());
			Cells.Add(Player->GetPawn() ? NSLOCTEXT("MWGameplayUI", "RaceReady", "준비 완료")
				: NSLOCTEXT("MWGameplayUI", "RaceWaiting", "기다리는 중"));
			VehicleTints.Add(GetPlayerTint(Player));
		}
		if (bRosterBuilt && Cells.Num() == DisplayedCells.Num() && VehicleTints == DisplayedVehicleTints)
		{
			bool bChanged = false;
			for (int32 Index = 0; Index < Cells.Num() && !bChanged; ++Index)
				bChanged = !Cells[Index].EqualTo(DisplayedCells[Index]);
			if (!bChanged) return;
		}

		bRosterBuilt = true;
		DisplayedCells = MoveTemp(Cells);
		DisplayedVehicleTints = MoveTemp(VehicleTints);
		Roster->ClearChildren();
		AddRosterCell(0, 0, NSLOCTEXT("MWGameplayUI", "GridColumn", "위치"), FLinearColor::White);
		AddRosterCell(1, 0, NSLOCTEXT("MWGameplayUI", "DriverColumn", "드라이버"), FLinearColor::White);
		AddRosterCell(2, 0, NSLOCTEXT("MWGameplayUI", "VehicleColumn", "차량"), FLinearColor::White);
		AddRosterCell(3, 0, NSLOCTEXT("MWGameplayUI", "StatusColumn", "상태"), FLinearColor::White);
		constexpr int32 Columns = 4;
		for (int32 Index = 0; Index < DisplayedCells.Num(); ++Index)
		{
			const int32 Column = Index % Columns;
			const int32 Row = Index / Columns;
			AddRosterCell(Column, Row + 1, DisplayedCells[Index],
				Column == 2 ? DisplayedVehicleTints[Row] : FLinearColor::White);
		}
	}

	FLinearColor GetPlayerTint(const APlayerState* State) const
	{
		const auto* Player = Cast<AMWPlayerState>(State);
		return Palette && Player ? Palette->GetParticipantTintForMember(Player->GetLobbyMemberId()) : FLinearColor::White;
	}

	TWeakObjectPtr<APlayerController> Controller;
	TSharedPtr<SOverlay> WaitingBoard;
	TSharedPtr<SGridPanel> Roster;
	TSharedPtr<SMWGameplaySign> SelectSign;
	TSharedPtr<SMWGameplaySign> BuildSign;
	TSharedPtr<SBox> ItemSlot;
	TSharedPtr<SMWVehicleWind> VehicleWind;
	TAttribute<EMWItemId> HeldItem;
	TWeakObjectPtr<const AMWVehiclePawn> ItemSlotVehicle;
	EMWItemId DisplayedItem = EMWItemId::None;
	EMWItemId TargetItem = EMWItemId::None;
	TOptional<double> ItemTransitionStart;
	float ItemIconScale = 1.f;
	float ItemIconOpacity = 1.f;
	int32 RouletteStep = INDEX_NONE;
	TArray<TStrongObjectPtr<UTexture2D>> RouletteTextures;
	TArray<FSlateBrush> RouletteBrushes;
	TStrongObjectPtr<UTexture2D> BoosterTexture;
	TStrongObjectPtr<UMaterialInstanceDynamic> BoosterIconMaterial;
	FVector2D BoosterIconOffset = FVector2D::ZeroVector;
	FSlateBrush BoosterBrush;
	TArray<FText> DisplayedCells;
	TArray<FLinearColor> DisplayedVehicleTints;
	FMWGameplayUIArt Art;
	TStrongObjectPtr<UMWVehicleSelectionWidget> Palette;
	TOptional<EMWRoomPhase> LastPhase;
	double PhaseStarted = 0.0;
	double NextRosterRefresh = 0.0;
	bool bRosterBuilt = false;
	bool bItemPickupTest = false;
	mutable bool bLoggedNameplate = false;
};
}

bool UMWGameplayHUDSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{
	return Type == EWorldType::Game || Type == EWorldType::PIE;
}

bool UMWGameplayHUDSubsystem::IsTickable() const
{
	const UWorld* World = GetWorld();
	return !Overlay && World && World->GetNetMode() != NM_DedicatedServer
		&& (World->GetGameState<AMWGameplayGameState>() || IsItemPickupTestPIE(*World));
}

void UMWGameplayHUDSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	auto* GameInstance = World->GetGameInstance();
	auto* PC = GameInstance ? Cast<AMWGameplayPlayerController>(GameInstance->GetFirstLocalPlayerController(World)) : nullptr;
	auto* GameViewport = GameInstance ? GameInstance->GetGameViewportClient() : nullptr;
	if (!GameViewport || !PC || PC->GetWorld() != World || !PC->GetLocalPlayer() || !PC->HasActorBegunPlay()) return;

	ItemController = PC;
	PC->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::HandlePossessedPawnChanged);
	HandlePossessedPawnChanged(nullptr, PC->GetPawn());

	const TWeakObjectPtr<UMWGameplayHUDSubsystem> WeakThis(this);
	Overlay = SNew(SMWGameplayHUD).Controller(PC).Visibility(EVisibility::HitTestInvisible)
		.bItemPickupTest(IsItemPickupTestPIE(*World))
		.HeldItem_Lambda([WeakThis] {
			const UMWGameplayHUDSubsystem* HUD = WeakThis.Get();
			return HUD ? HUD->HeldItem : EMWItemId::None;
		});
	Viewport = GameViewport;
	GameViewport->AddViewportWidgetContent(Overlay.ToSharedRef(), 20);
	UE_LOG(LogMWInGameUI, Verbose, TEXT("Attached gameplay HUD in %s to %s"), *GetNameSafe(World), *GetNameSafe(PC));
}

TStatId UMWGameplayHUDSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMWGameplayHUDSubsystem, STATGROUP_Tickables);
}

void UMWGameplayHUDSubsystem::OnWorldEndPlay(UWorld& World)
{
	RemoveOverlay();
	Super::OnWorldEndPlay(World);
}

void UMWGameplayHUDSubsystem::Deinitialize()
{
	RemoveOverlay();
	Super::Deinitialize();
}

void UMWGameplayHUDSubsystem::HandlePossessedPawnChanged(APawn*, APawn* NewPawn)
{
	if (UMWItemComponent* OldItems = ObservedItems.Get())
	{
		OldItems->OnHeldItemChanged.RemoveDynamic(this, &ThisClass::HandleHeldItemChanged);
	}

	const auto* Vehicle = Cast<AMWVehiclePawn>(NewPawn);
	UMWItemComponent* Items = Vehicle ? Vehicle->GetItemComponent() : nullptr;
	ObservedItems = Items;
	if (Items)
	{
		Items->OnHeldItemChanged.AddUniqueDynamic(this, &ThisClass::HandleHeldItemChanged);
	}
	HandleHeldItemChanged(Items ? Items->GetCurrentItemId() : EMWItemId::None);
}

void UMWGameplayHUDSubsystem::HandleHeldItemChanged(EMWItemId ItemId)
{
	if (HeldItem != ItemId)
	{
		HeldItem = ItemId;
		UE_LOG(LogMWInGameUI, Verbose, TEXT("Item HUD state: %s"), *UEnum::GetValueAsString(HeldItem));
	}
}

void UMWGameplayHUDSubsystem::RemoveOverlay()
{
	if (APlayerController* PC = ItemController.Get())
	{
		PC->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePossessedPawnChanged);
	}
	ItemController.Reset();
	HandlePossessedPawnChanged(nullptr, nullptr);
	if (Overlay && Viewport.IsValid())
		Viewport->RemoveViewportWidgetContent(Overlay.ToSharedRef());
	Overlay.Reset();
	Viewport.Reset();
}
