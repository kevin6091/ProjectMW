#include "Gameplay/Construction/MWTrackPreviewPreload.h"

#include "Engine/StreamableManager.h"
#include "Gameplay/Construction/MWTrackPreview.h"
#include "Materials/MaterialInterface.h"
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

	if (Paths != CandidatePaths)
	{
		if (CandidateLoadHandle) CandidateLoadHandle->CancelHandle();
		CandidatePaths = MoveTemp(Paths);
		CandidateLoadHandle = CandidatePaths.IsEmpty()
			? nullptr : UAssetManager::GetStreamableManager().RequestAsyncLoad(CandidatePaths);
	}
	if (CandidateLoadHandle && !CandidateLoadHandle->HasLoadCompleted()) return false;

	TSet<UMaterialInterface*> Materials;
	for (const FSoftObjectPath& Path : CandidatePaths)
	{
		MWTrackPreview::CollectWireframeMaterials(Cast<UClass>(Path.ResolveObject()), Materials);
	}

	// 파일을 읽었더라도 PIE에서는 셰이더가 준비될 때까지 기본 체크 재질이 보일 수 있다.
	for (UMaterialInterface* Material : Materials)
	{
		if (!Material->IsComplete())
		{
			Material->CacheShaders(EMaterialShaderPrecompileMode::Background);
		}
	}
	return true;
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
}

void UMWTrackPreviewPreload::Deinitialize()
{
	ReleaseCandidates();
	Super::Deinitialize();
}
