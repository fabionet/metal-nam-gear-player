# METAL NAM GEAR PLAYER — Windows (MinGW cross-build)

Windows x64 port of [METAL NAM GEAR PLAYER](https://github.com/fabionet/metal-nam-gear-player). Same sources as the Linux [`juce-rewrite`](../../tree/juce-rewrite) branch, built for Windows via **MinGW-w64** cross-compilation from a Linux host. The plugin is exposed to hosts as **"NAM Custom"**.

## Distribution

Pre-built binaries are attached to each Windows GitHub Release:

- `MetalNAMGearPlayer-windows-x64-portable.zip` — extract anywhere, contains Standalone `.exe` + VST3 bundle + docs.
- `MetalNAMGearPlayer-windows-x64-setup.exe` — NSIS installer, copies VST3 to `%CommonProgramFiles%\VST3\` and Standalone to `%ProgramFiles%\MetalNAMGearPlayer\`.

Requirements: Windows 10 / 11 x64. No Visual C++ runtime needed (MinGW binaries are self-contained modulo Windows system libs).

## Installation from portable zip

1. Extract the zip.
2. Copy `NAM Custom.vst3` (the whole folder) into your VST3 directory:
   - System-wide: `C:\Program Files\Common Files\VST3\`
   - Per-user: `%APPDATA%\VST3\` (rescan the folder in your DAW)
3. Run `NAM Custom.exe` directly for standalone use.
4. Load a `.nam` model and (optionally) an IR `.wav` — see `docs/guida-rapida.pdf`.

## Build from source (Linux host, MinGW cross-compile)

Requires: `mingw-w64` (with `-posix` variants), `cmake ≥ 3.15`, `wine` (only for the VST3 manifest helper post-build step).

```bash
git clone https://github.com/fabionet/metal-nam-gear-player.git -b windows-mingw
cd metal-nam-gear-player
git submodule update --init
git submodule update --init --recursive deps/NeuralAudio
(cd deps/JUCE && git apply ../../patches/juce-vst3-helper-wine.patch)
cmake -B build-win -S . -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake
cmake --build build-win -j 1
```

> The CMake warning "Manually-specified variables were not used: `CMAKE_TOOLCHAIN_FILE`" is a **false alarm** — verify with `grep COMPILER build-win/CMakeCache.txt` (should show `x86_64-w64-mingw32-gcc-*-posix`). Use `-j 1` on machines with limited RAM; parallel jobs multi-target can OOM.

Artefacts:

- Standalone: `build-win/juce/NAMCustom_artefacts/Release/Standalone/NAM Custom.exe`
- VST3: `build-win/juce/NAMCustom_artefacts/Release/VST3/NAM Custom.vst3/Contents/x86_64-win/NAM Custom.vst3`

## Why JUCE 7.0.12 (not JUCE 8)

This branch pins `deps/JUCE` to **JUCE 7.0.12**. JUCE 8 does not compile under MinGW: `juce_core/system/juce_TargetPlatform.h` has an explicit `#error "MinGW is not supported"`, and even after neutering it the tree misuses `<cstring>` in multiple headers and mis-detects `pointer_sized_uint` as 32-bit on Win64. JUCE 8 core is genuinely unmaintained on MinGW as of the 8.0.9 tag.

To keep the source files identical with the Linux tree (which uses JUCE 8 idioms like `juce::Font (juce::FontOptions (...))`), this branch ships `juce/JuceFontCompat.h` — a small header that re-implements a JUCE-8-style `FontOptions` builder on top of JUCE 7's `Font` API. The two per-repo edits over the Linux sources are the `#include "JuceFontCompat.h"` lines in `juce/PluginEditor.h` and `juce/PresetPanelComponent.cpp`.

## Native Windows build

Not tested. In principle possible with MSYS2's `mingw-w64-x86_64-*` toolchain (equivalent to the cross-compiler used here) or Visual Studio + JUCE's Projucer, but you'd have to regenerate the CMake project without the toolchain file and re-source the wine patch. Contributions welcome.

## Testing under wine

Wine 9 misses the WinRT `Windows.UI.ViewManagement.UIViewSettings` API that JUCE 7 polls in a loop; scale factor and hit-testing get corrupted → buttons appear dead. **Wine ≥ 10 or a native Windows tester is required for actual functional testing** — this project uses wine only for cross-build glue (VST3 manifest helper), not as a runtime target.

## Licensing

**AGPL-3.0-or-later.** This Windows build links against:

- **JUCE 7.0.12** — GPL-3.0-or-later or Raw Material Software commercial licence
- **VST3 SDK 3.7.9** — dual GPLv3 / Steinberg proprietary (used under the GPLv3 branch)
- **NeuralAudio** — MIT (fabionet fork `nam-custom-patches`)
- The upstream `mikeoliphant/neural-amp-modeler-lv2` sources are GPL-3.0
- **Embedded UI fonts** — SIL Open Font License 1.1: Metal Mania (© 2012 Open Window / Dathan Boardman), Nosifer (© 2011 Typomondo), Pirata One (© 2012 Rodrigo Fuenzalida, Nicolas Massi). Full text bundled as `LICENSE-fonts.txt`.

The combined work is distributed under **AGPL-3.0-or-later** (per GPL-3 §13, since the Linux sibling branch links JUCE 8/AGPLv3 and the license needs to be consistent across the same product family). See `LICENSE`.

Trademarks: JUCE™ Raw Material Software Limited; VST® Steinberg Media Technologies GmbH; Neural Amp Modeler® Steven Atkinson. This project is independent and not affiliated with any of them.

## Related

- Linux build (main development): [`juce-rewrite`](../../tree/juce-rewrite)
- Legacy LV2 headless plugin: [`custom-dual-stereo`](../../tree/custom-dual-stereo)
- Model source: [Tone3000](https://www.tone3000.com/)
