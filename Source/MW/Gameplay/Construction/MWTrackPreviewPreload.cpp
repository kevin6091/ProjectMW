#include "Gameplay/Construction/MWTrackPreviewPreload.h"

#include "Engine/StreamableManager.h"
#include "Gameplay/Construction/MWTrackPreview.h"
#include "Gameplay/Construction/MWTrackPresentationSettings.h"
#include "Materials/MaterialInterface.h"
#include "MaterialShared.h"
#include "MWGameplayTags.h"
#include "Player/MWLobbyPlayerController.h"
#include "Player/MWGameplayPlayerController.h"
#include "System/MWAssetManager.h"

void UMWTrackPreviewPreload::PlayerControllerChanged(APlayerController* NewController)
{
	Super::PlayerControllerChanged(NewController);

	if (!NewController || NewController->IsA<AMWGameplayPlayerController>())
	{
		// 월드 이동 중에는 잠시 컨트롤러가 비어도 선택판까지 참조를 넘겨야 한다.
		return;
	}

	if (!NewController->IsA<AMWLobbyPlayerController>())
	{
		ReleaseCandidates();
		return;
	}

	if (!CatalogLoadHandle)
	{
		const FSoftObjectPath Path = UMWAssetManager::Get().FindAssetPath(MWGameplayTags::Asset_Track_RuntimeCatalog);
		if (Path.IsValid())
		{
			CatalogLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Path);
		}
	}
}

bool UMWTrackPreviewPreload::PrepareCandidates(const TArray<FMWTrackOffer>& Offers)
{
	// 후보는 경기 서버에서 추첨한다. 카탈로그 전체 BP를 읽지 않고 실제 전시할 조각만 준비한다.
	TArray<FSoftObjectPath> Paths;
	for (const FMWTrackOffer& Offer : Offers)
	{
		if (Offer.ClaimedByMemberId == INDEX_NONE && !Offer.TrackClass.IsNull())
		{
			Paths.AddUnique(Offer.TrackClass.ToSoftObjectPath());
		}
	}

	// 후보가 있을 때 투명 표면 재질도 BP와 같은 로딩 핸들로 준비한다.
	const auto& CandidateSurface = GetDefault<UMWTrackPresentationSettings>()->CandidateSurfaceMaterial;
	if (!Paths.IsEmpty() && !CandidateSurface.IsNull())
	{
		Paths.AddUnique(CandidateSurface.ToSoftObjectPath());
	}

	if (Paths != CandidatePaths)
	{
		if (CandidateLoadHandle) CandidateLoadHandle->CancelHandle();
		CandidatePaths = MoveTemp(Paths);
		RequestedMaterials.Reset();
		CandidateLoadHandle = CandidatePaths.IsEmpty()
			? nullptr : UAssetManager::GetStreamableManager().RequestAsyncLoad(CandidatePaths);
	}
	if (CandidateLoadHandle && !CandidateLoadHandle->HasLoadCompleted()) return false;

	TSet<UMaterialInterface*> Materials;
	for (const FSoftObjectPath& Path : CandidatePaths)
	{
		UObject* Loaded = Path.ResolveObject();
		if (auto* Material = Cast<UMaterialInterface>(Loaded))
		{
			Materials.Add(Material);
		}
		else
		{
			MWTrackPreview::CollectWireframeMaterials(Cast<UClass>(Loaded), Materials);
		}
	}

	bool bReady = true;
	TArray<UMaterialInterface*> MaterialsToCache;
	for (UMaterialInterface* Material : Materials)
	{
		if (!Material->IsComplete() || Material->IsCompiling())
		{
			bReady = false;
			// 로드 시 시작된 컴파일은 그대로 기다린다. 누락된 셰이더만 후보당 한 번 요청한다.
			if (!Material->IsCompiling() && !RequestedMaterials.Contains(Material))
			{
				MaterialsToCache.Add(Material);
			}
		}
	}
	if (!MaterialsToCache.IsEmpty())
	{
		// CacheShaders는 Uniform Buffer를 재생성한다. 이미 그리는 재질의 캐시된
		// draw command도 함께 갱신해야 해제된 버퍼 참조가 남지 않는다.
		// 엔진의 UMaterialInterface::SubmitRemainingJobsForWorld와 같은 갱신 경로다.
		FMaterialUpdateContext UpdateContext(FMaterialUpdateContext::EOptions::SyncWithRenderingThread);
		for (UMaterialInterface* Material : MaterialsToCache)
		{
			RequestedMaterials.Add(Material);
			UpdateContext.AddMaterialInterface(Material);
			Material->CacheShaders(EMaterialShaderPrecompileMode::Background);
		}
	}
	return bReady;
}

void UMWTrackPreviewPreload::ReleaseCandidates()
{
	if (CandidateLoadHandle)
	{
		CandidateLoadHandle->CancelHandle();
		CandidateLoadHandle.Reset();
	}
	if (CatalogLoadHandle)
	{
		CatalogLoadHandle->CancelHandle();
		CatalogLoadHandle.Reset();
	}
	CandidatePaths.Reset();
	RequestedMaterials.Reset();
}

void UMWTrackPreviewPreload::Deinitialize()
{
	ReleaseCandidates();
	Super::Deinitialize();
}
