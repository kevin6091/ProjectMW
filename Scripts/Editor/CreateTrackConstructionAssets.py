"""선택 기능 제작에 필요한 새 BP/정의/전용 레벨을 생성한다. PIE나 검증은 실행하지 않는다."""
import json
from pathlib import Path

import unreal

PROJECT = Path(unreal.Paths.project_dir())
SAVE = PROJECT / 'Saved' / 'TrackConstructionAuthoring'
SAVE.mkdir(parents=True, exist_ok=True)
MANIFEST = SAVE / 'created_assets.json'
created = json.loads(MANIFEST.read_text(encoding='utf-8')) if MANIFEST.exists() else []
tools = unreal.AssetToolsHelpers.get_asset_tools()


def remember(path):
    if path not in created:
        created.append(path)
    MANIFEST.write_text(json.dumps(created, ensure_ascii=False, indent=2), encoding='utf-8')


def save(asset):
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError('에셋을 저장하지 못했습니다: ' + asset.get_path_name())


def create(path, cls, factory):
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        if path not in created:
            raise RuntimeError('기존 에셋을 덮어쓰지 않습니다: ' + path)
        return unreal.load_asset(path)
    folder, name = path.rsplit('/', 1)
    unreal.EditorAssetLibrary.make_directory(folder)
    asset = tools.create_asset(name, folder, cls, factory)
    if asset is None:
        raise RuntimeError('에셋 생성 실패: ' + path)
    remember(path)
    return asset


def blueprint(path, native_parent):
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', native_parent)
    bp = create(path, unreal.Blueprint, factory)
    cls = unreal.load_class(None, path + '.' + path.rsplit('/', 1)[1] + '_C')
    if cls is None:
        raise RuntimeError('생성한 Blueprint 클래스를 준비할 수 없습니다: ' + path)
    return bp, cls, unreal.get_default_object(cls)


def tag(name):
    value = unreal.GameplayTag()
    value.import_text('(TagName="' + name + '")')
    return value


def entries_with_definitions(catalog, definitions):
    entries = list(catalog.get_editor_property('entries'))
    for asset_id, definition in definitions:
        existing = [e for e in entries if e.get_editor_property('asset_id').export_text() == tag(asset_id).export_text()]
        if existing:
            continue
        entry = unreal.MWAssetEntry()
        entry.set_editor_property('asset_id', tag(asset_id))
        entry.set_editor_property('asset', definition)
        entries.append(entry)
    catalog.set_editor_property('entries', entries)
    save(catalog)


preview_parent = unreal.load_class(None, '/Script/MW.MWTrackOfferPreview')
definition_class = unreal.load_class(None, '/Script/MW.MWTrackPieceDefinition')
test_mode_parent = unreal.load_class(None, '/Script/MW.MWTrackConstructionTestGameMode')
controller_parent = unreal.load_class(None, '/Script/MW.MWPlayerController')
board_class = unreal.load_class(None, '/Script/MW.MWTrackSelectionBoard')
if any(c is None for c in [preview_parent, definition_class, test_mode_parent, controller_parent, board_class]):
    raise RuntimeError('새 네이티브 클래스가 필요합니다. 에셋 제작용 에디터 모듈을 먼저 반영하세요.')

material = create('/Game/Gameplay/Tracks/Tests/M_TrackSelection_Test', unreal.Material, unreal.MaterialFactoryNew())
if unreal.MaterialEditingLibrary.get_num_material_expressions(material) == 0:
    color = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -250, 0)
    color.set_editor_property('parameter_name', 'Color')
    color.set_editor_property('default_value', unreal.LinearColor(0.38, 0.48, 0.65, 1))
    unreal.MaterialEditingLibrary.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)
    roughness = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant, -250, 180)
    roughness.set_editor_property('r', 0.7)
    unreal.MaterialEditingLibrary.connect_material_property(roughness, '', unreal.MaterialProperty.MP_ROUGHNESS)
save(material)

cube = unreal.load_asset('/Engine/BasicShapes/Cube')
definitions = []
for suffix, shape, label in [('Straight', 'STRAIGHT', '직선'), ('Curve', 'CURVE', '커브'), ('Ramp', 'RAMP', '경사')]:
    bp_path = '/Game/Gameplay/Tracks/Tests/BP_Track_' + suffix + '_Test'
    bp, bp_class, defaults = blueprint(bp_path, preview_parent)
    defaults.set_editor_property('preview_shape', getattr(unreal.MWTrackPreviewShape, shape))
    defaults.set_editor_property('source_mesh', cube)
    defaults.set_editor_property('preview_material', material)
    save(bp)

    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', definition_class)
    definition = create('/Game/Gameplay/Tracks/Data/DA_Track_' + suffix + '_Test', definition_class, factory)
    definition.set_editor_property('piece_id', 'Track.' + suffix)
    definition.set_editor_property('display_name', label)
    definition.set_editor_property('mesh', cube)
    definition.set_editor_property('preview_actor_class', bp_class)
    save(definition)
    definitions.append(('Asset.Track.' + suffix, definition))

# 기존 카탈로그의 입력·차량·맵 항목은 유지하고 트랙 항목만 추가한다.
catalog = unreal.load_asset('/Game/Data/DA_AssetData')
entries_with_definitions(catalog, definitions)
test_catalog_path = '/Game/Data/Catalogs/DA_TrackConstructionTestAssets'
if not unreal.EditorAssetLibrary.does_asset_exist(test_catalog_path):
    unreal.EditorAssetLibrary.make_directory('/Game/Data/Catalogs')
    test_catalog = unreal.EditorAssetLibrary.duplicate_asset('/Game/Data/DA_AssetData', test_catalog_path)
    if test_catalog is None:
        raise RuntimeError('테스트 카탈로그 생성 실패')
    remember(test_catalog_path)
elif test_catalog_path in created:
    test_catalog = unreal.load_asset(test_catalog_path)
else:
    raise RuntimeError('기존 테스트 카탈로그를 덮어쓰지 않습니다.')
test_catalog.set_editor_property('entries', list(catalog.get_editor_property('entries')))
save(test_catalog)

folder = '/Game/Developers/user/Construction'
pc_bp, pc_class, pc_defaults = blueprint(folder + '/BP_TrackConstructionTestController', controller_parent)
pc_defaults.set_editor_property('create_lobby_screen', False)
save(pc_bp)
gm_bp, gm_class, gm_defaults = blueprint(folder + '/BP_TrackConstructionTestGameMode', test_mode_parent)
gm_defaults.set_editor_property('player_controller_class', pc_class)
gm_defaults.set_editor_property('test_catalog', test_catalog)
save(gm_bp)

map_path = folder + '/L_TrackConstruction_Test'
if unreal.EditorAssetLibrary.does_asset_exist(map_path):
    if map_path not in created:
        raise RuntimeError('기존 레벨을 덮어쓰지 않습니다: ' + map_path)
    world = unreal.EditorLoadingAndSavingUtils.load_map(map_path)
else:
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    remember(map_path)
    ws = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.WorldSettings)[0]
    ws.set_editor_property('default_game_mode', gm_class)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    board = actors.spawn_actor_from_class(board_class, unreal.Vector(0, 0, 0))
    board.set_actor_label('Track Selection Board')
    floor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, -50))
    floor.set_actor_label('Selection Floor')
    floor.get_component_by_class(unreal.StaticMeshComponent).set_static_mesh(cube)
    floor.set_actor_scale3d(unreal.Vector(32, 44, 0.5))
    light = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 1500), unreal.Rotator(-55, -30, 0))
    light.get_component_by_class(unreal.DirectionalLightComponent).set_editor_property('intensity', 5.0)
    actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 1000))
    actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(1300, -1800, 400))

if not unreal.EditorLoadingAndSavingUtils.save_map(world, map_path):
    raise RuntimeError('테스트 레벨 저장 실패')
unreal.log('Track construction assets created. No PIE, automation, cook or gameplay validation was run.')
