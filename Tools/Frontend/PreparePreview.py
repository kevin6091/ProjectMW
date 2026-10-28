import unreal as u
from pathlib import Path
# FontFileImportFactory registers the matching Font but AssetImportTask only saves the FontFace.
font_path = '/Game/UI/Common/F_NotoSansKR_Bold_Font'
if not u.EditorAssetLibrary.does_asset_exist(font_path):
    source = Path('C:/workspace/Unpack/AssetStudio/Font/NotoSansKR-Bold.otf')
    face = u.load_asset('/Game/UI/Common/F_NotoSansKR_Bold')
    assert Path(face.get_editor_property('source_filename')).resolve() == source.resolve()
    factory = u.FontFileImportFactory()
    factory.set_editor_property('batch_create_font_asset', u.BatchCreateFontAsset.YES)
    task = u.AssetImportTask()
    task.set_editor_properties(dict(filename=str(source), destination_path='/Game/UI/Common',
        destination_name='F_NotoSansKR_Bold', automated=True, replace_existing=True, save=True, factory=factory))
    u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    assert u.EditorAssetLibrary.save_asset(font_path)
assert u.MWFrontendAuthoringLibrary.build_test_preview_widget()
for path in ['/Game/UI/Common/WBP_RoadButton', '/Game/UI/Frontend/WBP_RoomRow', '/Game/UI/Frontend/WBP_FrontendRoot', '/Game/UI/Lobby/WBP_LoginTest']:
    bp = u.load_asset(path)
    assert bp, path
    u.BlueprintEditorLibrary.compile_blueprint(bp)
    assert bp.get_editor_property('status') == u.BlueprintStatus.BS_UP_TO_DATE, path
    u.log('MW_FRONTEND_COMPILE ' + path + ' ' + str(bp.get_editor_property('status')))
level = u.get_editor_subsystem(u.LevelEditorSubsystem)
if not u.EditorAssetLibrary.does_asset_exist('/Game/Levels/L_Frontend'):
    assert level.new_level('/Game/Levels/L_Frontend')
    world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property('default_game_mode', u.MWFrontendGameMode)
    assert level.save_current_level()
u.log('MW_FRONTEND_TEST_PREPARED')
