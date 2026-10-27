"""Import only explicitly selected, existing original UI sources; never overwrite an asset."""
import hashlib
import os
import json
from pathlib import Path
import unreal as u

ROOT = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir()))
SOURCE_ROOT = Path(os.environ.get('MW_UI_SOURCE_ROOT', 'C:/workspace/Unpack/AssetStudio'))
PROJECT = str(ROOT/'MW.uproject')
assert Path(u.Paths.convert_relative_path_to_full(u.Paths.get_project_file_path())).resolve() == Path(PROJECT).resolve()
DEST = '/Game/UI/Common'
sources = [
    ('T_MW_MenuSign', Path(str(SOURCE_ROOT/'Sprite/T_listMenuOptionSignStandard.png'))),
    ('T_MW_MenuOverlay', Path(str(SOURCE_ROOT/'Sprite/T_listMenuOptionSignOverlay.png'))),
    ('T_MW_MenuDiamond', Path(str(SOURCE_ROOT/'Sprite/T_listMenuOptionSignDiamond.png'))),
    ('T_MW_Arrow', Path(str(SOURCE_ROOT/'Sprite/Arrow Rounded.png'))),
    ('F_NotoSansKR_Bold', Path(str(SOURCE_ROOT/'Font/NotoSansKR-Bold.otf'))),
]
tools = u.AssetToolsHelpers.get_asset_tools()
records = []
for name, source in sources:
    target = DEST + '/' + name
    if u.EditorAssetLibrary.does_asset_exist(target):
        asset = u.load_asset(target)
        existing_sources = [asset.get_editor_property('source_filename')] if isinstance(asset, u.FontFace) else asset.get_editor_property('asset_import_data').extract_filenames()
        assert len(existing_sources) == 1 and Path(existing_sources[0]).resolve() == source.resolve(), 'Unexpected target owner/source: ' + target
        records.append(dict(source=str(source), sha256=hashlib.sha256(source.read_bytes()).hexdigest(), target=target, reused_own_import=True))
        continue
    task = u.AssetImportTask()
    task.set_editor_properties(dict(filename=str(source), destination_path=DEST,
        destination_name=name, automated=True, replace_existing=False, save=True))
    if source.suffix.lower() in ('.otf', '.ttf'):
        factory = u.FontFileImportFactory()
        factory.set_editor_property('batch_create_font_asset', u.BatchCreateFontAsset.YES)
        task.set_editor_property('factory', factory)
    tools.import_asset_tasks([task])
    imported = list(task.get_editor_property('imported_object_paths'))
    if not imported:
        raise RuntimeError('Import returned no assets: ' + str(source))
    asset = u.load_asset(target)
    if isinstance(asset, u.Texture2D):
        asset.set_editor_properties(dict(compression_settings=u.TextureCompressionSettings.TC_EDITOR_ICON,
            mip_gen_settings=u.TextureMipGenSettings.TMGS_NO_MIPMAPS, lod_group=u.TextureGroup.TEXTUREGROUP_UI,
            srgb=True))
        if name in ('T_MW_MenuSign','T_MW_MenuOverlay'):
            asset.set_editor_property('max_texture_size',128)
        assert u.EditorAssetLibrary.save_loaded_asset(asset)
    elif isinstance(asset,u.FontFace):
        asset.set_editor_property('loading_policy',u.FontLoadingPolicy.INLINE)
        assert u.EditorAssetLibrary.save_loaded_asset(asset)
    records.append(dict(source=str(source), sha256=hashlib.sha256(source.read_bytes()).hexdigest(), target=target, imported=imported))
assert u.EditorAssetLibrary.save_asset(DEST + '/F_NotoSansKR_Bold_Font')
(ROOT/'Docs/Engineering/Client/FrontendOriginalAssets.json').write_text(json.dumps(records, indent=2, ensure_ascii=False), encoding='utf-8')
targets=['/Game/UI/Common/WBP_RoadButton','/Game/UI/Frontend/WBP_RoomRow','/Game/UI/Frontend/WBP_FrontendRoot']
if not all(u.EditorAssetLibrary.does_asset_exist(p) for p in targets):
    assert u.MWFrontendAuthoringLibrary.build_new_widgets(), 'Widget authoring failed; inspect log and retain existing assets'
u.log('MW_FRONTEND_ASSETS_BUILT')
