#include "UI/Gameplay/MWRaceHUD.h"

#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
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

    // 내부 enum 이름을 노출하지 않고 현재 서버 단계의 의미만 표시한다.
    FText GetRoomPhaseLabel(EMWRoomPhase Phase)
    {
        switch (Phase)
        {
        case EMWRoomPhase::PreparingConstruction:
            return NSLOCTEXT("MWRaceHUD", "SelectingTrack", "트랙 선택");

        case EMWRoomPhase::TrackPlacement:
            return NSLOCTEXT("MWRaceHUD", "PlacingTrack", "트랙 배치");

        case EMWRoomPhase::ElementPlacement:
            return NSLOCTEXT("MWRaceHUD", "PlacingElements", "위험 요소 배치");

        case EMWRoomPhase::PreparingRace:
            return NSLOCTEXT("MWRaceHUD", "PreparingRace", "경기 준비");

        case EMWRoomPhase::Countdown:
            // 종료 시각을 조회할 API가 아직 없어 임의의 숫자 카운트다운을 만들지 않는다.
            return NSLOCTEXT("MWRaceHUD", "WaitingForStart", "출발 대기");

        case EMWRoomPhase::Racing:
            return NSLOCTEXT("MWRaceHUD", "Racing", "주행 중");

        case EMWRoomPhase::Failed:
            return NSLOCTEXT("MWRaceHUD", "GameplayFailed", "경기 진행 오류");

        default:
            return FText::GetEmpty();
        }
    }
}

void SMWRaceHUD::Construct(const FArguments& Args)
{
    Controller = Args._Controller;
    bItemPickupTest = Args._bItemPickupTest;
    RoomPhase = Args._RoomPhase;

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
            ]
            + SOverlay::Slot()
            .HAlign(HAlign_Left)
            .VAlign(VAlign_Top)
            .Padding(48.f, 48.f, 0.f, 0.f)
            [
                SAssignNew(PhasePanel, SMWGameplayPanel)
                .Size(FVector2D(360.f, 96.f))
                .Fill(FLinearColor::FromSRGBColor(FColor(40, 81, 120)))
                .Border(FLinearColor::FromSRGBColor(FColor(126, 174, 227)))
                .BorderWidth(4.f)
                .Visibility(EVisibility::Collapsed)
                [
                    SNew(SBox)
                    .Padding(FMargin(42.f, 12.f))
                    [
                        SNew(SScaleBox)
                        .Stretch(EStretch::ScaleToFit)
                        [
                            SAssignNew(PhaseText, STextBlock)
                            .Font(Art.Font(32.f))
                            .ColorAndOpacity(FLinearColor::White)
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
    const TOptional<EMWRoomPhase> CurrentPhase = RoomPhase.Get();
    const AMWVehiclePawn* Vehicle = PC ? PC->GetPawn<AMWVehiclePawn>() : nullptr;

    // 실제 레이스와 기존 아이템 단독 PIE에서만 소유 차량의 속도를 표시한다.
    const bool bDrivingPhase = bItemPickupTest || (CurrentPhase.IsSet() && CurrentPhase.GetValue() == EMWRoomPhase::Racing);
    const bool bShowSpeed = bDrivingPhase && PC && PC->IsLocalController() && IsValid(Vehicle);
    SpeedPanel->SetVisibility(bShowSpeed ? EVisibility::HitTestInvisible : EVisibility::Collapsed);

    UpdateSpeed(bShowSpeed ? Vehicle : nullptr, CurrentTime);
    UpdatePhase(bItemPickupTest ? TOptional<EMWRoomPhase>() : CurrentPhase);
}

void SMWRaceHUD::UpdatePhase(TOptional<EMWRoomPhase> Phase)
{
    if (DisplayedPhase == Phase)
    {
        return;
    }

    DisplayedPhase = Phase;

    const FText Label = Phase.IsSet() ? GetRoomPhaseLabel(Phase.GetValue()) : FText::GetEmpty();
    PhasePanel->SetVisibility(Label.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible);
    PhaseText->SetText(Label);
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
