#include "ReverbEngine.h"

void ReverbEngine::prepare(double newSampleRate, int, int newNumChannels)
{
    sampleRate = newSampleRate;
    numChannels = juce::jmin(2, newNumChannels);

    for (int ch = 0; ch < 2; ++ch)
    {
        preDelayBuf[ch].assign(kMaxPreDelaySamples, 0.0f);
        erBuf[ch].assign(kMaxErSamples, 0.0f);
        dripBuf[ch].assign(kMaxDripSamples, 0.0f);
    }

    for (int i = 0; i < 8; ++i)
        combBuf[i].assign(kMaxCombSamples, 0.0f);

    for (int i = 0; i < 4; ++i)
        for (int ch = 0; ch < 2; ++ch)
            apBuf[i][ch].assign(kMaxAllpassSamples, 0.0f);

    reset();
}

void ReverbEngine::reset()
{
    for (int ch = 0; ch < 2; ++ch)
    {
        std::fill(preDelayBuf[ch].begin(), preDelayBuf[ch].end(), 0.0f);
        std::fill(erBuf[ch].begin(), erBuf[ch].end(), 0.0f);
        std::fill(dripBuf[ch].begin(), dripBuf[ch].end(), 0.0f);
        preDelayWrite[ch] = 0;
        erWrite[ch] = 0;
        dripWrite[ch] = 0;
        toneState[ch] = 0.0f;
    }

    for (int i = 0; i < 8; ++i)
    {
        std::fill(combBuf[i].begin(), combBuf[i].end(), 0.0f);
        combWrite[i] = 0;
        combDampState[i] = 0.0f;
        modPhase[i] = i * (1.0 / 8.0);
    }

    for (int i = 0; i < 4; ++i)
        for (int ch = 0; ch < 2; ++ch)
        {
            std::fill(apBuf[i][ch].begin(), apBuf[i][ch].end(), 0.0f);
            apWrite[i][ch] = 0;
        }
}

void ReverbEngine::process(juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();

    const float effectiveSize = massive ? juce::jmin(1.0f, size * 1.6f + 0.2f) : size;
    const float effectiveDrip = massive ? juce::jmin(1.0f, drip * 1.4f) : drip;
    const float effectiveMod = massive ? juce::jmin(1.0f, modDepth * 1.3f) : modDepth;

    const float feedback = 0.5f + effectiveSize * (massive ? 0.40f : 0.38f);

    const int preDelaySamp = juce::jlimit(0, kMaxPreDelaySamples - 1,
        (int)(predelayMs * (float) sampleRate / 1000.0f));

    const float dripMs = 60.0f + effectiveSize * 80.0f;
    const int dripSamp = juce::jlimit(1, kMaxDripSamples - 1,
        (int)(dripMs * (float) sampleRate / 1000.0f));

    const float toneCutoff = 3000.0f + tone * 14000.0f;
    const float toneCoeff = std::exp(
        -juce::MathConstants<float>::twoPi * toneCutoff / (float) sampleRate);

    const float dampCutoff = 1500.0f + tone * 8000.0f;
    const float dampCoeff = std::exp(
        -juce::MathConstants<float>::twoPi * dampCutoff / (float) sampleRate);

    int combSamp[8];
    for (int i = 0; i < 8; ++i)
    {
        float hallScale = 0.5f + hall * 1.5f;
        float ms = kCombDelayMs[i] * hallScale * (0.5f + effectiveSize * 1.5f);
        combSamp[i] = juce::jlimit(1, kMaxCombSamples - 1,
            (int)(ms * (float) sampleRate / 1000.0f));
    }

    int apSamp[4];
    for (int i = 0; i < 4; ++i)
    {
        apSamp[i] = juce::jlimit(1, kMaxAllpassSamples - 1,
            (int)(kAllpassDelayMs[i] * (float) sampleRate / 1000.0f));
    }

    int erSamp[6];
    for (int i = 0; i < 6; ++i)
        erSamp[i] = juce::jlimit(1, kMaxErSamples - 1,
            (int)(kErDelayMs[i] * (0.7f + hall * 0.6f) * (float) sampleRate / 1000.0f));

    const float erGains[6] = { 0.55f, 0.45f, 0.38f, 0.30f, 0.22f, 0.15f };

    double modInc[8];
    for (int i = 0; i < 8; ++i)
        modInc[i] = (0.15 + i * 0.03) / sampleRate;

    for (int n = 0; n < numSamples; ++n)
    {
        for (int i = 0; i < 8; ++i)
        {
            modPhase[i] += modInc[i];
            if (modPhase[i] >= 1.0)
                modPhase[i] -= 1.0;
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float input = buffer.getSample(ch, n);

            int pdSize = (int) preDelayBuf[ch].size();
            preDelayBuf[ch][preDelayWrite[ch] % pdSize] = input;
            int pdRead = (preDelayWrite[ch] - preDelaySamp + pdSize) % pdSize;
            float pdOut = preDelayBuf[ch][pdRead];

            erBuf[ch][erWrite[ch] % kMaxErSamples] = pdOut;

            float erSum = 0.0f;
            for (int t = 0; t < 6; ++t)
            {
                int erRead = (erWrite[ch] - erSamp[t] + kMaxErSamples) % kMaxErSamples;
                erSum += erBuf[ch][erRead] * erGains[t];
            }

            float networkIn = pdOut * 0.3f + erSum * 0.7f;

            int base = ch * 4;
            float combSum = 0.0f;

            for (int i = 0; i < 4; ++i)
            {
                int idx = base + i;
                int bufSize = (int) combBuf[idx].size();

                float modAmt = (float) std::sin(modPhase[idx] * juce::MathConstants<double>::twoPi);
                int modSamp = (int)(modAmt * effectiveMod * 2.0f);
                int readIdx = (combWrite[idx] - combSamp[idx] + modSamp + bufSize * 2) % bufSize;

                float delayed = combBuf[idx][readIdx];

                combDampState[idx] = delayed + dampCoeff * (combDampState[idx] - delayed);
                float dampedFeedback = combDampState[idx] * feedback;

                float writeVal = juce::jlimit(-1.0f, 1.0f, networkIn + dampedFeedback);
                combBuf[idx][combWrite[idx] % bufSize] = writeVal;
                combSum += delayed;
            }

            float diffuse = combSum * 0.25f;

            for (int i = 0; i < 4; ++i)
            {
                int apSize = (int) apBuf[i][ch].size();
                int readIdx = (apWrite[i][ch] - apSamp[i] + apSize) % apSize;
                float apDelayed = apBuf[i][ch][readIdx];
                float apIn = diffuse + apDelayed * 0.5f;
                apBuf[i][ch][apWrite[i][ch] % apSize] = apIn;
                diffuse = apDelayed - 0.5f * apIn;
            }

            toneState[ch] = diffuse + toneCoeff * (toneState[ch] - diffuse);
            float wet = toneState[ch];

            int dripBufSize = (int) dripBuf[ch].size();
            int dripRead = (dripWrite[ch] - dripSamp + dripBufSize) % dripBufSize;
            float dripOut = dripBuf[ch][dripRead] * effectiveDrip * 0.25f;
            dripBuf[ch][dripWrite[ch] % dripBufSize] = wet;

            float finalWet = wet + dripOut;
            buffer.setSample(ch, n, input * (1.0f - mix) + finalWet * mix);
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            ++preDelayWrite[ch];
            if (preDelayWrite[ch] >= (int) preDelayBuf[ch].size())
                preDelayWrite[ch] = 0;

            ++erWrite[ch];
            if (erWrite[ch] >= (int) erBuf[ch].size())
                erWrite[ch] = 0;

            ++dripWrite[ch];
            if (dripWrite[ch] >= (int) dripBuf[ch].size())
                dripWrite[ch] = 0;
        }

        for (int i = 0; i < 8; ++i)
        {
            ++combWrite[i];
            if (combWrite[i] >= (int) combBuf[i].size())
                combWrite[i] = 0;
        }

        for (int i = 0; i < 4; ++i)
            for (int ch = 0; ch < numChannels; ++ch)
            {
                ++apWrite[i][ch];
                if (apWrite[i][ch] >= (int) apBuf[i][ch].size())
                    apWrite[i][ch] = 0;
            }
    }
}
