# ProjectMW Environment

Snow, Toybox and Tropic are the official Environment themes. The room theme IDs resolve to the existing `/Game/Levels/L_Dev_Drive` map. The Environment bridge reads the selected ID and loads its Theme DataAsset. `DriveDefault` remains available by ID for development and uses the existing level backdrop. It is hidden from the room selector while the optional Environment catalogue is active.

The base catalogue remains `/Game/Data/DA_AssetData`. It supports the existing development flow when the optional Environment catalogue is missing or invalid. City, Forest, Lunar, Canyon and Volcano remain available in the local restoration projects, along with their existing gameplay tags and diagnostic maps.

## Local content setup

1. Use Unreal Engine 5.8.4 and the matching ProjectMW content. Provision the restored Environment packages locally with appropriate rights. The public branch includes authored code, config and metadata. The visual assets require separate local provision.
2. Build `MWEditor Win64 Development` and run `SetupLocalCatalog.py` inside Unreal Editor with source control disabled. For example, supply `-ExecutePythonScript=<absolute script path> -NoSourceControl` at Editor launch. The script reads the base catalogue, checks for unrelated private entries, and saves only `/Game/EnvironmentRestore/Integration/DA_EnvironmentGameplayCatalog`. Repeating it on the same inputs preserves the catalogue contents and skips the save.
3. Restart the Editor so Asset Manager loads the optional catalogue. The room selector should show Snow, Toybox and Tropic. The legacy diagnostic maps are still available for local restoration work.

`Asset_Dependencies.csv` is the historical eight-theme restoration inventory. It preserves the original path and SHA references. Use the current Cook inventory for the three-theme shipping scope. The generated catalogue SHA depends on the base catalogue version.

## Windows Development packaging

Use the normal project map list. The Asset Manager explicitly cooks the private catalogue and the three official Theme DataAssets and follows their package references. The four existing project maps stay in the normal cook. The Environment diagnostic maps are Editor validation maps. Run a full Cook when switching between the old and new configs so the resulting container represents the selected scope.

The restored materials, meshes, textures and shared dependencies resolve through the Theme DataAssets. Preserve the five shared materials and use the existing private remaps. See [validation](VALIDATION.md) for the current results and the scope of the historical validation summary.
