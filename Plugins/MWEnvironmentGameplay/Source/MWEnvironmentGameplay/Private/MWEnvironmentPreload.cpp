#include "MWEnvironmentPreload.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "MWEnvironmentRestore.h"

void UMWEnvironmentPreload::SelectTheme(FName ThemeId, FName DestinationPackage,
                                       TSoftObjectPtr<UMWEnvironmentTheme> Theme, bool bTraveling)
{
    if (SelectedTheme != Theme)
    {
        ClearSelection();
        SelectedTheme = Theme;
        if (!Theme.IsNull())
        {
            LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
                Theme.ToSoftObjectPath(), FStreamableDelegate::CreateUObject(this, &ThisClass::WarmEntryTextures, Theme));
        }
    }
    else if (bTraveling && !bDepartureStarted && Theme.IsValid())
    {
        // 긴 로비 대기 중 밉 유지 시간이 끝난 경우, 실제 출발 때 한 번 더 요청한다.
        WarmEntryTextures(Theme);
    }
    SelectedThemeId = ThemeId;
    SelectedDestinationPackage = DestinationPackage;
    bDepartureStarted = bTraveling;
}

FName UMWEnvironmentPreload::FindLoadedArrivalTheme(const UWorld& World) const
{
    // 로비의 마지막 선택이 가리킨 목적지에서 이미 로드된 테마만 초기 화면에 쓴다.
    const FName WorldPackage(*UWorld::RemovePIEPrefix(World.GetOutermost()->GetName()));
    return SelectedTheme.IsValid() && SelectedDestinationPackage == WorldPackage ? SelectedThemeId : NAME_None;
}

void UMWEnvironmentPreload::WarmEntryTextures(TSoftObjectPtr<UMWEnvironmentTheme> Theme)
{
    const UMWEnvironmentTheme* Loaded = Theme.Get();
    if (!Loaded || Theme != SelectedTheme || EntryTextureSeconds <= 0.f)
    {
        return;
    }

    // 장식 전체의 고해상도 밉을 고정하면 예산을 잠식하므로 첫 화면을 채우는 재질만 대상으로 한다.
    const auto RequestMaterialMips = [this](const UMaterialInterface* Material)
    {
        if (!Material)
        {
            return;
        }
        TArray<UTexture*> Textures;
        Material->GetUsedTextures(Textures, GetCurrentMaterialQualityLevelChecked());
        for (UTexture* Texture : Textures)
        {
            if (UTexture2D* Surface = Cast<UTexture2D>(Texture); Surface && Surface->IsStreamable())
            {
                Surface->SetForceMipLevelsToBeResident(EntryTextureSeconds);
            }
        }
    };
    RequestMaterialMips(Loaded->Sky);
    for (const UMaterialInterface* Floor : Loaded->Floors)
    {
        RequestMaterialMips(Floor);
    }
}

void UMWEnvironmentPreload::ClearSelection()
{
    if (LoadHandle)
    {
        LoadHandle->CancelHandle();
        LoadHandle.Reset();
    }
    SelectedTheme.Reset();
    SelectedThemeId = NAME_None;
    SelectedDestinationPackage = NAME_None;
    bDepartureStarted = false;
}

void UMWEnvironmentPreload::Deinitialize()
{
    ClearSelection();
    Super::Deinitialize();
}
