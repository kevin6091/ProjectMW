#include "Gameplay/Construction/MWTrackSelectionBoard.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Data/MWTrackDefinitionData.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Gameplay/Construction/MWPlayerConstructionComponent.h"
#include "Gameplay/Construction/MWTrackConstructionComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/MWGameplayPlayerController.h"
#include "System/GameState/MWGameplayGameState.h"
#include "System/MWAssetManager.h"
#include "System/MWPlayerState.h"
#include "UnrealClient.h"

AMWTrackSelectionBoard::AMWTrackSelectionBoard()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	for (int32 Index = 0; Index < 3; ++Index)
	{
		auto* Anchor = CreateDefaultSubobject<USceneComponent>(*FString::Printf(TEXT("Anchor%d"), Index));
		Anchor->SetupAttachment(RootComponent);
		Anchor->SetRelativeLocation(FVector(0, (Index - 1) * 1100.f, 80));
		Anchors.Add(Anchor);
		auto* Hit = CreateDefaultSubobject<UBoxComponent>(*FString::Printf(TEXT("OfferHit%d"), Index));
		Hit->SetupAttachment(Anchor);
		Hit->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Hit->SetCollisionResponseToAllChannels(ECR_Ignore);
		Hit->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Hit->SetGenerateOverlapEvents(false);
		HitAreas.Add(Hit);
	}
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("SelectionCamera"));
	Camera->SetupAttachment(RootComponent);
	const FVector Position(1800, -1900, 2900);
	Camera->SetRelativeLocation(Position);
	Camera->SetRelativeRotation((FVector(0, 0, 100) - Position).Rotation());
	Camera->FieldOfView = 65;
}

bool AMWTrackSelectionBoard::BuildOffers(const FMWTrackSelectionSession& Session)
{
	ClearOffers();
	for (const FMWTrackOffer& Offer : Session.Offers)
	{
		const auto* Definition = UMWAssetManager::Get().LoadAssetSynchronous<UMWTrackDefinitionData>(Offer.DefinitionAssetId);
		if (!Definition || !Definition->GetTrackActorClass() || !Anchors.IsValidIndex(Offer.BoardSlotIndex))
		{
			ClearOffers();
			return false;
		}
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Track = GetWorld()->SpawnActor<AActor>(Definition->GetTrackActorClass(), Anchors[Offer.BoardSlotIndex]->GetComponentTransform(), Params);
		if (!Track) { ClearOffers(); return false; }
		Offers.Add(Offer.OfferId, Track);
		Track->SetReplicates(false);
		Track->SetActorTickEnabled(false);
		// 원본 BP를 전시 인스턴스로 사용하고, 판정은 전시 영역의 Box에만 맡긴다.
		TInlineComponentArray<UStaticMeshComponent*> Meshes(Track);
		for (auto* Mesh : Meshes)
		{
			Mesh->SetSimulatePhysics(false);
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			if (DisplayMaterial) Mesh->SetMaterial(0, DisplayMaterial);
			Mesh->CreateDynamicMaterialInstance(0);
		}
		FVector Center, Extent;
		Track->GetActorBounds(false, Center, Extent);
		Track->AddActorWorldOffset(Anchors[Offer.BoardSlotIndex]->GetComponentLocation() - Center);
		HitAreas[Offer.BoardSlotIndex]->SetBoxExtent(Extent + FVector(20));
		HitAreas[Offer.BoardSlotIndex]->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		auto* Label = NewObject<UTextRenderComponent>(this);
		Label->SetupAttachment(Anchors[Offer.BoardSlotIndex]);
		Label->SetRelativeLocation(FVector(0, 0, Extent.Z + 130));
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetWorldSize(45);
		Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Label->RegisterComponent();
		Labels.Add(Offer.OfferId, Label);
	}
	SessionId = Session.SelectionSessionId;
	Revision = Session.OffersRevision;
	return true;
}

void AMWTrackSelectionBoard::ClearOffers()
{
	for (const auto& Pair : Offers) if (IsValid(Pair.Value)) Pair.Value->Destroy();
	for (const auto& Pair : Labels) if (IsValid(Pair.Value)) Pair.Value->DestroyComponent();
	Offers.Reset();
	Labels.Reset();
	for (const auto& Hit : HitAreas) Hit->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SessionId = Revision = 0;
	LocalHover = SentHover = INDEX_NONE;
}

bool AMWTrackSelectionBoard::GetCursorPosition(FVector& OutPosition) const
{
	if (!Viewer.IsValid()) return false;
	const auto* LP = Viewer->GetLocalPlayer();
	const FViewport* Viewport = LP && LP->ViewportClient ? LP->ViewportClient->Viewport : nullptr;
	if (!Viewport || !Viewport->IsForegroundWindow()) return false;
	float X, Y;
	int32 Width, Height;
	Viewer->GetViewportSize(Width, Height);
	if (!Viewer->GetMousePosition(X, Y) || X < 0 || Y < 0 || X >= Width || Y >= Height) return false;
	FVector Origin, Direction;
	if (!Viewer->DeprojectMousePositionToWorld(Origin, Direction)) return false;
	const FVector Normal = GetActorUpVector();
	const double Dot = FVector::DotProduct(Direction, Normal);
	if (FMath::Abs(Dot) < KINDA_SMALL_NUMBER) return false;
	const double Distance = FVector::DotProduct(Anchors[1]->GetComponentLocation() - Origin, Normal) / Dot;
	if (Distance < 0 || Distance > 100000) return false;
	OutPosition = Origin + Direction * Distance;
	return !OutPosition.ContainsNaN() && OutPosition.GetAbsMax() < 1000000;
}

int32 AMWTrackSelectionBoard::FindHoveredOffer() const
{
	FVector Cursor;
	FHitResult Hit;
	if (!GetCursorPosition(Cursor) || !Viewer->GetHitResultUnderCursor(ECC_Visibility, false, Hit)) return INDEX_NONE;
	const int32 Slot = HitAreas.IndexOfByPredicate([&Hit](const UBoxComponent* Box) { return Hit.GetComponent() == Box; });
	const auto* GS = GetWorld()->GetGameState<AMWGameplayGameState>();
	if (!GS) return INDEX_NONE;
	const FMWTrackSelectionSession Session = GS->GetConstructionComponent()->GetSelectionSession();
	const auto* Offer = Session.Offers.FindByPredicate([Slot](const FMWTrackOffer& Item) { return Item.BoardSlotIndex == Slot; });
	return Offer ? Offer->OfferId : INDEX_NONE;
}

void AMWTrackSelectionBoard::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Viewer.IsValid())
	{
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			auto* PC = Cast<AMWGameplayPlayerController>(It->Get());
			if (PC && PC->IsLocalController())
			{
				Viewer = PC;
				PC->SetTrackSelectionBoard(this);
				PC->SetViewTarget(this);
				break;
			}
		}
	}
	if (!Viewer.IsValid()) return;
	const auto* GS = GetWorld()->GetGameState<AMWGameplayGameState>();
	if (!GS) return;
	const FMWTrackSelectionSession Session = GS->GetConstructionComponent()->GetSelectionSession();
	if (Session.SelectionSessionId <= 0) return;
	if ((SessionId != Session.SelectionSessionId || Revision != Session.OffersRevision) && !BuildOffers(Session)) return;
	if (Session.bIsOpen)
	{
		const auto* PS = Viewer->GetPlayerState<AMWPlayerState>();
		const bool bConfirmed = PS && PS->GetPlayerConstructionComponent()->GetSelection().bConfirmed;
		LocalHover = bConfirmed ? INDEX_NONE : FindHoveredOffer();
		const double Now = GetWorld()->GetTimeSeconds();
		if (SentHover != LocalHover && Now - LastHoverTime >= .1)
		{
			Viewer->RequestTrackHover(SessionId, Revision, LocalHover);
			SentHover = LocalHover;
			LastHoverTime = Now;
		}
		if (Now - LastCursorTime >= .05)
		{
			FVector Position = FVector::ZeroVector;
			const bool bVisible = GetCursorPosition(Position);
			Viewer->RequestTrackCursor(SessionId, Revision, bVisible, Position);
			LastCursorTime = Now;
		}
	}
	RefreshMarkers(Session);
}

void AMWTrackSelectionBoard::RefreshMarkers(const FMWTrackSelectionSession& Session)
{
	const auto* GS = GetWorld()->GetGameState<AMWGameplayGameState>();
	for (const auto& Pair : Offers)
	{
		FString Text = FString::Printf(TEXT("TRACK %d"), Pair.Key);
		bool bSelected = false;
		for (APlayerState* State : GS->PlayerArray)
		{
			const auto* PS = Cast<AMWPlayerState>(State);
			if (!PS) continue;
			const auto Selection = PS->GetPlayerConstructionComponent()->GetSelection();
			if (Selection.SelectionSessionId != Session.SelectionSessionId || Selection.OffersRevision != Session.OffersRevision) continue;
			const int32 Id = Selection.bConfirmed ? Selection.SelectedOfferId
				: (PS == Viewer->GetPlayerState<AMWPlayerState>() ? LocalHover : Selection.HoveredOfferId);
			if (Id != Pair.Key) continue;
			bSelected |= Selection.bConfirmed;
			Text += FString::Printf(TEXT("\nP%d %s"), PS->GetLobbyMemberId(), Selection.bConfirmed ? TEXT("SELECTED") : TEXT("HOVER"));
		}
		const FLinearColor Tint = bSelected ? FLinearColor(1, .65f, .08f)
			: LocalHover == Pair.Key ? FLinearColor(.05f, .75f, 1) : FLinearColor(.38f, .48f, .65f);
		TInlineComponentArray<UStaticMeshComponent*> Meshes(Pair.Value);
		for (auto* Mesh : Meshes)
			if (auto* Material = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0))) Material->SetVectorParameterValue(TEXT("Color"), Tint);
		if (UTextRenderComponent* Label = Labels.FindRef(Pair.Key))
		{
			Label->SetText(FText::FromString(Text));
			Label->SetTextRenderColor(bSelected ? FColor::Yellow : FColor::White);
			Label->SetWorldRotation((Camera->GetComponentLocation() - Label->GetComponentLocation()).Rotation());
		}
	}
}

void AMWTrackSelectionBoard::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Viewer.IsValid()) Viewer->SetTrackSelectionBoard(nullptr);
	ClearOffers();
	Super::EndPlay(Reason);
}
