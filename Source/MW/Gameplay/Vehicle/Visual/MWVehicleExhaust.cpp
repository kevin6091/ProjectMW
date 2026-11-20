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
    if (!State || State->GetRoomPhase() != EMWRoomPhase::Racing)
    {
        StopExhaustEffect();
        return;
    }

    const auto& Settings = *GetDefault<UMWVehicleExhaustSettings>();
    const double LongitudinalSpeed = FMath::Abs(FVector::DotProduct(GetVelocity(), GetActorForwardVector()));
    const bool bPedal = IsLocallyControlled() && FMath::Max(DriveInput.Throttle, DriveInput.Reverse) > UE_SMALL_NUMBER;
    // 일반 배기와 아이템 분사구가 같은 프레임에서 겹치지 않게 한다.
    const bool bEmitting = (LongitudinalSpeed >= Settings.WakeSpeed || bPedal) && !ItemComponent->IsBoosterActive();
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
