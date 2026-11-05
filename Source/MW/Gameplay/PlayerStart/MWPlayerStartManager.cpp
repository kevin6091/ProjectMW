#include "MWPlayerStartManager.h"

#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "LogChannel.h"

AMWPlayerStartManager::AMWPlayerStartManager()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("GridRoot")));

	const auto CreateSlot = [this](FName Name)
	{
		auto* Slot = CreateDefaultSubobject<UArrowComponent>(Name);
		Slot->SetupAttachment(GetRootComponent());
		return Slot;
	};
	
	FrontLeft  = CreateSlot(TEXT("FrontLeft"));
	FrontRight = CreateSlot(TEXT("FrontRight"));
	RearLeft   = CreateSlot(TEXT("RearLeft"));
	RearRight  = CreateSlot(TEXT("RearRight"));
}

bool AMWPlayerStartManager::PreparePlayerStarts(int32 PlayerCount, TArray<APlayerStart*>& OutStarts)
{
	OutStarts.Reset();
	UWorld* World = GetWorld();
	UArrowComponent* const Slots[] = {FrontLeft, FrontRight, RearLeft, RearRight};
	constexpr int32 SlotCount = static_cast<int32>(UE_ARRAY_COUNT(Slots));
	if (!World || GetNetMode() == NM_Client || PlayerCount < 2 || PlayerCount > SlotCount)
	{
		return false;
	}

	SpawnedStarts.SetNum(SlotCount);
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// 다른 참가자의 도착 콜백에서 다시 호출돼도 이전 슬롯의 Start를 재사용합니다.
	TArray<APlayerStart*, TInlineAllocator<4>> NewStarts;
	for (int32 Index = 0; Index < PlayerCount; ++Index)
	{
		APlayerStart* Start = SpawnedStarts[Index];
		if (!IsValid(Start))
		{
			UArrowComponent* Slot = Slots[Index];
			Start = World->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), Slot->GetComponentLocation(), Slot->GetComponentRotation(), Params);
			if (IsValid(Start))
			{
				NewStarts.Add(Start);
				// 기본 PlayerStart 루트는 Static이므로 이동 가능한 슬롯에 부착하기 전에 바꿉니다.
				Start->GetRootComponent()->SetMobility(EComponentMobility::Movable);
				if (!Start->AttachToComponent(Slot, FAttachmentTransformRules::KeepWorldTransform))
				{
					Start = nullptr;
				}
			}

			if (!IsValid(Start))
			{
				for (APlayerStart* NewStart : NewStarts)
				{
					NewStart->Destroy();
				}
				OutStarts.Reset();
				UE_LOG(LogMWServer, Error, TEXT("[MWRace] 출발 슬롯 %s 준비 실패."), *GetNameSafe(Slot));
				return false;
			}
		}
		OutStarts.Add(Start);
	}

	for (int32 Index = 0; Index < PlayerCount; ++Index)
	{
		SpawnedStarts[Index] = OutStarts[Index];
	}
	for (int32 Index = PlayerCount; Index < SpawnedStarts.Num(); ++Index)
	{
		if (IsValid(SpawnedStarts[Index]))
		{
			SpawnedStarts[Index]->Destroy();
		}
		SpawnedStarts[Index] = nullptr;
	}

	return true;
}

void AMWPlayerStartManager::Destroyed()
{
	for (APlayerStart* Start : SpawnedStarts)
	{
		if (IsValid(Start))
		{
			Start->Destroy();
		}
	}
	SpawnedStarts.Reset();
	Super::Destroyed();
}