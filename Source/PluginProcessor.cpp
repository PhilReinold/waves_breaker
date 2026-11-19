#include "PluginProcessor.h"
#include "PluginEditor.h"

using Float = juce::AudioParameterFloat;
using Range = juce::NormalisableRange<float>;
using Attr  = juce::AudioParameterFloatAttributes;

juce::AudioProcessorValueTreeState::ParameterLayout AutoRiderProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<Float> (juce::ParameterID { "target", 1 }, "Target",       Range (-42.0f, 0.0f, 0.1f),  -18.0f, Attr().withLabel ("dB")));
    l.add (std::make_unique<Float> (juce::ParameterID { "range",  1 }, "Range",        Range (0.0f, 12.0f, 0.1f),     3.0f, Attr().withLabel ("dB")));
    l.add (std::make_unique<Float> (juce::ParameterID { "boost",  1 }, "Max Boost",    Range (0.0f, 12.0f, 0.1f),     9.0f, Attr().withLabel ("dB")));
    l.add (std::make_unique<Float> (juce::ParameterID { "cut",    1 }, "Max Cut",      Range (0.0f, 12.0f, 0.1f),     9.0f, Attr().withLabel ("dB")));
    l.add (std::make_unique<Float> (juce::ParameterID { "speed",  1 }, "Speed",        Range (0.0f, 100.0f, 1.0f),   50.0f, Attr().withLabel ("%")));
    l.add (std::make_unique<Float> (juce::ParameterID { "gate",   1 }, "Vocal Sens",   Range (-80.0f, -20.0f, 0.1f), -50.0f, Attr().withLabel ("dB")));
    l.add (std::make_unique<Float> (juce::ParameterID { "music",  1 }, "Music Sens",   Range (-12.0f, 12.0f, 0.1f),   0.0f, Attr().withLabel ("dB")));
    l.add (std::make_unique<Float> (juce::ParameterID { "idle",   1 }, "Idle",         Range (-12.0f, 12.0f, 0.1f),   0.0f, Attr().withLabel ("dB")));
    l.add (std::make_unique<Float> (juce::ParameterID { "output", 1 }, "Output",       Range (-12.0f, 12.0f, 0.1f),   0.0f, Attr().withLabel ("dB")));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "lookahead", 1 }, "Lookahead", true));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "breath",    1 }, "Breath",    true));
    // Rider Fader: wird im Write-Modus automatisch geschrieben, im Read-Modus aus der Automation gelesen
    l.add (std::make_unique<Float> (juce::ParameterID { "rider", 1 }, "Rider Fader", Range (-12.0f, 12.0f, 0.01f), 0.0f, Attr().withLabel ("dB")));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "automode", 1 }, "Automation",
                                                         juce::StringArray { "Off", "Read", "Write" }, 0));
    return l;
}

namespace
{
struct Preset { const char* name; float target, range, boost, cut, speed, gate, music, idle; bool look, breath; };

const Preset presets[] =
{
    { "Init",                        -18.0f, 3.0f,  9.0f,  9.0f, 50.0f, -50.0f,  0.0f, 0.0f, true,  true },
    { "Lead Vocal - sanft",          -18.0f, 4.0f,  6.0f,  6.0f, 35.0f, -52.0f,  0.0f, 0.0f, true,  true },
    { "Lead Vocal - straff (Pop)",   -16.0f, 2.0f,  9.0f,  9.0f, 65.0f, -50.0f,  0.0f, 0.0f, true,  true },
    { "Lead Vocal + Musik folgen",   -18.0f, 3.0f,  8.0f,  8.0f, 45.0f, -50.0f,  6.0f, 0.0f, true,  true },
    { "Rap / Sprechgesang",          -17.0f, 1.5f, 10.0f, 10.0f, 80.0f, -48.0f,  0.0f, 0.0f, true,  true },
    { "Intim / Fluestern",           -24.0f, 4.0f, 12.0f,  6.0f, 30.0f, -58.0f,  0.0f, 0.0f, true,  true },
    { "Backing Vocals",              -22.0f, 5.0f,  6.0f,  8.0f, 40.0f, -50.0f,  0.0f, -2.0f, true, true },
    { "Podcast / Voiceover",         -20.0f, 2.0f,  9.0f, 12.0f, 55.0f, -52.0f,  0.0f, 0.0f, true,  true },
    { "Vintage Handfader (langsam)", -18.0f, 6.0f,  6.0f,  6.0f, 15.0f, -50.0f,  0.0f, 0.0f, true,  true },
    { "Live / wilder Saenger",       -15.0f, 3.0f,  8.0f, 12.0f, 85.0f, -45.0f,  0.0f, 0.0f, false, true },
};
constexpr int numPresets = (int) (sizeof (presets) / sizeof (presets[0]));
}

int AutoRiderProcessor::getNumPrograms() { return numPresets; }

const juce::String AutoRiderProcessor::getProgramName (int i)
{
    return juce::isPositiveAndBelow (i, numPresets) ? juce::String (presets[i].name) : juce::String();
}

void AutoRiderProcessor::setCurrentProgram (int i)
{
    if (! juce::isPositiveAndBelow (i, numPresets)) return;
    currentProgram = i;
    apvts.state.setProperty ("preset", i, nullptr);

    const auto& p = presets[i];
    auto set = [this] (const char* id, float v)
    {
        if (auto* prm = apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    };
    set ("target", p.target); set ("range", p.range); set ("boost", p.boost); set ("cut", p.cut);
    set ("speed", p.speed);   set ("gate", p.gate);   set ("music", p.music); set ("idle", p.idle);
    set ("output", 0.0f);
    set ("lookahead", p.look ? 1.0f : 0.0f);
    set ("breath", p.breath ? 1.0f : 0.0f);
}

AutoRiderProcessor::AutoRiderProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false) // Musik/Instrumental
                          .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pTarget = apvts.getRawParameterValue ("target");
    pRange  = apvts.getRawParameterValue ("range");
    pBoost  = apvts.getRawParameterValue ("boost");
    pCut    = apvts.getRawParameterValue ("cut");
    pSpeed  = apvts.getRawParameterValue ("speed");
    pGate   = apvts.getRawParameterValue ("gate");
    pMusic  = apvts.getRawParameterValue ("music");
    pIdle   = apvts.getRawParameterValue ("idle");
    pLook   = apvts.getRawParameterValue ("lookahead");
    pBreath = apvts.getRawParameterValue ("breath");
    pOutput = apvts.getRawParameterValue ("output");
    pRider  = apvts.getRawParameterValue ("rider");
    pMode   = apvts.getRawParameterValue ("automode");
    riderParam = apvts.getParameter ("rider");
    startTimerHz (30);
}

AutoRiderProcessor::~AutoRiderProcessor()
{
    stopTimer();
    if (writeGestureOpen && riderParam != nullptr) riderParam->endChangeGesture();
}

// Läuft auf dem Message-Thread – auch wenn das Plugin-Fenster geschlossen ist.
void AutoRiderProcessor::timerCallback()
{
    const bool writing = getAutoMode() == autoWrite && hostPlaying.load();
    if (riderParam == nullptr) return;

    if (writing)
    {
        if (! writeGestureOpen) { riderParam->beginChangeGesture(); writeGestureOpen = true; lastWrittenDb = 1000.0f; }
        const float db = juce::jlimit (-12.0f, 12.0f, meterGainDb.load());
        if (std::abs (db - lastWrittenDb) >= 0.05f) // nur echte Bewegungen schreiben (schlanke Automation)
        {
            riderParam->setValueNotifyingHost (riderParam->convertTo0to1 (db));
            lastWrittenDb = db;
        }
    }
    else if (writeGestureOpen)
    {
        riderParam->endChangeGesture();
        writeGestureOpen = false;
    }
}

bool AutoRiderProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (in != out || (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo()))
        return false;

    const auto sc = l.getChannelSet (true, 1);
    return sc.isDisabled() || sc == juce::AudioChannelSet::mono() || sc == juce::AudioChannelSet::stereo();
}

void AutoRiderProcessor::prepareToPlay (double sampleRate, int)
{
    rider.prepare (sampleRate, juce::jmax (1, getMainBusNumInputChannels()));
    lastLatency = -1;
}

void AutoRiderProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int mainChannels = juce::jmax (1, getMainBusNumInputChannels());
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    autorider::Params p;
    p.targetDb     = pTarget->load();
    p.rangeDb      = pRange->load();
    p.maxBoostDb   = pBoost->load();
    p.maxCutDb     = pCut->load();
    p.speed        = pSpeed->load() * 0.01f;
    p.gateDb       = pGate->load();
    p.musicSensDb  = pMusic->load();
    p.idleDb       = pIdle->load();
    p.lookahead    = pLook->load() > 0.5f;
    p.breathDetect = pBreath->load() > 0.5f;
    p.outputDb     = pOutput->load();
    p.manual       = getAutoMode() == autoRead;
    p.manualGainDb = pRider->load();

    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            playing = pos->getIsPlaying();
    hostPlaying.store (playing);

    const int latency = rider.getLatencySamples (p.lookahead);
    if (latency != lastLatency) { setLatencySamples (latency); lastLatency = latency; }

    // Sidechain (Musik), falls in Logic zugewiesen
    const float* scPtrs[2] = { nullptr, nullptr };
    int scChannels = 0;
    if (auto* scBus = getBus (true, 1); scBus != nullptr && scBus->isEnabled())
    {
        auto sc = getBusBuffer (buffer, true, 1);
        scChannels = juce::jmin (2, sc.getNumChannels());
        for (int c = 0; c < scChannels; ++c) scPtrs[c] = sc.getReadPointer (c);
    }

    if (buffer.getNumChannels() >= mainChannels)
        rider.process (buffer.getArrayOfWritePointers(), scPtrs, scChannels, buffer.getNumSamples(), p);

    meterGainDb.store (rider.getGainDb());
    meterLevelDb.store (rider.getLevelDb());
    meterMusicShiftDb.store (rider.getMusicShiftDb());
    meterVocalActive.store (rider.isVocalActive());
    meterClip.store (rider.isClipping());
}

juce::AudioProcessorEditor* AutoRiderProcessor::createEditor() { return new WavesBreakerEditor (*this); }

void AutoRiderProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void AutoRiderProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            currentProgram = juce::jlimit (0, numPresets - 1, (int) apvts.state.getProperty ("preset", 0));
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AutoRiderProcessor(); }
