#include "Gameplay/Construction/MWTrackPlacementViewComponent.h"

#include "Data/MWInputData.h"
#include "Data/MWTrackDefinitionData.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Gameplay/Construction/MWPlayerConstructionComponent.h"
#include "Gameplay/Construction/MWTrackConstructionComponent.h"
#include "Gameplay/Construction/MWTrackPreview.h"
#include "InputMappingContext.h"
#include "MWGameplayTags.h"
#include "Player/MWGameplayPlayerController.h"
#include "System/GameState/MWGameplayGameState.h"
#include "System/MWAssetManager.h"
#include "System/MWPlayerState.h"
#include "UI/MWTrackSelectionWidget.h"
#include "UnrealClient.h"

namespace
{
	const FLinearColor LocalColor(0.05f, 0.55f, 1.0f);
	const FLinearColor SnapColor(0.1f, 1.0f, 0.35f);
	const FLinearColor RemoteColors[] = {
		FLinearColor(1.0f, 0.42f, 0.08f),
		FLinearColor(0.82f, 0.18f, 0.75f),
		FLinearColor(0.55f, 0.26f, 1.0f)
	};
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
	FString PlayerLines;
	bool bOwnPlaced = false;
	bool bOwnReady = false;
	bCanPlace = false;
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
		PlayerLines += FString::Printf(TEXT("P%d %s%s · %s\n"), PlayerId, *PS->GetPlayerName(),
			bLocal ? TEXT(" (나)") : TEXT(""), bPlaced ? TEXT("설치 완료") : TEXT("배치 중"));
		if (bLocal)
		{
			bOwnReady = true;
			bOwnPlaced = bPlaced;
		}
		if (bPlaced || !Session.bIsOpen)
		{
			continue;
		}

		if (FGhost* Ghost = GetOrCreateGhost(PlayerId, Player, bLocal))
		{
			VisiblePlayers.Add(PlayerId);
			if (bLocal)
			{
				UpdateLocalGhost(*PC, Session, *Ghost);
			}
			else if (AActor* Actor = Ghost->Actor.Get())
			{
				const FTransform& Target = Player.PreviewTransform;
				const double Alpha = Player.bSnapped ? 1.0 : FMath::Clamp(DeltaSeconds * 16.0, 0.0, 1.0);
				const FVector Location = FMath::Lerp(Actor->GetActorLocation(), Target.GetLocation(), Alpha);
				const FQuat Rotation = FQuat::Slerp(Actor->GetActorQuat(), Target.GetRotation(), Alpha);
				Actor->SetActorLocationAndRotation(Location, Rotation);
			}
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

	const TCHAR* Status = TEXT("끝점에 가까이 이동하세요. Q/E로 방향을 바꿉니다.");
	if (SessionId <= 0) Status = TEXT("배치 정보를 준비 중입니다.");
	else if (!Session.bIsOpen) Status = TEXT("트랙 배치가 끝났습니다.");
	else if (!bOwnReady) Status = TEXT("참가 정보를 준비 중입니다.");
	else if (bOwnPlaced) Status = TEXT("설치했습니다. 다른 플레이어를 기다립니다.");
	else if (bWaitingForReply) Status = TEXT("서버에서 설치 위치를 확인하고 있습니다.");
	else if (LastResult == EMWTrackPlacementResult::CourseChanged) Status = TEXT("코스가 바뀌었습니다. 끝점을 다시 확인하세요.");
	else if (LastResult == EMWTrackPlacementResult::Unavailable) Status = TEXT("설치할 수 없습니다. 위치를 다시 확인하세요.");
	else if (bCanPlace) Status = TEXT("스냅되었습니다. 좌클릭 또는 Space로 설치하세요.");

	if (auto* Widget = Cast<UMWTrackSelectionWidget>(PC->TrackSelectionWidget))
	{
		Widget->ShowPlacementStatus(FText::FromString(PlayerLines), FText::FromString(Status));
	}
}

UMWTrackPlacementViewComponent::FGhost* UMWTrackPlacementViewComponent::GetOrCreateGhost(
	int32 PlayerId, const FMWPlayerTrackPlacement& Player, bool bLocal)
{
	if (FGhost* Existing = Ghosts.Find(PlayerId); Existing && Existing->Actor.IsValid())
	{
		return Existing;
	}
	const auto* Definition = UMWAssetManager::Get().LoadAssetSynchronous<UMWTrackDefinitionData>(Player.DefinitionAssetId);
	if (!Definition || !Definition->GetTrackActorClass())
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Actor = GetWorld()->SpawnActor<AActor>(Definition->GetTrackActorClass(), Player.PreviewTransform, Params);
	if (!Actor)
	{
		return nullptr;
	}
	Actor->SetReplicates(false);
	Actor->SetActorEnableCollision(false);
	Actor->SetActorTickEnabled(false);
	FGhost Ghost;
	Ghost.Actor = Actor;
	if (!MWTrackSnap::ReadPorts(*Actor, Ghost.Ports))
	{
		Actor->Destroy();
		return nullptr;
	}
	const FLinearColor Color = bLocal ? LocalColor : RemoteColors[(PlayerId - 1) % UE_ARRAY_COUNT(RemoteColors)];
	MWTrackPreview::ShowWireframes(*Actor);
	MWTrackPreview::SetWireframeColor(*Actor, Color);
	return &Ghosts.Add(PlayerId, MoveTemp(Ghost));
}

bool UMWTrackPlacementViewComponent::UpdatePointer(const AMWGameplayPlayerController& PC,
	const FMWTrackPlacementSession& Session, const FMWTrackPortFrames& Ports)
{
	const auto* LocalPlayer = PC.GetLocalPlayer();
	const FViewport* Viewport = LocalPlayer && LocalPlayer->ViewportClient ? LocalPlayer->ViewportClient->Viewport : nullptr;
	float X, Y;
	if (!Viewport || !Viewport->IsForegroundWindow() || !PC.GetMousePosition(X, Y)
		|| X < 0 || Y < 0 || X >= Viewport->GetSizeXY().X || Y >= Viewport->GetSizeXY().Y)
	{
		return false;
	}
	FVector Origin, Ray;
	return PC.DeprojectMousePositionToWorld(Origin, Ray)
		&& MWTrackSnap::UpdateIntentFromRay(Ports, Session, Origin, Ray, Intent);
}

void UMWTrackPlacementViewComponent::UpdateLocalGhost(AMWGameplayPlayerController& PC,
	const FMWTrackPlacementSession& Session, FGhost& Ghost)
{
	if (bWaitingForReply || !UpdatePointer(PC, Session, Ghost.Ports))
	{
		return;
	}
	FTransform Display;
	bCanPlace = MWTrackSnap::Resolve(Ghost.Ports, Session, Intent, Display);
	DisplayedRevision = Session.CourseRevision;
	Ghost.Actor->SetActorTransform(Display);
	SetGhostSnapColor(Ghost, bCanPlace);

	const double Now = GetWorld()->GetTimeSeconds();
	if (Now >= NextPreviewTime)
	{
		PC.ServerUpdateTrackPlacementPreview(SessionId, ++PreviewSequence, Intent);
		NextPreviewTime = Now + 0.05;
	}
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
		bCanPlace = false;
		LastResult = EMWTrackPlacementResult::Accepted;
	}
}

void UMWTrackPlacementViewComponent::HandleAccept()
{
	if (bCanPlace && !bWaitingForReply && !bAccepted)
	{
		bWaitingForReply = true;
		CastChecked<AMWGameplayPlayerController>(GetOwner())->ServerPlaceTrack(SessionId, DisplayedRevision, Intent);
	}
}

void UMWTrackPlacementViewComponent::HandlePlacementReply(int32 ReplySessionId, EMWTrackPlacementResult Result)
{
	if (SessionId != ReplySessionId || !bWaitingForReply)
	{
		return;
	}
	bWaitingForReply = false;
	bAccepted = Result == EMWTrackPlacementResult::Accepted;
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
	DisplayedRevision = 0;
	PreviewSequence = 0;
	NextPreviewTime = 0.0;
	bCanPlace = false;
	bWaitingForReply = false;
	bAccepted = false;
	LastResult = EMWTrackPlacementResult::Accepted;
}
