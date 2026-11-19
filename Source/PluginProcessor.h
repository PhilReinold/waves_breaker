#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "VocalRiderDSP.h"

class AutoRiderProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    AutoRiderProcessor();
    ~AutoRiderProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Waves Breaker"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // Factory-Presets (erscheinen auch im Preset-Menü von Logic)
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // fürs GUI (Audio-Thread schreibt, GUI liest)
    std::atomic<float> meterGainDb { 0.0f };
    std::atomic<float> meterLevelDb { -120.0f };
    std::atomic<float> meterMusicShiftDb { 0.0f };
    std::atomic<bool>  meterVocalActive { false };
    std::atomic<bool>  meterClip { false };
    std::atomic<bool>  hostPlaying { false };

    enum AutoMode { autoOff = 0, autoRead = 1, autoWrite = 2 };
    int getAutoMode() const { return juce::jlimit (0, 2, (int) std::lround (pMode->load())); }
    juce::RangedAudioParameter* getRiderParam() const { return riderParam; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void timerCallback() override; // Automation Write: Rider-Fader an die DAW senden (Message-Thread)

    juce::RangedAudioParameter* riderParam = nullptr;
    bool writeGestureOpen = false;
    float lastWrittenDb = 1000.0f;

    autorider::VocalRider rider;
    int lastLatency = -1;
    int currentProgram = 0;

    std::atomic<float>* pTarget = nullptr;
    std::atomic<float>* pRange = nullptr;
    std::atomic<float>* pBoost = nullptr;
    std::atomic<float>* pCut = nullptr;
    std::atomic<float>* pSpeed = nullptr;
    std::atomic<float>* pGate = nullptr;
    std::atomic<float>* pMusic = nullptr;
    std::atomic<float>* pIdle = nullptr;
    std::atomic<float>* pLook = nullptr;
    std::atomic<float>* pBreath = nullptr;
    std::atomic<float>* pOutput = nullptr;
    std::atomic<float>* pRider = nullptr;
    std::atomic<float>* pMode = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoRiderProcessor)
};
