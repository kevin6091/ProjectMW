#include "Gameplay/Construction/MWTrackSelectionBoard.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Gameplay/Construction/MWPlayerConstructionComponent.h"
#include "Gameplay/Construction/MWTrackConstructionComponent.h"
#include "Gameplay/Construction/MWTrackPreview.h"
#include "Gameplay/Track/MWTrackRuntimeSubsystem.h"
#include "LogChannel.h"
#include "Player/MWGameplayPlayerController.h"
#include "System/GameState/MWGameplayGameState.h"
#include "System/MWPlayerState.h"
#include "UnrealClient.h"

namespace
{
	constexpr double RowSpacing = 1260.0;
	constexpr double ColumnSpacing = 1120.0;
	constexpr double BoardHeight = 80.0;
	const FVector SlotExtent(410.0, 460.0, 225.0);
	const FVector SelectionViewDirection(-1.0, 0.0, -1.8);

	void FrameBounds(FMinimalViewInfo& View, const FBox& Bounds, double Aspect, double ScreenFraction)
	{
		View.ProjectionMode = ECameraProjectionMode::Perspective;
		View.AspectRatioAxisConstraint = AspectRatio_MaintainXFOV;
		const FRotationMatrix ViewAxes(View.Rotation);
		const double HorizontalLimit = FMath::Tan(FMath::DegreesToRadians(View.FOV * 0.5)) * ScreenFraction;
		const double VerticalLimit = HorizontalLimit / Aspect;
		const FVector Extent = Bounds.GetExtent();
		double CameraDistance = 0.0;
		// 원근 카메라에서는 가까운 쪽의 모서리도 화면 안에 들어와야 한다.
		for (int32 Index = 0; Index < 8; ++Index)
		{
			const FVector Corner(
				(Index & 1) ? Extent.X : -Extent.X,
				(Index & 2) ? Extent.Y : -Extent.Y,
				(Index & 4) ? Extent.Z : -Extent.Z);
			const FVector ViewCorner = ViewAxes.InverseTransformVector(Corner);
			const double RequiredDepth = FMath::Max(
				FMath::Abs(ViewCorner.Y) / HorizontalLimit, FMath::Abs(ViewCorner.Z) / VerticalLimit);
			CameraDistance = FMath::Max(CameraDistance, FMath::Max(RequiredDepth, 100.0) - ViewCorner.X);
		}
		View.Location = Bounds.GetCenter() - ViewAxes.GetUnitAxis(EAxis::X) * CameraDistance;
	}
}

AMWTrackSelectionBoard::AMWTrackSelectionBoard()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	for (int32 Index = 0; Index < MWTrackSelection::OfferCount; ++Index)
	{
		auto* Anchor = CreateDefaultSubobject<USceneComponent>(*FString::Printf(TEXT("Anchor%d"), Index));
		Anchor->SetupAttachment(RootComponent);
		SlotAnchors.Add(Anchor);
		auto* Hit = CreateDefaultSubobject<UBoxComponent>(*FString::Printf(TEXT("OfferHit%d"), Index));
		Hit->SetupAttachment(Anchor);
		Hit->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Hit->SetCollisionResponseToAllChannels(ECR_Ignore);
		Hit->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Hit->SetGenerateOverlapEvents(false);
		SlotHitAreas.Add(Hit);
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
	for (int32 Index = 0; Index < SlotAnchors.Num(); ++Index)
	{
		const int32 Row = Index / MWTrackSelection::Columns;
		const int32 Column = Index % MWTrackSelection::Columns;
		const FVector Slot((Row - 0.5) * RowSpacing, (1 - Column) * ColumnSpacing, BoardHeight);
		SlotAnchors[Index]->SetRelativeLocation(Slot * LayoutScale);
	}
	Camera->SetRelativeLocation(Camera->GetRelativeLocation() * LayoutScale);
}

void AMWTrackSelectionBoard::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	Super::CalcCamera(DeltaTime, OutResult);

	const auto* GS = GetWorld()->GetGameState<AMWGameplayGameState>();
	if (!GS)
	{
		return;
	}

	int32 Width = 0;
	int32 Height = 0;
	if (Viewer.IsValid()) Viewer->GetViewportSize(Width, Height);
	else if (const auto* PC = GetWorld()->GetFirstPlayerController()) PC->GetViewportSize(Width, Height);
	const double Aspect = Width > 0 && Height > 0 ? static_cast<double>(Width) / Height : 16.0 / 9.0;
	if (GS->GetRoomPhase() == EMWRoomPhase::PreparingConstruction)
	{
		// 전시 화면은 열을 수평으로 보고, 선택 후 빈 칸이 생겨도 카메라 범위는 고정한다.
		OutResult.Rotation = GetActorTransform().TransformVectorNoScale(SelectionViewDirection).Rotation();
		const FVector Extent = (SlotExtent + FVector(RowSpacing * 0.5, ColumnSpacing, 0.0)) * LayoutScale;
		const FBox LocalBounds(-Extent, Extent);
		const FVector Center = GetBuildOrigin();
		FrameBounds(OutResult, LocalBounds.TransformBy(FTransform(GetActorQuat(), Center)), Aspect, 0.74);
		// 왼쪽 안내판과 후보가 겹치지 않도록 전시 영역을 화면 오른쪽으로 옮긴다.
		const double HalfWidth = FVector::Distance(OutResult.Location, Center)
			* FMath::Tan(FMath::DegreesToRadians(OutResult.FOV * 0.5));
		OutResult.Location -= FRotationMatrix(OutResult.Rotation).GetUnitAxis(EAxis::Y) * (HalfWidth * 0.24);
		return;
	}
	if (!bUseTopDownPlacementCamera || GS->GetRoomPhase() != EMWRoomPhase::TrackPlacement)
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
	// 끝점 너머에도 다음 조각의 중심을 옮겨 놓을 공간을 남긴다.
	// 기존 배치 카메라 각도를 출발 트랙의 방향에 맞춰 수평으로 돌린다.
	OutResult.Rotation = Camera->GetRelativeRotation();
	OutResult.Rotation.Yaw = FRotator::NormalizeAxis(OutResult.Rotation.Yaw + Session.PreviewYaw);
	FrameBounds(OutResult, Bounds.ExpandBy(FVector(400.0, 400.0, 120.0) * Session.LayoutScale), Aspect, 0.9);
}

bool AMWTrackSelectionBoard::BuildOffers(const FMWTrackSelectionSession& Session)
{
	ClearOffers();
	for (const FMWTrackOffer& Offer : Session.Offers)
	{
		UClass* TrackClass = Offer.TrackClass.LoadSynchronous();
		if (!TrackClass || !SlotAnchors.IsValidIndex(Offer.BoardSlotIndex) || !SlotHitAreas.IsValidIndex(Offer.BoardSlotIndex))
		{
			ClearOffers();
			return false;
		}
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const auto* Anchor = SlotAnchors[Offer.BoardSlotIndex].Get();
		AActor* Track = GetWorld()->SpawnActor<AActor>(TrackClass, Anchor->GetComponentTransform(), Params);
		if (!Track) { ClearOffers(); return false; }
		Track->SetReplicates(false);
		Track->SetActorTickEnabled(false);
		Track->SetActorEnableCollision(false);
		if (auto* Runtime = GetWorld()->GetSubsystem<UMWTrackRuntimeSubsystem>())
		{
			Runtime->SetTrackPreview(*Track, true);
		}
		// 판정은 전시 영역의 Box에 맡기고 보이는 와이어프레임의 중심을 전시 칸에 맞춘다.
		const FBox VisualBounds = MWTrackPreview::ShowWireframes(*Track);
		if (!VisualBounds.IsValid)
		{
			UE_LOG(LogDefault, Warning, TEXT("%s에 전시할 Wireframes 메시가 없습니다."), *GetNameSafe(Track));
			Track->Destroy();
			continue;
		}
		Offers.Add(Offer.OfferId, Track);
		// 큰 트랙도 이 칸 안에서 선택할 수 있도록 전시 Actor의 크기만 조정한다.
		const FVector Extent = VisualBounds.TransformBy(Anchor->GetComponentTransform().Inverse()).GetExtent();
		const FVector Fit = SlotExtent * LayoutScale / Extent.ComponentMax(FVector(1.0));
		const double Scale = FMath::Min(1.0, Fit.GetMin());
		const FVector VisualOffset = VisualBounds.GetCenter() - Track->GetActorLocation();
		Track->SetActorScale3D(Track->GetActorScale3D() * Scale);
		Track->SetActorLocation(Anchor->GetComponentLocation() - VisualOffset * Scale);
		SlotHitAreas[Offer.BoardSlotIndex]->SetBoxExtent(Extent * Scale + FVector(20 * LayoutScale));
		SlotHitAreas[Offer.BoardSlotIndex]->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		auto* Label = NewObject<UTextRenderComponent>(this);
		Label->SetupAttachment(SlotAnchors[Offer.BoardSlotIndex]);
		Label->SetRelativeLocation(FVector(-Extent.X * Scale - 55 * LayoutScale, 0, Extent.Z * Scale + 32 * LayoutScale));
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
	for (const auto& Hit : SlotHitAreas) Hit->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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
	const double Distance = FVector::DotProduct(GetBuildOrigin() - Origin, Normal) / Dot;
	if (Distance < 0 || Distance > 100000) return false;
	OutPosition = Origin + Direction * Distance;
	return !OutPosition.ContainsNaN() && OutPosition.GetAbsMax() < 1000000;
}

int32 AMWTrackSelectionBoard::FindHoveredOffer() const
{
	FVector Cursor;
	FHitResult Hit;
	if (!GetCursorPosition(Cursor) || !Viewer->GetHitResultUnderCursor(ECC_Visibility, false, Hit)) return INDEX_NONE;
	const int32 Slot = SlotHitAreas.IndexOfByPredicate([&Hit](const UBoxComponent* Box) { return Hit.GetComponent() == Box; });
	const auto* GS = GetWorld()->GetGameState<AMWGameplayGameState>();
	if (!GS) return INDEX_NONE;
	const FMWTrackSelectionSession Session = GS->GetConstructionComponent()->GetSelectionSession();
	const auto* Offer = Session.Offers.FindByPredicate([Slot](const FMWTrackOffer& Item) { return Item.BoardSlotIndex == Slot; });
	return Offer && Offer->ClaimedByMemberId == INDEX_NONE ? Offer->OfferId : INDEX_NONE;
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
	// 슬롯 수와 관계없이 기존 선택 평면과 테스트 맵의 출발 기준을 유지한다.
	return GetActorTransform().TransformPosition(FVector(0, 0, BoardHeight * LayoutScale));
}

void AMWTrackSelectionBoard::RefreshMarkers(const FMWTrackSelectionSession& Session)
{
	const auto* GS = GetWorld()->GetGameState<AMWGameplayGameState>();
	for (const auto& Pair : Offers)
	{
		const auto* Offer = Session.Offers.FindByPredicate([&Pair](const FMWTrackOffer& Item)
		{
			return Item.OfferId == Pair.Key;
		});
		const bool bAvailable = Offer && Offer->ClaimedByMemberId == INDEX_NONE;
		// 전시 Actor는 각 화면의 로컬 객체다. 복제된 소유 상태로 외형과 클릭 영역을 함께 숨긴다.
		if (IsValid(Pair.Value))
		{
			Pair.Value->SetActorHiddenInGame(!bAvailable);
		}
		if (Offer && SlotHitAreas.IsValidIndex(Offer->BoardSlotIndex))
		{
			SlotHitAreas[Offer->BoardSlotIndex]->SetCollisionEnabled(
				bAvailable ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		}
		if (UTextRenderComponent* Label = Labels.FindRef(Pair.Key))
		{
			Label->SetVisibility(bAvailable);
		}
		if (!bAvailable) continue;

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
			Label->SetWorldRotation(GetActorTransform().TransformVectorNoScale(-SelectionViewDirection).Rotation());
		}
	}
}

void AMWTrackSelectionBoard::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Viewer.IsValid()) Viewer->SetTrackSelectionBoard(nullptr);
	ClearOffers();
	Super::EndPlay(Reason);
}
