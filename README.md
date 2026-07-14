# METAL NAM GEAR PLAYER — Windows (MinGW cross-build)

This branch (`windows-mingw`) hosts the **Windows x64** port of
[METAL NAM GEAR PLAYER](https://github.com/fabionet/metal-nam-gear-player/tree/juce-rewrite),
built with **MinGW-w64** cross-compilation from a Linux host. The plugin is
exposed to hosts as **"NAM Custom"** (VST3 + Standalone).

Full build, installation and licensing documentation lives in
**[`README-Windows.md`](README-Windows.md)**:

- Pre-built binaries (portable zip + NSIS installer) attached to each Windows GitHub Release
- Build from source with the MinGW-w64 toolchain (`cmake/mingw-w64-x86_64.cmake`)
- Why JUCE 7.0.12 is pinned (JUCE 8 does not compile under MinGW)
- Wine testing caveats (Wine ≥ 10 required for functional testing)
- AGPL-3.0-or-later licensing and third-party attributions

For the Linux build and main development branch see
[`juce-rewrite`](https://github.com/fabionet/metal-nam-gear-player/tree/juce-rewrite).
