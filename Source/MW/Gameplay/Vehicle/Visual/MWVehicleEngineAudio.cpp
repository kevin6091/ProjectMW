#include "Gameplay/Vehicle/Visual/MWVehicleEngineAudioSettings.h"

#include "Components/AudioComponent.h"
#include "Gameplay/Vehicle/MWVehicleMovementComponent.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWave.h"
#include "System/GameState/MWGameplayGameState.h"

void AMWVehiclePawn::UpdateEngineAudio(float DeltaSeconds)
{
    if (GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    const auto* State = GetWorld()->GetGameState<AMWGameplayGameState>();
    if (!State || State->GetRoomPhase() != EMWRoomPhase::Racing)
    {
        StopEngineAudio();
        return;
    }

    const auto& Settings = *GetDefault<UMWVehicleEngineAudioSettings>();
    const float MaxSpeed = VehicleMovement->GetUnboostedForwardSpeed();
    if (Settings.Loop.IsNull() || MaxSpeed <= UE_SMALL_NUMBER)
    {
        return;
    }

    // 이 차량은 RPM을 복제하지 않는다. 모든 창에서 예측 또는 서버 스냅샷의 전후진 속도를 사용한다.
    const double ForwardSpeed = FMath::Abs(FVector::DotProduct(GetVelocity(), GetActorForwardVector()));
    const float SpeedLoad = static_cast<float>(FMath::Clamp(ForwardSpeed / MaxSpeed, 0.0, 1.0));
    // 실제 가속 전에 밟는 순간의 반응은 해당 차량을 조종하는 창에서만 적용한다.
    const float Pedal = IsLocallyControlled() ? FMath::Max(DriveInput.Throttle, DriveInput.Reverse) : 0.f;
    const float Load = SpeedLoad + (1.f - SpeedLoad) * FMath::Clamp(Pedal, 0.f, 1.f) * 0.25f;
    if (!EngineAudio)
    {
        if (bEngineAudioLoadFailed)
        {
            return;
        }
        USoundWave* Loop = Settings.Loop.LoadSynchronous();
        bEngineAudioLoadFailed = !IsValid(Loop);
        if (bEngineAudioLoadFailed)
        {
            UE_LOG(LogTemp, Warning, TEXT("[MWVehicleEngineAudio] 배기음 루프 로드 실패: %s"), *Settings.Loop.ToString());
            return;
        }

        EngineAudio = NewObject<UAudioComponent>(this, TEXT("VehicleEngineAudio"), RF_Transient);
        EngineAudio->bAutoActivate = false;
        EngineAudio->bStopWhenOwnerDestroyed = true;
        EngineAudio->SetupAttachment(DrivePresentation);
        EngineAudio->SetSound(Loop);
        FSoundAttenuationSettings Attenuation;
        Attenuation.bSpatialize = true;
        Attenuation.AttenuationShapeExtents = FVector(Settings.InnerRadius, 0, 0);
        Attenuation.FalloffDistance = Settings.FalloffDistance;
        EngineAudio->AdjustAttenuation(Attenuation);
        AddInstanceComponent(EngineAudio);
        EngineAudio->RegisterComponent();
    }

    const float NextLoad = bEngineAudioStarted
        ? FMath::FInterpTo(EngineAudioLoad, Load, DeltaSeconds, Settings.ResponseSpeed)
        : Load;
    if (!bEngineAudioStarted || !FMath::IsNearlyEqual(EngineAudioLoad, NextLoad))
    {
        EngineAudioLoad = NextLoad;
        EngineAudio->SetPitchMultiplier(FMath::Lerp(Settings.PitchRange.X, Settings.PitchRange.Y, EngineAudioLoad));
        EngineAudio->SetVolumeMultiplier(FMath::Lerp(Settings.VolumeRange.X, Settings.VolumeRange.Y, EngineAudioLoad));
    }
    if (!bEngineAudioStarted)
    {
        // 가청 범위를 벗어났을 때 프레임마다 루프를 다시 시작하지 않는다.
        bEngineAudioStarted = true;
        EngineAudio->Play();
    }
}

void AMWVehiclePawn::StopEngineAudio()
{
    if (EngineAudio && bEngineAudioStarted)
    {
        EngineAudio->Stop();
        bEngineAudioStarted = false;
        EngineAudioLoad = 0.f;
    }
}
