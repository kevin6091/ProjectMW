#include "Gameplay/Construction/MWTrackSnap.h"

#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"

namespace
{
	FTransform ReverseFrame(const FTransform& Frame)
	{
		const FQuat Reverse(FVector::UpVector, UE_PI);
		return FTransform((Frame.GetRotation() * Reverse).GetNormalized(), Frame.GetLocation());
	}

	FVector GetPortCenter(const FMWTrackPortFrames& Ports)
	{
		return (Ports[0].GetLocation() + Ports[1].GetLocation()) * 0.5;
	}

	FTransform AlignPorts(const FTransform& Source, const FTransform& Target)
	{
		// 두 바깥 방향을 마주 보게 하고 도로 위쪽은 유지한다. UE 합성은 왼쪽부터 적용한다.
		return Source.Inverse() * ReverseFrame(Target);
	}
}

bool MWTrackSnap::ReadPorts(const AActor& Track, FMWTrackPortFrames& OutPorts)
{
	static const FName PointNames[] = { TEXT("ConnectionPoint1"), TEXT("ConnectionPoint2") };
	// 기존 WoodenStraight의 SceneComponent 규약. +Y는 각 끝점에서 도로 안쪽을 향한다.
	// 계산에서는 로컬 -Y를 +X로 옮겨 양쪽 모두 바깥 방향을 가진 프레임으로 통일한다.
	const FQuat PortBasis(FVector::UpVector, -UE_HALF_PI);
	if (!Track.GetActorScale3D().Equals(FVector::OneVector, KINDA_SMALL_NUMBER))
	{
		return false;
	}
	TInlineComponentArray<USceneComponent*> Components(&Track);
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const USceneComponent* const* Point = Components.FindByPredicate([Index](const USceneComponent* Component)
		{
			return Component->GetFName() == PointNames[Index];
		});
		if (!Point)
		{
			return false;
		}

		const FTransform Local = (*Point)->GetComponentTransform().GetRelativeTransform(Track.GetActorTransform());
		if (Local.ContainsNaN() || !Local.GetRotation().IsNormalized() || Local.GetScale3D().GetMin() <= 0.0)
		{
			return false;
		}

		// 중첩된 BP의 스케일은 이미 끝점 위치에 반영되어 있다. 방향 프레임에는 넘기지 않는다.
		OutPorts[Index] = FTransform((Local.GetRotation() * PortBasis).GetNormalized(), Local.GetLocation());
	}
	return !OutPorts[0].GetLocation().Equals(OutPorts[1].GetLocation(), KINDA_SMALL_NUMBER);
}

FTransform MWTrackSnap::MakeRawTransform(const FMWTrackPortFrames& Ports, const FMWTrackPlacementSession& Session,
	const FMWTrackPlacementIntent& Intent)
{
	const FQuat Rotation(FRotator(0.0, Session.PreviewYaw + (Intent.bReversed ? 180.0 : 0.0), 0.0));
	// 임포트된 BP의 피벗은 도로 밖에 있을 수 있어 마우스는 두 포트의 중앙에 맞춘다.
	return FTransform(Rotation, Intent.CursorLocation - Rotation.RotateVector(GetPortCenter(Ports)));
}

bool MWTrackSnap::IsInBuildArea(const FMWTrackPlacementIntent& Intent, const FMWTrackPlacementSession& Session)
{
	return !Intent.CursorLocation.ContainsNaN()
		&& FVector::DistSquared(Intent.CursorLocation, Session.BuildOrigin) <= FMath::Square(BuildRadius * Session.LayoutScale);
}

bool MWTrackSnap::Resolve(const FMWTrackPortFrames& Ports, const FMWTrackPlacementSession& Session,
	const FMWTrackPlacementIntent& Intent, FTransform& OutTransform)
{
	OutTransform = MakeRawTransform(Ports, Session, Intent);
	if (!Intent.Target.IsValid() || (Intent.SourcePort != 0 && Intent.SourcePort != 1))
	{
		return false;
	}

	const FMWOpenTrackEnd* Target = Session.OpenEnds.FindByPredicate([&Intent](const FMWOpenTrackEnd& End)
	{
		return End.Id == Intent.Target;
	});
	if (!Target)
	{
		return false;
	}

	const FTransform Aligned = AlignPorts(Ports[Intent.SourcePort], Target->WorldFrame);
	const FVector Offset = Intent.CursorLocation - Aligned.TransformPosition(GetPortCenter(Ports));
	const double DistanceSquared = Offset.SizeSquared2D();
	if (DistanceSquared > FMath::Square(ApproachDistance * Session.LayoutScale))
	{
		return false;
	}

	OutTransform = Aligned;
	// 연결면은 먼저 평행하게 표시하되, 서버가 거리까지 확인했을 때만 끝점에 붙인다.
	const bool bSnapped = Intent.bSnapRequested && DistanceSquared <= FMath::Square(LeaveDistance * Session.LayoutScale);
	if (!bSnapped)
	{
		// 화면에 보이는 포트도 거리 판정에 사용한 오프셋만큼 이동한다.
		OutTransform.AddToTranslation(Offset);
	}
	return bSnapped;
}

bool MWTrackSnap::UpdateIntentFromRay(const FMWTrackPortFrames& Ports, const FMWTrackPlacementSession& Session,
	const FVector& RayOrigin, const FVector& RayDirection, FMWTrackPlacementIntent& Intent)
{
	if (FMath::IsNearlyZero(RayDirection.Z))
	{
		return false;
	}

	const auto ProjectCursor = [&](double PlaneZ, FMWTrackPlacementIntent& Candidate)
	{
		const double Distance = (PlaneZ - RayOrigin.Z) / RayDirection.Z;
		if (Distance < 0.0)
		{
			return false;
		}
		Candidate.CursorLocation = RayOrigin + RayDirection * Distance;
		return IsInBuildArea(Candidate, Session);
	};

	FMWTrackPlacementIntent Best;
	Best.bReversed = Intent.bReversed;
	bool bHasPosition = ProjectCursor(Session.BuildOrigin.Z, Best);
	const FVector PortCenter = GetPortCenter(Ports);
	const FQuat BaseRotation(FRotator(0.0, Session.PreviewYaw, 0.0));
	double BestDistanceSquared = FMath::Square(ApproachDistance * Session.LayoutScale);
	const double EnterDistanceSquared = FMath::Square(EnterDistance * Session.LayoutScale);
	const double LeaveDistanceSquared = FMath::Square(LeaveDistance * Session.LayoutScale);
	for (const FMWOpenTrackEnd& End : Session.OpenEnds)
	{
		const FMWTrackPortFrames Aligned = {
			AlignPorts(Ports[0], End.WorldFrame),
			AlignPorts(Ports[1], End.WorldFrame)
		};
		// 연결 가능한 두 자세 중 기본 회전과 가까운 쪽을 마우스 좌표의 기준으로 삼는다.
		const int32 DefaultPort = FMath::Abs(Aligned[0].GetRotation() | BaseRotation)
			>= FMath::Abs(Aligned[1].GetRotation() | BaseRotation) ? 0 : 1;
		const FVector ReferenceCenter = Aligned[DefaultPort].TransformPosition(PortCenter);
		FMWTrackPlacementIntent Candidate;
		Candidate.bReversed = Intent.bReversed;
		if (!ProjectCursor(ReferenceCenter.Z, Candidate))
		{
			continue;
		}
		const FVector Offset = Candidate.CursorLocation - ReferenceCenter;
		const double DistanceSquared = Offset.SizeSquared2D();
		const bool bKeepSnap = Intent.bSnapRequested && Intent.Target == End.Id && DistanceSquared <= LeaveDistanceSquared;
		if (!bKeepSnap && DistanceSquared >= BestDistanceSquared)
		{
			continue;
		}
		Candidate.Target = End.Id;
		Candidate.SourcePort = Candidate.bReversed ? 1 - DefaultPort : DefaultPort;
		// Q/E로 반대쪽 포트를 골라도 같은 마우스 위치에서 같은 연결점과 거리를 유지한다.
		Candidate.CursorLocation = Aligned[Candidate.SourcePort].TransformPosition(PortCenter) + Offset;
		if (!IsInBuildArea(Candidate, Session))
		{
			continue;
		}
		Candidate.bSnapRequested = bKeepSnap || DistanceSquared < EnterDistanceSquared;
		if (bKeepSnap)
		{
			Intent = Candidate;
			return true;
		}
		BestDistanceSquared = DistanceSquared;
		Best = Candidate;
		bHasPosition = true;
	}
	if (bHasPosition)
	{
		Intent = Best;
	}
	return bHasPosition;
}
