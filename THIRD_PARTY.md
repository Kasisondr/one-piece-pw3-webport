# External dependencies

Exact checkout revisions and the downloaded Vita3K executable hash are in `dependencies.lock.json`.

- [soyjxck/vitarecomp](https://github.com/soyjxck/vitarecomp), forked from [sp00nznet/vitarecomp](https://github.com/sp00nznet/vitarecomp): MIT. Used for the ARM-to-C recompiler, runtime, and MIT homebrew sample. Its original licence is `external/vitarecomp/LICENSE` and must accompany redistributed runtime/sample builds. The CMake-generated thread-manager overlay retains upstream’s MIT code.
- [vitasdk/vita-headers](https://github.com/vitasdk/vita-headers): MIT. NID database and ABI reference used at build time.
- [Emscripten SDK](https://github.com/emscripten-core/emsdk): SDK bootstrap and compiler distribution; each bundled component retains its own licence. See its external checkout and the Emscripten distribution notices.
- [Vita3K](https://github.com/Vita3K/Vita3K): GPLv2 or later. Used as an external local decryption/reference application; not linked into the browser runtime. Its downloaded distribution includes `COPYING.txt`.
- [cobra838/pfs-vita-enc](https://github.com/cobra838/pfs-vita-enc): external local PFS tool. No licence file was present in the inspected revision; it is downloaded separately and is not vendored or relicensed by this project.
- [PyCryptodome](https://www.pycryptodome.org/): local dependency of the external PFS tool, installed into an isolated virtual environment. See its installed distribution licence.
- [Playwright](https://github.com/microsoft/playwright): Apache-2.0, development/browser testing only.

The retail dump, decrypted assets, game ELF, translated game functions and retail WASM build are private, Git-ignored local artifacts. They are not covered by this project’s MIT licence. No game media is embedded into the source UI.
