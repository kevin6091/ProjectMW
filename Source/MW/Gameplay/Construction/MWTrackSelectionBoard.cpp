#include "Gameplay/Construction/MWTrackSelectionBoard.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "Data/MWTrackDefinitionData.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Gameplay/Construction/MWPlayerConstructionComponent.h"
#include "Gameplay/Construction/MWTrackConstructionComponent.h"
#include "Gameplay/Construction/MWTrackPreview.h"
#include "LogChannel.h"
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
	// 후보가 놓이는 열을 옆에서 바라봐 커브의 윤곽과 경사의 높이 차를 함께 보여 준다.
	const FVector Position(2700, -1200, 2300);
	Camera->SetRelativeLocation(Position);
	Camera->SetRelativeRotation((FVector(0, 0, 100) - Position).Rotation());
	Camera->FieldOfView = 65;
}

void AMWTrackSelectionBoard::BeginPlay()
{
	Super::BeginPlay();
	for (USceneComponent* Anchor : Anchors)
	{
		Anchor->SetRelativeLocation(Anchor->GetRelativeLocation() * LayoutScale);
	}
	Camera->SetRelativeLocation(Camera->GetRelativeLocation() * LayoutScale);
}

void AMWTrackSelectionBoard::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	Super::CalcCamera(DeltaTime, OutResult);

	const auto* GS = GetWorld()->GetGameState<AMWGameplayGameState>();
	if (!bUseTopDownPlacementCamera || !GS || GS->GetRoomPhase() != EMWRoomPhase::TrackPlacement)
	{
		return;
	}

	const auto& Session = GS->GetConstructionComponent()->GetPlacementSession();
	if (Session.SessionId <= 0)
	{
		return;
	}

	FBox Bounds(Session.BuildOrigin, Session.BuildOrigin);
	Bounds += Session.CourseBounds;
	for (const FMWOpenTrackEnd& End : Session.OpenEnds)
	{
		Bounds += End.WorldFrame.GetLocation();
	}
	const FVector Center = Bounds.GetCenter();
	// 끝점 너머에도 다음 조각의 중심을 옮겨 놓을 공간을 남긴다.
	const FVector Extent = Bounds.GetExtent() + FVector(400.0, 400.0, 120.0) * Session.LayoutScale;
	int32 Width = 0;
	int32 Height = 0;
	if (const auto* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->GetViewportSize(Width, Height);
	}
	const double Aspect = Height > 0 && Width > 0 ? static_cast<double>(Width) / Height : 16.0 / 9.0;

	// 선택 카메라와 같은 각도에서 보고, 시작 트랙의 방향에 맞춰 수평으로만 돌린다.
	OutResult.Rotation = Camera->GetRelativeRotation();
	OutResult.Rotation.Yaw = FRotator::NormalizeAxis(OutResult.Rotation.Yaw + Session.PreviewYaw);
	OutResult.ProjectionMode = ECameraProjectionMode::Perspective;
	OutResult.AspectRatioAxisConstraint = AspectRatio_MaintainXFOV;

	const FRotationMatrix ViewAxes(OutResult.Rotation);
	const double HalfFovTangent = FMath::Tan(FMath::DegreesToRadians(OutResult.FOV * 0.5));
	const double HorizontalLimit = HalfFovTangent * 0.9;
	const double VerticalLimit = HalfFovTangent * 0.9 / Aspect;
	double CameraDistance = 0.0;
	// 원근 화면에서는 가까운 모서리가 더 크게 보이므로 각 모서리의 깊이를 함께 계산한다.
	for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
	{
		const FVector Corner(
			(CornerIndex & 1) ? Extent.X : -Extent.X,
			(CornerIndex & 2) ? Extent.Y : -Extent.Y,
			(CornerIndex & 4) ? Extent.Z : -Extent.Z);
		const FVector ViewCorner = ViewAxes.InverseTransformVector(Corner);
		const double RequiredDepth = FMath::Max(
			FMath::Abs(ViewCorner.Y) / HorizontalLimit, FMath::Abs(ViewCorner.Z) / VerticalLimit);
		CameraDistance = FMath::Max(CameraDistance, FMath::Max(RequiredDepth, 100.0) - ViewCorner.X);
	}
	OutResult.Location = Center - ViewAxes.GetUnitAxis(EAxis::X) * CameraDistance;
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
		Track->SetReplicates(false);
		Track->SetActorTickEnabled(false);
		// 판정은 전시 영역의 Box에 맡기고 보이는 와이어프레임의 중심을 전시 칸에 맞춘다.
		const FBox VisualBounds = MWTrackPreview::ShowWireframes(*Track);
		if (!VisualBounds.IsValid)
		{
			UE_LOG(LogDefault, Warning, TEXT("%s에 전시할 Wireframes 메시가 없습니다."), *GetNameSafe(Track));
			Track->Destroy();
			continue;
		}
		Offers.Add(Offer.OfferId, Track);
		const FVector Extent = VisualBounds.GetExtent();
		Track->AddActorWorldOffset(Anchors[Offer.BoardSlotIndex]->GetComponentLocation() - VisualBounds.GetCenter());
		HitAreas[Offer.BoardSlotIndex]->SetBoxExtent(Extent + FVector(20 * LayoutScale));
		HitAreas[Offer.BoardSlotIndex]->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		auto* Label = NewObject<UTextRenderComponent>(this);
		Label->SetupAttachment(Anchors[Offer.BoardSlotIndex]);
		Label->SetRelativeLocation(FVector(0, 0, Extent.Z + 130 * LayoutScale));
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetWorldSize(45 * LayoutScale);
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
	const auto* GS = GetWorld()->GetGameState<AMWGameplayGameState>();
	if (!GS)
	{
		return;
	}
	if (GS->GetRoomPhase() != EMWRoomPhase::PreparingConstruction)
	{
		if (Viewer.IsValid())
		{
			Viewer->SetTrackSelectionBoard(nullptr);
			Viewer.Reset();
		}
		if (!Offers.IsEmpty())
		{
			ClearOffers();
		}
		// 선택판의 카메라는 배치에서도 쓴다. 카메라와 전시 Actor의 수명을 분리한다.
		return;
	}
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

FVector AMWTrackSelectionBoard::GetBuildOrigin() const
{
	return Anchors[1]->GetComponentLocation();
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
		if (UTextRenderComponent* Label = Labels.FindRef(Pair.Key))
		{
			Label->SetText(FText::FromString(Text));
			Label->SetTextRenderColor(bSelected ? FColor::Yellow : LocalHover == Pair.Key ? FColor::Cyan : FColor::White);
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
