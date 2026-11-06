# Restored environment integration

This branch integrates the existing `MWEnvironmentRestore` generator and `MWEnvironmentGameplay` adapter with ProjectMW. It preserves the approved floor-index state transition, initial Y-outer/X-inner registration order, seeded layout behavior, 2000 Unity-unit view/far defaults, and the separated UE fog integration/cutoff meanings. It does not introduce new gameplay, UI, networking, vehicle logic, or a new environment generator.

## What is published

The three project plugins contain authored C++ and module descriptors. `MWEnvironmentAcceptance` is a local validation harness whose behavior requires explicit command-line flags. Runtime environment control remains in the existing Restore/Gameplay plugins. Config changes add only environment bindings/tags, plugin enablement, a private catalogue path, and the environment cook directory.

Files added by this integration contain no recovered meshes, textures, materials, prefab/map packages, Unity binaries, reverse-engineering dumps, credentials, logs containing account data, build products, or ZIP archives. Existing unrelated baseline settings are preserved and require their own distribution review. No third-party asset rights are granted. `Asset_Dependencies.csv` is a path/hash manifest, not a redistribution of the referenced assets. Git LFS patterns do not authorize adding restricted binaries. The repository's existing `/Content/` ignore policy remains unchanged.

**A Git checkout cannot reproduce the complete visual environment by itself.** The existing ProjectMW Content and the separately held, locally authorized restored packages must be provisioned locally. There is no downloader or invented substitute asset set. Missing theme packages prevent runtime environment display even though the native source builds.

## Local setup

1. Use Unreal Engine 5.8.4 and the project prerequisites, including its existing editor plugins. Keep Git work separate from any UVCS workspace. This branch runs no UVCS operation.
2. Obtain the matching local ProjectMW Content and the environment packages listed in the manifest through an authorized local workflow. Keep their binary files out of this public Git branch. Do not overwrite the five protected shared materials; retain the existing 22 private IntegrationMaterials remaps.
3. Build `MWEditor Win64 Development` using Unreal's Build.bat.
4. Generate the private catalogue from the **current** `/Game/Data/DA_AssetData` using `SetupLocalCatalog.py`. For the first editor script launch, override the startup catalogue to the base path: `-ini:Game:[/Script/MW.MWAssetManager]:StartupAssetData=/Game/Data/DA_AssetData.DA_AssetData`. Execute the script with `-ExecutePythonScript=<absolute path to SetupLocalCatalog.py> -NoSourceControl`. The script preserves all current base entries and adds only the eight environment theme IDs, using the current DriveDefault map and labels. It saves only the environment-owned private catalogue.
5. Restart the Editor normally to load the generated private catalogue. Inspect the eight existing `L_EnvironmentRestore_<Theme>` maps, then use the existing lobby/vehicle-selection/travel flow. The adapter reads replicated settings/phase and follows the owning local player's camera; it does not add a replicated seed or change server rules.

The base catalogue, original maps, shared materials, and vehicle packages must not be replaced with stale copies from an older lab. Check dependency paths/hashes against the matching local delivery. The generated catalogue hash depends on the latest base catalogue and is intentionally not a Git asset.

## Validation and remaining differences

See `VALIDATION.md` and `Validation_Summary.json`. Environment-map PIE, actual match-entry PIE, forced input, full race acceptance, cook/stage, and packaged execution are reported separately. The generator remains cosmetic and camera/order dependent; equal seeds do not guarantee identical peer layouts. Fog/visibility are UE approximations, and missing source VFX/shader/animation behavior is not marked restored. Main-branch review/merge is a separate team step; this branch is not merged into main.
