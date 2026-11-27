#include "MWPlayerStartManager.h"

#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "Gameplay/Construction/MWTrackSnap.h"
#include "Gameplay/Track/MWTrackRuntimeSettings.h"
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

bool AMWPlayerStartManager::PreparePlayerStarts(int32 PlayerCount, const AActor* CourseStart, TArray<APlayerStart*>& OutStarts)
{
	OutStarts.Reset();
	UWorld* World = GetWorld();
	UArrowComponent* const Slots[] = {FrontLeft, FrontRight, RearLeft, RearRight};
	constexpr int32 SlotCount = static_cast<int32>(UE_ARRAY_COUNT(Slots));
	if (!World || GetNetMode() == NM_Client || PlayerCount < 1 || PlayerCount > SlotCount)
	{
		return false;
	}

	FTransform StartFrame;
	const TArray<FName>& Markers = GetDefault<UMWTrackRuntimeSettings>()->StartSlotMarkers;
	if (CourseStart)
	{
		FTransform LocalGrid;
		if (Markers.Num() < PlayerCount || !MWTrackSnap::ReadStartGrid(*CourseStart, LocalGrid))
		{
			UE_LOG(LogMWServer, Error, TEXT("[MWRace] 시작 트랙의 그리드 또는 출발 표식 설정이 없습니다."));
			return false;
		}
		StartFrame = LocalGrid * CourseStart->GetActorTransform();
	}

	TArray<FTransform, TInlineAllocator<4>> StartTransforms;
	for (int32 Index = 0; Index < PlayerCount; ++Index)
	{
		const UArrowComponent* Slot = Slots[Index];
		if (!CourseStart)
		{
			StartTransforms.Add(Slot->GetComponentTransform());
			continue;
		}
		FVector Position;
		if (!MWTrackSnap::ReadStartSlot(*CourseStart, Markers[Index], Position))
		{
			UE_LOG(LogMWServer, Error, TEXT("[MWRace] 시작 트랙에 출발 표식 %s가 없습니다."), *Markers[Index].ToString());
			return false;
		}
		// 바닥 표식에 테마에서 편집한 차량 시작 높이만 더한다.
		Position += StartFrame.GetUnitAxis(EAxis::Z) * Slot->GetRelativeLocation().Z;
		StartTransforms.Emplace(StartFrame.GetRotation(), Position);
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
		const FTransform& Transform = StartTransforms[Index];
		USceneComponent* Anchor = CourseStart ? GetRootComponent() : Slots[Index];
		if (!IsValid(Start))
		{
			Start = World->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), Transform.GetLocation(), Transform.Rotator(), Params);
			if (IsValid(Start))
			{
				NewStarts.Add(Start);
				// 기본 PlayerStart 루트는 Static이므로 이동 가능한 슬롯에 부착하기 전에 바꿉니다.
				Start->GetRootComponent()->SetMobility(EComponentMobility::Movable);
				if (!Start->AttachToComponent(Anchor, FAttachmentTransformRules::KeepWorldTransform))
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
				UE_LOG(LogMWServer, Error, TEXT("[MWRace] 출발 슬롯 %s 준비 실패."), *GetNameSafe(Anchor));
				return false;
			}
		}
		else
		{
			Start->SetActorLocationAndRotation(Transform.GetLocation(), Transform.GetRotation());
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
