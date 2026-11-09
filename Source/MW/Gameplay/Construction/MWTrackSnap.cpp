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
	const FVector PortCenter = (Ports[0].GetLocation() + Ports[1].GetLocation()) * 0.5;
	return FTransform(Rotation, Intent.CursorLocation - Rotation.RotateVector(PortCenter));
}

bool MWTrackSnap::IsInBuildArea(const FMWTrackPlacementIntent& Intent, const FMWTrackPlacementSession& Session)
{
	return !Intent.CursorLocation.ContainsNaN()
		&& FVector::DistSquared(Intent.CursorLocation, Session.BuildOrigin) <= FMath::Square(BuildRadius * Session.LayoutScale);
}

bool MWTrackSnap::Resolve(const FMWTrackPortFrames& Ports, const FMWTrackPlacementSession& Session,
	const FMWTrackPlacementIntent& Intent, FTransform& OutTransform)
{
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

	const FTransform& Source = Ports[Intent.SourcePort];
	const FVector RawEnd = MakeRawTransform(Ports, Session, Intent).TransformPosition(Source.GetLocation());
	// 포인터는 수평 작업 평면 위에 있다. 높이는 선택된 포트의 월드 변환으로 맞춘다.
	if (FVector::DistSquaredXY(RawEnd, Target->WorldFrame.GetLocation()) > FMath::Square(LeaveDistance * Session.LayoutScale))
	{
		return false;
	}

	// 두 바깥 방향을 마주 보게 하고 도로 위쪽은 유지한다. UE 합성은 왼쪽부터 적용한다.
	OutTransform = Source.Inverse() * ReverseFrame(Target->WorldFrame);
	return true;
}

void MWTrackSnap::FindCandidate(const FMWTrackPortFrames& Ports, const FMWTrackPlacementSession& Session,
	FMWTrackPlacementIntent& Intent)
{
	FTransform Ignored;
	if (Resolve(Ports, Session, Intent, Ignored))
	{
		return;
	}

	Intent.Target = {};
	Intent.SourcePort = INDEX_NONE;
	const FTransform Raw = MakeRawTransform(Ports, Session, Intent);
	double BestDistanceSquared = FMath::Square(EnterDistance * Session.LayoutScale);
	for (int32 Source = 0; Source < 2; ++Source)
	{
		const FVector RawEnd = Raw.TransformPosition(Ports[Source].GetLocation());
		for (const FMWOpenTrackEnd& End : Session.OpenEnds)
		{
			const double DistanceSquared = FVector::DistSquaredXY(RawEnd, End.WorldFrame.GetLocation());
			if (DistanceSquared < BestDistanceSquared)
			{
				BestDistanceSquared = DistanceSquared;
				Intent.Target = End.Id;
				Intent.SourcePort = Source;
			}
		}
	}
}
