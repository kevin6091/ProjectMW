#include "UI/Gameplay/MWRaceHUDWidget.h"

#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/Gameplay/MWGameplayHUDSubsystem.h"

void UMWRaceHUDWidget::SetSpeedometerOnly(bool bEnabled)
{
    bSpeedometerOnly = bEnabled;
}

void UMWRaceHUDWidget::NativePreConstruct()
{
    Super::NativePreConstruct();

    // 상한을 바꾸면 Designer 미리보기의 눈금 숫자도 함께 바뀐다.
    TArray<UTextBlock*> Labels;
    if (Panel_Ticks)
    {
        for (UWidget* Child : Panel_Ticks->GetAllChildren())
        {
            if (UTextBlock* Label = Cast<UTextBlock>(Child))
            {
                Labels.Add(Label);
            }
        }
    }

    for (int32 Index = 0; Index < Labels.Num(); ++Index)
    {
        const float Fraction = Labels.Num() > 1 ? static_cast<float>(Index) / (Labels.Num() - 1) : 0.f;
        Labels[Index]->SetText(FText::AsNumber(FMath::RoundToInt(MaxSpeedKmh * Fraction)));
    }

    if (IsDesignTime() && Image_Needle)
    {
        Image_Needle->SetRenderTransformAngle(NeedleStartAngle);
    }
}

void UMWRaceHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();

    ObservedVehicle.Reset();
    DisplayedPhase.Reset();
    DisplayedSpeedKmh = INDEX_NONE;
    DisplayedRatio = -1.f;
    NextSpeedSampleTime = 0.0;
    TargetSpeedKmh = 0.f;
    NeedleSpeedKmh = 0.f;
    Panel_Speed->SetVisibility(ESlateVisibility::Collapsed);
    Panel_Phase->SetVisibility(ESlateVisibility::Collapsed);
    DialMaterial = Image_Dial ? Image_Dial->GetDynamicMaterial() : nullptr;
}

void UMWRaceHUDWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);

    const APlayerController* PC = GetOwningPlayer();
    const UWorld* World = GetWorld();
    const auto* HUD = World ? World->GetSubsystem<UMWGameplayHUDSubsystem>() : nullptr;
    const TOptional<EMWRoomPhase> Phase = HUD ? HUD->GetObservedRoomPhase() : TOptional<EMWRoomPhase>();
    const AMWVehiclePawn* Vehicle = IsValid(PC) ? PC->GetPawn<AMWVehiclePawn>() : nullptr;
    const bool bDriving = bSpeedometerOnly || (Phase.IsSet() && Phase.GetValue() == EMWRoomPhase::Racing);
    const bool bShowSpeed = bDriving && IsValid(PC) && PC->IsLocalController() && IsValid(Vehicle);

    Panel_Speed->SetVisibility(bShowSpeed ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    UpdatePhase(bSpeedometerOnly ? TOptional<EMWRoomPhase>() : Phase);
    SampleSpeed(bShowSpeed ? Vehicle : nullptr, World ? World->GetTimeSeconds() : 0.0);

    if (!bShowSpeed)
    {
        return;
    }

    // 바늘만 보간한다. 원본 속도나 조향·서버 보정 값을 수정하지 않는다.
    NeedleSpeedKmh = FMath::FInterpTo(NeedleSpeedKmh, TargetSpeedKmh, DeltaTime, FMath::Max(1.f, NeedleResponse));
    const float Ratio = FMath::Clamp(NeedleSpeedKmh / FMath::Max(1.f, MaxSpeedKmh), 0.f, 1.f);
    if (!FMath::IsNearlyEqual(DisplayedRatio, Ratio, .0001f))
    {
        DisplayedRatio = Ratio;
        Image_Needle->SetRenderTransformAngle(FMath::Lerp(NeedleStartAngle, NeedleEndAngle, Ratio));
        if (DialMaterial)
        {
            DialMaterial->SetScalarParameterValue(SpeedRatioParameter, Ratio);
        }
    }
}

void UMWRaceHUDWidget::SampleSpeed(const AMWVehiclePawn* Vehicle, double CurrentTime)
{
    if (ObservedVehicle.Get() != Vehicle)
    {
        ObservedVehicle = Vehicle;
        DisplayedSpeedKmh = INDEX_NONE;
        NextSpeedSampleTime = 0.0;
        TargetSpeedKmh = 0.f;
        NeedleSpeedKmh = 0.f;
        DisplayedRatio = -1.f;
    }

    if (!Vehicle || CurrentTime < NextSpeedSampleTime)
    {
        return;
    }

    NextSpeedSampleTime = CurrentTime + .1;
    const UPawnMovementComponent* Movement = Vehicle->GetMovementComponent();
    if (!Movement)
    {
        Panel_Speed->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    // 기존 HUD의 측정식을 유지한다. 횡방향 속도 처리 방식은 이번 분석에서 변경하지 않는다.
    const double SpeedKmh = Movement->Velocity.Size2D() * .036;
    TargetSpeedKmh = FMath::IsFinite(SpeedKmh) ? static_cast<float>(SpeedKmh) : 0.f;
    const int32 RoundedSpeed = FMath::RoundToInt(TargetSpeedKmh);
    if (DisplayedSpeedKmh != RoundedSpeed)
    {
        DisplayedSpeedKmh = RoundedSpeed;
        Text_Speed->SetText(FText::AsNumber(RoundedSpeed));
    }
}

void UMWRaceHUDWidget::UpdatePhase(TOptional<EMWRoomPhase> Phase)
{
    if (DisplayedPhase == Phase)
    {
        return;
    }

    DisplayedPhase = Phase;
    const FText* Label = Phase.IsSet() ? PhaseLabels.Find(Phase.GetValue()) : nullptr;
    Panel_Phase->SetVisibility(Label && !Label->IsEmpty() ? ESlateVisibility::HitTestInvisible
                                                        : ESlateVisibility::Collapsed);
    Text_Phase->SetText(Label ? *Label : FText::GetEmpty());
}

void UMWRaceHUDWidget::NativeDestruct()
{
    DialMaterial = nullptr;
    ObservedVehicle.Reset();
    DisplayedPhase.Reset();

    Super::NativeDestruct();
}
