#include "Gameplay/Construction/MWTrackPlacementViewComponent.h"

#include "Components/SceneComponent.h"
#include "Data/MWInputData.h"
#include "Gameplay/Track/MWTrackRuntimeSubsystem.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Gameplay/Construction/MWPlayerConstructionComponent.h"
#include "Gameplay/Construction/MWTrackConstructionComponent.h"
#include "Gameplay/Construction/MWTrackPreview.h"
#include "Gameplay/Construction/MWTrackPlacementClearance.h"
#include "InputMappingContext.h"
#include "MWGameplayTags.h"
#include "Player/MWGameplayPlayerController.h"
#include "System/GameState/MWGameplayGameState.h"
#include "System/MWAssetManager.h"
#include "System/MWPlayerState.h"
#include "UI/MWTrackSelectionWidget.h"
#include "UI/Cursor/MWCursorProjection.h"
#include "UnrealClient.h"

namespace
{
	constexpr double GhostSnapBlendSeconds = 0.12;
	constexpr double GhostSnapSettleSeconds = 0.18;
	constexpr double MaxSnapOvershoot = 38.0;
	constexpr double SnapContactTolerance = 10.0;
	constexpr float HeldSwayMaxAngleDegrees = 2.0f;
	constexpr double HeldSwayDegreesAtOneWidthPerSecond = 4.0;
	constexpr double HeldSwayMinAngleDegrees = 0.02;
	const FLinearColor LocalColor(0.05f, 0.55f, 1.0f);
	const FLinearColor SnapColor(0.1f, 1.0f, 0.35f);
	const FLinearColor RemoteColors[] = {
		FLinearColor(1.0f, 0.42f, 0.08f),
		FLinearColor(0.82f, 0.18f, 0.75f),
		FLinearColor(0.55f, 0.26f, 1.0f)
	};

	double GetProjectedHorizontalDelta(const APlayerController& PC, const FVector& From, const FVector& To)
	{
		if (From.Equals(To))
		{
			return 0.0;
		}

		int32 ViewportWidth, ViewportHeight;
		PC.GetViewportSize(ViewportWidth, ViewportHeight);
		FVector2D FromScreen, ToScreen;
		if (ViewportWidth <= 0 || !PC.ProjectWorldLocationToScreen(From, FromScreen)
			|| !PC.ProjectWorldLocationToScreen(To, ToScreen))
		{
			return 0.0;
		}
		return (ToScreen.X - FromScreen.X) / ViewportWidth;
	}
}

AActor* UMWTrackPlacementViewComponent::GetArrivalGhost(int32 ExpectedSession, int32 MemberId) const
{
    const FGhost* Ghost = SessionId == ExpectedSession ? Ghosts.Find(MemberId) : nullptr;
    return Ghost ? Ghost->Actor.Get() : nullptr;
}

void UMWTrackPlacementViewComponent::Update(float DeltaSeconds, UEnhancedInputComponent* Input)
{
	auto* PC = Cast<AMWGameplayPlayerController>(GetOwner());
	const auto* GS = GetWorld()->GetGameState<AMWGameplayGameState>();
	if (!PC || !PC->IsLocalController() || !GS || GS->GetRoomPhase() != EMWRoomPhase::TrackPlacement)
	{
		if (SessionId != 0)
		{
			ResetView();
		}
		return;
	}
	const auto& Session = GS->GetConstructionComponent()->GetPlacementSession();
	if (SessionId != Session.SessionId)
	{
		ResetView();
		SessionId = Session.SessionId;
	}

	const auto* OwnPS = PC->GetPlayerState<AMWPlayerState>();
	TSet<int32> VisiblePlayers;
	bool bOwnPlaced = false;
	bool bOwnReady = false;
	bool bCanPlace = false;
	for (const APlayerState* State : GS->PlayerArray)
	{
		const auto* PS = Cast<AMWPlayerState>(State);
		if (!IsValid(PS) || PS->IsInactive() || PS->GetLobbyMemberId() <= 0)
		{
			continue;
		}
		const auto Player = PS->GetPlayerConstructionComponent()->GetPlacement();
		if (SessionId <= 0 || Player.SessionId != SessionId)
		{
			continue;
		}
		const bool bLocal = PS == OwnPS;
		const bool bPlaced = Player.bPlaced || (bLocal && bAccepted);
		const int32 PlayerId = PS->GetLobbyMemberId();
		if (bLocal)
		{
			bOwnReady = true;
			bOwnPlaced = bPlaced;
			if (!AwaitedReplacementFor.IsNull() && Player.TrackClass != AwaitedReplacementFor)
			{
				AwaitedReplacementFor.Reset();
			}
		}
		if (bPlaced || !Session.bIsOpen)
		{
			continue;
		}

		if (FGhost* Ghost = GetOrCreateGhost(PlayerId, Player, bLocal))
		{
			VisiblePlayers.Add(PlayerId);
			const bool bWasBlending = Ghost->SnapBlend.IsSet();
			double ScreenDelta = 0.0;
			if (bLocal)
			{
				TOptional<FPointerSample> Pointer;
				bCanPlace = UpdateLocalPreview(*PC, Session, *Ghost, DeltaSeconds, Pointer);
				if (Ghost->HeldSway.IsSet())
				{
					auto& Previous = Ghost->HeldSway->PreviousPointer;
					// 포커스·뷰포트 크기가 바뀌거나 스냅으로 이동할 때는 포인터 속도를 새로 잰다.
					if (Pointer.IsSet() && Previous.IsSet() && Pointer->ViewportWidth == Previous->ViewportWidth
						&& !Ghost->bSnapped && !bWasBlending && !Ghost->SnapBlend.IsSet())
					{
						ScreenDelta = (Pointer->X - Previous->X) / Pointer->ViewportWidth;
					}
					Previous = Pointer;
				}
			}
			else if (AActor* Actor = Ghost->Actor.Get())
			{
				const double FreeMoveAlpha = FMath::Clamp(DeltaSeconds * 16.0, 0.0, 1.0);
				// 서버의 회전 플래그로 자유 회전 시작을 찾고, 표시 자세만 포트 중심으로 보간한다.
				const bool bTurned = Ghost->bLastRemoteReversed != Player.bReversed;
				Ghost->bLastRemoteReversed = Player.bReversed;
				const FTransform Pose = BlendGhostPose(*Ghost, Player.PreviewTransform, Player.bSnapped, DeltaSeconds, FreeMoveAlpha, bTurned);
				if (Ghost->HeldSway.IsSet() && !Player.bSnapped && !bWasBlending && !Ghost->SnapBlend.IsSet())
				{
					// 원격 입력은 기존 복제 자세로 표시한다. 두 자세의 포트 중앙을 같은 카메라로 투영한다.
					const FVector Center = (Ghost->Ports[0].GetLocation() + Ghost->Ports[1].GetLocation()) * 0.5;
					ScreenDelta = GetProjectedHorizontalDelta(*PC, Actor->GetActorTransform().TransformPosition(Center),
						Pose.TransformPosition(Center));
				}
				Actor->SetActorLocationAndRotation(Pose.GetLocation(), Pose.GetRotation());
			}
			UpdateHeldSway(*Ghost, ScreenDelta, DeltaSeconds);
		}
	}

	for (auto It = Ghosts.CreateIterator(); It; ++It)
	{
		if (!VisiblePlayers.Contains(It.Key()))
		{
			if (AActor* Ghost = It.Value().Actor.Get())
			{
				Ghost->Destroy();
			}
			It.RemoveCurrent();
		}
	}

	if (Session.bIsOpen && bOwnReady && !bOwnPlaced)
	{
		BindInput(Input);
	}
	else
	{
		ClearInput();
	}

	const TCHAR* Status = TEXT("끝점에 가까이 · Q/E 회전");
	if (SessionId <= 0) Status = TEXT("배치 정보를 준비 중입니다.");
	else if (!Session.bIsOpen) Status = TEXT("트랙 배치가 끝났습니다.");
	else if (!bOwnReady) Status = TEXT("참가 정보를 준비 중입니다.");
	else if (bOwnPlaced) Status = TEXT("설치 완료 · 다른 참가자 대기 중");
	else if (LastResult == EMWTrackPlacementResult::CourseChanged) Status = TEXT("코스가 바뀌었습니다. 끝점을 다시 확인하세요.");
	else if (LastResult == EMWTrackPlacementResult::Unavailable) Status = TEXT("설치할 수 없습니다. 위치를 다시 확인하세요.");
	// 승인 대기에는 스냅 안내를 유지해 요청 순간 문구가 잠깐 교체되는 현상을 막는다.
	else if (bWaitingForReply || bCanPlace) Status = TEXT("좌클릭 / Space · 설치");

	if (auto* Widget = Cast<UMWTrackSelectionWidget>(PC->TrackSelectionWidget))
	{
		FText StatusText = FText::FromString(Status);
		if (Session.bIsOpen && bOwnReady && !bOwnPlaced)
		{
			if (!AwaitedReplacementFor.IsNull())
			{
				StatusText = NSLOCTEXT("MWTrackPlacement", "LoadingReplacement", "대체 트랙을 준비 중입니다.");
			}
			else if (bCanRecoverBlocked)
			{
				StatusText = NSLOCTEXT("MWTrackPlacement", "RecoverBlocked", "지면에 걸립니다 · 좌클릭 / Space로 대체 또는 건너뛰기");
			}
			else if (bBelowFloor || LastResult == EMWTrackPlacementResult::BelowPlacementFloor)
			{
				StatusText = NSLOCTEXT("MWTrackPlacement", "TurnAboveFloor", "지면에 걸립니다 · Q/E로 방향 변경");
			}
		}
		Widget->ShowPlacementStatus(StatusText);
		const bool bNeedsGuide = Session.bIsOpen && bOwnReady && !bOwnPlaced && !bCanPlace && !bWaitingForReply;
		const FVector Endpoint = Session.OpenEnds.IsEmpty() ? FVector::ZeroVector : Session.OpenEnds[0].WorldFrame.GetLocation();
		Widget->ShowPlacementEndpoint(bNeedsGuide && !Session.OpenEnds.IsEmpty() ? &Endpoint : nullptr);
	}
}

UMWTrackPlacementViewComponent::FGhost* UMWTrackPlacementViewComponent::GetOrCreateGhost(
	int32 PlayerId, const FMWPlayerTrackPlacement& Player, bool bLocal)
{
	if (FGhost* Existing = Ghosts.Find(PlayerId))
	{
		if (AActor* Actor = Existing->Actor.Get())
		{
			if (TSoftClassPtr<AActor>(Actor->GetClass()) == Player.TrackClass)
			{
				return Existing;
			}
			Actor->Destroy();
		}
		Ghosts.Remove(PlayerId);
	}

	UClass* TrackClass = Player.TrackClass.LoadSynchronous();

	if (!TrackClass)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Actor = GetWorld()->SpawnActor<AActor>(TrackClass, Player.PreviewTransform, Params);

	if (!Actor)
	{
		return nullptr;
	}

	Actor->SetReplicates(false);
	Actor->SetActorTickEnabled(false);
	if (auto* Runtime = GetWorld()->GetSubsystem<UMWTrackRuntimeSubsystem>())
	{
		Runtime->SetTrackPreview(*Actor, true);
	}

	FGhost Ghost;
	Ghost.Actor = Actor;
	Ghost.bLastRemoteReversed = Player.bReversed;

	if (!MWTrackSnap::ReadPorts(*Actor, Ghost.Ports))
	{
		Actor->Destroy();
		return nullptr;
	}

	Ghost.GroundPoints = MWTrackPlacementClearance::ReadLocalPoints(*Actor, MakeArrayView(&Ghost.Ports[0], 2));
	Actor->SetActorEnableCollision(false);
	const FLinearColor Color = bLocal ? LocalColor : RemoteColors[(PlayerId - 1) % UE_ARRAY_COUNT(RemoteColors)];
	MWTrackPreview::ShowWireframes(*Actor);
	MWTrackPreview::SetWireframeColor(*Actor, Color);
	if (USceneComponent* Wireframes = MWTrackPreview::FindWireframeGroup(*Actor))
	{
		if (const USceneComponent* Parent = Wireframes->GetAttachParent();
			Parent && !Wireframes->IsUsingAbsoluteLocation() && !Wireframes->IsUsingAbsoluteRotation())
		{
			// 표시 그룹의 부모 좌표로 연결점 두 개를 옮겨 트랙 중앙을 지나는 축을 잡는다.
			const FTransform& ParentWorld = Parent->GetComponentTransform();
			const FTransform& ActorWorld = Actor->GetActorTransform();
			const FVector First = ParentWorld.InverseTransformPosition(ActorWorld.TransformPosition(Ghost.Ports[0].GetLocation()));
			const FVector Last = ParentWorld.InverseTransformPosition(ActorWorld.TransformPosition(Ghost.Ports[1].GetLocation()));
			const FVector Axis = (Last - First).GetSafeNormal();
			if (!Axis.IsNearlyZero())
			{
				Ghost.HeldSway.Emplace(FHeldSway{Wireframes, Wireframes->GetRelativeTransform(), (First + Last) * 0.5, Axis});
			}
		}
	}
	return &Ghosts.Add(PlayerId, MoveTemp(Ghost));
}

bool UMWTrackPlacementViewComponent::UpdateLocalPreview(AMWGameplayPlayerController& PC,
	const FMWTrackPlacementSession& Session, FGhost& Ghost, float DeltaSeconds, TOptional<FPointerSample>& OutPointer)
{
	if (bWaitingForReply || !AwaitedReplacementFor.IsNull())
	{
		return false;
	}

	bBelowFloor = false;
	bCanRecoverBlocked = false;
	FTransform Target;
	bool bSnapped;
	FPointerSample Pointer;
	if (!ResolveGhostFromPointer(PC, Session, Ghost, Target, bSnapped, &Pointer))
	{
		if (Ghost.SnapBlend.IsSet())
		{
			// 포인터를 읽을 수 없을 때는 마지막 표시 목표까지 보간만 마치고 입력을 전송하지 않는다.
			const FTransform LastTarget = Ghost.SnapBlend->Target;
			const bool bLastTargetSnapped = Ghost.PreviousSnapTarget.IsSet();
			Ghost.Actor->SetActorTransform(BlendGhostPose(Ghost, LastTarget, bLastTargetSnapped, DeltaSeconds, 1.0));
			SetGhostSnapColor(Ghost, bLastTargetSnapped && !Ghost.SnapBlend.IsSet());
#if WITH_EDITOR
			if (!Ghost.SnapBlend.IsSet() && FParse::Param(FCommandLine::Get(), TEXT("MWVerifyTrackPlacement")))
			{
				UE_LOG(LogTemp, Display, TEXT("MWTrackSnapBlend world=%s ghost=%s completed_without_pointer"),
					*GetWorld()->GetName(), *Ghost.Actor->GetName());
			}
#endif
		}
		return false;
	}
	OutPointer = Pointer;
	const bool bTurned = Ghost.bLastLocalReversed != Intent.bReversed;
	Ghost.bLastLocalReversed = Intent.bReversed;
	if (bSnapped)
	{
		bBelowFloor = !MWTrackPlacementClearance::FitsWithFinish(Ghost.GroundPoints, Session.Ground,
			Target, Ghost.Ports[1 - Intent.SourcePort]);
		bCanRecoverBlocked = bBelowFloor && !MWTrackPlacementClearance::FitsEitherPort(Ghost.GroundPoints,
			Session.Ground, Ghost.Ports, Session.OpenEnds[0].WorldFrame);
	}
	// 거절된 자세에는 도착 색상과 접점 섬광도 같이 막는다.
	const bool bValidSnap = bSnapped && !bBelowFloor;
	Ghost.Actor->SetActorTransform(BlendGhostPose(Ghost, Target, bValidSnap, DeltaSeconds, 1.0, bTurned));
	const bool bVisiblySnapped = bValidSnap && !Ghost.SnapBlend.IsSet();
	SetGhostSnapColor(Ghost, bVisiblySnapped);

	const double Now = GetWorld()->GetTimeSeconds();
	// 로컬 고스트는 매 프레임 갱신하되 원격 표시용 위치 전송은 초당 20회로 제한한다.
	if (Now >= NextPreviewTime)
	{
		PC.ServerUpdateTrackPlacementPreview(SessionId, ++PreviewSequence, Intent);
		NextPreviewTime = Now + 0.05;
	}
	return bVisiblySnapped;
}

bool UMWTrackPlacementViewComponent::ResolveGhostFromPointer(const AMWGameplayPlayerController& PC,
	const FMWTrackPlacementSession& Session, const FGhost& Ghost, FTransform& OutTarget, bool& bOutSnapped,
	FPointerSample* OutPointer)
{
	FVector2D Screen;
	FVector Origin, Ray;
	if (!MWCursorProjection::SampleViewportPointer(PC, Screen) || !PC.DeprojectScreenPositionToWorld(Screen.X, Screen.Y, Origin, Ray)
		|| !MWTrackSnap::UpdateIntentFromRay(Ghost.Ports, Session, Origin, Ray, Intent))
	{
		return false;
	}
	if (OutPointer)
	{
		int32 Width, Height;
		PC.GetViewportSize(Width, Height);
		*OutPointer = {static_cast<float>(Screen.X), Width};
	}
	bOutSnapped = MWTrackSnap::Resolve(Ghost.Ports, Session, Intent, OutTarget);
	return true;
}

FTransform UMWTrackPlacementViewComponent::BlendGhostPose(FGhost& Ghost, const FTransform& Target, bool bSnapped,
	float DeltaSeconds, double FreeMoveAlpha, bool bTurned) const
{
	const AActor& Actor = *Ghost.Actor.Get();
	const FTransform& Current = Actor.GetActorTransform();
	const bool bWasSnapped = Ghost.PreviousSnapTarget.IsSet();
	const bool bChangedTarget = bWasSnapped && bSnapped && !Ghost.PreviousSnapTarget->Equals(Target);
	const bool bStartTurn = bTurned && !bWasSnapped && !bSnapped;
	const bool bStartBlend = bWasSnapped != bSnapped || bChangedTarget || bStartTurn;
	if (bStartBlend)
	{
		// 스냅 전 Q/E와 연결 자세 전환은 화면에 그린 마지막 자세부터 이어 간다.
		Ghost.SnapBlend.Emplace(FGhostSnapBlend{Current, Target, 0.0, bStartTurn});
	}
	if (bSnapped)
	{
		Ghost.PreviousSnapTarget = Target;
	}
	else
	{
		Ghost.PreviousSnapTarget.Reset();
	}

	const FTransform* Start = &Current;
	double Alpha = bSnapped ? 1.0 : FreeMoveAlpha;
	// 원격의 자유 자세도 포트 중앙을 따라 보간해 Q/E에서 트랙이 크게 휘돌지 않게 한다.
	bool bRotateAroundCenter = !bSnapped;
	if (Ghost.SnapBlend.IsSet())
	{
		FGhostSnapBlend& Blend = Ghost.SnapBlend.GetValue();
		Blend.Target = Target;
		Blend.ElapsedSeconds += DeltaSeconds;
		const double Duration = bSnapped ? GhostSnapSettleSeconds : GhostSnapBlendSeconds;
		const double Progress = FMath::Clamp(Blend.ElapsedSeconds / Duration, 0.0, 1.0);
		Alpha = FMath::InterpEaseOut(0.0, 1.0, Progress, 2.0);
		if (bSnapped)
		{
			// 끝점에 빠르게 도달한 뒤 아주 조금 넘어가고 원래 스냅 자세에 정착한다.
			const double TimeLeft = Progress - 1.0;
			const double BackOut = 1.0 + TimeLeft * TimeLeft * (2.25 * TimeLeft + 1.25);
			const double Gap = FVector::Distance(Blend.Start.GetLocation(), Target.GetLocation());
			Alpha = FMath::Min(BackOut, 1.0 + MaxSnapOvershoot / FMath::Max(Gap, MaxSnapOvershoot));
		}
#if WITH_EDITOR
		if ((bStartBlend || Progress >= 1.0) && FParse::Param(FCommandLine::Get(), TEXT("MWVerifyTrackPlacement")))
		{
			UE_LOG(LogTemp, Display, TEXT("MWTrackSnapBlend world=%s ghost=%s %s mode=%s elapsed=%.4f alpha=%.4f gap_cm=%.3f"),
				*GetWorld()->GetName(), *Actor.GetName(), bStartBlend ? TEXT("begin") : TEXT("end"),
				Blend.bRotateAroundCenter ? TEXT("turn") : bChangedTarget ? TEXT("retarget")
					: bSnapped ? TEXT("snap") : TEXT("release"), Blend.ElapsedSeconds,
				Alpha, FVector::Distance(Blend.Start.GetLocation(), Target.GetLocation()));
		}
#endif
		if (Progress >= 1.0)
		{
			Ghost.SnapBlend.Reset();
			if (bSnapped)
			{
				PulseSnapContact(Ghost, Target);
			}
			return Target;
		}
		Start = &Blend.Start;
		bRotateAroundCenter = Blend.bRotateAroundCenter;
	}
	else if (Alpha >= 1.0)
	{
		return Target;
	}
	const FQuat Rotation = FQuat::Slerp(Start->GetRotation(), Target.GetRotation(), Alpha);
	FVector Location = FMath::Lerp(Start->GetLocation(), Target.GetLocation(), Alpha);
	if (bRotateAroundCenter)
	{
		// BP 피벗이 도로 밖에 있어도 양쪽 포트의 중앙이 커서에서 벗어나지 않게 회전한다.
		const FVector PortCenter = (Ghost.Ports[0].GetLocation() + Ghost.Ports[1].GetLocation()) * 0.5;
		const FVector Center = FMath::Lerp(Start->TransformPosition(PortCenter), Target.TransformPosition(PortCenter), Alpha);
		Location = Center - Rotation.RotateVector(Target.GetScale3D() * PortCenter);
	}
	return FTransform(Rotation, Location, Target.GetScale3D());
}

void UMWTrackPlacementViewComponent::PulseSnapContact(const FGhost& Ghost, const FTransform& Target) const
{
	auto* PC = CastChecked<AMWGameplayPlayerController>(GetOwner());
	const auto* GS = GetWorld()->GetGameState<AMWGameplayGameState>();
	auto* Widget = Cast<UMWTrackSelectionWidget>(PC->TrackSelectionWidget);
	if (!GS || !Widget)
	{
		return;
	}

	// 원격 보간이 늦게 끝났을 때 이미 바뀐 끝점에서 섬광을 재생하지 않는다.
	const auto& Ends = GS->GetConstructionComponent()->GetPlacementSession().OpenEnds;
	if (Ends.IsEmpty())
	{
		return;
	}
	const FVector Contact = Ends[0].WorldFrame.GetLocation();
	const auto ReachesContact = [&Target, &Contact](const FTransform& Port)
	{
		return Target.TransformPosition(Port.GetLocation()).Equals(Contact, SnapContactTolerance);
	};
	if (ReachesContact(Ghost.Ports[0]) || ReachesContact(Ghost.Ports[1]))
	{
		Widget->PulsePlacementEndpoint(Contact);
#if WITH_EDITOR
		if (FParse::Param(FCommandLine::Get(), TEXT("MWVerifyTrackPlacement")))
		{
			UE_LOG(LogTemp, Display, TEXT("MWPlacementContact world=%s ghost=%s point=%s"),
				*GetWorld()->GetOutermost()->GetName(), *Ghost.Actor->GetName(), *Contact.ToCompactString());
		}
#endif
	}
}

void UMWTrackPlacementViewComponent::UpdateHeldSway(FGhost& Ghost, double ScreenDelta, float DeltaSeconds) const
{
	if (!Ghost.HeldSway.IsSet())
	{
		return;
	}
	FHeldSway& Sway = Ghost.HeldSway.GetValue();
	USceneComponent* Wireframes = Sway.Wireframes.Get();
	if (!Wireframes)
	{
		return;
	}

	// 화면 폭당 이동 속도를 써서 창 크기와 프레임 시간에 따라 기울기가 달라지는 것을 줄인다.
	const double Speed = DeltaSeconds > UE_SMALL_NUMBER ? ScreenDelta / DeltaSeconds : 0.0;
	const double DesiredAngle = Speed * HeldSwayDegreesAtOneWidthPerSecond;
	const float TargetAngle = FMath::Abs(DesiredAngle) < HeldSwayMinAngleDegrees ? 0.0f
		: static_cast<float>(FMath::Clamp(DesiredAngle, -HeldSwayMaxAngleDegrees, HeldSwayMaxAngleDegrees));
	const float LastAngle = Sway.AngleDegrees;
	Sway.AngleDegrees = FMath::FInterpConstantTo(LastAngle, TargetAngle, DeltaSeconds,
		HeldSwayMaxAngleDegrees / static_cast<float>(GhostSnapBlendSeconds));
	if (Sway.AngleDegrees == LastAngle)
	{
		return;
	}

	const FQuat Tilt(Sway.Axis, FMath::DegreesToRadians(Sway.AngleDegrees));
	// 루트와 소켓은 그대로 두고 와이어프레임의 위치와 회전만 원래 자세에서 계산한다.
	const FVector Location = Sway.Center + Tilt.RotateVector(Sway.RestPose.GetLocation() - Sway.Center);
	Wireframes->SetRelativeLocationAndRotation(Location, Tilt * Sway.RestPose.GetRotation());
}

void UMWTrackPlacementViewComponent::SetGhostSnapColor(FGhost& Ghost, bool bSnapped) const
{
	if (Ghost.bSnapped == bSnapped)
	{
		return;
	}
	Ghost.bSnapped = bSnapped;
	if (AActor* Actor = Ghost.Actor.Get())
	{
		MWTrackPreview::SetWireframeColor(*Actor, bSnapped ? SnapColor : LocalColor);
	}
}

void UMWTrackPlacementViewComponent::BindInput(UEnhancedInputComponent* Input)
{
	if (!Input || BoundInput.Get() == Input)
	{
		return;
	}
	if (!bTriedLoadingInput)
	{
		bTriedLoadingInput = true;
		InputData = UMWAssetManager::Get().LoadAssetSynchronous<UMWInputData>(MWGameplayTags::Asset_Input_PlaceConstruction);
	}
	const auto* Accept = InputData ? InputData->FindInputActionByTag(MWGameplayTags::Input_Accept) : nullptr;
	const auto* Turn = InputData ? InputData->FindInputActionByTag(MWGameplayTags::Input_Turn) : nullptr;
	if (!Accept || !Turn || !InputData->GetMappingContext())
	{
		return;
	}
	auto* PC = CastChecked<AMWGameplayPlayerController>(GetOwner());
	auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
	if (!Subsystem)
	{
		return;
	}
	ClearInput();
	BoundInput = Input;
	InputBindings.Add(Input->BindAction(Accept, ETriggerEvent::Started, this, &ThisClass::HandleAccept).GetHandle());
	InputBindings.Add(Input->BindAction(Turn, ETriggerEvent::Started, this, &ThisClass::HandleTurn).GetHandle());
	FModifyContextOptions Options;
	Options.bIgnoreAllPressedKeysUntilRelease = true;
	Subsystem->AddMappingContext(InputData->GetMappingContext(), 1, Options);
}

void UMWTrackPlacementViewComponent::ClearInput()
{
	if (InputBindings.IsEmpty())
	{
		return;
	}
	if (auto* Input = BoundInput.Get())
	{
		for (uint32 Handle : InputBindings)
		{
			Input->RemoveBindingByHandle(Handle);
		}
	}
	auto* PC = CastChecked<AMWGameplayPlayerController>(GetOwner());
	if (auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
	{
		Subsystem->RemoveMappingContext(InputData->GetMappingContext());
	}
	BoundInput.Reset();
	InputBindings.Reset();
}

void UMWTrackPlacementViewComponent::HandleTurn()
{
	if (!bWaitingForReply && !bAccepted)
	{
		Intent.bReversed = !Intent.bReversed;
		LastResult = EMWTrackPlacementResult::Accepted;
	}
}

void UMWTrackPlacementViewComponent::HandleAccept()
{
	if (bWaitingForReply || bAccepted || !AwaitedReplacementFor.IsNull())
	{
		return;
	}

	auto* PC = CastChecked<AMWGameplayPlayerController>(GetOwner());
	const auto* GS = GetWorld()->GetGameState<AMWGameplayGameState>();
	const auto* PS = PC->GetPlayerState<AMWPlayerState>();
	if (!PC->IsLocalController() || !GS || !PS || GS->GetRoomPhase() != EMWRoomPhase::TrackPlacement)
	{
		return;
	}

	const auto& Session = GS->GetConstructionComponent()->GetPlacementSession();
	const auto& Player = PS->GetPlayerConstructionComponent()->GetPlacement();
	if (!Session.bIsOpen || Session.SessionId != SessionId || Player.SessionId != SessionId || Player.bPlaced)
	{
		return;
	}

	// 확정 입력은 미리보기 틱보다 먼저 들어올 수 있으므로 지금 커서 위치로 다시 판정한다.
	FGhost* Ghost = Ghosts.Find(PS->GetLobbyMemberId());
	FTransform ResolvedTarget;
	bool bSnapped;
	if (!Ghost || !Ghost->Actor.IsValid() || !ResolveGhostFromPointer(*PC, Session, *Ghost, ResolvedTarget, bSnapped) || !bSnapped)
	{
		return;
	}

	FMWTrackPlacementIntent Request = Intent;
	const bool bFits = MWTrackPlacementClearance::FitsWithFinish(Ghost->GroundPoints, Session.Ground,
		ResolvedTarget, Ghost->Ports[1 - Intent.SourcePort]);
	if (!bFits)
	{
		Request.bRecoverBlocked = !MWTrackPlacementClearance::FitsEitherPort(Ghost->GroundPoints,
			Session.Ground, Ghost->Ports, Session.OpenEnds[0].WorldFrame);
		// 서버도 같은 포트와 지면을 확인한다. 거절될 클릭에는 확정 색상을 표시하지 않는다.
		bWaitingForReply = true;
		PC->ServerPlaceTrack(SessionId, Session.CourseRevision, Request);
		return;
	}

#if WITH_EDITOR
	const FTransform Visual = Ghost->Actor->GetActorTransform();
#endif
	// 확정 입력이 보간보다 먼저 들어오면 클릭으로 판정한 위치에서 서버 응답을 기다린다.
	Ghost->SnapBlend.Reset();
	Ghost->PreviousSnapTarget = ResolvedTarget;
	Ghost->Actor->SetActorTransform(ResolvedTarget);
	SetGhostSnapColor(*Ghost, true);

#if WITH_EDITOR
	if (FParse::Param(FCommandLine::Get(), TEXT("MWVerifyTrackPlacement")))
	{
		const FVector P = ResolvedTarget.GetLocation(), Scale = ResolvedTarget.GetScale3D();
		const FQuat Q = ResolvedTarget.GetRotation();
		float X = 0.f, Y = 0.f;
		PC->GetMousePosition(X, Y);
		UE_LOG(LogTemp, Display, TEXT("MWPlacement accept world=%s member=%d session=%d revision=%d target=%d:%d source=%d pointer=(%.1f,%.1f) resolved=(%.9f,%.9f,%.9f) quat=(%.9f,%.9f,%.9f,%.9f) scale=(%.9f,%.9f,%.9f) visual_gap_cm=%.3f visual_gap_deg=%.3f locked_gap_cm=%.6f locked_gap_deg=%.6f"),
			*GetWorld()->GetName(), PS->GetLobbyMemberId(), SessionId, Session.CourseRevision,
			Intent.Target.PieceId, Intent.Target.PortIndex, Intent.SourcePort, X, Y,
			P.X, P.Y, P.Z, Q.X, Q.Y, Q.Z, Q.W, Scale.X, Scale.Y, Scale.Z,
			FVector::Distance(P, Visual.GetLocation()), FMath::RadiansToDegrees(Q.AngularDistance(Visual.GetRotation())),
			FVector::Distance(P, Ghost->Actor->GetActorLocation()),
			FMath::RadiansToDegrees(Q.AngularDistance(Ghost->Actor->GetActorQuat())));
	}
#endif
	bWaitingForReply = true;
	PC->ServerPlaceTrack(SessionId, Session.CourseRevision, Intent);
}

void UMWTrackPlacementViewComponent::HandlePlacementReply(int32 ReplySessionId, EMWTrackPlacementResult Result)
{
	if (SessionId != ReplySessionId || !bWaitingForReply)
	{
		return;
	}
	bWaitingForReply = false;
	bAccepted = Result == EMWTrackPlacementResult::Accepted;
	if (Result == EMWTrackPlacementResult::ReplacementOffered)
	{
		// 회신이 PlayerState 복제보다 앞서면 옛 고스트에 새 클릭이 들어가지 않게 한다.
		const auto* PC = CastChecked<AMWGameplayPlayerController>(GetOwner());
		const auto* PS = PC->GetPlayerState<AMWPlayerState>();
		const FGhost* Ghost = PS ? Ghosts.Find(PS->GetLobbyMemberId()) : nullptr;
		if (Ghost && Ghost->Actor.IsValid())
		{
			AwaitedReplacementFor = TSoftClassPtr<AActor>(Ghost->Actor->GetClass());
		}
	}
	LastResult = Result;
}

void UMWTrackPlacementViewComponent::ResetView()
{
	ClearInput();
	for (const auto& Entry : Ghosts)
	{
		if (AActor* Actor = Entry.Value.Actor.Get())
		{
			Actor->Destroy();
		}
	}
	Ghosts.Reset();
	Intent = {};
	SessionId = 0;
	PreviewSequence = 0;
	NextPreviewTime = 0.0;
	bWaitingForReply = false;
	bAccepted = false;
	bBelowFloor = false;
	bCanRecoverBlocked = false;
	AwaitedReplacementFor.Reset();
	LastResult = EMWTrackPlacementResult::Accepted;
}
