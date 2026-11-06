import unreal as u
def setup_catalog():
    source=u.load_asset('/Game/Data/DA_AssetData');assert source
    entries=list(source.get_editor_property('entries'))
    ids=[e.get_editor_property('asset_id').export_text() for e in entries]
    drive=next((e for e in entries if 'Asset.MapTheme.DriveDefault' in e.get_editor_property('asset_id').export_text()),None)
    assert drive,'Latest base catalogue must provide DriveDefault map'
    target='/Game/EnvironmentRestore/Integration/DA_EnvironmentGameplayCatalog'
    catalog=u.load_asset(target) or u.EditorAssetLibrary.duplicate_asset('/Game/Data/DA_AssetData',target)
    assert catalog
    for theme in ('City','Tropic','Lunar','Canyon','Forest','Toybox','Volcano','Snow'):
        name=f'Asset.MapTheme.Environment.{theme}'
        assert not any(name in i for i in ids)
        tag=u.GameplayTag();assert tag.import_text(f'(TagName="{name}")')
        e=u.MWAssetEntry();e.set_editor_property('asset_id',tag)
        e.set_editor_property('asset',drive.get_editor_property('asset'))
        e.set_editor_property('labels',drive.get_editor_property('labels'));entries.append(e)
    catalog.set_editor_property('entries',entries)
    assert u.EditorAssetLibrary.save_loaded_asset(catalog)
    result={'preserved_base_entries':len(ids),'total':len(entries),'ids_before':ids,
        'ids_after':[e.get_editor_property('asset_id').export_text() for e in catalog.get_editor_property('entries')],
        'drive_map':str(drive.get_editor_property('asset')),'source':'/Game/Data/DA_AssetData','target':target,'base_catalog_saved':False}

    u.log("Environment private catalogue rebuilt from latest base entries: "+str(result))

setup_catalog()
