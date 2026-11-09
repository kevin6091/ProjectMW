#include "Gameplay/Construction/MWTrackConstructionComponent.h"

#include "Data/MWTrackDefinitionData.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/Construction/MWPlayerConstructionComponent.h"
#include "Gameplay/Construction/MWTrackPreview.h"
#include "LogChannel.h"
#include "System/GameState/MWGameplayGameState.h"
#include "System/MWAssetManager.h"
#include "System/MWPlayerState.h"

bool UMWTrackConstructionComponent::PrepareTrack(FGameplayTag DefinitionId, const FVector& Origin,
	FPreparedTrack& OutTrack, FText& OutError)
{
	const auto* Definition = UMWAssetManager::Get().LoadAssetSynchronous<UMWTrackDefinitionData>(DefinitionId);
	if (!Definition || !Definition->GetTrackActorClass())
	{
		OutError = FText::FromString(FString::Printf(TEXT("트랙 정의를 불러올 수 없습니다. %s"), *DefinitionId.ToString()));
		return false;
	}

	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Track = GetWorld()->SpawnActor<AActor>(Definition->GetTrackActorClass(), FTransform(Origin), Params);
	if (!Track)
	{
		OutError = FText::FromString(TEXT("배치할 트랙을 만들 수 없습니다."));
		return false;
	}

	// 서버는 BP에서 포트를 직접 읽고 설치 전까지 Actor를 숨겨 둔다.
	OutTrack.Actor = Track;
	OutTrack.bEnableCollision = Track->GetActorEnableCollision();
	OutTrack.bEnableTick = Track->IsActorTickEnabled();
	Track->SetReplicates(false);
	Track->SetActorHiddenInGame(true);
	Track->SetActorEnableCollision(false);
	Track->SetActorTickEnabled(false);
	if (!MWTrackSnap::ReadPorts(*Track, OutTrack.Ports))
	{
		OutError = FText::FromString(FString::Printf(
			TEXT("%s의 ConnectionPoint1/2를 확인해 주세요."), *DefinitionId.ToString()));
		Track->Destroy();
		return false;
	}
	return true;
}

bool UMWTrackConstructionComponent::PublishTrack(const FPreparedTrack& Prepared, const FTransform& Transform)
{
	AActor* Track = Prepared.Actor.Get();
	if (!Track || !Track->SetActorTransform(Transform, false, nullptr, ETeleportType::TeleportPhysics))
	{
		return false;
	}
	// 설치된 도로와 같은 위치에 있던 미리보기 선을 도로가 보이기 전에 숨긴다.
	MWTrackPreview::HideWireframes(*Track);
	Track->SetActorHiddenInGame(false);
	Track->SetActorEnableCollision(Prepared.bEnableCollision);
	Track->SetActorTickEnabled(Prepared.bEnableTick);
	Track->bAlwaysRelevant = true;
	Track->SetReplicates(true);
	// 설치 도로는 움직이지 않는다. 생성 패킷의 양자화 오차는 확정 Transform으로 보정한다.
	Track->SetReplicateMovement(false);
	Track->ForceNetUpdate();
	return true;
}

void UMWTrackConstructionComponent::OnRep_PlacementSession()
{
	for (const auto& Placed : PlacementSession.PlacedTracks)
	{
		if (IsValid(Placed.Actor))
		{
			// 컴포넌트 표시 상태는 Actor의 생성 정보와 별개라 수신 쪽에서도 적용한다.
			MWTrackPreview::HideWireframes(*Placed.Actor);
			if (!Placed.Actor->GetActorTransform().Equals(Placed.WorldTransform))
			{
				Placed.Actor->SetActorTransform(Placed.WorldTransform, false, nullptr, ETeleportType::TeleportPhysics);
			}
		}
	}
}

bool UMWTrackConstructionComponent::BeginPlacement(const FTransform& BuildFrame, double LayoutScale, FText& OutError)
{
	auto* GS = Cast<AMWGameplayGameState>(GetOwner());
	if (!HasServerAuthority() || !GS || !AreAllSelectionsConfirmed() || PlacementSession.bIsOpen
		|| BuildFrame.ContainsNaN() || !BuildFrame.GetRotation().IsNormalized() || !FMath::IsFinite(LayoutScale) || LayoutScale < 1.0)
	{
		OutError = FText::FromString(TEXT("트랙 배치를 시작할 수 없습니다."));
		return false;
	}
	const FVector BuildOrigin = BuildFrame.GetLocation();

	TMap<TWeakObjectPtr<AMWPlayerState>, FPreparedTrack> PreparedTracks;
	TMap<TWeakObjectPtr<AMWPlayerState>, FMWPlayerTrackPlacement> PlayerPlacements;
	const auto DiscardPrepared = [&PreparedTracks]()
	{
		for (const auto& Entry : PreparedTracks)
		{
			if (AActor* Track = Entry.Value.Actor.Get())
			{
				Track->Destroy();
			}
		}
	};

	for (const auto& Participant : Participants)
	{
		AMWPlayerState* PS = Participant.Get();
		if (!PS || PS->IsInactive())
		{
			continue;
		}
		const auto Selection = PS->GetPlayerConstructionComponent()->GetSelection();
		const auto* Offer = SelectionSession.Offers.FindByPredicate([&Selection](const FMWTrackOffer& Item)
		{
			return Item.OfferId == Selection.SelectedOfferId;
		});
		if (!Selection.bConfirmed || Selection.SelectionSessionId != SelectionSession.SelectionSessionId
			|| Selection.OffersRevision != SelectionSession.OffersRevision || !Offer)
		{
			DiscardPrepared();
			OutError = FText::FromString(TEXT("아직 확정하지 않은 트랙이 있습니다."));
			return false;
		}

		FPreparedTrack& Prepared = PreparedTracks.Add(Participant);
		if (!PrepareTrack(Offer->DefinitionAssetId, BuildOrigin, Prepared, OutError))
		{
			DiscardPrepared();
			return false;
		}
		auto& Player = PlayerPlacements.Add(Participant);
		Player.SessionId = SelectionSession.SelectionSessionId;
		Player.DefinitionAssetId = Offer->DefinitionAssetId;
	}

	if (PreparedTracks.IsEmpty())
	{
		OutError = FText::FromString(TEXT("배치에 참가할 플레이어가 없습니다."));
		return false;
	}

	// 첫 라운드에는 기준 트랙을 한 개 놓는다. 이후 배치 세션은 기존 코스 끝을 이어 쓸 수 있다.
	if (PlacementSession.PlacedTracks.IsEmpty())
	{
		const FGameplayTag StarterId = SelectionSession.Offers[0].DefinitionAssetId;
		FPreparedTrack Starter;
		if (!PrepareTrack(StarterId, BuildOrigin, Starter, OutError))
		{
			DiscardPrepared();
			return false;
		}
		const FVector Center = (Starter.Ports[0].GetLocation() + Starter.Ports[1].GetLocation()) * 0.5;
		// 두 번째 포트의 바깥 방향이 출발 슬롯의 전방과 일치하도록 도로를 돌린다.
		const FQuat Rotation = BuildFrame.GetRotation() * Starter.Ports[1].GetRotation().Inverse();
		const FTransform Transform(Rotation, BuildOrigin - Rotation.RotateVector(Center));
		if (!PublishTrack(Starter, Transform))
		{
			Starter.Actor->Destroy();
			DiscardPrepared();
			OutError = FText::FromString(TEXT("기준 트랙을 설치할 수 없습니다."));
			return false;
		}
		PlacementSession.PlacedTracks.Add({1, StarterId, Starter.Actor.Get(), Transform, {}, INDEX_NONE});
		for (int32 Port = 0; Port < 2; ++Port)
		{
			PlacementSession.OpenEnds.Add({{1, Port}, Starter.Ports[Port] * Transform});
		}
		PlacementSession.CourseRevision = 1;
		PlacementSession.PreviewYaw = Rotation.Rotator().Yaw;
	}

	PendingTracks = MoveTemp(PreparedTracks);
	PlacementSession.BuildOrigin = BuildOrigin;
	PlacementSession.LayoutScale = LayoutScale;
	PlacementSession.SessionId = SelectionSession.SelectionSessionId;
	PlacementSession.bIsOpen = true;
	int32 OffsetIndex = 0;
	for (auto& Entry : PlayerPlacements)
	{
		if (AMWPlayerState* PS = Entry.Key.Get())
		{
			// 아직 포인터를 보내지 않은 플레이어의 프리뷰는 출발 도로 옆에 둔다.
			FMWTrackPlacementIntent Intent;
			Intent.CursorLocation = BuildOrigin + BuildFrame.GetUnitAxis(EAxis::Y) * (600.0 * LayoutScale * ++OffsetIndex);
			Entry.Value.PreviewTransform = MWTrackSnap::MakeRawTransform(PendingTracks.FindChecked(Entry.Key).Ports, PlacementSession, Intent);
			PS->GetPlayerConstructionComponent()->SetPlacementForServer(Entry.Value);
		}
	}
	EndSelection();
	GS->SetRoomPhaseFromServer(EMWRoomPhase::TrackPlacement);
	OutError = FText::GetEmpty();
	UE_LOG(LogMWServer, Log, TEXT("[MWTrackPlacement] Opened session=%d players=%d"),
		PlacementSession.SessionId, PendingTracks.Num());
	return true;
}

UMWTrackConstructionComponent::FPreparedTrack* UMWTrackConstructionComponent::FindPendingTrack(
	APlayerController* Requester, int32 SessionId)
{
	const auto* GS = Cast<AMWGameplayGameState>(GetOwner());
	if (!HasServerAuthority() || !GS || GS->GetRoomPhase() != EMWRoomPhase::TrackPlacement
		|| !PlacementSession.bIsOpen || PlacementSession.SessionId != SessionId
		|| !IsValid(Requester) || Requester->GetWorld() != GetWorld())
	{
		return nullptr;
	}
	AMWPlayerState* PS = Requester->GetPlayerState<AMWPlayerState>();
	if (!IsValid(PS) || PS->IsInactive() || PS->GetOwner() != Requester || !GS->PlayerArray.Contains(PS))
	{
		return nullptr;
	}
	return PendingTracks.Find(TWeakObjectPtr<AMWPlayerState>(PS));
}

void UMWTrackConstructionComponent::UpdatePlacementPreview(APlayerController* Requester, int32 SessionId,
	int32 Sequence, const FMWTrackPlacementIntent& Intent)
{
	FPreparedTrack* Prepared = FindPendingTrack(Requester, SessionId);
	const double Now = FPlatformTime::Seconds();
	if (!Prepared || Sequence <= Prepared->LastPreviewSequence || Now < Prepared->NextPreviewTime
		|| !MWTrackSnap::IsInBuildArea(Intent, PlacementSession))
	{
		return;
	}
	Prepared->LastPreviewSequence = Sequence;
	Prepared->NextPreviewTime = Now + 1.0 / 30.0;

	auto* Component = Requester->GetPlayerState<AMWPlayerState>()->GetPlayerConstructionComponent();
	auto Player = Component->GetPlacement();
	FTransform Display;
	const bool bSnapped = MWTrackSnap::Resolve(Prepared->Ports, PlacementSession, Intent, Display);
	if (Player.bSnapped != bSnapped || !Player.PreviewTransform.Equals(Display))
	{
		Player.bSnapped = bSnapped;
		Player.PreviewTransform = Display;
		Component->SetPlacementForServer(Player);
	}
}

EMWTrackPlacementResult UMWTrackConstructionComponent::PlaceTrack(APlayerController* Requester,
	int32 SessionId, int32 CourseRevision, const FMWTrackPlacementIntent& Intent)
{
	FPreparedTrack* Prepared = FindPendingTrack(Requester, SessionId);
	if (bCommittingPlacement || !Prepared || !Prepared->Actor.IsValid()
		|| !MWTrackSnap::IsInBuildArea(Intent, PlacementSession))
	{
		return EMWTrackPlacementResult::Unavailable;
	}
	if (CourseRevision != PlacementSession.CourseRevision)
	{
		return EMWTrackPlacementResult::CourseChanged;
	}

	FTransform Transform;
	if (!MWTrackSnap::Resolve(Prepared->Ports, PlacementSession, Intent, Transform))
	{
		return EMWTrackPlacementResult::NotSnapped;
	}
	TGuardValue<bool> CommitGuard(bCommittingPlacement, true);
	if (!PublishTrack(*Prepared, Transform))
	{
		return EMWTrackPlacementResult::Unavailable;
	}

	AMWPlayerState* PS = Requester->GetPlayerState<AMWPlayerState>();
	auto* Component = PS->GetPlayerConstructionComponent();
	auto Player = Component->GetPlacement();
	const int32 PieceId = PlacementSession.PlacedTracks.Num() + 1;
	const int32 FreePort = 1 - Intent.SourcePort;
	PlacementSession.PlacedTracks.Add({PieceId, Player.DefinitionAssetId, Prepared->Actor.Get(), Transform, Intent.Target, Intent.SourcePort});
	PlacementSession.OpenEnds.RemoveAll([&Intent](const FMWOpenTrackEnd& End) { return End.Id == Intent.Target; });
	PlacementSession.OpenEnds.Add({{PieceId, FreePort}, Prepared->Ports[FreePort] * Transform});
	++PlacementSession.CourseRevision;
	PendingTracks.Remove(TWeakObjectPtr<AMWPlayerState>(PS));
	Player.bPlaced = true;
	Player.PreviewTransform = Transform;
	Component->SetPlacementForServer(Player);
	FinishPlacementIfReady();
	GetOwner()->ForceNetUpdate();
	UE_LOG(LogMWServer, Log, TEXT("[MWTrackPlacement] Placed piece=%d member=%d target=%d/%d source=%d"),
		PieceId, PS->GetLobbyMemberId(), Intent.Target.PieceId, Intent.Target.PortIndex, Intent.SourcePort);
	return EMWTrackPlacementResult::Accepted;
}

void UMWTrackConstructionComponent::FinishPlacementIfReady()
{
	if (PlacementSession.bIsOpen && PendingTracks.IsEmpty())
	{
		// 코스와 현재 레벨을 유지한다. 이후 GameMode가 이 완료 상태에서 차량 준비로 전환한다.
		PlacementSession.bIsOpen = false;
		GetOwner()->ForceNetUpdate();
		OnPlacementFinished.Broadcast();
	}
}

void UMWTrackConstructionComponent::RemovePlacementPlayer(AMWPlayerState* Player)
{
	FPreparedTrack Prepared;
	if (HasServerAuthority() && Player
		&& PendingTracks.RemoveAndCopyValue(TWeakObjectPtr<AMWPlayerState>(Player), Prepared))
	{
		if (AActor* Track = Prepared.Actor.Get())
		{
			Track->Destroy();
		}
		FinishPlacementIfReady();
	}
}

void UMWTrackConstructionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelPlacement();
	Super::EndPlay(EndPlayReason);
}

void UMWTrackConstructionComponent::CancelPlacement()
{
	if (!HasServerAuthority() || !PlacementSession.bIsOpen)
	{
		return;
	}
	// 이미 설치한 도로는 월드가 정리한다. 취소 시에는 서버의 미확정 Actor만 파괴한다.
	for (const auto& Entry : PendingTracks)
	{
		if (AActor* Track = Entry.Value.Actor.Get())
		{
			Track->Destroy();
		}
	}
	PendingTracks.Reset();
	PlacementSession.bIsOpen = false;
	GetOwner()->ForceNetUpdate();
}
