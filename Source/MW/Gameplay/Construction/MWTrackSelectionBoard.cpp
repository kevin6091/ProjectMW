#include "Gameplay/Construction/MWTrackSelectionBoard.h"
#include "Gameplay/Construction/MWTrackPreviewPreload.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Gameplay/Construction/MWTrackSelectionExposure.h"
#include "Gameplay/Construction/MWTrackPresentationSettings.h"
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
	SelectionStage = CreateDefaultSubobject<USceneComponent>(TEXT("SelectionStage"));
	SelectionStage->SetupAttachment(RootComponent);
	SelectionBackdrop = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SelectionBackdrop"));
	SelectionBackdrop->SetupAttachment(SelectionStage);
	SelectionBackdrop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SelectionBackdrop->SetGenerateOverlapEvents(false);
	SelectionBackdrop->SetCanEverAffectNavigation(false);
	SelectionBackdrop->SetCastShadow(false);
	SelectionBackdrop->SetHiddenInGame(true);

	for (int32 Index = 0; Index < MWTrackSelection::OfferCount; ++Index)
	{
		auto* Anchor = CreateDefaultSubobject<USceneComponent>(*FString::Printf(TEXT("Anchor%d"), Index));
		Anchor->SetupAttachment(SelectionStage);
		SlotAnchors.Add(Anchor);
		auto* VisualPivot = CreateDefaultSubobject<USceneComponent>(*FString::Printf(TEXT("OfferVisualPivot%d"), Index));
		VisualPivot->SetupAttachment(Anchor);
		SlotVisualPivots.Add(VisualPivot);
		auto* Hit = CreateDefaultSubobject<UBoxComponent>(*FString::Printf(TEXT("OfferHit%d"), Index));
		Hit->SetupAttachment(Anchor);
		// 기존 클릭 상자의 월드 크기를 보존한다. 확대는 전시 조각에만 적용한다.
		Hit->SetAbsolute(false, false, true);
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
	// 이전 레벨에 저장된 배경 슬롯의 로컬 배치를 새 전시 루트에서도 사용한다.
	SelectionBackdrop->AttachToComponent(SelectionStage, FAttachmentTransformRules::KeepRelativeTransform);
	for (int32 Index = 0; Index < SlotAnchors.Num(); ++Index)
	{
		const int32 Row = Index / MWTrackSelection::Columns;
		const int32 Column = Index % MWTrackSelection::Columns;
		const FVector Slot((Row - (MWTrackSelection::Rows - 1) * 0.5) * RowSpacing,
			((MWTrackSelection::Columns - 1) * 0.5 - Column) * ColumnSpacing, BoardHeight);
		SlotAnchors[Index]->AttachToComponent(SelectionStage, FAttachmentTransformRules::KeepRelativeTransform);
		SlotAnchors[Index]->SetRelativeLocation(Slot * LayoutScale);
		SlotVisualPivots[Index]->AttachToComponent(SlotAnchors[Index], FAttachmentTransformRules::KeepRelativeTransform);
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
	const auto* PC = Viewer.IsValid() ? Viewer.Get() : Cast<AMWGameplayPlayerController>(GetWorld()->GetFirstPlayerController());
	if (PC && PC->GetConstructionPresentationPhase() == EMWRoomPhase::PreparingConstruction)
	{
		FrameSelection(OutResult, PC);
		OutResult.Location += StageOffset;
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

TOptional<double> AMWTrackSelectionBoard::GetBackdropTop() const
{
	// 실제 레벨 메시와 트랜스폼에서 전시판 상면을 읽는다. 배경을 사용하지 않는 레벨도 있다.
	if (const UStaticMesh* Mesh = SelectionBackdrop->GetStaticMesh())
	{
		return Mesh->GetBoundingBox().TransformBy(SelectionBackdrop->GetRelativeTransform()).Max.Z;
	}
	return {};
}

void AMWTrackSelectionBoard::FrameSelection(FMinimalViewInfo& View, const APlayerController* PC) const
{
	int32 Width = 0, Height = 0;
	if (PC) PC->GetViewportSize(Width, Height);
	const double Aspect = Width > 0 && Height > 0 ? static_cast<double>(Width) / Height : 16.0 / 9.0;
	View.Rotation = GetActorTransform().TransformVectorNoScale(SelectionViewDirection).Rotation();
	const FVector Extent = SelectionExtent * LayoutScale;
	FBox LocalBounds(-Extent, Extent);
	if (const TOptional<double> Top = GetBackdropTop(); Top.IsSet())
	{
		const double TopFromOrigin = Top.GetValue() - BoardHeight * LayoutScale;
		const double Peak = FMath::Max(1.0, GetDefault<UMWTrackPresentationSettings>()->CandidateHoverPeakScale);
		// 확대 피벗과 최대 높이를 포함한 범위로 화면을 맞춘다.
		LocalBounds.Max.Z = FMath::Max(TopFromOrigin, -Extent.Z / Peak) + 2.0 * Extent.Z;
	}
	FrameBounds(View, LocalBounds.TransformBy(FTransform(GetActorQuat(), GetBuildOrigin())), Aspect, 0.74);
}

void AMWTrackSelectionBoard::UpdateSelectionStage(const FMWTrackPlacementSession& Course, const APlayerController& PC)
{
	FMinimalViewInfo View;
	Camera->GetCameraView(0.0f, View);
	FrameSelection(View, &PC);

	const double BoardWidth = SelectionExtent.Y * LayoutScale * 2.0;
	double Distance = BoardWidth;
	if (Course.CourseBounds.IsValid)
	{
		FBox CourseBounds = Course.CourseBounds;
		CourseBounds.Min.Z = FMath::Min(CourseBounds.Min.Z, Course.Ground.FloorZ);
		const FBox InCamera = CourseBounds.TransformBy(FTransform(View.Rotation, View.Location).Inverse());
		const double FrustumWidth = FMath::Max(0.0, InCamera.Max.X) * FMath::Tan(FMath::DegreesToRadians(View.FOV * 0.5));
		// 코스의 가장 먼 깊이까지 포함해서 출발점과 설치된 도로를 전시 카메라의 왼쪽 밖으로 보낸다.
		Distance = FMath::Max(Distance, InCamera.Max.Y + FrustumWidth + BoardWidth);
	}

	const FVector Offset = FRotationMatrix(View.Rotation).GetUnitAxis(EAxis::Y) * Distance;
	if (!Offset.Equals(StageOffset))
	{
		StageOffset = Offset;
		SelectionStage->SetWorldLocation(GetActorLocation() + StageOffset);
	}
}

bool AMWTrackSelectionBoard::BuildOffers(const FMWTrackSelectionSession& Session)
{
	auto* Preload = ULocalPlayer::GetSubsystem<UMWTrackPreviewPreload>(Viewer->GetLocalPlayer());
	if (Preload && !Preload->PrepareCandidates(Session.Offers))
	{
		return false;
	}
	ClearOffers();
	const auto* Settings = GetDefault<UMWTrackPresentationSettings>();
	auto* SurfaceMaterial = Settings->CandidateSurfaceMaterial.LoadSynchronous();
	const TOptional<double> BackdropTop = GetBackdropTop();
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
		auto* Anchor = SlotAnchors[Offer.BoardSlotIndex].Get();
		AActor* Track = GetWorld()->SpawnActor<AActor>(TrackClass, Anchor->GetComponentTransform(), Params);
		if (!Track) { ClearOffers(); return false; }
		Track->SetReplicates(false);
		Track->SetActorTickEnabled(false);
		Track->SetActorEnableCollision(false);
		if (auto* Runtime = GetWorld()->GetSubsystem<UMWTrackRuntimeSubsystem>())
		{
			Runtime->SetTrackPreview(*Track, true);
		}
		// 판정은 전시 영역의 Box에 맡기고 표면과 와이어프레임 전체를 전시 칸 중앙에 맞춘다.
		const FBox VisualBounds = MWTrackPreview::ShowSelectionPreview(*Track, SurfaceMaterial);
		if (!VisualBounds.IsValid)
		{
			UE_LOG(LogDefault, Warning, TEXT("%s에 전시할 메시가 없습니다."), *GetNameSafe(Track));
			Track->Destroy();
			continue;
		}
		Offers.Add(Offer.OfferId, Track);
		// 큰 트랙도 이 칸 안에서 선택할 수 있도록 전시 Actor의 크기만 조정한다.
		const FVector Extent = VisualBounds.TransformBy(Anchor->GetComponentTransform().Inverse()).GetExtent();
		// 한 바퀴를 회전해도 이웃 칸으로 침범하지 않도록 수평 회전 반경으로 크기를 제한한다.
		const double Radius = Extent.Size2D();
		const FVector SweepExtent(Radius, Radius, Extent.Z);
		const double PeakScale = FMath::Max(1.0, Settings->CandidateHoverPeakScale);
		const FVector Fit = SlotExtent * LayoutScale / (PeakScale * SweepExtent.ComponentMax(FVector::OneVector));
		const double Scale = FMath::Min(1.0, Fit.GetMin());
		const FVector VisualOffset = VisualBounds.GetCenter() - Track->GetActorLocation();
		const double HalfHeight = Extent.Z * Scale;
		double CenterHeight = 0.0;
		double PivotHeight = 0.0;
		if (BackdropTop.IsSet())
		{
			// 조각의 실제 아랫면을 판 위에 두고 그 높이를 확대 피벗으로 쓴다.
			const double RestBottom = FMath::Max(-HalfHeight, BackdropTop.GetValue() - Anchor->GetRelativeLocation().Z);
			CenterHeight = RestBottom + HalfHeight;
			PivotHeight = RestBottom;
		}
		auto* VisualPivot = SlotVisualPivots[Offer.BoardSlotIndex].Get();
		VisualPivot->SetRelativeLocation(FVector::UpVector * PivotHeight);
		Track->SetActorScale3D(Track->GetActorScale3D() * Scale);
		Track->SetActorLocation(Anchor->GetComponentTransform().TransformPosition(FVector::UpVector * CenterHeight) - VisualOffset * Scale);
		Track->AttachToComponent(VisualPivot, FAttachmentTransformRules::KeepWorldTransform);
		MWTrackPreview::SetWireframeColor(*Track, Settings->CandidateTint);
		SlotHitAreas[Offer.BoardSlotIndex]->SetRelativeLocation(FVector::UpVector * CenterHeight);
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
	HoveredOffers.Reset();
	HoverPulseElapsed.Reset();
	CandidateYaw = 0.0;
	for (const auto& Anchor : SlotAnchors)
	{
		Anchor->SetRelativeRotation(FRotator::ZeroRotator);
	}
	for (const auto& VisualPivot : SlotVisualPivots)
	{
		VisualPivot->SetRelativeScale3D(FVector::OneVector);
	}
}

bool AMWTrackSelectionBoard::GetCursorPosition(FVector& OutPosition) const
{
	if (!Viewer.IsValid() || !MWCursorProjection::SampleWorldPlane(*Viewer, GetBuildOrigin() + StageOffset, GetActorUpVector(), OutPosition))
	{
		return false;
	}
	// 커서의 좌표 계약은 기존 전시 원점을 유지한다. 수신 창이 자기 카메라의 분리 거리만 적용한다.
	OutPosition -= StageOffset;
	return true;
}

int32 AMWTrackSelectionBoard::FindHoveredOffer() const
{
	const auto* GS = GetWorld()->GetGameState<AMWGameplayGameState>();
	FVector2D Screen;
	FVector Start, Direction;
	if (!GS || !Viewer.IsValid() || !MWCursorProjection::SampleViewportPointer(*Viewer, Screen)
		|| !Viewer->DeprojectScreenPositionToWorld(Screen.X, Screen.Y, Start, Direction))
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
	auto* Local = Viewer.Get();
	if (!Local)
	{
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (auto* PC = Cast<AMWGameplayPlayerController>(It->Get()); PC && PC->IsLocalController())
			{
				Local = PC;
				break;
			}
		}
	}
	if (!Local || Local->GetConstructionPresentationPhase() != EMWRoomPhase::PreparingConstruction)
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
	UpdateSelectionStage(GS->GetConstructionComponent()->GetPlacementSession(), *Local);
	if (!Viewer.IsValid())
	{
		Viewer = Local;
		Local->SetTrackSelectionBoard(this);
		if (APlayerCameraManager* Manager = Local->PlayerCameraManager;
			Manager && !Manager->FindCameraModifierByClass(UMWTrackSelectionExposure::StaticClass()))
		{
			Manager->AddNewCameraModifier(UMWTrackSelectionExposure::StaticClass());
		}
		Local->SetViewTarget(this);
	}
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
		// 포커스 전환은 송신 공백으로 처리한다. 실제 이탈은 활성 창의 숨김 표본으로 전달한다.
		if (Now - LastCursorTime >= .05 && MWCursorProjection::HasActiveViewportPointer(*Viewer))
		{
			FVector Position = FVector::ZeroVector;
			const bool bVisible = GetCursorPosition(Position);
			Viewer->RequestTrackCursor(SessionId, Revision, bVisible, Position);
			LastCursorTime = Now;
		}
	}
	RefreshOffers(*GS, Session, DeltaSeconds);
}

FVector AMWTrackSelectionBoard::GetBuildOrigin() const
{
	// 슬롯 수와 관계없이 기존 선택 평면과 테스트 맵의 출발 기준을 유지한다.
	return GetActorTransform().TransformPosition(FVector(0, 0, BoardHeight * LayoutScale));
}

FVector AMWTrackSelectionBoard::ProjectSelectionCursor(const FVector& BoardWorldPosition) const
{
	return BoardWorldPosition + StageOffset;
}

void AMWTrackSelectionBoard::RefreshOffers(const AMWGameplayGameState& GameState,
	const FMWTrackSelectionSession& Session, float DeltaSeconds)
{
	const auto* Settings = GetDefault<UMWTrackPresentationSettings>();
	TSet<int32> CurrentHover;
	if (Session.bIsOpen && LocalHover != INDEX_NONE)
	{
		CurrentHover.Add(LocalHover);
	}
	for (const APlayerState* Player : GameState.PlayerArray)
	{
		const auto* Remote = Cast<AMWPlayerState>(Player);
		if (!Session.bIsOpen || !Remote || Remote == Viewer->GetPlayerState<AMWPlayerState>())
		{
			continue;
		}
		// 원격 커서가 가리킨 후보도 밝힌다. 라운드나 후보가 바뀌면 이전 상태는 무시한다.
		const auto& Selection = Remote->GetPlayerConstructionComponent()->GetSelection();
		if (!Selection.bConfirmed && Selection.SelectionSessionId == SessionId &&
			Selection.OffersRevision == Revision && Selection.HoveredOfferId != INDEX_NONE)
		{
			CurrentHover.Add(Selection.HoveredOfferId);
		}
	}
	CandidateYaw = FMath::Fmod(CandidateYaw + DeltaSeconds * FMath::Max(0.0, Settings->CandidateDegreesPerSecond), 360.0);
	const FRotator Rotation(0.0, CandidateYaw, 0.0);
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

		// 조각과 클릭 상자가 같은 전시 앵커를 따라 회전해 선택 범위가 분리되지 않는다.
		USceneComponent* Anchor = SlotAnchors[Offer.BoardSlotIndex];
		Anchor->SetRelativeRotation(Rotation);
		const bool bHovered = CurrentHover.Contains(Offer.OfferId);
		if (bHovered != HoveredOffers.Contains(Offer.OfferId))
		{
			MWTrackPreview::SetWireframeColor(*Track, bHovered
				? Settings->HoveredCandidateTint : Settings->CandidateTint);
			if (bHovered && Settings->CandidateHoverSeconds > 0.0)
			{
				// 포인터 진입 때만 한 번 커졌다 돌아온다. 계속 가리켜도 박동하지 않는다.
				HoverPulseElapsed.Add(Offer.OfferId, 0.0);
			}
		}

		if (double* Elapsed = HoverPulseElapsed.Find(Offer.OfferId))
		{
			*Elapsed += DeltaSeconds;
			const double Alpha = FMath::Clamp(*Elapsed / Settings->CandidateHoverSeconds, 0.0, 1.0);
			const double Pulse = Alpha < 1.0 ? FMath::Sin(UE_DOUBLE_PI * Alpha) : 0.0;
			const double Scale = FMath::Lerp(1.0, FMath::Max(1.0, Settings->CandidateHoverPeakScale), Pulse);
			SlotVisualPivots[Offer.BoardSlotIndex]->SetRelativeScale3D(FVector(Scale));
			if (Alpha >= 1.0)
			{
				HoverPulseElapsed.Remove(Offer.OfferId);
			}
		}
	}
	HoveredOffers = MoveTemp(CurrentHover);
}

void AMWTrackSelectionBoard::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Viewer.IsValid()) Viewer->SetTrackSelectionBoard(nullptr);
	ClearOffers();
	Super::EndPlay(Reason);
}
