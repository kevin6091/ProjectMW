#include "MWEnvironmentPreload.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "MWEnvironmentRestore.h"

void UMWEnvironmentPreload::SelectTheme(TSoftObjectPtr<UMWEnvironmentTheme> Theme, bool bTraveling)
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
    bDepartureStarted = bTraveling;
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
    bDepartureStarted = false;
}

void UMWEnvironmentPreload::Deinitialize()
{
    ClearSelection();
    Super::Deinitialize();
}
