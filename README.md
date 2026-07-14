# METAL NAM GEAR PLAYER

A full-featured guitar amp-sim plugin (VST3 / LV2 / Standalone) built with [JUCE](https://juce.com), based on a fork of [mikeoliphant/neural-amp-modeler-lv2](https://github.com/mikeoliphant/neural-amp-modeler-lv2) and powered by the [NeuralAudio](https://github.com/mikeoliphant/NeuralAudio) engine for [Neural Amp Modeler](https://github.com/sdatkinson/neural-amp-modeler) model playback.

The plugin is exposed to hosts as **"NAM Custom"**.

> The original headless LV2 plugin lives on the [`custom-dual-stereo`](../../tree/custom-dual-stereo) branch. Active development happens on [`juce-rewrite`](../../tree/juce-rewrite) (default branch).

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

## Requirements

- Run your host at the sample rate the model was trained at (usually **48 kHz**)
- Linux is the primary target. A **Windows MinGW cross-build** lives on the [`windows-mingw`](../../tree/windows-mingw) branch — pinned to JUCE 7.0.12 (JUCE 8 does not build under MinGW) with a small `JuceFontCompat.h` shim so the sources stay in sync with the Linux tree; toolchain in `cmake/mingw-w64-x86_64.cmake` and a required post-clone patch in `patches/juce-vst3-helper-wine.patch`.

## Building (Linux)

```bash
git clone --recurse-submodules https://github.com/fabionet/metal-nam-gear-player.git
cd metal-nam-gear-player
cmake -B build-juce -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build-juce -j 2
```

> **Note:** `-DCMAKE_BUILD_TYPE=Release` is required — an empty build type produces an unoptimized binary that stutters under load.

The VST3 bundle is produced at `build-juce/juce/NAMCustom_artefacts/Release/VST3/NAM Custom.vst3`. There is no install target; copy it to your user VST3 folder:

```bash
rsync -a --delete "build-juce/juce/NAMCustom_artefacts/Release/VST3/NAM Custom.vst3/" "$HOME/.vst3/NAM Custom.vst3/"
```

### Submodule note

`deps/NeuralAudio` points to [fabionet/NeuralAudio](https://github.com/fabionet/NeuralAudio) (branch `nam-custom-patches`), a lightly patched fork that exposes model-metadata flags (`HasLoudness` / `HasInputLevel` / `HasOutputLevel`) used by the calibration logic.

## Models

Get `.nam` models from [Tone3000](https://www.tone3000.com/). Both V1 and A2 architectures are supported. For amp-only models, load an impulse response in the built-in IR loader to model the cabinet.

## License

**AGPL-3.0-or-later.** This project combines the GPL-3.0 upstream (`mikeoliphant/neural-amp-modeler-lv2`) with the JUCE 8 framework, which is licensed under AGPLv3 (or a commercial JUCE licence). Per GPL-3 §13, the combined derivative work is distributed under AGPLv3. Bundled demo preset assets are MIT-licensed. See the Info popup in the plugin and the `LICENSE` file for full text.

## Trademarks

- **Neural Amp Modeler**® is a trademark of Steven Atkinson.
- **VST**® is a trademark of Steinberg Media Technologies GmbH.

This project is an independent fork and is **not affiliated with, endorsed by, or sponsored by** Steven Atkinson, Mike Oliphant, Raw Material Software, or Steinberg.
