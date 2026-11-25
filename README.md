# FastVocal Comp Basic

A transparent leveling compressor for vocals, by TwinRoots Audio.
Smooth, optical-style gain riding with no added colour: no saturation, no harmonics, flat frequency response.

This repository is the **source code** of FastVocal Comp, with a plain interface. The sound is the same as the full version; only the interface is basic.

## Controls

| Control | What it does |
|---|---|
| **Compression** (0–100) | How much the vocal is levelled. The output level is compensated automatically as you turn it up. |
| **Output trim** (dB) | Fine level adjustment on top of the automatic make-up. |
| **Bypass** | Hear the original. Same latency, so the comparison is fair. |

The meter at the top shows the gain reduction actually applied (0 to 30 dB) with a 1.5 s peak hold.

Fixed design choices: limit-style release, smooth detector with natural attack, 5 ms lookahead (reported to the DAW, so tracks stay aligned).

## Get it running in your DAW (with Claude Code)

1. Download this repository: green **Code** button → **Download ZIP**, then unzip it (or `git clone` it).
2. Open [Claude Code](https://claude.com/claude-code) in the unzipped folder.
3. Ask: *"Build and install this plugin for my DAW."*

Claude Code follows [BUILD_WITH_CLAUDE_CODE.md](BUILD_WITH_CLAUDE_CODE.md): it checks the tools on your computer, builds the plugin (VST3 on Windows, macOS and Linux; also AU on macOS), installs it and tells you how to make your DAW find it.

## Build it yourself

You need CMake 3.22+, Git and a C++17 compiler (Visual Studio 2022 on Windows, Xcode on macOS).

```
cmake -S . -B build
cmake --build build --config Release
```

The first configure downloads JUCE 8.0.15 (a few minutes). The plugin is in `build/FastVocalCompBasic_artefacts/Release/`.

## Licence

Free to use, including in commercial music productions. You may not sell this plugin or a modified version of it. See [LICENSE](LICENSE).

This project uses the [JUCE framework](https://juce.com), which is downloaded at build time and has its own licence terms.

## Independence

This is an independent research project. It is not affiliated with, endorsed by, or derived from any plugin manufacturer. All measurements were made by black-box observation of input and output signals.
