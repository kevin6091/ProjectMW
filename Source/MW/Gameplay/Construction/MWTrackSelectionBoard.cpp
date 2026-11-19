#include "Gameplay/Construction/MWTrackSelectionBoard.h"
#include "Gameplay/Construction/MWTrackPreviewPreload.h"
#include "Engine/LocalPlayer.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Gameplay/Construction/MWTrackSelectionExposure.h"
#include "UI/MWTrackSelectionWidget.h"
#include "UI/Cursor/MWCursorProjection.h"
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

	}
	SessionId = Session.SelectionSessionId;
	Revision = Session.OffersRevision;
	// 이제 각 전시 액터가 메시와 재질을 보관하므로 로비의 선로딩 핸들을 놓는다.
	if (auto* Preload = ULocalPlayer::GetSubsystem<UMWTrackPreviewPreload>(Viewer->GetLocalPlayer()))
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
        const AActor* Track = Offers.FindRef(Offer.OfferId);
        if (!MWTrackPreview::CollectVisibleMaterials(Track, Materials))
        {
            Pending = FString::Printf(TEXT("offer %d (%s) mesh/material"), Offer.OfferId, *Offer.DefinitionAssetId.ToString());
            return;
        }
    }
}

void AMWTrackSelectionBoard::ClearOffers()
{
	for (const auto& Pair : Offers) if (IsValid(Pair.Value)) Pair.Value->Destroy();
	Offers.Reset();
	for (const auto& Hit : HitAreas) Hit->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SessionId = Revision = 0;
	LocalHover = SentHover = INDEX_NONE;
}

bool AMWTrackSelectionBoard::GetCursorPosition(FVector& OutPosition) const
{
	return Viewer.IsValid() && MWCursorProjection::SampleWorldPlane(*Viewer, GetBuildOrigin(), GetActorUpVector(), OutPosition);
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
	auto* Widget = Cast<UMWTrackSelectionWidget>(Viewer->TrackSelectionWidget);
	if (!Widget)
	{
		return;
	}

	// 같은 전시 칸의 윗면에 이름표를 붙여 카메라와 조각 크기가 변해도 함께 움직이게 한다.
	for (const FMWTrackOffer& Offer : Session.Offers)
	{
		const AActor* Track = Offers.FindRef(Offer.OfferId);
		// 액터 생성과 첫 렌더는 다른 프레임일 수 있다. 조각을 한 번 그린 뒤 이름표를 연다.
		if (Track && Track->GetLastRenderTime() > 0.f && HitAreas.IsValidIndex(Offer.BoardSlotIndex))
		{
			const UBoxComponent* Hit = HitAreas[Offer.BoardSlotIndex];
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
