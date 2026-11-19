#pragma once
#include "PluginProcessor.h"

// Vintage-Optik: gealtertes blaugraues Panel, verblasster Aufdruck, abgenutzte Kappen,
// Kratzer, Staub, Vignette, vergilbter Lack
class VintageLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VintageLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos,
                           float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;

    static juce::Font serif (float height, bool bold = false);
};

class WavesBreakerEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit WavesBreakerEditor (AutoRiderProcessor&);
    ~WavesBreakerEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void buildTextures();
    void stepPreset (int delta);

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment; // nach slider => zuerst zerstört
    };
    void addKnob (Knob&, const juce::String& paramID, const juce::String& text, juce::Colour cap);

    VintageLookAndFeel laf; // zuerst deklariert => zuletzt zerstört

    AutoRiderProcessor& proc;
    Knob target, range, gate, music, boost, cut, idle, speed, output;
    juce::Slider rideFader;          // reine Anzeige: bewegt sich mit dem automatischen Ride
    juce::ToggleButton lookahead { "Lookahead" }, breath { "Breath" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> lookaheadAttachment, breathAttachment;

    // Automation-Modus wie beim Vocal Rider: Off / Read / Write
    juce::TextButton modeButtons[3] { juce::TextButton ("OFF"), juce::TextButton ("READ"), juce::TextButton ("WRITE") };
    void setAutoMode (int mode);
    void updateFaderMode();
    int shownMode = -1;

    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" };

    juce::Image panelImage, overlayImage;
    juce::Rectangle<float> meterArea, ledArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WavesBreakerEditor)
};
