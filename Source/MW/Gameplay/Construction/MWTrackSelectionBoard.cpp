#include "Gameplay/Construction/MWTrackSelectionBoard.h"
#include "Gameplay/Construction/MWTrackPreviewPreload.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Gameplay/Construction/MWTrackSelectionExposure.h"
#include "UI/MWTrackSelectionWidget.h"
#include "UI/Cursor/MWCursorProjection.h"
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

namespace
{
	constexpr double RowSpacing = 1260.0;
	constexpr double ColumnSpacing = 1120.0;
	constexpr double BoardHeight = 80.0;
	const FVector SlotExtent(410.0, 460.0, 225.0);
	const FVector SelectionExtent = SlotExtent + FVector(RowSpacing * (MWTrackSelection::Rows - 1) * 0.5,
		ColumnSpacing * (MWTrackSelection::Columns - 1) * 0.5, 0.0);
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
	SelectionBackdrop = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SelectionBackdrop"));
	SelectionBackdrop->SetupAttachment(RootComponent);
	SelectionBackdrop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SelectionBackdrop->SetGenerateOverlapEvents(false);
	SelectionBackdrop->SetCanEverAffectNavigation(false);
	SelectionBackdrop->SetCastShadow(false);
	SelectionBackdrop->SetHiddenInGame(true);

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
		const FVector Slot((Row - (MWTrackSelection::Rows - 1) * 0.5) * RowSpacing,
			((MWTrackSelection::Columns - 1) * 0.5 - Column) * ColumnSpacing, BoardHeight);
		SlotAnchors[Index]->SetRelativeLocation(Slot * LayoutScale);
	}
	Camera->SetRelativeLocation(Camera->GetRelativeLocation() * LayoutScale);
	if (const UStaticMesh* BackdropMesh = SelectionBackdrop->GetStaticMesh())
	{
		const FBox MeshBounds = BackdropMesh->GetBoundingBox();
		const FVector MeshSize = MeshBounds.GetSize();
		if (MeshSize.X > UE_SMALL_NUMBER && MeshSize.Y > UE_SMALL_NUMBER)
		{
			// 레벨이 고른 메시와 높이는 유지하고, 전시 면만 현재 슬롯 배치에 맞춘다.
			const FVector RequiredSize = SelectionExtent * (2.0 * LayoutScale);
			FVector Scale = SelectionBackdrop->GetRelativeScale3D();
			Scale.X = FMath::Max(Scale.X, RequiredSize.X / MeshSize.X);
			Scale.Y = FMath::Max(Scale.Y, RequiredSize.Y / MeshSize.Y);
			const FVector Center = SelectionBackdrop->GetRelativeRotation().RotateVector(MeshBounds.GetCenter() * Scale);
			FVector Location = SelectionBackdrop->GetRelativeLocation();
			Location.X = -Center.X;
			Location.Y = -Center.Y;
			SelectionBackdrop->SetRelativeLocation(Location);
			SelectionBackdrop->SetRelativeScale3D(Scale);
		}
	}
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
		const FVector Extent = SelectionExtent * LayoutScale;
		const FBox LocalBounds(-Extent, Extent);
		const FVector Center = GetBuildOrigin();
		FrameBounds(OutResult, LocalBounds.TransformBy(FTransform(GetActorQuat(), Center)), Aspect, 0.74);
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
	auto* Preload = ULocalPlayer::GetSubsystem<UMWTrackPreviewPreload>(Viewer->GetLocalPlayer());
	if (Preload && !Preload->PrepareCandidates(Session.Offers))
	{
		return false;
	}
	ClearOffers();
	for (const FMWTrackOffer& Offer : Session.Offers)
	{
		if (Offer.ClaimedByMemberId != INDEX_NONE)
		{
			continue;
		}
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
	}
	SessionId = Session.SelectionSessionId;
	Revision = Session.OffersRevision;
	// 이제 각 전시 액터가 메시와 재질을 보관하므로 로비의 선로딩 핸들을 놓는다.
	if (Preload)
	{
		Preload->ReleaseCandidates();
	}
	return true;
}

void AMWTrackSelectionBoard::CollectArrivalMaterials(const FMWTrackSelectionSession& Session,
    TArray<UMaterialInterface*>& Materials, FString& Pending) const
{
    // 전시가 먼저 생겨도 서버가 보낸 후보 버전과 같아야 이번 선택 화면이 준비된 것으로 본다.
    if (!Session.bIsOpen || Session.SelectionSessionId <= 0 || Session.Offers.IsEmpty() ||
        SessionId != Session.SelectionSessionId || Revision != Session.OffersRevision)
    {
        Pending = TEXT("selection session/revision");
        return;
    }
    for (const auto& Offer : Session.Offers)
    {
        // 다른 참가자가 먼저 확정한 후보는 첫 화면에서 이미 숨겨져 있다.
        if (Offer.ClaimedByMemberId != INDEX_NONE) continue;
        const AActor* Track = Offers.FindRef(Offer.OfferId);
        if (!MWTrackPreview::CollectVisibleMaterials(Track, Materials))
        {
            Pending = FString::Printf(TEXT("offer %d (%s) mesh/material"), Offer.OfferId, *Offer.TrackClass.ToString());
            return;
        }
    }
    // 배경을 사용하는 맵에서는 후보와 같은 첫 장면 렌더 검사에 포함한다.
    if (SelectionBackdrop->GetStaticMesh() && (SelectionBackdrop->bHiddenInGame ||
        !MWTrackPreview::CollectVisibleMaterials(this, Materials)))
    {
        Pending = TEXT("selection backdrop mesh/material");
    }
}

void AMWTrackSelectionBoard::ClearOffers()
{
	for (const auto& Pair : Offers) if (IsValid(Pair.Value)) Pair.Value->Destroy();
	Offers.Reset();
	for (const auto& Hit : SlotHitAreas) Hit->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SessionId = Revision = 0;
	LocalHover = SentHover = INDEX_NONE;
}

bool AMWTrackSelectionBoard::GetCursorPosition(FVector& OutPosition) const
{
	return Viewer.IsValid() && MWCursorProjection::SampleWorldPlane(*Viewer, GetBuildOrigin(), GetActorUpVector(), OutPosition);
}

int32 AMWTrackSelectionBoard::FindHoveredOffer() const
{
	const auto* GS = GetWorld()->GetGameState<AMWGameplayGameState>();
	FVector PlanePosition, Start, Direction;
	if (!GS || !GetCursorPosition(PlanePosition) || !Viewer->DeprojectMousePositionToWorld(Start, Direction))
	{
		return INDEX_NONE;
	}

	// 전시판의 후보 상자만 비교하므로 배경과 장식의 월드 충돌이 선택을 가로채지 않는다.
	const FVector End = Start + Direction * Viewer->HitResultTraceDistance;
	const FMWTrackSelectionSession Session = GS->GetConstructionComponent()->GetSelectionSession();
	int32 BestOffer = INDEX_NONE;
	float BestTime = TNumericLimits<float>::Max();
	for (const FMWTrackOffer& Offer : Session.Offers)
	{
		if (Offer.ClaimedByMemberId != INDEX_NONE || !IsValid(Offers.FindRef(Offer.OfferId))
			|| !SlotHitAreas.IsValidIndex(Offer.BoardSlotIndex))
		{
			continue;
		}

		const UBoxComponent& Box = *SlotHitAreas[Offer.BoardSlotIndex];
		const FVector Extent = Box.GetUnscaledBoxExtent();
		const FTransform& Transform = Box.GetComponentTransform();
		FVector HitLocation, HitNormal;
		float HitTime;
		if (FMath::LineExtentBoxIntersection(FBox(-Extent, Extent), Transform.InverseTransformPosition(Start),
			Transform.InverseTransformPosition(End), FVector::ZeroVector, HitLocation, HitNormal, HitTime) && HitTime < BestTime)
		{
			BestTime = HitTime;
			BestOffer = Offer.OfferId;
		}
	}
	return BestOffer;
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
		// 배치 카메라로 옮겨도 선택 전시 배경은 경기 코스에 남지 않는다.
		SelectionBackdrop->SetHiddenInGame(true);
		if (Viewer.IsValid())
		{
			if (auto* Preload = ULocalPlayer::GetSubsystem<UMWTrackPreviewPreload>(Viewer->GetLocalPlayer()))
			{
				Preload->ReleaseCandidates();
			}
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
				if (APlayerCameraManager* Manager = PC->PlayerCameraManager;
					Manager && !Manager->FindCameraModifierByClass(UMWTrackSelectionExposure::StaticClass()))
				{
					Manager->AddNewCameraModifier(UMWTrackSelectionExposure::StaticClass());
				}
				PC->SetViewTarget(this);
				break;
			}
		}
	}
	if (!Viewer.IsValid()) return;
	const FMWTrackSelectionSession Session = GS->GetConstructionComponent()->GetSelectionSession();
	if (Session.SelectionSessionId <= 0) return;
	if ((SessionId != Session.SelectionSessionId || Revision != Session.OffersRevision) && !BuildOffers(Session)) return;
	SelectionBackdrop->SetHiddenInGame(false);
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
	auto* Widget = Cast<UMWTrackSelectionWidget>(Viewer->TrackSelectionWidget);
	for (const FMWTrackOffer& Offer : Session.Offers)
	{
		AActor* Track = Offers.FindRef(Offer.OfferId);
		if (!IsValid(Track) || !SlotHitAreas.IsValidIndex(Offer.BoardSlotIndex)) continue;

		// 확정된 후보는 기존 칸의 위치를 유지하면서 외형과 클릭 영역을 함께 숨긴다.
		UBoxComponent* Hit = SlotHitAreas[Offer.BoardSlotIndex];
		if (Offer.ClaimedByMemberId != INDEX_NONE)
		{
			if (!Track->IsHidden())
			{
				Track->SetActorHiddenInGame(true);
				Hit->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
			continue;
		}

		// 같은 전시 칸의 윗면에 이름표를 붙여 카메라와 조각 크기가 변해도 함께 움직이게 한다.
		// 액터 생성과 첫 렌더는 다른 프레임일 수 있다. 조각을 한 번 그린 뒤 이름표를 연다.
		if (Widget && Track->GetLastRenderTime() > 0.f)
		{
			const FVector Top = Hit->GetComponentLocation() + Hit->GetUpVector() * Hit->GetScaledBoxExtent().Z;
			Widget->PositionOfferLabel(Offer.OfferId, Top, Offer.OfferId == LocalHover);
		}
	}
}

void AMWTrackSelectionBoard::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Viewer.IsValid()) Viewer->SetTrackSelectionBoard(nullptr);
	ClearOffers();
	Super::EndPlay(Reason);
}
