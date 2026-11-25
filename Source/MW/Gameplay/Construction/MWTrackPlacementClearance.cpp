#include "Gameplay/Construction/MWTrackPlacementClearance.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Gameplay/Track/MWTrackRuntimeSubsystem.h"

void MWTrackPlacementClearance::AddCorners(const FBox& Box, const FTransform& Local, TArray<FVector>& OutPoints)
{
	if (!Box.IsValid)
	{
		return;
	}
	for (const double X : {Box.Min.X, Box.Max.X})
	{
		for (const double Y : {Box.Min.Y, Box.Max.Y})
		{
			for (const double Z : {Box.Min.Z, Box.Max.Z})
			{
				OutPoints.Add(Local.TransformPosition(FVector(X, Y, Z)));
			}
		}
	}
}

TArray<FVector> MWTrackPlacementClearance::ReadLocalPoints(const AActor& Track, TConstArrayView<FTransform> Ports)
{
	TArray<FVector> Points;
	for (const FTransform& Port : Ports)
	{
		Points.Add(Port.GetLocation());
	}

	const auto* Runtime = Track.GetWorld()->GetSubsystem<UMWTrackRuntimeSubsystem>();
	const auto* Definition = Runtime ? Runtime->FindTrackDefinition(TSoftClassPtr<AActor>(Track.GetClass())) : nullptr;
	if (Definition)
	{
		// 피벗과 장식 바운드는 실제 도로 높이를 나타내지 않으므로 카탈로그의 블로킹 형상을 읽는다.
		for (const FMWTrackShapeData& Shape : Definition->Shapes)
		{
			if (Shape.Trigger != EMWTrackTrigger::None)
			{
				continue;
			}
			if (Shape.Shape == EMWTrackShape::Mesh)
			{
				if (Shape.Mesh)
				{
					AddCorners(Shape.Mesh->GetBounds().GetBox(), Shape.Local, Points);
				}
			}
			else
			{
				FVector Extent = Shape.Extent;
				if (Shape.Shape == EMWTrackShape::Sphere)
				{
					Extent = FVector(Extent.X);
				}
				else if (Shape.Shape == EMWTrackShape::Capsule)
				{
					Extent.Y = Extent.X;
				}
				AddCorners(FBox(-Extent, Extent), Shape.Local, Points);
			}
		}
	}
	else
	{
		// 구형 BP와 종료 캡은 카탈로그가 없을 수 있다. 트리거·장식은 제외하고 등록 전에 원래 충돌을 읽는다.
		for (const UPrimitiveComponent* Part : TInlineComponentArray<UPrimitiveComponent*>(&Track))
		{
			if (Part->IsQueryCollisionEnabled() && Part->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block)
			{
				const FTransform Local = Part->GetComponentTransform().GetRelativeTransform(Track.GetActorTransform());
				AddCorners(Part->CalcBounds(FTransform::Identity).GetBox(), Local, Points);
			}
		}
	}
	return Points;
}

bool MWTrackPlacementClearance::FitsFloor(TConstArrayView<FVector> LocalPoints, const FTransform& Transform, double FloorZ)
{
	if (!FMath::IsFinite(FloorZ) || Transform.ContainsNaN())
	{
		return false;
	}
	for (const FVector& Point : LocalPoints)
	{
		// 정확히 닿는 접합부에서 좌표 계산 오차만 허용한다.
		const double Z = Transform.TransformPosition(Point).Z;
		if (!FMath::IsFinite(Z) || (Z < FloorZ && !FMath::IsNearlyEqual(Z, FloorZ)))
		{
			return false;
		}
	}
	return !LocalPoints.IsEmpty();
}

bool MWTrackPlacementClearance::FitsWithFinish(TConstArrayView<FVector> LocalPoints, const FMWTrackPlacementGround& Ground,
	const FTransform& Transform, const FTransform& FreePort)
{
	// 다음 참가자가 이탈해도 이 끝에 결승 조각을 안전하게 연결할 수 있어야 한다.
	return FitsFloor(LocalPoints, Transform, Ground.FloorZ)
		&& FitsFloor(Ground.FinishLocalPoints, MWTrackSnap::AlignPorts(Ground.FinishPort, FreePort * Transform), Ground.FloorZ);
}

bool MWTrackPlacementClearance::FitsEitherPort(TConstArrayView<FVector> LocalPoints, const FMWTrackPlacementGround& Ground,
	const FMWTrackPortFrames& Ports, const FTransform& OpenEnd)
{
	return FitsWithFinish(LocalPoints, Ground, MWTrackSnap::AlignPorts(Ports[0], OpenEnd), Ports[1])
		|| FitsWithFinish(LocalPoints, Ground, MWTrackSnap::AlignPorts(Ports[1], OpenEnd), Ports[0]);
}
