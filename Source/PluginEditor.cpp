#include "PluginEditor.h"

static constexpr float kUiScale = 0.6f;

static juce::Colour colourFromHex(juce::uint32 value)
{
    return juce::Colour((juce::uint32) value);
}

void ReverbLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                         juce::Slider&)
{
    const auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height);
    const float inset = 6.0f * uiScale;
    const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f - inset;
    const auto centre = bounds.getCentre();

    const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    const auto body = juce::Colour(0xCC1A1208);
    const auto accent = colourFromHex(0xFFE8B85A);

    g.setColour(body);
    g.fillEllipse(centre.getX() - radius, centre.getY() - radius,
                  radius * 2.0f, radius * 2.0f);

    g.setColour(colourFromHex(0xFF5A4820));
    g.drawEllipse(centre.getX() - radius, centre.getY() - radius,
                  radius * 2.0f, radius * 2.0f, 1.5f * uiScale);

    const float arcRadius = radius + inset;
    juce::Path arc;
    arc.addArc(centre.getX() - arcRadius, centre.getY() - arcRadius,
               arcRadius * 2.0f, arcRadius * 2.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(colourFromHex(0xFF2A1E10));
    g.strokePath(arc, juce::PathStrokeType(2.5f * uiScale));

    juce::Path arcValue;
    arcValue.addArc(centre.getX() - arcRadius, centre.getY() - arcRadius,
                    arcRadius * 2.0f, arcRadius * 2.0f, rotaryStartAngle, angle, true);
    g.setColour(accent);
    g.strokePath(arcValue, juce::PathStrokeType(3.0f * uiScale));

    const float pointerAngle = angle - (juce::MathConstants<float>::pi * 0.5f);
    juce::Line<float> pointer(centre.getX(), centre.getY(),
                               centre.getX() + radius * std::cos(pointerAngle),
                               centre.getY() + radius * std::sin(pointerAngle));
    g.setColour(colourFromHex(0xFFFFD87F));
    g.drawLine(pointer, 2.0f * uiScale);
}

void ReverbLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                         bool isHovered, bool)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(2.0f);
    const auto base = colourFromHex(0xFF1E160A);
    const auto border = colourFromHex(0xFF5A4820);
    const auto accent = colourFromHex(0xFFE8B85A);

    g.setColour(base);
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(border);
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

    if (button.getToggleState())
    {
        g.setColour(accent.withAlpha(isHovered ? 0.9f : 0.75f));
        g.fillRoundedRectangle(bounds.reduced(4.0f), 3.0f);
    }

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font("Courier New", 14.0f * uiScale, juce::Font::bold));
    g.drawFittedText(button.getButtonText(), button.getLocalBounds(),
                     juce::Justification::centred, 1);
}

KnobRow::KnobRow(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId,
                 const juce::String& labelText, ReverbLookAndFeel& laf)
{
    slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                               juce::MathConstants<float>::pi * 2.75f,
                               true);
    slider.setLookAndFeel(&laf);

    label.setText(labelText, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colours::white);
    label.setFont(juce::Font("Courier New", 9.0f, juce::Font::bold));

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramId, slider);

    addAndMakeVisible(slider);
    addAndMakeVisible(label);
}

void KnobRow::resized()
{
    auto bounds = getLocalBounds();
    auto labelArea = bounds.removeFromBottom(labelHeight).translated(0, -5);
    label.setBounds(labelArea);
    slider.setBounds(bounds);
}

CockReverbAudioProcessorEditor::CockReverbAudioProcessorEditor(CockReverbAudioProcessor& proc)
    : AudioProcessorEditor(&proc)
    , processor(proc)
{
    laf.setUiScale(kUiScale);
    setSize((int) std::round(1408.0f * kUiScale), (int) std::round(768.0f * kUiScale));

    massiveToggle.setButtonText("MASSIVE");
    massiveToggle.setLookAndFeel(&laf);
    massiveToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    massiveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, "massive", massiveToggle);

    powerToggle.setLookAndFeel(&invisibleLaf);
    powerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, "bypass", powerToggle);

    const int labelHeight = (int) std::round(20.0f * kUiScale);
    knobSize.setLabelHeight(labelHeight);
    knobMix.setLabelHeight(labelHeight);
    knobTone.setLabelHeight(labelHeight);
    knobPredelay.setLabelHeight(labelHeight);
    knobMod.setLabelHeight(labelHeight);
    knobHall.setLabelHeight(labelHeight);
    knobDrip.setLabelHeight(labelHeight);

    addAndMakeVisible(knobSize);
    addAndMakeVisible(knobMix);
    addAndMakeVisible(knobTone);
    addAndMakeVisible(knobPredelay);
    addAndMakeVisible(knobMod);
    addAndMakeVisible(knobHall);
    addAndMakeVisible(knobDrip);

    addAndMakeVisible(massiveToggle);
    addAndMakeVisible(powerToggle);
}

CockReverbAudioProcessorEditor::~CockReverbAudioProcessorEditor()
{
    massiveToggle.setLookAndFeel(nullptr);
    powerToggle.setLookAndFeel(nullptr);
}

void CockReverbAudioProcessorEditor::paint(juce::Graphics& g)
{
    auto image = juce::ImageCache::getFromMemory(BinaryData::CockReverbBG_png,
                                                 BinaryData::CockReverbBG_pngSize);
    g.drawImage(image, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
}

void CockReverbAudioProcessorEditor::resized()
{
    auto scale = [](float value) { return (int) std::round(value * kUiScale); };
    const int knobDiameter = scale(90.0f);
    const int knobHeight = knobDiameter + scale(20.0f);

    knobSize.setBounds(scale(280.0f), scale(380.0f), knobDiameter, knobHeight);
    knobMix.setBounds(scale(416.0f), scale(380.0f), knobDiameter, knobHeight);
    knobTone.setBounds(scale(553.0f), scale(380.0f), knobDiameter, knobHeight);
    knobPredelay.setBounds(scale(690.0f), scale(380.0f), knobDiameter, knobHeight);
    knobMod.setBounds(scale(827.0f), scale(380.0f), knobDiameter, knobHeight);
    knobHall.setBounds(scale(963.0f), scale(380.0f), knobDiameter, knobHeight);
    knobDrip.setBounds(scale(1100.0f), scale(380.0f), knobDiameter, knobHeight);

    massiveToggle.setBounds(scale(580.0f), scale(520.0f), scale(130.0f), scale(40.0f));
    powerToggle.setBounds(scale(1310.0f), scale(685.0f), scale(50.0f), scale(60.0f));
}
