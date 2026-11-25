#include "Gameplay/Construction/MWTrackSnap.h"

#include "Algo/AnyOf.h"
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

	bool ReadLocalFrame(const AActor& Track, FName Name, FTransform& OutFrame)
	{
		if (!Track.GetActorScale3D().Equals(FVector::OneVector, KINDA_SMALL_NUMBER))
		{
			return false;
		}
		TInlineComponentArray<USceneComponent*> Components(&Track);
		const USceneComponent* const* Point = Components.FindByPredicate([Name](const USceneComponent* Component)
		{
			return Component->GetFName() == Name;
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
		// 중첩 BP의 스케일은 위치에만 반영하고 연결 프레임으로 넘기지 않는다.
		OutFrame = FTransform(Local.GetRotation(), Local.GetLocation());
		return true;
	}

	bool ReadPort(const AActor& Track, FName Name, FTransform& OutPort)
	{
		if (!ReadLocalFrame(Track, Name, OutPort))
		{
			return false;
		}
		// BP의 +Y는 도로 안쪽이다. 계산에서는 +X가 바깥을 향하도록 바꾼다.
		const FQuat PortBasis(FVector::UpVector, -UE_HALF_PI);
		OutPort.SetRotation((OutPort.GetRotation() * PortBasis).GetNormalized());
		return true;
	}
}

bool MWTrackSnap::ReadFirstPort(const AActor& Track, FTransform& OutPort)
{
	return ReadPort(Track, TEXT("ConnectionPoint1"), OutPort);
}

bool MWTrackSnap::ReadPorts(const AActor& Track, FMWTrackPortFrames& OutPorts)
{
	return ReadFirstPort(Track, OutPorts[0]) && ReadPort(Track, TEXT("ConnectionPoint2"), OutPorts[1])
		&& !OutPorts[0].GetLocation().Equals(OutPorts[1].GetLocation(), KINDA_SMALL_NUMBER);
}

bool MWTrackSnap::ReadStartGrid(const AActor& Track, FTransform& OutFrame)
{
	return ReadLocalFrame(Track, TEXT("StartGrid"), OutFrame);
}

FTransform MWTrackSnap::AlignPorts(const FTransform& Source, const FTransform& Target)
{
	// 두 바깥 방향을 마주 보게 하고 도로 위쪽은 유지한다. UE 합성은 왼쪽부터 적용한다.
	return Source.Inverse() * ReverseFrame(Target);
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
	if (Intent.CursorLocation.ContainsNaN())
	{
		return false;
	}
	const double RadiusSquared = FMath::Square(BuildRadius * Session.LayoutScale);
	if (FVector::DistSquared(Intent.CursorLocation, Session.BuildOrigin) <= RadiusSquared)
	{
		return true;
	}

	// 코스가 길어지면 시작점에서 멀어져도 마지막 연결점 주변에서 설치할 수 있다.
	return Algo::AnyOf(Session.OpenEnds, [&](const FMWOpenTrackEnd& End)
	{
		return FVector::DistSquared(Intent.CursorLocation, End.WorldFrame.GetLocation()) <= RadiusSquared;
	});
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
