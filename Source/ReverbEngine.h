#pragma once

#include <JuceHeader.h>
#include <vector>

class ReverbEngine
{
public:
    void prepare(double newSampleRate, int maxBlockSize, int newNumChannels);
    void reset();
    void process(juce::AudioBuffer<float>& buffer);

    float size = 0.55f;
    float mix = 0.35f;
    float tone = 0.50f;
    float predelayMs = 20.0f;
    float modDepth = 0.25f;
    float hall = 0.50f;
    float drip = 0.20f;
    bool massive = false;

private:
    static constexpr float kCombDelayMs[8] = {
        53.3f, 61.7f, 71.1f, 83.5f,
        55.1f, 64.3f, 73.9f, 86.7f
    };

    static constexpr float kAllpassDelayMs[4] = { 5.0f, 1.7f, 0.7f, 0.3f };

    static constexpr int kMaxPreDelaySamples = 8192;
    static constexpr int kMaxCombSamples = 16384;
    static constexpr int kMaxAllpassSamples = 1024;
    static constexpr int kMaxDripSamples = 16384;
    static constexpr float kErDelayMs[6] = { 7.1f, 14.3f, 21.9f, 34.7f, 52.3f, 71.1f };
    static constexpr int kMaxErSamples = 8192;

    double sampleRate = 48000.0;
    int numChannels = 2;

    std::vector<float> preDelayBuf[2];
    int preDelayWrite[2] = {};

    std::vector<float> erBuf[2];
    int erWrite[2] = {};

    std::vector<float> combBuf[8];
    int combWrite[8] = {};
    float combDampState[8] = {};

    std::vector<float> apBuf[4][2];
    int apWrite[4][2] = {};

    std::vector<float> dripBuf[2];
    int dripWrite[2] = {};

    double modPhase[8] = {};

    float toneState[2] = {};
};
