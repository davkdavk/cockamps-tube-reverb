# AGENTS.md — Cockamps Hall Reverb VST3

---

## READ THIS FIRST

Read this entire file before touching any code. After every change rebuild and reinstall:
```bash
rm -rf build && ./build-linux.sh && cp -r build/**/CockReverb.vst3 ~/.vst3/
```
Do not ask for confirmation on routine tasks. Only pause for irreversible changes
(plugin IDs, parameter tree restructure, deleting state serialisation).

---

## What this plugin is

A VST3 analog-modelled hall reverb for Linux (x86_64) and Windows (x64).
No macOS. No AU. No AAX.

Modelled on the character of large analog hall reverb units — warm, diffuse, slightly
dark, with subtle modulation to prevent metallic artefacts. MASSIVE mode pushes the
decay and diffusion to cathedral/large church scale.

**Signal chain:**
```
Guitar In → Pre-delay → Hall Reverb Network → Tone Filter → Output
```

**Framework**: JUCE 7.0.9. Do not upgrade without instruction.
**Build**: CMake 3.22+ via `build-linux.sh`. Never use Projucer.

---

## Plugin Identity — NEVER CHANGE THESE

- **Plugin name**: CockReverb
- **PLUGIN_MANUFACTURER_CODE**: Ystu
- **PLUGIN_CODE**: Crvs
- **Manufacturer**: Cockamps Audio

---

## Directory map

```
CockReverbVST/
├── AGENTS.md
├── CMakeLists.txt
├── README.md
├── build-linux.sh
├── build-windows.ps1
├── .github/workflows/build.yml
└── Source/
    ├── ReverbEngine.h / .cpp    ← Hall reverb DSP engine
    ├── PluginProcessor.h / .cpp ← owns engine + all parameters
    ├── PluginEditor.h / .cpp    ← UI + knobs + MASSIVE toggle + power switch
    └── CockReverbBG.png         ← background image (to be provided)
```

---

## Parameters — complete list

| APVTS ID    | Label      | Range         | Default | Notes                                         |
|-------------|------------|---------------|---------|-----------------------------------------------|
| `size`      | SIZE       | 0.1–1.0       | 0.55    | Room scale / decay time                       |
| `mix`       | MIX        | 0.0–1.0       | 0.35    | Dry/wet blend                                 |
| `tone`      | TONE       | 0.0–1.0       | 0.50    | 0=dark/warm, 1=open/bright                    |
| `predelay`  | PRE-DELAY  | 0–100 ms      | 20.0    | Delay before reverb tail begins               |
| `modDepth`  | MOD        | 0.0–1.0       | 0.25    | Modulation depth — pitch warble on tail       |
| `drip`      | DRIP       | 0.0–1.0       | 0.20    | Spring-style echo bounce injected into tail   |
| `massive`   | MASSIVE    | bool          | false   | Cathedral mode — long decay, max diffusion    |
| `bypass`    | POWER      | bool          | false   | Master power switch                           |

---

## processBlock() — exact flow

```cpp
void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (apvts.getRawParameterValue("bypass")->load() > 0.5f)
        return;

    updateEngineParams();
    reverbEngine.process(buffer);
}
```

---

## updateEngineParams()

```cpp
void updateEngineParams()
{
    reverbEngine.size      = apvts.getRawParameterValue("size")->load();
    reverbEngine.mix       = apvts.getRawParameterValue("mix")->load();
    reverbEngine.tone      = apvts.getRawParameterValue("tone")->load();
    reverbEngine.predelayMs = apvts.getRawParameterValue("predelay")->load();
    reverbEngine.modDepth  = apvts.getRawParameterValue("modDepth")->load();
    reverbEngine.drip      = apvts.getRawParameterValue("drip")->load();
    reverbEngine.massive   = apvts.getRawParameterValue("massive")->load() > 0.5f;
}
```

---

## DSP: ReverbEngine

### Architecture — Schroeder/Moorer hall reverb network

8 parallel comb filters → 4 series allpass diffusers → stereo output
Modulation applied to comb filter delay times to prevent metallic colouration.
Pre-delay line before the network.
Drip: a single long delayed reflection injected back into the input of the network.

### Constants

```cpp
// Comb filter delay times in ms (prime-ish, slightly asymmetric for stereo)
static constexpr float kCombDelayMs[8] = {
    29.7f, 37.1f, 41.1f, 43.7f,   // L channel
    30.3f, 37.9f, 41.9f, 44.3f    // R channel (slightly detuned)
};

// Allpass delay times in ms
static constexpr float kAllpassDelayMs[4] = { 5.0f, 1.7f, 0.7f, 0.3f };

static constexpr int kMaxPreDelaySamples = 8192;   // ~170ms at 48kHz
static constexpr int kMaxCombSamples     = 4096;
static constexpr int kMaxAllpassSamples  = 1024;
static constexpr int kMaxDripSamples     = 16384;  // ~340ms
```

### Public fields

```cpp
float size       = 0.55f;
float mix        = 0.35f;
float tone       = 0.50f;
float predelayMs = 20.0f;
float modDepth   = 0.25f;
float drip       = 0.20f;
bool  massive    = false;
```

### Private fields

```cpp
double sampleRate  = 48000.0;
int    numChannels = 2;

// Pre-delay buffer
std::vector<float> preDelayBuf[2];
int preDelayWrite[2] = {};

// Comb filters — 4 per channel
std::vector<float> combBuf[8];   // combBuf[0..3] = L, combBuf[4..7] = R
int   combWrite[8]   = {};
float combState[8]   = {};  // feedback state

// Allpass filters — 4 in series (shared L/R)
std::vector<float> apBuf[4][2];
int   apWrite[4][2] = {};

// Drip delay
std::vector<float> dripBuf[2];
int dripWrite[2] = {};

// Modulation LFOs — one per comb filter
double modPhase[8] = {};

// Tone filter state
float toneState[2] = {};
```

### prepare()

```cpp
void prepare(double newSampleRate, int maxBlockSize, int newNumChannels)
{
    sampleRate  = newSampleRate;
    numChannels = juce::jmin(2, newNumChannels);

    for (int ch = 0; ch < 2; ++ch)
    {
        preDelayBuf[ch].assign(kMaxPreDelaySamples, 0.0f);
        dripBuf[ch].assign(kMaxDripSamples, 0.0f);
    }

    for (int i = 0; i < 8; ++i)
        combBuf[i].assign(kMaxCombSamples, 0.0f);

    for (int i = 0; i < 4; ++i)
        for (int ch = 0; ch < 2; ++ch)
            apBuf[i][ch].assign(kMaxAllpassSamples, 0.0f);

    reset();
}
```

### reset()

```cpp
void reset()
{
    for (int ch = 0; ch < 2; ++ch)
    {
        std::fill(preDelayBuf[ch].begin(), preDelayBuf[ch].end(), 0.0f);
        std::fill(dripBuf[ch].begin(), dripBuf[ch].end(), 0.0f);
        preDelayWrite[ch] = 0;
        dripWrite[ch]     = 0;
        toneState[ch]     = 0.0f;
    }

    for (int i = 0; i < 8; ++i)
    {
        std::fill(combBuf[i].begin(), combBuf[i].end(), 0.0f);
        combWrite[i] = 0;
        combState[i] = 0.0f;
        modPhase[i]  = i * (1.0 / 8.0);
    }

    for (int i = 0; i < 4; ++i)
        for (int ch = 0; ch < 2; ++ch)
        {
            std::fill(apBuf[i][ch].begin(), apBuf[i][ch].end(), 0.0f);
            apWrite[i][ch] = 0;
        }
}
```

### process() — per sample logic

```cpp
void ReverbEngine::process(juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();

    // In MASSIVE mode: push size toward max, increase diffusion, lengthen drip
    const float effectiveSize  = massive ? juce::jmin(1.0f, size * 1.6f + 0.2f) : size;
    const float effectiveDrip  = massive ? juce::jmin(1.0f, drip * 1.4f)        : drip;
    const float effectiveMod   = massive ? juce::jmin(1.0f, modDepth * 1.3f)    : modDepth;

    // Feedback gain from size (0.1→0.93 normal, up to 0.97 massive)
    const float feedback = 0.5f + effectiveSize * (massive ? 0.47f : 0.43f);

    // Pre-delay samples
    const int preDelaySamp = juce::jlimit(0, kMaxPreDelaySamples - 1,
        (int)(predelayMs * (float)sampleRate / 1000.0f));

    // Drip delay time — long echo bounce
    const float dripMs     = 60.0f + effectiveSize * 80.0f;
    const int   dripSamp   = juce::jlimit(1, kMaxDripSamples - 1,
        (int)(dripMs * (float)sampleRate / 1000.0f));

    // Tone filter coefficient
    const float toneCutoff = 3000.0f + tone * 14000.0f;
    const float toneCoeff  = std::exp(
        -juce::MathConstants<float>::twoPi * toneCutoff / (float)sampleRate);

    // Comb delay times in samples (with size scaling)
    int combSamp[8];
    for (int i = 0; i < 8; ++i)
    {
        float ms = kCombDelayMs[i] * (0.5f + effectiveSize * 1.5f);
        combSamp[i] = juce::jlimit(1, kMaxCombSamples - 1,
            (int)(ms * (float)sampleRate / 1000.0f));
    }

    // Allpass delay times in samples
    int apSamp[4];
    for (int i = 0; i < 4; ++i)
    {
        apSamp[i] = juce::jlimit(1, kMaxAllpassSamples - 1,
            (int)(kAllpassDelayMs[i] * (float)sampleRate / 1000.0f));
    }

    // Modulation LFO increments — slightly different per comb, very slow
    double modInc[8];
    for (int i = 0; i < 8; ++i)
        modInc[i] = (0.15 + i * 0.03) / sampleRate;

    for (int n = 0; n < numSamples; ++n)
    {
        // Advance modulation LFOs
        for (int i = 0; i < 8; ++i)
        {
            modPhase[i] += modInc[i];
            if (modPhase[i] >= 1.0) modPhase[i] -= 1.0;
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float input = buffer.getSample(ch, n);

            // --- Pre-delay ---
            int pdSize = (int)preDelayBuf[ch].size();
            preDelayBuf[ch][preDelayWrite[ch] % pdSize] = input;
            int pdRead = (preDelayWrite[ch] - preDelaySamp + pdSize) % pdSize;
            float pdOut = preDelayBuf[ch][pdRead];

            // --- Drip injection ---
            int dripBufSize = (int)dripBuf[ch].size();
            int dripRead    = (dripWrite[ch] - dripSamp + dripBufSize) % dripBufSize;
            float dripOut   = dripBuf[ch][dripRead] * effectiveDrip * 0.35f;
            float networkIn = pdOut + dripOut;

            // --- 4 parallel comb filters for this channel ---
            int base = ch * 4;  // combs 0-3 for L, 4-7 for R
            float combSum = 0.0f;

            for (int i = 0; i < 4; ++i)
            {
                int idx     = base + i;
                int bufSize = (int)combBuf[idx].size();

                // Modulation: ±2 samples at full mod depth
                float modAmt  = (float)std::sin(modPhase[idx] * juce::MathConstants<double>::twoPi);
                int   modSamp = (int)(modAmt * effectiveMod * 2.0f);
                int   readIdx = (combWrite[idx] - combSamp[idx] + modSamp + bufSize * 2) % bufSize;

                float delayed  = combBuf[idx][readIdx];
                float filtered = delayed - 0.15f * combState[idx];  // simple 1-pole damp
                combState[idx] = filtered;

                combBuf[idx][combWrite[idx] % bufSize] = networkIn + filtered * feedback;
                combSum += delayed;
            }

            float diffuse = combSum * 0.25f;

            // --- 4 series allpass diffusers ---
            for (int i = 0; i < 4; ++i)
            {
                int   apSize   = (int)apBuf[i][ch].size();
                int   readIdx  = (apWrite[i][ch] - apSamp[i] + apSize) % apSize;
                float apDelayed = apBuf[i][ch][readIdx];
                float apIn     = diffuse + apDelayed * 0.5f;
                apBuf[i][ch][apWrite[i][ch] % apSize] = apIn;
                diffuse = apDelayed - 0.5f * apIn;
            }

            // --- Tone filter on wet signal ---
            toneState[ch] = diffuse + toneCoeff * (toneState[ch] - diffuse);
            float wet = toneState[ch];

            // --- Write to drip buffer ---
            dripBuf[ch][dripWrite[ch] % dripBufSize] = wet;

            buffer.setSample(ch, n, input * (1.0f - mix) + wet * mix);
        }

        // Advance write positions AFTER channel loop
        for (int ch = 0; ch < numChannels; ++ch)
        {
            ++preDelayWrite[ch];
            if (preDelayWrite[ch] >= (int)preDelayBuf[ch].size()) preDelayWrite[ch] = 0;

            ++dripWrite[ch];
            if (dripWrite[ch] >= (int)dripBuf[ch].size()) dripWrite[ch] = 0;
        }

        for (int i = 0; i < 8; ++i)
        {
            ++combWrite[i];
            if (combWrite[i] >= (int)combBuf[i].size()) combWrite[i] = 0;
        }

        for (int i = 0; i < 4; ++i)
            for (int ch = 0; ch < numChannels; ++ch)
            {
                ++apWrite[i][ch];
                if (apWrite[i][ch] >= (int)apBuf[i][ch].size()) apWrite[i][ch] = 0;
            }
    }
}
```

**Critical rules:**
- Never advance any write pointer inside the per-channel loop
- Modulation LFO phases must be `double`
- Comb and allpass delay times calculated once per block not per sample

---

## MASSIVE mode behaviour

When `massive = true`:
- `size` pushed toward max decay (feedback up to 0.97)
- Drip amount increased by 1.4x — longer bounce echoes
- Modulation increased by 1.3x — more lush movement in the tail
- Result: enormous cathedral/large church reverb — dense, long, enveloping

---

## UI

### Window
Fixed size matching background image. Background is `CockReverbBG.png` as JUCE binary resource,
drawn filling the entire window in `paint()`.

### Knob layout
Knob size: 90x90. Labels white, font Courier New bold 9pt, below each knob.
Knob arc colour: `#E8B85A`. Body: semi-transparent dark.
Rotary parameters:
```cpp
slider.setRotaryParameters(
    juce::MathConstants<float>::pi * 1.25f,   // 7 o'clock at minimum
    juce::MathConstants<float>::pi * 2.75f,   // 5 o'clock at maximum
    true
);
```

**Knob row (y=380, centered in panel):**
Size x=230, Mix x=380, Tone x=530, Pre-Delay x=680, Mod x=830, Drip x=980

### MASSIVE toggle
Position: x=1080, y=400, width=130, height=40
Style: subtle toggle switch — no glow, no gold fill.
- OFF: dark background `#1A1A1A`, thin white border 1px, white label "MASSIVE" small font
- ON: dark background `#2A2A2A`, thin gold border `#E8B85A` 1px, gold label "MASSIVE"
- No animation, no glow — understated

```cpp
bool massiveOn = apvts.getRawParameterValue("massive")->load() > 0.5f;
g.setColour(juce::Colour(0xFF1A1A1A));
g.fillRoundedRectangle(massiveX, massiveY, massiveW, massiveH, 3.0f);
if (massiveOn)
{
    g.setColour(juce::Colour(0xFFE8B85A));
    g.drawRoundedRectangle(massiveX, massiveY, massiveW, massiveH, 3.0f, 1.0f);
    g.setColour(juce::Colour(0xFFE8B85A));
}
else
{
    g.setColour(juce::Colours::white.withAlpha(0.6f));
    g.drawRoundedRectangle(massiveX, massiveY, massiveW, massiveH, 3.0f, 1.0f);
    g.setColour(juce::Colours::white.withAlpha(0.6f));
}
g.setFont(juce::Font("Courier New", 11.0f, juce::Font::bold));
g.drawText("MASSIVE", massiveBounds, juce::Justification::centred);
```

### Power switch (bottom right of background image)
Invisible `juce::ToggleButton` at **x=1310, y=685, width=50, height=60**.
Attached to `bypass` parameter. Calls repaint() on click.

In `paint()`:
```cpp
bool isPowered = apvts.getRawParameterValue("bypass")->load() < 0.5f;

if (isPowered)
{
    g.setColour(juce::Colour(0xFFFFAA00).withAlpha(0.85f));
    g.fillEllipse(1238, 690, 28, 28);
    g.setColour(juce::Colour(0xFFFFAA00).withAlpha(0.25f));
    g.fillEllipse(1228, 680, 48, 48);
}
else
{
    g.setColour(juce::Colour(0xFF332200).withAlpha(0.85f));
    g.fillEllipse(1238, 690, 28, 28);
}

const float switchX = 1335.0f;
const float switchY = isPowered ? 715.0f : 695.0f;
g.setColour(juce::Colour(0xFF888888));
g.fillRoundedRectangle(switchX - 6, switchY - 14, 12, 28, 4.0f);
g.setColour(juce::Colour(0xFF444444));
g.drawRoundedRectangle(switchX - 6, switchY - 14, 12, 28, 4.0f, 1.5f);
```

---

## CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.22)
project(CockReverb VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)

if(NOT DEFINED JUCE_PATH)
    set(JUCE_PATH "${CMAKE_SOURCE_DIR}/../JUCE")
endif()
add_subdirectory(${JUCE_PATH} JUCE EXCLUDE_FROM_ALL)

juce_add_plugin(CockReverb
    PLUGIN_MANUFACTURER_CODE Ystu
    PLUGIN_CODE             Crvs
    FORMATS                 VST3
    PRODUCT_NAME            "CockReverb"
    COMPANY_NAME            "Cockamps Audio"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_add_binary_data(CockReverbData
    SOURCES Source/CockReverbBG.png
)

target_sources(CockReverb PRIVATE
    Source/ReverbEngine.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(CockReverb PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

if(UNIX AND NOT APPLE)
    target_compile_options(CockReverb PRIVATE -fvisibility=hidden)
endif()

if(WIN32)
    target_compile_definitions(CockReverb PUBLIC NOMINMAX)
endif()

target_link_libraries(CockReverb PRIVATE
    CockReverbData
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_basics
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
```

---

## build-linux.sh

```bash
#!/bin/bash
set -e
JUCE_PATH=$(realpath ../JUCE)
if [ ! -d "$JUCE_PATH" ]; then
    echo "ERROR: JUCE not found at $JUCE_PATH"
    echo "Run: git clone --branch 7.0.9 --depth 1 https://github.com/juce-framework/JUCE.git ../JUCE"
    exit 1
fi
rm -rf build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DJUCE_PATH="$JUCE_PATH"
cmake --build build --parallel $(nproc)
echo "Build complete."
```

---

## GitHub Actions — .github/workflows/build.yml

```yaml
name: Build

on:
  push:
    branches: [ main, master ]
    tags: [ 'v*' ]
  pull_request:
    branches: [ main, master ]

jobs:
  build-linux:
    runs-on: ubuntu-22.04
    steps:
      - uses: actions/checkout@v3
      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y build-essential cmake ninja-build pkg-config \
            libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxext-dev \
            libfreetype6-dev libasound2-dev libfontconfig1-dev libgl1-mesa-dev
      - name: Clone JUCE
        run: git clone --branch 7.0.9 --depth 1 https://github.com/juce-framework/JUCE.git ../JUCE
      - name: Build
        run: |
          cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DJUCE_PATH=$(realpath ../JUCE)
          cmake --build build --parallel
      - name: Upload artifact
        uses: actions/upload-artifact@v3
        with:
          name: CockReverb-Linux-VST3
          path: build/**/*.vst3

  build-windows:
    runs-on: windows-2022
    steps:
      - uses: actions/checkout@v3
      - name: Clone JUCE
        run: git clone --branch 7.0.9 --depth 1 https://github.com/juce-framework/JUCE.git ..\JUCE
      - name: Build
        run: |
          cmake -B build -G "Visual Studio 17 2022" -A x64 -DJUCE_PATH="..\JUCE"
          cmake --build build --config Release --parallel
      - name: Upload artifact
        uses: actions/upload-artifact@v3
        with:
          name: CockReverb-Windows-VST3
          path: build/**/*.vst3

  release:
    if: startsWith(github.ref, 'refs/tags/v')
    needs: [build-linux, build-windows]
    runs-on: ubuntu-22.04
    steps:
      - uses: actions/download-artifact@v3
        with:
          name: CockReverb-Linux-VST3
          path: artifacts/linux
      - uses: actions/download-artifact@v3
        with:
          name: CockReverb-Windows-VST3
          path: artifacts/windows
      - name: Create Release
        uses: softprops/action-gh-release@v1
        with:
          files: |
            artifacts/linux/**/*.vst3
            artifacts/windows/**/*.vst3
        env:
          GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

---

## Hard rules — never break these

| Rule | Consequence |
|---|---|
| Keep `-fvisibility=hidden` in Linux CMake block | Symbol collisions crash Reaper |
| Keep `NOMINMAX` in Windows CMake block | Breaks std::min/max on MSVC |
| Keep `FORMATS VST3` only | AU breaks Linux/Windows build |
| Never change `PLUGIN_CODE Crvs` or `PLUGIN_MANUFACTURER_CODE Ystu` | Breaks all existing Reaper sessions |
| Never advance any write pointer inside the per-channel loop | Phase drift and buffer corruption |
| Modulation LFO phases must be `double` | Float drift becomes audible pitch error |
| Never use std::cout or printf | Use DBG() only |
| getRawParameterValue()->load() only on audio thread | |
| Comb/allpass delay times calculated once per block not per sample | CPU kill |

---

## Glossary

| Term | Meaning |
|---|---|
| Size | Controls feedback gain — larger = longer decay tail |
| Mix | Dry/wet blend |
| Tone | Brightness of reverb tail — 0=dark/warm, 1=open/bright |
| Pre-delay | Gap between dry signal and reverb onset — creates sense of space |
| Mod | Modulation depth — subtle pitch warble prevents metallic flutter |
| Drip | Long echo bounce injected into network input — spring-style texture |
| MASSIVE | Cathedral mode — pushes size, drip and mod to maximum for enormous church sound |
| Comb filter | Core reverb building block — delayed feedback loop with damping |
| Allpass | Diffusion stage — smears reflections to create smooth density |
| bypass | Master power switch — true = fully bypassed |
