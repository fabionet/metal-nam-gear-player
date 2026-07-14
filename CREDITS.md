# Credits

**METAL NAM GEAR PLAYER** (host name: *NAM Custom*) — JUCE rewrite by fabionet, based on [mikeoliphant/neural-amp-modeler-lv2](https://github.com/mikeoliphant/neural-amp-modeler-lv2).

## Upstream lineage

- [Neural Amp Modeler](https://github.com/sdatkinson/neural-amp-modeler) — Steven Atkinson (MIT). The original NAM model format and reference implementation.
- [NeuralAmpModelerCore](https://github.com/sdatkinson/NeuralAmpModelerCore) — Steven Atkinson (MIT). Source of the bundled test model `demo_wavenet_a1.nam`.
- [neural-amp-modeler-lv2](https://github.com/mikeoliphant/neural-amp-modeler-lv2) — Mike Oliphant + [contributors](https://github.com/mikeoliphant/neural-amp-modeler-lv2/graphs/contributors) (GPL-3.0). Original LV2 plugin (`custom-dual-stereo` branch of this fork).
- [NeuralAudio](https://github.com/mikeoliphant/NeuralAudio) — Mike Oliphant (MIT). NAM model runtime engine.

## Third-party libraries (vendored / submodule)

- [JUCE 8](https://juce.com) — Raw Material Software Limited (AGPLv3 / commercial). GUI, DSP, plugin format wrappers.
- [VST3 SDK](https://github.com/steinbergmedia/vst3sdk) — Steinberg Media Technologies GmbH (Steinberg VST3 License / GPLv3, dual). Used via JUCE under the GPLv3 branch.
- [LV2](https://gitlab.com/lv2/lv2) — LV2 authors (ISC). Plugin format for the LV2 build.
- [FFTConvolver](https://github.com/HiFi-LoFi/FFTConvolver) — HiFi-LoFi (MIT). Two-stage FFT convolution used by the IR loader.
- [dr_wav](https://github.com/mackron/dr_libs) — David Reid (MIT / public domain choice). WAV file loading for the IR loader.

## Legacy LV2 build only (`custom-dual-stereo` branch)

These attributions apply **only** to the legacy LV2 build, which is not part of the JUCE release artifacts:

- The CMake structure and LV2 plugin scaffolding are based on code from [Dougal-s/Aether](https://github.com/Dougal-s/Aether).
- The `modgui` user interface is by Roman Brandstetter (@rominator1983) and Filipe Coelho (@falkTX), based on a design by Evan Heritage.

## Trademarks

"Neural Amp Modeler" is a trademark of Steven Atkinson. "VST" is a trademark of Steinberg Media Technologies GmbH. "JUCE" is a trademark of Raw Material Software Limited. This project is not affiliated with or endorsed by them.
