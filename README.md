# Cockamps Hall Reverb

**Analog-modelled hall reverb VST3 for Linux and Windows.**

Built by D. Kearsley / Cockamps Audio.

---

## What it is

Warm, diffuse hall reverb with subtle modulation to prevent metallic artifacts.
MASSIVE mode pushes decay and diffusion into cathedral territory.

---

## Signal Chain

```
Guitar In -> Pre-delay -> Hall Reverb Network -> Tone Filter -> Output
```

---

## Controls

| Control | What it does |
|---------|---------------|
| **Size** | Decay length / room scale |
| **Mix** | Dry/wet blend |
| **Tone** | Brightness of tail |
| **Pre-Delay** | Gap before tail begins |
| **Mod** | Modulation depth |
| **Drip** | Echo bounce injected into tail |
| **MASSIVE** | Cathedral mode toggle |
| **Power** | Master bypass |

---

## Formats

- VST3 (Linux x86_64)
- VST3 (Windows x64)

No macOS. No AU. No AAX.

---

## Install

### Linux
Copy `CockReverb.vst3` to `~/.vst3/` or `/usr/lib/vst3/`

### Windows
Copy `CockReverb.vst3` to `C:\Program Files\Common Files\VST3\`

Then rescan plugins in your DAW.

---

## Build from source

### Linux
```bash
git clone --branch 7.0.9 --depth 1 https://github.com/juce-framework/JUCE.git ../JUCE
chmod +x build-linux.sh && ./build-linux.sh
```

### Windows (PowerShell)
```powershell
git clone --branch 7.0.9 --depth 1 https://github.com/juce-framework/JUCE.git ..\JUCE
.\build-windows.ps1
```

---

## Built with

- JUCE 7.0.9
- CMake 3.22+
- C++17

---

Cockamps Audio - Professional Audio Analog Emulation
