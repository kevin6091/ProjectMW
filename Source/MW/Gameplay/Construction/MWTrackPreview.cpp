#include "Gameplay/Construction/MWTrackPreview.h"

#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	// 표시 전용 액터에만 붙는 내부 표식이다. 에셋이나 레벨에서 설정할 필요가 없다.
	const FName DisplayOnlyTag(TEXT("MWTrackDisplayOnly"));

	bool IsWireframePart(const USceneComponent& Component)
	{
		static const FName WireframesName(TEXT("Wireframes"));
		for (const USceneComponent* Parent = &Component; Parent; Parent = Parent->GetAttachParent())
		{
			if (Parent->GetFName() == WireframesName)
			{
				return true;
			}
		}
		return false;
	}
}

FBox MWTrackPreview::ShowWireframes(AActor& Track)
{
	Track.Tags.AddUnique(DisplayOnlyTag);
	FBox Bounds(ForceInit);
	TInlineComponentArray<UPrimitiveComponent*> Parts(&Track);
	for (UPrimitiveComponent* Part : Parts)
	{
		auto* Mesh = Cast<UStaticMeshComponent>(Part);
		const bool bShow = Mesh && Mesh->GetStaticMesh() && IsWireframePart(*Mesh);
		Part->SetVisibility(bShow);
		Part->SetHiddenInGame(!bShow);
		Part->SetSimulatePhysics(false);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (bShow)
		{
			Bounds += Mesh->CalcBounds(Mesh->GetComponentTransform()).GetBox();
		}
	}
	return Bounds;
}

bool MWTrackPreview::IsDisplayOnly(const AActor& Track)
{
	return Track.ActorHasTag(DisplayOnlyTag);
}

void MWTrackPreview::HideWireframes(AActor& Track)
{
	TInlineComponentArray<UPrimitiveComponent*> Parts(&Track);
	for (UPrimitiveComponent* Part : Parts)
	{
		if (IsWireframePart(*Part))
		{
			Part->SetHiddenInGame(true);
		}
	}
}

void MWTrackPreview::SetWireframeColor(AActor& Track, const FLinearColor& Color)
{
	static const FName ColorParameter(TEXT("BaseColor"));
	TInlineComponentArray<UStaticMeshComponent*> Meshes(&Track);
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		if (Mesh->IsVisible() && Mesh->GetStaticMesh())
		{
			// BP의 기존 와이어프레임 재질을 부모로 두고 미리보기의 상태 색만 바꾼다.
			if (auto* Material = Mesh->CreateDynamicMaterialInstance(0))
			{
				Material->SetVectorParameterValue(ColorParameter, Color);
			}
		}
	}
}
