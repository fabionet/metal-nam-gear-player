# METAL NAM GEAR PLAYER — Windows (MinGW cross-build)

A full-featured guitar amp-sim plugin (VST3 + Standalone `.exe`) built with [JUCE](https://juce.com), based on a fork of [mikeoliphant/neural-amp-modeler-lv2](https://github.com/mikeoliphant/neural-amp-modeler-lv2) and powered by the [NeuralAudio](https://github.com/mikeoliphant/NeuralAudio) engine for [Neural Amp Modeler](https://github.com/sdatkinson/neural-amp-modeler) model playback.

The plugin is exposed to hosts as **"NAM Custom"**.

> This branch (`windows-mingw`) is the **Windows x64** port of the Linux [`juce-rewrite`](../../tree/juce-rewrite) branch, cross-compiled with **MinGW-w64** from a Linux host. Sources are kept in sync with Linux; the only per-branch delta is JUCE 7.0.12 (JUCE 8 does not build under MinGW) plus a tiny `JuceFontCompat.h` shim.

## Screenshots

**MAIN tab** — Noise Gate, Overdrive/Distortion, Amp (NAM model), 5-band EQ, Power (Depth/Resonance), Cab (IR mix), IR Tools (HP/LP/Trim), Master (Loudness Normalization):

![MAIN tab](docs/screenshots/main-tab.png)

**FX tab** — Delay, Chorus, Flanger, Reverb, Tremolo (post-cab effects):

![FX tab](docs/screenshots/fx-tab.png)

## Features

- **NAM model playback** — supports both V1 (WaveNet/LSTM) and A2 (SlimmableContainer) models, with a **Slim** slider for real-time quality scaling on A2 models
- **IR loader** with quality tools: high-pass / low-pass filters, trim, phase invert
- **Full FX chain**:
  - Pre-model: Smart Gate, Overdrive, Distortion
  - Post-model: 5-band EQ + Depth + Resonance, Noise Gate, High-Pass, Loudness Normalization
  - Post-cab: Delay, Chorus, Flanger, Reverb, Tremolo
- **Gain-staging / calibration** (ported from the reference [NeuralAmpModelerPlugin](https://github.com/sdatkinson/NeuralAmpModelerPlugin)): Output Mode (Raw / Normalized / Calibrated) and Calibrate Input with dBu level — with automatic fallback when a model lacks calibration metadata
- **Preset system** — factory presets by genre (Clean / Rock / Metal / Extreme Metal), user presets, direct link to [Tone3000](https://www.tone3000.com/) for more models
- **2x oversampling** (true `juce::dsp::Oversampling`, latency reported to the host)
- CPU meter, level meters, Info popup with credits
- **Reaper helpers** — bundled ReaScripts in `extras/reaper/` for one-click VST3 insertion (`nam_insert.lua`) and `.nam` + IR autoload with clipboard fallback (`nam_autoload_vst3.lua`)

## Requirements

- Windows 10 / 11 x64
- Run your host at the sample rate the model was trained at (usually **48 kHz**)
- No Visual C++ runtime needed — MinGW binaries are self-contained modulo Windows system libs

## Installation (pre-built binaries)

Pre-built binaries are attached to each Windows GitHub Release:

- `MetalNAMGearPlayer-windows-x64-portable.zip` — extract anywhere; contains Standalone `.exe` + VST3 bundle + docs + Reaper helper
- `MetalNAMGearPlayer-windows-x64-setup.exe` — NSIS installer, copies VST3 to `%CommonProgramFiles%\VST3\` and Standalone to `%ProgramFiles%\MetalNAMGearPlayer\`

From the portable zip:

1. Extract the archive.
2. Copy the whole `NAM Custom.vst3` folder into your VST3 directory:
   - System-wide: `C:\Program Files\Common Files\VST3\`
   - Per-user:    `%APPDATA%\VST3\` (rescan the folder in your DAW)
3. Run `NAM Custom.exe` directly for standalone use.
4. Load a `.nam` model and (optionally) an IR `.wav` — see `docs\guida-rapida.pdf`.

## Building from source (Linux host, MinGW cross-compile)

Requires: `mingw-w64` (with `-posix` variants), `cmake ≥ 3.15`, `wine` (only for the VST3 manifest helper post-build step).

```bash
git clone https://github.com/fabionet/metal-nam-gear-player.git -b windows-mingw
cd metal-nam-gear-player
git submodule update --init
git submodule update --init --recursive deps/NeuralAudio
(cd deps/JUCE && git apply ../../patches/juce-mingw-crossbuild.patch)
cmake -B build-win -S . -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake
cmake --build build-win -j 1
```

> **Notes:**
> - The CMake warning "Manually-specified variables were not used: `CMAKE_TOOLCHAIN_FILE`" is a **false alarm** — verify with `grep COMPILER build-win/CMakeCache.txt` (should show `x86_64-w64-mingw32-gcc-*-posix`).
> - Use `-j 1` on machines with limited RAM; parallel multi-target builds can OOM.
> - `-DCMAKE_BUILD_TYPE=Release` is required — an empty build type produces an unoptimized binary that stutters under load.

Artefacts:

- Standalone: `build-win/juce/NAMCustom_artefacts/Release/Standalone/NAM Custom.exe`
- VST3: `build-win/juce/NAMCustom_artefacts/Release/VST3/NAM Custom.vst3/Contents/x86_64-win/NAM Custom.vst3`
- LV2: `build-win/juce/NAMCustom_artefacts/Release/LV2/NAM Custom.lv2/`

### Packaging (portable zip + NSIS installer)

Requires `zip` and `nsis` (`apt install zip nsis`). A sibling checkout of the Linux repo is needed for the bundled Italian PDF guides:

```bash
DOCS_SRC=../neural-amp-modeler-lv2/docs packaging/windows/build-package.sh 0.1.1
```

Produces `dist/MetalNAMGearPlayer-windows-x64-portable.zip` and `dist/MetalNAMGearPlayer-windows-x64-setup.exe`.

### Submodule note

`deps/NeuralAudio` points to [fabionet/NeuralAudio](https://github.com/fabionet/NeuralAudio) (branch `nam-custom-patches`), a lightly patched fork that exposes model-metadata flags (`HasLoudness` / `HasInputLevel` / `HasOutputLevel`) used by the calibration logic.

### Native Windows build

Not tested. In principle possible with MSYS2's `mingw-w64-x86_64-*` toolchain (equivalent to the cross-compiler used here) or Visual Studio + JUCE's Projucer, but you'd have to regenerate the CMake project without the toolchain file and re-source the wine patch. Contributions welcome.

### Testing under wine

Wine 9 misses the WinRT `Windows.UI.ViewManagement.UIViewSettings` API that JUCE 7 polls in a loop; scale factor and hit-testing get corrupted → buttons appear dead. **Wine ≥ 10 or a native Windows tester is required for actual functional testing** — this project uses wine only for cross-build glue (VST3 manifest helper), not as a runtime target.

## Why JUCE 7.0.12 (not JUCE 8)

This branch pins `deps/JUCE` to **JUCE 7.0.12**. JUCE 8 does not compile under MinGW: `juce_core/system/juce_TargetPlatform.h` has an explicit `#error "MinGW is not supported"`, and even after neutering it the tree misuses `<cstring>` in multiple headers and mis-detects `pointer_sized_uint` as 32-bit on Win64. JUCE 8 core is genuinely unmaintained on MinGW as of the 8.0.9 tag.

To keep the source files identical with the Linux tree (which uses JUCE 8 idioms like `juce::Font (juce::FontOptions (...))`), this branch ships `juce/JuceFontCompat.h` — a small header that re-implements a JUCE-8-style `FontOptions` builder on top of JUCE 7's `Font` API. The two per-repo edits over the Linux sources are the `#include "JuceFontCompat.h"` lines in `juce/PluginEditor.h` and `juce/PresetPanelComponent.cpp`.

## Models

Get `.nam` models from [Tone3000](https://www.tone3000.com/). Both V1 and A2 architectures are supported. For amp-only models, load an impulse response in the built-in IR loader to model the cabinet.

## Reaper quick-start helpers

Two ReaScripts ship in `extras/reaper/` for Windows Reaper users:

| Script                  | Behaviour                                                                                              |
|-------------------------|--------------------------------------------------------------------------------------------------------|
| `nam_insert.lua`        | Inserts a "NAM Custom" track, loads the VST3, opens the FX chain — for quick sanity checks             |
| `nam_autoload_vst3.lua` | Same as above, then prompts for a `.nam` model and (optionally) an IR `.wav` and loads them            |

Once the VST3 has been installed under `C:\Program Files\Common Files\VST3\` (or `%APPDATA%\VST3\`) and Reaper has scanned it, run either script via:

- Reaper GUI: **Actions → Load ReaScript → *nam_...lua* → Run**
- CLI (Windows): `"C:\Program Files\REAPER (x64)\reaper.exe" -nonewinst extras\reaper\nam_autoload_vst3.lua`

If Reaper doesn't accept the model path programmatically (varies by build), `nam_autoload_vst3.lua` copies the chosen `.nam` / IR paths to the system clipboard as a fallback so they can be pasted into the plugin's file dialog.

## License

**AGPL-3.0-or-later.** This Windows build links against:

- **JUCE 7.0.12** — GPL-3.0-or-later or Raw Material Software commercial licence
- **VST3 SDK 3.7.9** — dual GPLv3 / Steinberg proprietary (used under the GPLv3 branch)
- **NeuralAudio** (Mike Oliphant) — MIT; built from the [fabionet fork](https://github.com/fabionet/NeuralAudio) branch `nam-custom-patches` (metadata-flag additions only, upstream MIT preserved)
- **Neural Amp Modeler / NeuralAmpModelerCore** (Steven Atkinson) — MIT; also covers the bundled demo model `juce/Presets/Assets/nam/demo_wavenet_a1.nam`
- **FFTConvolver** (HiFi-LoFi) — MIT; used by the IR loader
- **dr_wav** (David Reid) — MIT-0 / public domain; single-file WAV parser vendored at `src/dsp/dr_wav.h`
- **denormal** (`deps/denormal/architecture.hpp`, from [Dougal-s/Aether](https://github.com/Dougal-s/Aether)) — GPL-3.0; FPU denormal-handling helper
- Upstream [`mikeoliphant/neural-amp-modeler-lv2`](https://github.com/mikeoliphant/neural-amp-modeler-lv2) sources (Mike Oliphant and contributors) — GPL-3.0
- **Embedded UI fonts** — SIL Open Font License 1.1: Metal Mania (© 2012 Open Window / Dathan Boardman), Nosifer (© 2011 Typomondo), Pirata One (© 2012 Rodrigo Fuenzalida, Nicolas Massi). Full text in `juce/fonts/LICENSE-fonts.txt`.

The combined work is distributed under **AGPL-3.0-or-later** (per GPL-3 §13, since the Linux sibling branch links JUCE 8/AGPLv3 and the license must remain consistent across the same product family). See `LICENSE`.

> Note: this branch does **not** build the LV2 target (Windows ships VST3 + Standalone only), so the LV2 SDK is not linked here.

## Trademarks

- **Neural Amp Modeler**® is a trademark of Steven Atkinson.
- **VST**® is a trademark of Steinberg Media Technologies GmbH.
- **JUCE**™ is a trademark of Raw Material Software Limited.

This project is an independent fork and is **not affiliated with, endorsed by, or sponsored by** Steven Atkinson, Mike Oliphant, Raw Material Software, or Steinberg.

## Related

- Linux build (main development): [`juce-rewrite`](https://github.com/fabionet/metal-nam-gear-player/tree/juce-rewrite)
- Legacy LV2 headless plugin: [`custom-dual-stereo`](https://github.com/fabionet/metal-nam-gear-player/tree/custom-dual-stereo)
- Model source: [Tone3000](https://www.tone3000.com/)
