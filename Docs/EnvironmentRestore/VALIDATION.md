# Validation scope

Base main: `035910bbc331986804ed8643350aa48da40430c0`. UE5.8.4 / CL58902052. The original ProjectMW working branch and uncommitted vehicle edits are not used as source changes in this integration.

| Check | Result |
|---|---|
| MWEditor Win64 Development | Passed, exit0 |
| MW Win64 Development | Passed, exit0 |
| 8 existing environment maps and Theme assets | Loaded; referenced static meshes/material slots non-null |
| Host/client environment-map PIE | 16 world/theme cases passed initial121 registration, feature generation, expansion, deactivation, re-entry, retained placement CRC, repeated seed, different seed, ISMC/timer cleanup |
| Current 5 vehicle BP assets | 5/5 in-memory compilation UpToDate; original packages not saved |
| Existing lobby/vehicle-selection/server-travel flow | Two-player PIE reached PreparingRace; each camera targets its own actual Pawn; local environment adapter active |
| Existing input routing | Forced EnhancedInput W/D moved each vehicle and changed yaw; camera stayed on Pawn |
| Full racing, drift/fall visual quality and long driving expansion | Not accepted by the short forced-input check |
| Source-game visual match / special VFX | Not verified; restored static data and UE approximations remain distinct |
| Windows Development Cook/Stage | Passed, normal project GameInstance/AssetManager, all8 maps, no SkipSoftReferences |
| Packaged executable | City and Forest generation/expansion/re-entry/seed/cleanup passed, exit0. Remaining6 maps were cooked and PIE-tested, not separately executed in packaged form |
| UVCS | No operation |

All tests use separately provisioned local content. They are not reproducible from the public source branch alone. The five Blueprint compile results do not imply the teammate's unmerged native wheel-visual code is present in main; that feature remains separately owned. Peer CRC differences remain cosmetic/order dependent; no new replication contract is added. Main input and shared-cursor code is preserved.
