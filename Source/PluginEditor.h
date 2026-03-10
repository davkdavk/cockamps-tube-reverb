#pragma once

#include <JuceHeader.h>
#include <memory>

#include "PluginProcessor.h"

class ReverbLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void setUiScale(float newScale) { uiScale = newScale; }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override;

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool isHovered, bool isDown) override;

private:
    float uiScale = 1.0f;
};

class InvisibleToggleLAF final : public juce::LookAndFeel_V4
{
public:
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool, bool) override {}
};

class KnobRow final : public juce::Component
{
public:
    KnobRow(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId,
            const juce::String& labelText, ReverbLookAndFeel& laf);

    void setLabelHeight(int newHeight) { labelHeight = newHeight; }

    void resized() override;

private:
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    int labelHeight = 20;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KnobRow)
};

class CockReverbAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit CockReverbAudioProcessorEditor(CockReverbAudioProcessor& processor);
    ~CockReverbAudioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    CockReverbAudioProcessor& processor;
    ReverbLookAndFeel laf;
    InvisibleToggleLAF invisibleLaf;


    KnobRow knobSize { processor.apvts, "size", "SIZE", laf };
    KnobRow knobMix { processor.apvts, "mix", "MIX", laf };
    KnobRow knobTone { processor.apvts, "tone", "TONE", laf };
    KnobRow knobPredelay { processor.apvts, "predelay", "PRE-DELAY", laf };
    KnobRow knobMod { processor.apvts, "modDepth", "MOD", laf };
    KnobRow knobHall { processor.apvts, "hall", "CHAPEL", laf };
    KnobRow knobDrip { processor.apvts, "drip", "DRIP", laf };

    juce::ToggleButton massiveToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> massiveAttachment;

    juce::ToggleButton powerToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CockReverbAudioProcessorEditor)
};
