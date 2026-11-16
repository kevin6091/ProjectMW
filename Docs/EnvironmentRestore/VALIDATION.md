# Official three-theme validation

The three-theme integration was validated on Unreal Engine 5.8.4, Windows Win64 Development. `Validation_Summary.json` describes the earlier eight-theme integration at `1049df5`.

| Theme | Two-world diagnostic PIE | Two-player lobby-to-match PIE | Staged game Host / Client |
|---|---|---|---|
| Snow | Pass | PreparingRace | Pass / Pass |
| Toybox | Pass | PreparingRace | Pass / Pass |
| Tropic | Pass | PreparingRace | Pass / Pass |

- MWEditor and MW Development builds passed. The Windows Cook and Stage passed using the standard project map list.
- The normal selector data returned exactly Snow, Toybox and Tropic in both PIE worlds. The Toybox and Tropic match PIE runs used a process-local default selection override for the existing PIE flow. Staged game runs submitted each theme through the normal room request.
- All three diagnostic maps loaded sky, floor and mesh/material references. They passed registration, expansion, deactivation, re-entry, placement CRC preservation, Seed and component/timer cleanup checks. Toybox's first 121 floor indices matched the existing Epic-derived two-material oracle at Seed 42. Tropic rendered the restored sea floor and island assets.
- The optional catalogue was regenerated with 9 base entries and 3 official entries. A second run had no changes. The missing-catalogue test travelled through the existing two-player flow to DriveDefault and possessed the existing vehicle on both peers.
- The IoStore inspection found all 146 required `/Game` theme packages and the private catalogue. It found zero retired exclusive packages and zero Environment diagnostic maps. Shared dependencies used by retained content are present.
- Stage size changed from 895,458,493 to 820,037,429 bytes, a reduction of 75,421,064 bytes (8.42%). Both measurements use a full Cook, Windows Development, the standard map list, and IoStore/Pak with debug symbols omitted.
- Cook logged the same two existing warnings in both runs, covering the legacy UCM frontend development tag and the EOS e-commerce flag. The test uses LocalIP and reaches PreparingRace. Full race, physical driving and original-game visual parity remain outside this validation.
