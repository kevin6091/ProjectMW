"""선택 기능 제작에 필요한 새 BP/정의/전용 레벨을 생성한다. PIE나 검증은 실행하지 않는다."""
import json
from pathlib import Path

import unreal

# Unreal Editor의 Python 환경에서 실행하는 제작 스크립트다. 게임 런타임에서는 사용하지 않는다.
# 기록된 경로는 재실행 시 이 스크립트가 만든 에셋을 구분하는 데 사용한다.
PROJECT = Path(unreal.Paths.project_dir())
SAVE = PROJECT / 'Saved' / 'TrackConstructionAuthoring'
SAVE.mkdir(parents=True, exist_ok=True)
MANIFEST = SAVE / 'created_assets.json'
created = json.loads(MANIFEST.read_text(encoding='utf-8')) if MANIFEST.exists() else []
tools = unreal.AssetToolsHelpers.get_asset_tools()


def remember(path):
    # 에셋을 만들 때마다 기록해 중간에 중단되어도 이미 만든 항목을 다음 실행에서 재사용한다.
    if path not in created:
        created.append(path)
    MANIFEST.write_text(json.dumps(created, ensure_ascii=False, indent=2), encoding='utf-8')


def save(asset):
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError('에셋을 저장하지 못했습니다: ' + asset.get_path_name())


def create(path, cls, factory):
    # 같은 경로가 있어도 제작 기록에 없는 에셋은 수정하지 않는다.
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
    # 네이티브 클래스를 부모로 BP를 만들고, 생성 클래스와 기본 객체(CDO)를 함께 돌려준다.
    # 아래 설정은 인스턴스가 아니라 BP의 기본값에 적용한다.
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', native_parent)
    bp = create(path, unreal.Blueprint, factory)
    cls = unreal.load_class(None, path + '.' + path.rsplit('/', 1)[1] + '_C')
    if cls is None:
        raise RuntimeError('생성한 Blueprint 클래스를 준비할 수 없습니다: ' + path)
    return bp, cls, unreal.get_default_object(cls)


def tag(name):
    # 카탈로그의 AssetId에 넣을 GameplayTag 구조체를 만든다.
    value = unreal.GameplayTag()
    value.import_text('(TagName="' + name + '")')
    return value


def entries_with_definitions(catalog, definitions):
    # 기존 항목과 같은 태그가 있으면 그대로 두고, 없는 트랙 정의만 추가한다.
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


# 새 C++ 클래스가 에디터 모듈에 있어야 BP 부모와 DataAsset 타입을 지정할 수 있다.
preview_parent = unreal.load_class(None, '/Script/MW.MWTrackOfferPreview')
definition_class = unreal.load_class(None, '/Script/MW.MWTrackPieceDefinition')
test_mode_parent = unreal.load_class(None, '/Script/MW.MWTrackConstructionTestGameMode')
controller_parent = unreal.load_class(None, '/Script/MW.MWPlayerController')
board_class = unreal.load_class(None, '/Script/MW.MWTrackSelectionBoard')
if any(c is None for c in [preview_parent, definition_class, test_mode_parent, controller_parent, board_class]):
    raise RuntimeError('새 네이티브 클래스가 필요합니다. 에셋 제작용 에디터 모듈을 먼저 반영하세요.')

# Preview의 동적 머티리얼 인스턴스가 갱신할 Color 파라미터를 BaseColor에 연결한다.
# 재실행 시 표현식이 이미 있는 머티리얼에는 같은 노드를 중복 생성하지 않는다.
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
# 세 BP는 같은 네이티브 Preview를 상속하고 형상 enum만 다르게 설정한다.
# 종류별 DataAsset에는 표시 이름·메시·생성할 BP 클래스를 연결한다.
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
# 테스트 모드에서도 기존 선택 입력 등 전체 카탈로그 항목을 사용할 수 있게 복제한다.
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

# 전용 Controller는 로비 화면 생성을 끄고, GameMode는 이 Controller와 테스트 카탈로그를 쓴다.
folder = '/Game/Developers/user/Construction'
pc_bp, pc_class, pc_defaults = blueprint(folder + '/BP_TrackConstructionTestController', controller_parent)
pc_defaults.set_editor_property('create_lobby_screen', False)
save(pc_bp)
gm_bp, gm_class, gm_defaults = blueprint(folder + '/BP_TrackConstructionTestGameMode', test_mode_parent)
gm_defaults.set_editor_property('player_controller_class', pc_class)
gm_defaults.set_editor_property('test_catalog', test_catalog)
save(gm_bp)

# 레벨 배치는 최초 생성 시에만 수행한다. 재실행 시 기존 테스트 레벨을 열어 저장하므로
# 사용자가 이후 옮긴 Actor의 위치를 초기 배치로 되돌리지 않는다.
map_path = folder + '/L_TrackConstruction_Test'
if unreal.EditorAssetLibrary.does_asset_exist(map_path):
    if map_path not in created:
        raise RuntimeError('기존 레벨을 덮어쓰지 않습니다: ' + map_path)
    world = unreal.EditorLoadingAndSavingUtils.load_map(map_path)
else:
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    remember(map_path)
    # WorldSettings의 GameMode override로 일반 로비와 구분되는 제작용 진입점을 연결한다.
    ws = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.WorldSettings)[0]
    ws.set_editor_property('default_game_mode', gm_class)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    # 후보와 카메라는 Board의 네이티브 코드가 구성한다. 레벨에는 기준 Actor만 배치한다.
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

# 에셋과 레벨 저장으로 제작을 마친다. PIE나 자동화 테스트를 시작하는 코드는 호출하지 않는다.
if not unreal.EditorLoadingAndSavingUtils.save_map(world, map_path):
    raise RuntimeError('테스트 레벨 저장 실패')
unreal.log('Track construction assets created. No PIE, automation, cook or gameplay validation was run.')
