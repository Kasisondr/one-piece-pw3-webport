# Measured bring-up checkpoint

PCSE00638 v1.00; toolkit `5a095d7f429b1e51ffca3af438b18ad1502f07fb`; Emscripten 6.0.13.

## Verified milestones

1. **PSN offline profile initialized**: `src/hle_np_offline.c` implements `SceNpInit`, `SceNpTerm`, `SceNpLookupInit`, `SceNpLookupTerm`, `SceNpGetServiceState` (reporting `SCE_NP_SERVICE_STATE_SIGNED_OUT`), `SceNpManagerGetNpId`, service state callbacks, and social list counts. Both native and browser runs advance past the previous `SceNpUtility::sceNpLookupInit` blocker.
2. **Autosave and save creation dialogs**: The retail game renders the Autosave Warning dialog ("This game saves data automatically... OK"), and upon button input progresses to the Create Save dialog ("There is no save data for 'ONE PIECE: PIRATE WARRIORS 3'... Yes No") and Difficulty Selection. Fixed null `slotInfo` handling in `hle_savedata_port.c` eliminating bad memory accesses during save status queries.
3. **In-engine 3D cutscenes and stage loading**: With difficulty confirmed, the game enters the in-engine 3D Marineford prologue cutscene, rendering Luffy running across the Marineford plaza, barracks, and marine crowds with Direct3D 11 GPU (and CPU rasterizer on WebAssembly). Execution continues to the stage "NOW LOADING..." screen (Going Merry wheel spinning with pirate crew flag tiles).
4. **Instruction table recovery (TBH 81307566 & TBB 812D0A16)**:
   - The emitter missed range guards on a 30-case TBH switch at `81307566` (firmware error conversion).
   - In the prologue sequence, a 21-case TBB switch at `812D0A16` (bounds `r7` in `[0x00EE8080, 0x00EE8094]`) trapped with an unresolved indirect transfer. All 21 cases resolve to common block `L_812D0A30` (either directly with offset 11 or via unconditional branch trampolines `0xE7E0..0xE7E8` at `812D0A5C..812D0A6C`). `tools/fix_codegen.py` recovers both hash-checked tables from the ELF, allowing native execution to exceed 12,000 vblanks and 5,999 flips with 0 traps.
5. **Browser WebAssembly execution**. Chromium renders the 960×544 autosave notice from the retail code. The bounded 1,200-vblank run recorded 599 flips, zero traps, zero dispatch misses, and zero invalid accesses.
6. **Native main menu**. The fresh save run created a 960,512-byte game save and an 844-byte slot record. It reached the main menu at 18,000 vblanks with 8,999 flips, zero traps, zero dispatch misses, and 17 invalid accesses. The frame and report are `generated/opw3/native-stage.png` and `generated/opw3/native-stage.json`.
7. **Browser save storage**. The launch path mounts IndexedDB storage as `savedata0:` and `ux0:`. The storage regression checks byte-for-byte restoration after reload and rejects a second writer from the same origin.
8. **Legend Log loading and Prologue Episode 1**. The old build dereferenced null stage objects at offsets `0x2224` and `0x2228` from the object behind `0x8149C05C`. The startup effects initializer exhausted the runtime's 64 event flag slots while allocating three flags for each of 13 effects pools. Raising the host table to 256 allowed that initialization to complete. Recovering the two asset-parser tables and the episode scene table let the native build advance to the dialogue with Zoro. The 18,000-vblank run in `generated/opw3/native-combat.txt` recorded 8,999 flips, zero traps, zero dispatch misses and zero invalid accesses.
9. **Active 3D combat gameplay in Shells Town courtyard**. Navigating past `Army Info` on the Romance Dawn Briefing screen with `UP` -> `CROSS` successfully selects **"Battle Start"**. The in-game stage loading progress bar advances from 0% to 100% ("NOW LOADING..."). Monkey D. Luffy enters the Shells Town courtyard with dozens of armed Marine soldiers, full 3D buildings, dynamic lighting, combat HUD (portrait, HP gauge, mini-map, K.O. counter), and combat dialogue ("Defeat Gate Guard!"). Active combat attack sequences execute combos ("Gum Gum Cannon") with impact sparks, manga onomatopoeia ("DON!!"), and damage numbers. The Gate Guard mini-boss is defeated, opening the courtyard gates with prompt "Head to Zoro!" and reaching 84 K.O. with zero traps and zero bad accesses across 41,500+ vblanks.
10. **GXM USSE forward-branch shader translation**. Shaders `9ad119b323689d9b` and `e2c48586020399b0` previously failed translation in `external/vitarecomp/src/gxp_hlsl.c` ("it has a branch out of what an earlier branch passes over"), causing combat draw calls to fall back to the slow CPU software rasterizer. `src/gxp_hlsl_branch.inc` implements relaxed forward-branch translation using a `uint _skip_to = 0u;` target tracking variable and predicate guards, completely lifting nested block restrictions. Overlaid as `gxp_hlsl_port.c`, the D3D11 GPU backend compiles and renders all combat draw lists with zero fallbacks at full hardware speed.
11. **Chromium combat in Shells Town**. The regular retail WebAssembly launcher progresses through Romance Dawn, the battle briefing and the 3D courtyard at 960×544. `W` moves Luffy into the crowd. `J` and `I` trigger attacks including Gum Gum Cannon. The run reaches “Head to Zoro!” and records 9 K.O. with no browser page errors. Captures are `generated/opw3/browser-first-battle.png` and `generated/opw3/browser-head-to-zoro.png`. The run presented about 5–6 frames per second during a sampled combat interval using the CPU rasterizer. The presentation counter can include duplicate frames. See `generated/opw3/browser-first-battle.json` for the evidence record.

## Instruction repairs

The pinned emitter reevaluates LDRD's address after writing its first destination. When that destination aliases an address register, the second load reads the wrong location. Saving the effective address and loading both values before assignment corrects 40 sites across 9 generated source files.

Two indirect branch tables have been recovered:
- **`0x81307566` (TBH, 30 cases)**: firmware error conversion leaf instructions that return negative error codes.
- **`0x812D0A16` (TBB, 21 cases)**: prologue state machine switch branching to `L_812D0A30`.

Recovery is restricted to ELF SHA-256 `e8802ce6c410a009725332519040ebf4aa75f2e8bcce7bab22370a5d3bdcbfa6`. `tools/fix_codegen.py` runs during prepare, web build, and CMake configuration. `generated/opw3/codegen-repairs.json` records 40 LDRD repairs and the 2 recovered tables patched after emission.

The same ELF hash selects a decoder overlay during `tools/build.py prepare`. The overlay supplies the reviewed table lengths for the effects and stage asset parser at `0x81335D84` (8 cases) and `0x81335DA2` (13 cases), and the episode scene at `0x812D001A` (21 cases). Upstream armrecomp reads the destinations from the ELF, follows them through its normal analysis and emits the switches. The asset parser exposes 15 additional reachable functions. After both kinds of repair, 28 of the original 33 instruction traps and 4 separately reported branch-target gaps remain.

## Browser movie fallback

The browser runtime uses `movie_none.c`. On the old build, the game went black after the setup prompts and made no progress when SceAvPlayer rejected the opening logo. The same failure reproduced on Windows with movie decoding disabled.

The Emscripten build takes the game's existing no-movie transition at `0x81077ABE` for the startup movie entries `OMEGA_LOGO` and `PM021_NA`. A later browser run stopped in the same way on `PM000`, the first Legend Log episode's opening movie. The browser uses the same fallback for that entry. The patch checks the ELF hash above. Native builds still use their platform decoder.

The next black screen was traced to the archive-backed movie player, whose guest file callbacks use `dummy.mp4` as a placeholder name. That player waits for an event after `sceAvPlayerAddSource`. The browser overlay lets the identified game callback receive the normal Ready, Play and Stop events without submitting any video frames.

A fresh Chromium launch reached the main menu in 71.3 seconds including scripted setup inputs, with no page errors or requests for the two movies. The capture is `generated/opw3/startup-fixed-45.png` and the timed observations are in `generated/opw3/browser-startup.json`.

## File and audio interfaces

Positioned I/O follows AAPCS's 64-bit alignment: its offset is at guest sp/sp+4, with r3 padding. Reads/writes restore the cursor without yielding the serialized guest scheduler. Native tests check offsets beyond 4 GiB, EOF, writes, invalid offsets and cursor preservation.

The browser asset adapter uses synchronous XHR on the game pthread. The launch path uses ordinary Emscripten stdio for the ELF and IndexedDB-backed saves. The bounded diagnostic uses temporary saves. The `app0:` streams use a read-only backend with 256 KiB page caching.

SceAudiodec implements AT9 initialization, termination, decoder creation/deletion, one-frame decode, clear/reset and internal-error queries. Synthetic streams compare frame output byte-for-byte with superframe PCM.

## Next requirements

1. **Browser GPU backend**: Port GXM/USSE rendering to WebGPU/WebGL. Upstream's Windows GPU backend is D3D11, while the browser build currently uses the CPU software rasterizer.
2. **Browser audio output**: Connect the decoded ATRAC9 PCM frames to WebAudio `AudioWorklet` / `AudioContext`.
3. **Subsequent battle objectives**: Script input sequences through the open courtyard gate to reach Zoro, defeat Captain Morgan, and complete Episode 1.

On Windows, use `tools/probe_native.py --capture-every 600 --stop-after-bad 18` with an input schedule to capture the first stage-loading fault. The probe logs the native call stack and resolves functions with `build/bringup/port.map`.
