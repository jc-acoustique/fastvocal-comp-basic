# Build and install FastVocal Comp Basic — instructions for Claude Code

The user wants this plugin working in their DAW. They may not be a developer: explain each step in plain words, in the user's language, and ask before installing any software.

## Rules
- Do **not** change the audio code (`source/OptoEngine.h`, `source/StaticLaw.h`, `source/MakeupLaw.h`, `source/PluginProcessor.cpp`) unless the user explicitly asks for a modification.
- Ask before installing tools (Visual Studio, Xcode tools, CMake, Git) or writing outside this folder.
- If a step fails, show the error and explain it simply. Do not hide failures.

## 1. Find out the user's setup
- Operating system (Windows, macOS, Linux) and processor (Intel/AMD or Apple Silicon).
- Their DAW, to choose the format: VST3 for almost every DAW; **AU** for Logic Pro and GarageBand (macOS only).

## 2. Check the tools
| System | Needed | Check |
|---|---|---|
| Windows | Visual Studio 2022 (or Build Tools) with "Desktop development with C++", CMake 3.22+, Git | `cmake --version`, `git --version`, Visual Studio installer |
| macOS | Xcode Command Line Tools (`xcode-select --install`), CMake 3.22+, Git | `cmake --version`, `git --version`, `xcode-select -p` |
| Linux | g++ or clang, CMake 3.22+, Git, and development packages for ALSA, X11 (xrandr, xinerama, xcursor), Freetype and fontconfig (Debian/Ubuntu: `libasound2-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libfreetype-dev libfontconfig1-dev`) | `cmake --version` |

If something is missing, tell the user what it is, why it is needed and how to install it, then wait for their OK.

## 3. Build
From this folder:

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

- The first configure downloads JUCE 8.0.15 from GitHub (internet needed, a few minutes).
- On macOS, for a plugin that works on both Apple Silicon and Intel, add `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"` to the first command.
- Results: `build/FastVocalCompBasic_artefacts/Release/VST3/FastVocal Comp Basic.vst3` (and `.../AU/FastVocal Comp Basic.component` on macOS).

## 4. Install
Copy the whole `.vst3` folder (it is a folder, not a single file) to the user's plugin folder:

| System | VST3 | AU |
|---|---|---|
| Windows | `%LOCALAPPDATA%\Programs\Common\VST3\` (current user) or `C:\Program Files\Common Files\VST3\` (all users, needs admin) | — |
| macOS | `~/Library/Audio/Plug-Ins/VST3/` | `~/Library/Audio/Plug-Ins/Components/` |
| Linux | `~/.vst3/` | — |

On macOS, sign the copied plugins locally so the system accepts them:

```
codesign --force --deep -s - "~/Library/Audio/Plug-Ins/VST3/FastVocal Comp Basic.vst3"
codesign --force --deep -s - "~/Library/Audio/Plug-Ins/Components/FastVocal Comp Basic.component"
```

## 5. Check and hand over
- Confirm the files are in place.
- Tell the user to restart their DAW (or rescan plugins). The plugin appears as **FastVocal Comp Basic**, vendor **TwinRoots Audio**, category Dynamics.
- Logic Pro: if the AU does not show up, run `killall -9 AudioComponentRegistrar` and restart Logic.
- Remind them: Compression sets the amount, Output trim fine-tunes the level, Bypass compares with the original at the same latency.
