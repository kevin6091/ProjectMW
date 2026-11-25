#include "Gameplay/Construction/MWTrackPreview.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Actor.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"

// 표시 전용 액터에만 붙는 내부 표식이다. 에셋이나 레벨에서 설정할 필요가 없다.
const FName MWTrackPreview::DisplayOnlyTag(TEXT("MWTrackDisplayOnly"));
const FName MWTrackPreview::WireframesName(TEXT("Wireframes"));

bool MWTrackPreview::IsWireframePart(const USceneComponent& Component)
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

void MWTrackPreview::ShowCandidateSurface(UStaticMeshComponent& Mesh, UMaterialInterface& PreviewMaterial)
{
	static const FName TextureParameter(TEXT("BaseColorTexture"));
	static const FName TintParameter(TEXT("BaseColor"));
	static const FName UseAlphaParameter(TEXT("UseBaseColorAlpha"));

	TArray<UMaterialInstanceDynamic*, TInlineAllocator<4>> SurfaceSlots;
	for (UMaterialInterface* Original : Mesh.GetMaterials())
	{
		if (!Original)
		{
			return;
		}
		UTexture* Texture = nullptr;
		FLinearColor Tint = FLinearColor::White;
		const bool bHasTint = Original->GetVectorParameterValue(TintParameter, Tint);
		if (!Original->GetTextureParameterValue(TextureParameter, Texture) && !bHasTint)
		{
			// 로고·데칼처럼 파라미터 없이 텍스처 하나만 읽는 재질도 원본 그림을 표시한다.
			TArray<UTexture*> UsedTextures;
			Original->GetUsedTextures(UsedTextures);
			if (UsedTextures.Num() != 1)
			{
				return;
			}
			Texture = UsedTextures[0];
		}
		auto* Surface = UMaterialInstanceDynamic::Create(&PreviewMaterial, &Mesh);
		if (Texture)
		{
			Surface->SetTextureParameterValue(TextureParameter, Texture);
		}
		Surface->SetVectorParameterValue(TintParameter, Tint);
		Surface->SetScalarParameterValue(UseAlphaParameter, Original->IsMasked() ? 1.f : 0.f);
		SurfaceSlots.Add(Surface);
	}

	// 모든 슬롯을 변환한 메시만 켜서 불투명한 원본 슬롯이 와이어프레임을 가리지 않게 한다.
	if (SurfaceSlots.IsEmpty())
	{
		return;
	}
	for (int32 Slot = 0; Slot < SurfaceSlots.Num(); ++Slot)
	{
		Mesh.SetMaterial(Slot, SurfaceSlots[Slot]);
	}
	Mesh.SetCastShadow(false);
	Mesh.SetVisibility(true);
	Mesh.SetHiddenInGame(false);
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

FBox MWTrackPreview::ShowSelectionPreview(AActor& Track, UMaterialInterface* SurfaceMaterial)
{
	TInlineComponentArray<UStaticMeshComponent*> Meshes(&Track);
	// 공통 미리보기 초기화가 숨기기 전에 BP가 원래 켜 둔 표면만 기억한다.
	Meshes.RemoveAll([](const UStaticMeshComponent* Mesh)
	{
		return !Mesh->IsVisible() || Mesh->bHiddenInGame || !Mesh->GetStaticMesh() || IsWireframePart(*Mesh);
	});
	FBox Bounds = ShowWireframes(Track);
	if (SurfaceMaterial)
	{
		for (UStaticMeshComponent* Mesh : Meshes)
		{
			ShowCandidateSurface(*Mesh, *SurfaceMaterial);
			if (Mesh->IsVisible())
			{
				Bounds += Mesh->CalcBounds(Mesh->GetComponentTransform()).GetBox();
			}
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
		if (Mesh->IsVisible() && Mesh->GetStaticMesh() && IsWireframePart(*Mesh))
		{
			// BP의 기존 와이어프레임 재질을 부모로 두고 미리보기의 상태 색만 바꾼다.
			auto* Material = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
			if (!Material) Material = Mesh->CreateDynamicMaterialInstance(0);
			if (Material)
			{
				Material->SetVectorParameterValue(ColorParameter, Color);
			}
		}
	}
}
