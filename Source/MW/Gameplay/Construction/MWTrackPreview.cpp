#include "Gameplay/Construction/MWTrackPreview.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Actor.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	// 표시 전용 액터에만 붙는 내부 표식이다. 에셋이나 레벨에서 설정할 필요가 없다.
	const FName DisplayOnlyTag(TEXT("MWTrackDisplayOnly"));
	const FName WireframesName(TEXT("Wireframes"));

	bool IsWireframePart(const USceneComponent& Component)
	{
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

bool MWTrackPreview::CollectVisibleMaterials(const AActor* Track, TArray<UMaterialInterface*>& OutMaterials)
{
    if (!IsValid(Track) || Track->IsHidden()) return false;
    bool bHasSurface = false;
    TInlineComponentArray<UStaticMeshComponent*> Meshes(Track);
    for (const UStaticMeshComponent* Mesh : Meshes)
    {
        if (!Mesh->IsVisible() || !Mesh->GetStaticMesh()) continue;
        if (!Mesh->IsRegistered() || !Mesh->IsRenderStateCreated() || Mesh->GetNumMaterials() == 0) return false;
        for (UMaterialInterface* Material : Mesh->GetMaterials())
        {
            if (!Material) return false;
            OutMaterials.AddUnique(Material);
        }
        bHasSurface = true;
    }
    return bHasSurface;
}

void MWTrackPreview::CollectWireframeMaterials(UClass* TrackClass, TSet<UMaterialInterface*>& OutMaterials)
{
	auto* GeneratedClass = Cast<UBlueprintGeneratedClass>(TrackClass);
	for (auto* Class = GeneratedClass; Class; Class = Cast<UBlueprintGeneratedClass>(Class->GetSuperClass()))
	{
		const USimpleConstructionScript* Script = Class->SimpleConstructionScript;
		if (!Script)
		{
			continue;
		}

		TArray<USCS_Node*> Pending;
		for (USCS_Node* Node : Script->GetAllNodes())
		{
			if (Node && (Node->GetVariableName() == WireframesName || Node->ParentComponentOrVariableName == WireframesName))
			{
				Pending.Add(Node);
			}
		}
		while (!Pending.IsEmpty())
		{
			USCS_Node* Node = Pending.Pop(EAllowShrinking::No);
			Pending.Append(Node->GetChildNodes());
			// 상속한 노드라도 선택한 트랙 BP의 재정의가 반영된 템플릿에서 재질을 읽는다.
			const auto* Mesh = Cast<UStaticMeshComponent>(Node->GetActualComponentTemplate(GeneratedClass));
			if (!Mesh || !Mesh->GetStaticMesh())
			{
				continue;
			}
			for (UMaterialInterface* Material : Mesh->GetMaterials())
			{
				if (Material)
				{
					OutMaterials.Add(Material);
					// 자체 셰이더가 없는 인스턴스는 원본 머티리얼의 셰이더를 사용한다.
					if (UMaterial* Base = Material->GetMaterial())
					{
						OutMaterials.Add(Base);
					}
				}
			}
		}
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

USceneComponent* MWTrackPreview::FindWireframeGroup(const AActor& Track)
{
	TInlineComponentArray<USceneComponent*> Components(&Track);
	USceneComponent* const* Group = Components.FindByPredicate([](const USceneComponent* Component)
	{
		return Component->GetFName() == WireframesName;
	});
	return Group ? *Group : nullptr;
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
