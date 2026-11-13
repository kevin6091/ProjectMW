#include "UI/Gameplay/MWRaceHUD.h"

#include "Engine/World.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "System/GameState/MWGameplayGameState.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    constexpr double CentimetersPerSecondToKmh = 0.036;

    // 숫자는 초당 최대 10번 갱신하고, 같은 정수 속도면 텍스트를 다시 만들지 않는다.
    constexpr double SpeedRefreshInterval = 0.1;
}

void SMWRaceHUD::Construct(const FArguments& Args)
{
    Controller = Args._Controller;
    bItemPickupTest = Args._bItemPickupTest;

    ChildSlot
    [
        MWGameplayUI::FitDesignCanvas(
            SNew(SOverlay)
            + SOverlay::Slot()
            .HAlign(HAlign_Right)
            .VAlign(VAlign_Bottom)
            .Padding(0.f, 0.f, 48.f, 48.f)
            [
                SAssignNew(SpeedPanel, SMWGameplayPanel)
                .Size(FVector2D(300.f, 150.f))
                .Fill(FLinearColor::FromSRGBColor(FColor(40, 81, 120)))
                .Border(FLinearColor::FromSRGBColor(FColor(126, 174, 227)))
                .BorderWidth(4.f)
                .Visibility(EVisibility::Collapsed)
                [
                    SNew(SBox)
                    .Padding(FMargin(42.f, 20.f))
                    [
                        SNew(SScaleBox)
                        .Stretch(EStretch::ScaleToFit)
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .VAlign(VAlign_Center)
                            [
                                SAssignNew(SpeedText, STextBlock)
                                .Font(Art.Font(72.f))
                                .ColorAndOpacity(FLinearColor::White)
                            ]
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .VAlign(VAlign_Center)
                            .Padding(12.f, 0.f, 0.f, 0.f)
                            [
                                SNew(STextBlock)
                                .Text(NSLOCTEXT("MWRaceHUD", "SpeedUnit", "km/h"))
                                .Font(Art.Font(24.f))
                                .ColorAndOpacity(FLinearColor::White)
                            ]
                        ]
                    ]
                ]
            ],
            HAlign_Right,
            VAlign_Bottom)
    ];
}

void SMWRaceHUD::Tick(const FGeometry& Geometry, double CurrentTime, float DeltaTime)
{
    SCompoundWidget::Tick(Geometry, CurrentTime, DeltaTime);

    const APlayerController* PC = Controller.Get();
    const UWorld* World = PC ? PC->GetWorld() : nullptr;
    const AMWGameplayGameState* GameState = World ? World->GetGameState<AMWGameplayGameState>() : nullptr;
    const AMWVehiclePawn* Vehicle = PC ? PC->GetPawn<AMWVehiclePawn>() : nullptr;

    // 실제 레이스와 기존 아이템 단독 PIE에서만 소유 차량의 속도를 표시한다.
    const bool bDrivingPhase = bItemPickupTest || (GameState && GameState->GetRoomPhase() == EMWRoomPhase::Racing);
    const bool bShowSpeed = bDrivingPhase && PC && PC->IsLocalController() && IsValid(Vehicle);
    SpeedPanel->SetVisibility(bShowSpeed ? EVisibility::HitTestInvisible : EVisibility::Collapsed);

    UpdateSpeed(bShowSpeed ? Vehicle : nullptr, CurrentTime);
}

void SMWRaceHUD::UpdateSpeed(const AMWVehiclePawn* Vehicle, double CurrentTime)
{
    // 재소유·차량 파괴 후에는 이전 차량의 숫자를 재사용하지 않는다.
    if (DisplayedVehicle.Get() != Vehicle)
    {
        DisplayedVehicle = Vehicle;
        DisplayedSpeedKmh = INDEX_NONE;
        NextSpeedUpdateTime = 0.0;
    }

    if (!Vehicle || CurrentTime < NextSpeedUpdateTime)
    {
        return;
    }

    NextSpeedUpdateTime = CurrentTime + SpeedRefreshInterval;

    const UPawnMovementComponent* Movement = Vehicle->GetMovementComponent();
    if (!Movement)
    {
        SpeedPanel->SetVisibility(EVisibility::Collapsed);
        return;
    }

    // UE의 cm/s를 km/h로 환산한다. 낙하 속도(Z)는 제외하고 후진도 양수로 표시한다.
    const double SpeedKmh = Movement->Velocity.Size2D() * CentimetersPerSecondToKmh;
    const int32 RoundedSpeedKmh = FMath::IsFinite(SpeedKmh) ? FMath::RoundToInt(SpeedKmh) : 0;

    if (DisplayedSpeedKmh != RoundedSpeedKmh)
    {
        DisplayedSpeedKmh = RoundedSpeedKmh;
        SpeedText->SetText(FText::AsNumber(DisplayedSpeedKmh));
    }
}
