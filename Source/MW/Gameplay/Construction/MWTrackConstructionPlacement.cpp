#include "Gameplay/Construction/MWTrackConstructionComponent.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/Construction/MWPlayerConstructionComponent.h"
#include "Gameplay/Construction/MWTrackPreview.h"
#include "Gameplay/Track/MWTrackRuntimeSubsystem.h"
#include "Gameplay/Track/MWTrackRuntimeSettings.h"
#include "LogChannel.h"
#include "Player/Cursor/MWPlayerCursorComponent.h"
#include "System/GameState/MWGameplayGameState.h"
#include "System/MWPlayerState.h"

bool UMWTrackConstructionComponent::FPreparedTrack::SetTransform(const FTransform& Transform) const
{
	AActor* Track = Actor.Get();
	return Track && Track->SetActorTransform(Transform, false, nullptr, ETeleportType::TeleportPhysics);
}

void UMWTrackConstructionComponent::FPreparedTrack::Destroy() const
{
	if (AActor* Track = Actor.Get())
	{
		Track->Destroy();
	}
}

bool UMWTrackConstructionComponent::PrepareTrack(const TSoftClassPtr<AActor>& TrackClass, const FVector& Origin, bool bSinglePort,
	FPreparedTrack& OutTrack, FText& OutError)
{
	// 일반 조각과 시작·종료 조각 모두 같은 BP 참조로 준비한다.
	UClass* Class = TrackClass.LoadSynchronous();
	if (!Class || Class->HasAnyClassFlags(CLASS_Abstract))
	{
		OutError = FText::FromString(FString::Printf(TEXT("트랙 BP를 불러올 수 없습니다. %s"), *TrackClass.ToString()));
		return false;
	}

	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Track = GetWorld()->SpawnActor<AActor>(Class, FTransform(Origin), Params);
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
	if (auto* Runtime = GetWorld()->GetSubsystem<UMWTrackRuntimeSubsystem>())
	{
		Runtime->SetTrackPreview(*Track, true);
	}
	const bool bHasPorts = bSinglePort ? MWTrackSnap::ReadFirstPort(*Track, OutTrack.Ports[0])
		: MWTrackSnap::ReadPorts(*Track, OutTrack.Ports);
	if (!bHasPorts)
	{
		OutError = FText::FromString(FString::Printf(
			TEXT("%s의 연결점을 확인해 주세요."), *GetNameSafe(Class)));
		OutTrack.Destroy();
		return false;
	}
	return true;
}

void UMWTrackConstructionComponent::PublishTrack(const FPreparedTrack& Prepared, const FTransform& Transform,
	FMWTrackEndId ConnectedTo, int32 PortIndex)
{
	AActor* Track = Prepared.Actor.Get();
	check(Track);
	// 설치된 도로와 같은 위치에 있던 미리보기 선을 도로가 보이기 전에 숨긴다.
	MWTrackPreview::HideWireframes(*Track);
	Track->SetActorHiddenInGame(false);
	Track->SetActorEnableCollision(Prepared.bEnableCollision);
	Track->SetActorTickEnabled(Prepared.bEnableTick);
	Track->bAlwaysRelevant = true;
	Track->SetReplicates(true);
	// 설치 도로는 움직이지 않는다. 생성 패킷의 양자화 오차는 확정 Transform으로 보정한다.
	Track->SetReplicateMovement(false);
	if (auto* Runtime = GetWorld()->GetSubsystem<UMWTrackRuntimeSubsystem>())
	{
		Runtime->SetTrackPreview(*Track, false);
	}
	Track->ForceNetUpdate();

	const int32 PieceId = PlacementSession.PlacedTracks.Num() + 1;
	PlacementSession.PlacedTracks.Add({PieceId, Track, Transform, ConnectedTo, PortIndex,
		TSoftClassPtr<AActor>(Track->GetClass())});
	PlacementSession.CourseBounds += Track->GetComponentsBoundingBox(false);
	++PlacementSession.CourseRevision;
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
	if (!HasServerAuthority() || !GS || !AreAllSelectionsConfirmed() || PlacementSession.bIsOpen || !PlacementSession.PlacedTracks.IsEmpty()
		|| BuildFrame.ContainsNaN() || !BuildFrame.GetRotation().IsNormalized() || !FMath::IsFinite(LayoutScale) || LayoutScale < 1.0)
	{
		OutError = FText::FromString(TEXT("트랙 배치를 시작할 수 없습니다."));
		return false;
	}
	const auto* Settings = GetDefault<UMWTrackRuntimeSettings>();
	const auto& StartTrackClass = Settings->StartTrackClass;
	const auto& FinishTrackClass = Settings->FinishTrackClass;
	if (StartTrackClass.IsNull() || FinishTrackClass.IsNull() || StartTrackClass == FinishTrackClass)
	{
		OutError = FText::FromString(TEXT("프로젝트 설정에서 코스의 시작 및 종료 트랙 BP를 지정해 주세요."));
		return false;
	}
	const FVector BuildOrigin = BuildFrame.GetLocation();

	TMap<TWeakObjectPtr<AMWPlayerState>, FPreparedTrack> PreparedTracks;
	TMap<TWeakObjectPtr<AMWPlayerState>, FMWPlayerTrackPlacement> PlayerPlacements;
	FPreparedTrack Starter;
	FPreparedTrack EndCap;
	const auto DiscardPrepared = [&PreparedTracks, &Starter, &EndCap]()
	{
		for (const auto& Entry : PreparedTracks)
		{
			Entry.Value.Destroy();
		}
		Starter.Destroy();
		EndCap.Destroy();
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
			|| Selection.OffersRevision != SelectionSession.OffersRevision || !Offer
			|| Offer->ClaimedByMemberId != PS->GetLobbyMemberId())
		{
			DiscardPrepared();
			OutError = FText::FromString(TEXT("아직 확정하지 않은 트랙이 있습니다."));
			return false;
		}

		FPreparedTrack& Prepared = PreparedTracks.Add(Participant);
		if (!PrepareTrack(Offer->TrackClass, BuildOrigin, false, Prepared, OutError))
		{
			DiscardPrepared();
			return false;
		}
		auto& Player = PlayerPlacements.Add(Participant);
		Player.SessionId = SelectionSession.SelectionSessionId;
		Player.TrackClass = Offer->TrackClass;
	}

	if (PreparedTracks.IsEmpty())
	{
		OutError = FText::FromString(TEXT("배치에 참가할 플레이어가 없습니다."));
		return false;
	}

	// 종료 조각까지 미리 검증해, 배치를 다 끝낸 뒤 에셋이나 연결점 오류로 멈추는 일을 막는다.
	if (!PrepareTrack(StartTrackClass, BuildOrigin, true, Starter, OutError)
		|| !PrepareTrack(FinishTrackClass, BuildOrigin, true, EndCap, OutError))
	{
		DiscardPrepared();
		return false;
	}
	FTransform StartGrid;
	if (!MWTrackSnap::ReadStartGrid(*Starter.Actor.Get(), StartGrid))
	{
		DiscardPrepared();
		OutError = FText::FromString(TEXT("시작 트랙의 StartGrid 기준점을 확인해 주세요."));
		return false;
	}
	const FTransform StartTransform = StartGrid.Inverse() * FTransform(BuildFrame.GetRotation(), BuildOrigin);
	if (!Starter.SetTransform(StartTransform))
	{
		DiscardPrepared();
		OutError = FText::FromString(TEXT("시작 트랙을 설치할 수 없습니다."));
		return false;
	}
	PublishTrack(Starter, StartTransform);
	PlacementSession.OpenEnds.Add({{1, 0}, Starter.Ports[0] * StartTransform});
	// 일반 트랙 BP의 기본 진행축은 +Y다. 자유 미리보기는 레벨의 출발 방향을 기준으로 돌린다.
	PlacementSession.PreviewYaw = FRotator::NormalizeAxis(BuildFrame.Rotator().Yaw - 90.0);

	PendingEndCap = EndCap;
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
			// 서버가 설치할 조각을 받은 참가자에게만 배치 포인터 전송을 허용한다.
			PS->GetCursorComponent()->OpenStreamForServer(MWCursorContexts::TrackPlacement, EMWCursorSpace::World);
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

	if (Player.bSnapped != bSnapped || Player.bReversed != Intent.bReversed || !Player.PreviewTransform.Equals(Display))
	{
		Player.bSnapped = bSnapped;
		Player.bReversed = Intent.bReversed;
		Player.PreviewTransform = Display;
		Component->SetPlacementForServer(Player);
	}
}

EMWTrackPlacementResult UMWTrackConstructionComponent::PlaceTrack(APlayerController* Requester,
	int32 SessionId, int32 CourseRevision, const FMWTrackPlacementIntent& Intent)
{
	FPreparedTrack* Prepared = FindPendingTrack(Requester, SessionId);
	if (bCommittingPlacement || !Prepared || !Prepared->Actor.IsValid() || PlacementSession.OpenEnds.Num() != 1
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
	const int32 PieceId = PlacementSession.PlacedTracks.Num() + 1;
	const int32 FreePort = 1 - Intent.SourcePort;
	const FMWOpenTrackEnd NextEnd{{PieceId, FreePort}, Prepared->Ports[FreePort] * Transform};
	const bool bLastPlayer = PendingTracks.Num() == 1;
	FTransform EndTransform;
	// 마지막 조각과 종료 캡의 배치를 모두 확인한 뒤 두 Actor를 공개한다.
	if (!Prepared->SetTransform(Transform) || (bLastPlayer && !PositionEndCap(NextEnd, EndTransform)))
	{
		return EMWTrackPlacementResult::Unavailable;
	}

	AMWPlayerState* PS = Requester->GetPlayerState<AMWPlayerState>();
	auto* Component = PS->GetPlayerConstructionComponent();
	auto Player = Component->GetPlacement();
	PublishTrack(*Prepared, Transform, Intent.Target, Intent.SourcePort);
	PlacementSession.OpenEnds[0] = NextEnd;
	PendingTracks.Remove(TWeakObjectPtr<AMWPlayerState>(PS));
	// 설치가 끝난 참가자의 포인터는 남은 참가자의 배치 화면에서 제거한다.
	PS->GetCursorComponent()->CloseStreamForServer();
	Player.bPlaced = true;
	Player.PreviewTransform = Transform;
	Component->SetPlacementForServer(Player);
	if (bLastPlayer)
	{
		FinishPlacement(NextEnd, EndTransform);
	}
	GetOwner()->ForceNetUpdate();
	UE_LOG(LogMWServer, Log, TEXT("[MWTrackPlacement] Placed piece=%d member=%d target=%d/%d source=%d"),
		PieceId, PS->GetLobbyMemberId(), Intent.Target.PieceId, Intent.Target.PortIndex, Intent.SourcePort);
	return EMWTrackPlacementResult::Accepted;
}

bool UMWTrackConstructionComponent::PositionEndCap(const FMWOpenTrackEnd& Tail, FTransform& OutTransform) const
{
	if (!PendingEndCap.Actor.IsValid())
	{
		return false;
	}
	OutTransform = MWTrackSnap::AlignPorts(PendingEndCap.Ports[0], Tail.WorldFrame);
	return PendingEndCap.SetTransform(OutTransform);
}

void UMWTrackConstructionComponent::FinishPlacement(const FMWOpenTrackEnd& Tail, const FTransform& EndTransform)
{
	// 캡에는 다음 열린 포트가 없다. 코스 복제와 차량 준비는 캡까지 포함한 리비전을 기다린다.
	PublishTrack(PendingEndCap, EndTransform, Tail.Id, 0);
	PendingEndCap = {};
	PlacementSession.OpenEnds.Reset();
	PlacementSession.bIsOpen = false;
	GetOwner()->ForceNetUpdate();
	OnPlacementFinished.Broadcast();
}

void UMWTrackConstructionComponent::RemovePlacementPlayer(AMWPlayerState* Player)
{
	FPreparedTrack Prepared;
	if (HasServerAuthority() && Player
		&& PendingTracks.RemoveAndCopyValue(TWeakObjectPtr<AMWPlayerState>(Player), Prepared))
	{
		Player->GetCursorComponent()->CloseStreamForServer();
		Prepared.Destroy();
		if (PendingTracks.IsEmpty())
		{
			FTransform EndTransform;
			if (PlacementSession.OpenEnds.Num() == 1 && PositionEndCap(PlacementSession.OpenEnds[0], EndTransform))
			{
				FinishPlacement(PlacementSession.OpenEnds[0], EndTransform);
				return;
			}
			CancelPlacement();
			if (auto* GS = Cast<AMWGameplayGameState>(GetOwner()))
			{
				GS->SetRoomPhaseFromServer(EMWRoomPhase::Failed);
			}
		}
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
		if (AMWPlayerState* PS = Entry.Key.Get()) PS->GetCursorComponent()->CloseStreamForServer();
		Entry.Value.Destroy();
	}
	PendingTracks.Reset();
	PendingEndCap.Destroy();
	PendingEndCap = {};
	PlacementSession.bIsOpen = false;
	GetOwner()->ForceNetUpdate();
}
