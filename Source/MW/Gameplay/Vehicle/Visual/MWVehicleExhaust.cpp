#include "Gameplay/Vehicle/Visual/MWVehicleExhaustSettings.h"

#include "Components/BoxComponent.h"
#include "Gameplay/Items/MWItemComponent.h"
#include "Gameplay/Vehicle/MWVehiclePawn.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "System/GameState/MWGameplayGameState.h"

void AMWVehiclePawn::UpdateExhaustEffect()
{
    if (GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    const auto* State = GetWorld()->GetGameState<AMWGameplayGameState>();
    // 소유자 전용 부스터 타이머 대신 이 창의 분사구가 제거되는 시점까지 기다린다.
    if (!State || State->GetRoomPhase() != EMWRoomPhase::Racing || ItemComponent->IsBoosterJetActive())
    {
        StopExhaustEffect();
        return;
    }

    const auto& Settings = *GetDefault<UMWVehicleExhaustSettings>();
    const double LongitudinalSpeed = FMath::Abs(FVector::DotProduct(GetVelocity(), GetActorForwardVector()));
    const bool bPedal = IsLocallyControlled() && FMath::Max(DriveInput.Throttle, DriveInput.Reverse) > UE_SMALL_NUMBER;
    const bool bEmitting = LongitudinalSpeed >= Settings.WakeSpeed || bPedal;
    if (bEmitting == bExhaustEmitting)
    {
        return;
    }
    if (ExhaustEffects.IsEmpty())
    {
        if (bExhaustLoadFailed || Settings.System.IsNull())
        {
            return;
        }
        UNiagaraSystem* System = Settings.System.LoadSynchronous();
        bExhaustLoadFailed = !IsValid(System);
        if (bExhaustLoadFailed)
        {
            UE_LOG(LogTemp, Warning, TEXT("[MWVehicleExhaust] 나이아가라 시스템 로드 실패: %s"), *Settings.System.ToString());
            return;
        }

        const FVector HalfSize = CollisionRoot->GetUnscaledBoxExtent();
        const FVector Rear = Settings.RearAnchor * HalfSize;
        const double Side = Settings.SideOffset * HalfSize.Y;
        for (const double Lateral : {-Side, Side})
        {
            auto* Exhaust = NewObject<UNiagaraComponent>(this, NAME_None, RF_Transient);
            Exhaust->ComponentTags.Add(TEXT("MWVehicleExhaust"));
            Exhaust->SetAutoActivate(false);
            Exhaust->SetAutoDestroy(false);
            Exhaust->SetAsset(System);
            Exhaust->SetupAttachment(DrivePresentation);
            Exhaust->SetRelativeLocation(Rear + FVector(0, Lateral, 0));
            Exhaust->SetRelativeScale3D(FVector(Settings.EffectScale));
            AddInstanceComponent(Exhaust);
            Exhaust->RegisterComponent();
            ExhaustEffects.Add(Exhaust);
        }
    }

    bExhaustEmitting = bEmitting;
    for (UNiagaraComponent* Exhaust : ExhaustEffects)
    {
        if (bEmitting)
        {
            Exhaust->Activate(false);
        }
        else
        {
            // 정차하면 새 입자 생성을 멈추고 이미 나온 입자는 끝까지 감쇠한다.
            Exhaust->Deactivate();
        }
    }
}

void AMWVehiclePawn::StopExhaustEffect()
{
    for (UNiagaraComponent* Exhaust : ExhaustEffects)
    {
        if (Exhaust->IsActive())
        {
            Exhaust->DeactivateImmediate();
        }
    }
    bExhaustEmitting = false;
}
