#include "Gameplay/Construction/MWTrackPreviewPreload.h"

#include "Engine/StreamableManager.h"
#include "Data/MWTrackDefinitionData.h"
#include "Gameplay/Construction/MWTrackPreview.h"
#include "Materials/MaterialInterface.h"
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

	if (LoadHandle)
	{
		return;
	}

	const FGameplayTag Tracks = FGameplayTag::RequestGameplayTag(TEXT("Asset.Track"));
	const FGameplayTag Course = FGameplayTag::RequestGameplayTag(TEXT("Asset.Track.Course"), false);
	const UMWAssetManager& Assets = UMWAssetManager::Get();
	TArray<FSoftObjectPath> Paths;
	for (const FGameplayTag Id : Assets.GetAssetIdsUnderRoot(Tracks))
	{
		// 출발선·종료 캡은 코스 전용이다. 선택 조각 정의가 참조하는 BP·메시·재질만 함께 읽는다.
		if (!Id.MatchesTag(Course))
		{
			Paths.AddUnique(Assets.FindAssetPath(Id));
		}
	}

	if (!Paths.IsEmpty())
	{
		LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths,
			FStreamableDelegate::CreateUObject(this, &ThisClass::PrepareMaterials, Paths));
	}
}

void UMWTrackPreviewPreload::PrepareMaterials(TArray<FSoftObjectPath> Paths)
{
	TSet<UMaterialInterface*> Materials;
	for (const FSoftObjectPath& Path : Paths)
	{
		if (const auto* Definition = Cast<UMWTrackDefinitionData>(Path.ResolveObject()))
		{
			MWTrackPreview::CollectWireframeMaterials(Definition->GetTrackActorClass(), Materials);
		}
	}

	// 파일을 읽었더라도 PIE에서는 셰이더가 준비될 때까지 기본 체크 재질이 보일 수 있다.
	for (UMaterialInterface* Material : Materials)
	{
		if (!Material->IsComplete())
		{
			Material->CacheShaders(EMaterialShaderPrecompileMode::Background);
		}
	}
}

void UMWTrackPreviewPreload::ReleaseCandidates()
{
	if (LoadHandle)
	{
		LoadHandle->CancelHandle();
		LoadHandle.Reset();
	}
}

void UMWTrackPreviewPreload::Deinitialize()
{
	ReleaseCandidates();
	Super::Deinitialize();
}
