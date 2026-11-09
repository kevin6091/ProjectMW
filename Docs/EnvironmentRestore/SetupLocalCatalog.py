"""Rebuild the optional local catalogue from the current ProjectMW catalogue."""

import unreal
import re


BASE = '/Game/Data/DA_AssetData'
TARGET = '/Game/EnvironmentRestore/Integration/DA_EnvironmentGameplayCatalog'
DRIVE = 'Asset.MapTheme.DriveDefault'
ENVIRONMENT_PREFIX = 'Asset.MapTheme.Environment.'
THEMES = ('Snow', 'Toybox', 'Tropic')


def asset_id(entry):
    text = entry.get_editor_property('asset_id').export_text()
    match = re.fullmatch(r'\(TagName="([^"]+)"\)', text)
    if not match:
        raise RuntimeError(f'Invalid asset ID: {text}')
    return match.group(1)


def setup_catalog():
    source = unreal.load_asset(BASE)
    if not source:
        raise RuntimeError(f'Missing base catalogue: {BASE}')
    base_entries = list(source.get_editor_property('entries'))
    base_ids = [asset_id(entry) for entry in base_entries]
    drive = next((entry for entry in base_entries if asset_id(entry) == DRIVE), None)
    if drive is None or not isinstance(drive.get_editor_property('asset'), unreal.World):
        raise RuntimeError('The base catalogue must supply a valid DriveDefault map.')

    catalogue = unreal.load_asset(TARGET)
    previous = list(catalogue.get_editor_property('entries')) if catalogue else []
    unexpected = [asset_id(entry) for entry in previous
                  if asset_id(entry) not in base_ids and not asset_id(entry).startswith(ENVIRONMENT_PREFIX)]
    if unexpected:
        raise RuntimeError(f'Private catalogue has unrelated entries absent from the base: {unexpected}')

    entries = [entry for entry in base_entries if not asset_id(entry).startswith(ENVIRONMENT_PREFIX)]
    theme_ids = [ENVIRONMENT_PREFIX + name for name in THEMES]
    for theme_id in theme_ids:
        tag = unreal.GameplayTag()
        if not tag.import_text(f'(TagName="{theme_id}")'):
            raise RuntimeError(f'Invalid configured gameplay tag: {theme_id}')
        entry = unreal.MWAssetEntry()
        entry.set_editor_property('asset_id', tag)
        entry.set_editor_property('asset', drive.get_editor_property('asset'))
        entry.set_editor_property('labels', drive.get_editor_property('labels'))
        entries.append(entry)

    expected_ids = [asset_id(entry) for entry in entries]
    if len(set(expected_ids)) != len(expected_ids):
        raise RuntimeError('Duplicate catalogue IDs detected.')
    if not catalogue:
        catalogue = unreal.EditorAssetLibrary.duplicate_asset(BASE, TARGET)
    if not catalogue:
        raise RuntimeError(f'Could not create {TARGET}')
    if [entry.export_text() for entry in previous] != [entry.export_text() for entry in entries]:
        catalogue.set_editor_property('entries', entries)
        if not unreal.EditorAssetLibrary.save_loaded_asset(catalogue):
            raise RuntimeError(f'Could not save {TARGET}')

    actual_ids = [asset_id(entry) for entry in catalogue.get_editor_property('entries')]
    if actual_ids != expected_ids:
        raise RuntimeError('Saved catalogue differs from the expected entries.')
    result = {'base_ids': base_ids, 'ids': actual_ids, 'official_theme_ids': theme_ids,
              'previous_ids': [asset_id(entry) for entry in previous],
              'changed': [entry.export_text() for entry in previous] != [entry.export_text() for entry in entries],
              'drive_map': drive.get_editor_property('asset').get_path_name()}
    unreal.log(f'Environment catalogue: {result}')
    return result


if __name__ == '__main__':
    setup_catalog()
