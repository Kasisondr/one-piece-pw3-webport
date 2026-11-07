# One Piece: Pirate Warriors 3 — browser port bring-up

This is an **unfinished static recompilation project** using the PS Vita base game, PCSE00638 v1.00. The browser runs statically translated ARM code as WebAssembly. Game data stays local and is excluded from Git.

The recompiled retail game reaches the first Shells Town battle in Chromium at 960×544. The browser skips the startup videos and some Legend Log clips. Its current software renderer is slow during combat, and browser audio output is pending. The browser host supports keyboard and mouse, standard gamepads, multitouch, frame capture, and letterboxed 1080p presentation.

The supplied retail dump has been PFS-decrypted into `generated/decrypted/PCSE00638`. Vita3K exported its decrypted executable and five bundled system modules into `external/Vita3K/portable/cache/decrypted_selfs/PCSE00638`. `generated/opw3/module.elf` is extracted and the game’s generated C is in that same directory. These are private build outputs, not redistributable source.

## Run the browser host

```powershell
npm install
python tools/serve.py
```

After building the retail WASM from your local dump, run `python tools/status_report.py` to enable it in the launcher. Open http://127.0.0.1:8080 and select **OPW3 · development build**, then **Run game build**.

For the first battle, select **Legend Log** and proceed to **Romance Dawn**. Use `K` to confirm prompts and advance the briefing. In the briefing menu, use the arrow keys to select **Battle Start** and press `K`. When **Start Battle** appears after loading, press `K` again. Use `W` / `A` / `S` / `D` to move, `J` for normal attacks, and `I` for charge attacks.

### PC Action Controls (Steam-matched Keyboard & Mouse)
- **Movement**: `W` / `A` / `S` / `D` (Left Analog Stick)
- **Normal Attack (□)**: `Left Mouse Button` (LMB) or `J`
- **Heavy / Charge Attack (△)**: `Right Mouse Button` (RMB) or `I`
- **Special / Musou (○)**: `Middle Mouse Button` (MMB), `L`, or `F`
- **Jump / Evade / Confirm (×)**: `Spacebar` or `K`
- **Dash / Dodge / Kizuna (R)**: `Left Shift` or `E`
- **Lock-on / Camera Reset (L)**: `Left Ctrl` or `Q`
- **Map / Status Screen (Select)**: `Tab` or `M`
- **Pause / Menu (Start)**: `Escape` or `Enter`
- **Mouse Camera Control**: Click inside the game window to lock pointer for full mouse look. `Escape` releases the pointer.

The retail browser build stores saves in IndexedDB for this browser profile and address. Use **Stop test** to let queued save writes finish before reloading. Open one retail session per browser profile. Native probes use a separate save directory and accept `--save-dir` to isolate a test run.

The server binds to loopback. It serves `web/` and a restricted `/__assets/` endpoint for files beneath the decrypted `FILE_NGP` / `LINKDATA_NGP` directories and `sce_sys/param.sfo` with HTTP/1.1 keep-alive and cache headers. Asset GETs require an explicit byte range of at most 1 MiB. The runtime uses an in-memory stat cache and caches one 256 KiB page per open asset, fetching on its game worker rather than blocking the browser UI. COOP/COEP headers enable shared WASM memory and pthreads. Use this server instead of opening `index.html` as a file. Stop it with Ctrl+C.

## Rebuild

Requirements: Python, Git, CMake, Ninja; on Windows, Visual Studio 2022 C++ build tools and a Windows SDK.

```powershell
powershell -ExecutionPolicy Bypass -File tools/setup.ps1
python tools/build.py demo
```

The toolkit is pinned to `soyjxck/vitarecomp@5a095d7f429b1e51ffca3af438b18ad1502f07fb`; Emscripten is pinned to **6.0.13**. The setup is local to `external/`, without permanent PATH changes. The Emscripten pthread pool has eight workers plus the proxied main thread; guest memory is upstream’s 416 MiB and the WASM limit is 2 GiB. The browser adapter synchronously proxies presentation and input to the browser’s UI thread, while translated code executes in a pthread worker.

```powershell
python tools/build.py prepare --module external/Vita3K/portable/cache/decrypted_selfs/PCSE00638/eboot.bin --name opw3
python tools/build.py web --name opw3
# Optional browser I/O regression probe, after configuring either web build:
cmake --build build/web-fbdemo --target web_io_probe -j 3
```

The retail output is `web/runtime/opw3.js`, `opw3.wasm`, and `opw3.data`. This output is for bring-up and does not establish that the game works. Never mix an ELF from one game build with C translated from another. `prepare` rejects unrecognized executable formats. A recognized SELF can still contain encrypted segments; extraction must succeed before translation begins.

## Decryption

There are two independent layers:

1. **PFS filesystem encryption**: the original `eboot.bin`, movies, images and archives are encrypted. [pfs-vita-enc](https://github.com/cobra838/pfs-vita-enc) successfully decrypted this supplied dump using its existing `sce_pfs` metadata and `sce_sys/package/work.bin`. The helper keeps input intact and refuses to overwrite an output directory:

   ```powershell
   powershell -ExecutionPolicy Bypass -File tools/decrypt_pfs.ps1
   ```

2. **SELF segment encryption**: after PFS decryption, retail executable segments are still encrypted. In the official [Vita3K](https://github.com/Vita3K/Vita3K) build, select the installed title, right-click → **Other → Decrypt All SELF**. Exports land in its cache’s `decrypted_selfs/PCSE00638/` directory. Our local portable installation and export have already completed this step. No firmware was needed for this export. A licence copied only into the app’s package directory was insufficient; the portable install also needed its licence under `fs/ux0/license/PCSE00638/`.

Neither a decrypted movie nor a valid SELF header alone proves the ARM instructions are decrypted. Confirm using `armrecomp info` and `extract`. Licences, PFS keys and executable code are not put in browser diagnostics or committed source. The downloaded PFS tool stays external; it is not vendored into the web runtime. The portable Vita3K app directory contains only files needed for SELF export; an actual emulator gameplay comparison needs the full game data and the required firmware installed.

## Retail translation evidence

`generated/opw3/translation.txt` records:

| Measurement | Result |
|---|---:|
| Discovered functions | 52,381 |
| Emitted reachable functions | 18,576 |
| Reached instructions | 2,667,641 |
| Translated instructions | 2,667,608 |
| Untranslated instructions | 33 |
| Firmware imports | 586 across 43 libraries |

The emitter rounds the translated percentage to 100.00%; the raw output has 33 instruction traps, including saturation and indirect-transfer cases, plus four separately reported branches whose targets were not discovered as functions. The build repairs 40 incorrect LDRD address-aliasing sites and recovers one of those trapped table branches from its hash-checked ELF (32 raw instruction traps remain). `generated/opw3/codegen-repairs.json` records the repairs. Import defaults are generated for linking; they trap if the runtime has no implementation. These counts are not proof that all imports are implemented or that code paths are correct.

## Validation

```powershell
npm test
python -m unittest discover -s tests -p test_asset_server.py
npx playwright install chromium
python tools/status_report.py
npx playwright test
```

Browser integration tests cover sample input and pixels, 1080p presentation, simultaneous touch, simulated gamepad mapping, retail startup through the main menu, and save storage across reloads. The `web_io_probe` target checks archive reads against native bytes and supplies the runtime for the save storage test. Screenshots are written into `test-results/`.

The initial native probe entered actual translated game code, then stopped on `SceLibKernel::sceKernelGetLwMutexInfo`. A narrow runtime overlay implements lightweight mutexes and queries (`sceGxmProgramParameterIsRegFormat`, `sceGxmProgramParameterGetIndex`, `sceDisplayGetRefreshRate`, `sceKernelIsPSVitaTV`). The ULT runtime/job imports execute guest callbacks through the scheduler, with blocking/try joins, exit status and reusable guest stacks. Positioned I/O, save data dialog null-slot handling, and AT9 decoding are implemented.

`src/hle_np_offline.c` supplies a signed-out offline PSN profile, resolving the previous `SceNpUtility::sceNpLookupInit` blocker.

The native build progresses through save creation and difficulty selection, renders the Marineford prologue, and reaches the main menu. The 18,000-vblank run logged 8,999 flips, zero traps or dispatch misses, and 17 invalid memory accesses. A fresh-save Chromium run reached the menu in about 71 seconds, including scripted input for the setup prompts. [Bring-up evidence](docs/BRINGUP.md)

The Legend Log fixes let the native build load Prologue Episode 1 and reach its dialogue with Zoro. Navigating past `Army Info` on the Romance Dawn Briefing screen with `UP` -> `CROSS` starts the battle. Monkey D. Luffy enters the Shells Town courtyard with full 3D combat, combos ("Gum Gum Cannon"), damage numbers, and dynamic lighting. Defeating the Gate Guard mini-boss opens the courtyard gates with prompt "Head to Zoro!", reaching 84 K.O. with zero traps and zero bad accesses across 41,500+ vblanks. [Battle bring-up evidence](docs/BRINGUP.md)

USSE shader forward branching (`src/gxp_hlsl_branch.inc`) eliminates previous compiler translation failures on programs `9ad119b323689d9b` and `e2c48586020399b0`, allowing hardware GPU rendering with zero CPU software rasterizer fallbacks.

`ctest --test-dir build/bringup -C Release --output-on-failure` runs six tests: kernel/graphics queries, ULT execution/join/stack reuse, positioned I/O, actual repaired instruction semantics, AT9 decoding/lifecycle/errors, and offline NP profile semantics. The generated CMake fixtures remain private build outputs. Mutex callback waits and cancellation remain unverified on hardware; this is a bring-up implementation, not a claim of full kernel compatibility. The source overlays leave the upstream checkout unchanged.

For a bounded native retail boot, build the diagnostic target without optimization, then run the probes (the server must be running for the browser probe):

```powershell
cmake -S . -B build/bringup -G "Visual Studio 17 2022" -A x64 -DRECOMP_DIR="$PWD/generated/opw3" -DMODULE_ELF="$PWD/generated/opw3/module.elf" "-DCMAKE_C_FLAGS_RELEASE=/Od /DNDEBUG"
cmake --build build/bringup --config Release --target port test_lwmutex test_ult test_positioned_io test_codegen test_audiodec test_np_offline -j 3
ctest --test-dir build/bringup -C Release --output-on-failure
python tools/probe_native.py
python tools/probe_native.py --frames 600 --name native-600
python tools/probe_native.py --frames 3600 --timeout 90
node tools/probe_browser.mjs
```

The native probe mounts decrypted game data and a separate save directory. Its default bound is 120 vblanks or a 60-second timeout; flags select longer checkpoints. The browser probe mounts the read-only local asset backend and temporary in-memory saves, recording actual HTTP range reads. These diagnostic runs use a virtual clock, so their duration does not measure game performance. Neither probe suppresses traps or changes failures into success.

## Work still required

- Verify stage completion and later battles. Compare graphics, input and firmware behavior against Vita3K or hardware.
- Add user-selected asset storage for standalone use. The current archive backend requires the supplied local server.
- Port GXM/USSE rendering to WebGPU or WebGL for browser 3D performance. The current browser build uses the CPU reference renderer.
- Connect decoded audio to WebAudio and validate movie playback and full save recovery in the browser.
- Verify native 960×544 rendering and performance before adding true higher internal resolution. The existing 1080p setting scales the presented image and letterboxes it; it does not change guest render targets.
- Investigate the game’s timing before any 60 FPS patch. Changing vblank pacing alone is not a correct frame-rate patch.
- Reverse the multiplayer state/protocol and player/camera ownership before adding online or local split-screen. No lobby or duplicated viewport is presented as game co-op. DLC is deferred until the base game is playable.

For Vita3K comparisons, use the same game revision, save/checkpoint, mission, character, camera route, settings and timed input sequence. Record game FPS separately from presented FPS, p95/p99 frame times, visual differences, input behavior, audio, missing effects, crashes and save integrity. The sample’s timing report is only a host benchmark and is not representative of the retail game.

## Project code and dependencies

`src/window_web.c` implements the upstream host contract. `tools/configure_runtime.py` builds narrow overlays for mutexes, scheduler queries, positioned I/O, browser asset streams and frame-sized AT9 decoding. `src/hle_ult_runtime.c` and `src/hle_audiodec.c` implement the new firmware surfaces. `tools/fix_codegen.py` repairs measured emitter defects before compilation. `web/input.js` converts browser devices to Vita button bits and 0–255 analog values. Upstream is kept unchanged under `external/vitarecomp` and retains its MIT licence. The homebrew sample and synthetic AT9 test encoder are upstream MIT code. [Emscripten](https://emscripten.org/docs/compiling/WebAssembly.html) compiles C to WASM; [pthread support](https://emscripten.org/docs/porting/pthreads.html) requires shared memory and isolation headers. Vita3K is used as an external decryption/reference tool, not linked into this browser host.
