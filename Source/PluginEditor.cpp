#include "PluginEditor.h"

namespace
{
const juce::Colour panelC   { 0xff56666b };   // gealtertes Blaugrau
const juce::Colour headerC  { 0xff3c474a };
const juce::Colour printC   { 0xffe6dcc2 };   // vergilbter, leicht verblasster Aufdruck
const juce::Colour capRed   { 0xff9c3a2e };
const juce::Colour capBlue  { 0xff3f6a91 };
const juce::Colour capCream { 0xffcfc3a4 };
}

juce::Font VintageLookAndFeel::serif (float height, bool bold)
{
    return juce::Font (juce::FontOptions (height)
                           .withName (juce::Font::getDefaultSerifFontName())
                           .withStyle (bold ? "Bold" : "Regular"));
}

//==============================================================================
VintageLookAndFeel::VintageLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, printC);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, printC);

    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2a3033));
    setColour (juce::ComboBox::textColourId, printC);
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff12161a));
    setColour (juce::ComboBox::arrowColourId, printC);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff2a3033));
    setColour (juce::PopupMenu::textColourId, printC);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff7a6a3e));
    setColour (juce::PopupMenu::highlightedTextColourId, printC);
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a3033));
    setColour (juce::TextButton::textColourOffId, printC);
}

void VintageLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                           float startAngle, float endAngle, juce::Slider& s)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float capR = r * 0.66f;

    // Skalenstriche (verblasst)
    g.setColour (printC.withAlpha (0.85f));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        g.drawLine (juce::Line<float> (c.getPointOnCircumference (capR + 5.0f, a),
                                       c.getPointOnCircumference (r, a)),
                    (i % 5 == 0) ? 1.8f : 1.0f);
    }

    // Schatten + Kappe
    g.setColour (juce::Colours::black.withAlpha (0.40f));
    g.fillEllipse (c.x - capR + 2.0f, c.y - capR + 3.0f, capR * 2.0f, capR * 2.0f);

    const auto col = s.findColour (juce::Slider::rotarySliderFillColourId);
    g.setGradientFill (juce::ColourGradient (col.brighter (0.22f), c.x - capR * 0.5f, c.y - capR * 0.6f,
                                             col.darker (0.55f),   c.x + capR * 0.6f, c.y + capR * 0.7f, false));
    g.fillEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f);
    g.setColour (col.darker (0.85f));
    g.drawEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f, 1.4f);

    const float topR = capR * 0.82f;
    g.setColour (col.brighter (0.06f));
    g.fillEllipse (c.x - topR, c.y - topR, topR * 2.0f, topR * 2.0f);
    g.setColour (col.darker (0.4f));
    g.drawEllipse (c.x - topR, c.y - topR, topR * 2.0f, topR * 2.0f, 0.8f);

    // abgegriffene Stelle (Glanz) + feine Kratzer, pro Knopf stabil
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.16f), c.x - topR * 0.35f, c.y - topR * 0.4f,
                                             juce::Colours::transparentWhite, c.x + topR * 0.4f, c.y + topR * 0.5f, true));
    g.fillEllipse (c.x - topR, c.y - topR, topR * 2.0f, topR * 2.0f);
    juce::Random rnd (s.getX() * 31 + s.getY() * 17 + (int) col.getARGB());
    for (int i = 0; i < 5; ++i)
    {
        const float ang = rnd.nextFloat() * juce::MathConstants<float>::twoPi;
        const float rad = topR * (0.2f + 0.6f * rnd.nextFloat());
        const auto p1 = c.getPointOnCircumference (rad, ang);
        const auto p2 = p1 + juce::Point<float> ((rnd.nextFloat() - 0.5f) * topR * 0.8f, (rnd.nextFloat() - 0.5f) * topR * 0.8f);
        g.setColour (juce::Colours::white.withAlpha (0.10f + 0.12f * rnd.nextFloat()));
        g.drawLine (juce::Line<float> (p1, p2), 0.7f);
    }

    // Zeiger (cremefarben)
    const float a = startAngle + pos * (endAngle - startAngle);
    g.setColour (juce::Colour (0xfff1e8cc));
    g.drawLine (juce::Line<float> (c.getPointOnCircumference (topR * 0.25f, a),
                                   c.getPointOnCircumference (topR * 0.95f, a)), 2.6f);
}

void VintageLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos,
                                           float minPos, float maxPos, juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, minPos, maxPos, style, s);
        return;
    }

    const float capH = 58.0f, capW = 38.0f;
    const float top = (float) y + capH * 0.5f, bottom = (float) (y + h) - capH * 0.5f;
    auto posOf = [&] (double v) { return juce::jmap ((float) (1.0 - s.valueToProportionOfLength (v)), top, bottom); };
    const float cx = (float) x + (float) w * 0.5f;

    g.setFont (serif (12.0f));
    for (int db = 12; db >= -12; db -= 3)
    {
        const float py = posOf (db);
        const bool major = (db % 6 == 0);
        g.setColour (printC.withAlpha (db == 0 ? 1.0f : 0.8f));
        g.drawLine (cx - 28.0f, py, cx - (major ? 20.0f : 24.0f), py, db == 0 ? 2.0f : 1.2f);
        g.drawLine (cx + 20.0f, py, cx + (major ? 28.0f : 24.0f), py, db == 0 ? 2.0f : 1.2f);
        if (major)
            g.drawText (db > 0 ? "+" + juce::String (db) : juce::String (db),
                        juce::Rectangle<float> (cx - 54.0f, py - 7.0f, 24.0f, 14.0f), juce::Justification::centredRight);
    }

    // Schlitz mit Rahmen (leicht verschmutzt)
    g.setColour (juce::Colour (0xff1f2528));
    g.fillRoundedRectangle (cx - 9.0f, (float) y + 2.0f, 18.0f, (float) h - 4.0f, 3.0f);
    g.setColour (juce::Colour (0xff050607));
    g.fillRoundedRectangle (cx - 3.0f, (float) y + 6.0f, 6.0f, (float) h - 12.0f, 2.0f);

    // Kappe: dunkel, geriffelt, cremefarbene (gealterte) Mittellinie
    const float cy = posOf (s.getValue());
    juce::Rectangle<float> cap (cx - capW * 0.5f, cy - capH * 0.5f, capW, capH);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (cap.translated (2.0f, 3.0f), 4.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff45403b), cap.getX(), cap.getY(),
                                             juce::Colour (0xff171412), cap.getRight(), cap.getBottom(), false));
    g.fillRoundedRectangle (cap, 4.0f);
    g.setColour (juce::Colour (0xff000000));
    g.drawRoundedRectangle (cap, 4.0f, 1.2f);
    g.setColour (juce::Colours::white.withAlpha (0.09f));
    for (int i = 1; i <= 4; ++i)
    {
        const float ly = cap.getY() + 6.0f + (float) i * 4.0f;
        g.drawLine (cap.getX() + 5.0f, ly, cap.getRight() - 5.0f, ly, 1.0f);
        const float ly2 = cap.getBottom() - 6.0f - (float) i * 4.0f;
        g.drawLine (cap.getX() + 5.0f, ly2, cap.getRight() - 5.0f, ly2, 1.0f);
    }
    g.setColour (juce::Colour (0xffe9dfc0));
    g.fillRect (cap.getX() + 2.0f, cap.getCentreY() - 1.5f, capW - 4.0f, 3.0f);
}

void VintageLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool down)
{
    auto r = b.getLocalBounds().toFloat();
    const auto c = r.getCentre();
    const bool on = b.getToggleState();

    // Lampe (Bernstein, mit Schein wenn an)
    const float ledY = r.getY() + 10.0f;
    if (on)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0x66ffa63a), c.x, ledY, juce::Colours::transparentBlack, c.x + 14.0f, ledY, true));
        g.fillEllipse (c.x - 14.0f, ledY - 14.0f, 28.0f, 28.0f);
    }
    g.setColour (on ? juce::Colour (0xffffa63a) : juce::Colour (0xff4a2a10));
    g.fillEllipse (c.x - 5.0f, ledY - 5.0f, 10.0f, 10.0f);
    g.setColour (juce::Colours::black);
    g.drawEllipse (c.x - 5.0f, ledY - 5.0f, 10.0f, 10.0f, 1.0f);

    // Drucktaste (beige, abgegriffen)
    juce::Rectangle<float> btn (c.x - 17.0f, ledY + 13.0f, 34.0f, 22.0f);
    g.setColour (down ? juce::Colour (0xff9a9078) : capCream);
    g.fillRoundedRectangle (btn, 3.0f);
    g.setColour (juce::Colour (0xff2a2e32));
    g.drawRoundedRectangle (btn, 3.0f, 1.2f);
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawLine (btn.getX() + 4.0f, btn.getY() + 3.0f, btn.getRight() - 4.0f, btn.getY() + 3.0f, 1.0f);

    g.setColour (printC);
    g.setFont (serif (13.0f, true));
    g.drawText (b.getButtonText().toUpperCase(), r.withTop (btn.getBottom() + 4.0f).toNearestInt(), juce::Justification::centredTop);
}

//==============================================================================
WavesBreakerEditor::WavesBreakerEditor (AutoRiderProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);
    setSize (620, 560);

    addKnob (target, "target", "Target",     capRed);
    addKnob (range,  "range",  "Range",      capBlue);
    addKnob (gate,   "gate",   "Vocal Sens", capCream);
    addKnob (music,  "music",  "Music Sens", capCream);
    addKnob (boost,  "boost",  "Max Boost",  capBlue);
    addKnob (cut,    "cut",    "Max Cut",    capBlue);
    addKnob (idle,   "idle",   "Idle",       capBlue);
    addKnob (speed,  "speed",  "Speed",      capCream);
    addKnob (output, "output", "Output",     capRed);

    rideFader.setSliderStyle (juce::Slider::LinearVertical);
    rideFader.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    rideFader.setRange (-12.0, 12.0, 0.01);
    rideFader.setDoubleClickReturnValue (true, 0.0);
    // Im READ-Modus kann der Fader von Hand gezogen werden (schreibt mit Logic "Touch"/"Latch" Automation)
    rideFader.onDragStart = [this] { if (auto* rp = proc.getRiderParam()) rp->beginChangeGesture(); };
    rideFader.onDragEnd   = [this] { if (auto* rp = proc.getRiderParam()) rp->endChangeGesture(); };
    rideFader.onValueChange = [this]
    {
        if (proc.getAutoMode() == AutoRiderProcessor::autoRead)
            if (auto* rp = proc.getRiderParam())
                rp->setValueNotifyingHost (rp->convertTo0to1 ((float) rideFader.getValue()));
    };
    addAndMakeVisible (rideFader);

    for (int i = 0; i < 3; ++i)
    {
        auto& b = modeButtons[i];
        b.setClickingTogglesState (false);
        b.setColour (juce::TextButton::buttonOnColourId, i == 2 ? juce::Colour (0xff9c3a2e) : juce::Colour (0xff7a6a3e));
        b.onClick = [this, i] { setAutoMode (i); };
        addAndMakeVisible (b);
    }
    updateFaderMode();

    addAndMakeVisible (lookahead);
    addAndMakeVisible (breath);
    lookaheadAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "lookahead", lookahead);
    breathAttachment    = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "breath", breath);

    // Presets
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);
    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (+1); };
    addAndMakeVisible (prevButton);
    addAndMakeVisible (nextButton);

    buildTextures();
    startTimerHz (30);
}

WavesBreakerEditor::~WavesBreakerEditor()
{
    setLookAndFeel (nullptr);
}

void WavesBreakerEditor::stepPreset (int delta)
{
    const int n = proc.getNumPrograms();
    const int idx = (presetBox.getSelectedId() - 1 + delta + n) % n;
    presetBox.setSelectedId (idx + 1, juce::sendNotificationSync);
}

void WavesBreakerEditor::addKnob (Knob& k, const juce::String& id, const juce::String& text, juce::Colour cap)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 16);
    k.slider.setColour (juce::Slider::rotarySliderFillColourId, cap);
    addAndMakeVisible (k.slider);

    k.label.setText (text.toUpperCase(), juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (VintageLookAndFeel::serif (13.0f, true));
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.slider);
}

void WavesBreakerEditor::setAutoMode (int mode)
{
    if (auto* prm = proc.apvts.getParameter ("automode"))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) mode));
        prm->endChangeGesture();
    }
    updateFaderMode();
}

void WavesBreakerEditor::updateFaderMode()
{
    const int mode = proc.getAutoMode();
    if (mode == shownMode) return;
    shownMode = mode;
    for (int i = 0; i < 3; ++i) modeButtons[i].setToggleState (i == mode, juce::dontSendNotification);
    rideFader.setInterceptsMouseClicks (mode == AutoRiderProcessor::autoRead, false);
}

void WavesBreakerEditor::timerCallback()
{
    updateFaderMode();
    if (! rideFader.isMouseButtonDown())
        rideFader.setValue (proc.getAutoMode() == AutoRiderProcessor::autoRead
                                ? (double) proc.getRiderParam()->convertFrom0to1 (proc.getRiderParam()->getValue())
                                : (double) proc.meterGainDb.load(),
                            juce::dontSendNotification);
    repaint (meterArea.expanded (14.0f, 4.0f).toNearestInt());
    repaint (ledArea.toNearestInt());
}

void WavesBreakerEditor::buildTextures()
{
    const int w = getWidth(), h = getHeight();
    juce::Random rnd (1973);

    // --- Panel: Lack mit Körnung, Streifen, Flecken ---
    panelImage = juce::Image (juce::Image::RGB, w, h, false);
    {
        std::vector<float> rowOff ((size_t) h);
        for (auto& v : rowOff) v = (rnd.nextFloat() - 0.5f) * 0.03f;

        juce::Image::BitmapData bd (panelImage, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                const float n = (rnd.nextFloat() - 0.5f) * 0.07f;
                bd.setPixelColour (x, y, panelC.withMultipliedBrightness (1.0f + n + rowOff[(size_t) y]));
            }
    }
    {
        juce::Graphics g (panelImage);
        for (int i = 0; i < 9; ++i) // Nikotin-/Altersflecken
        {
            const float cx = rnd.nextFloat() * (float) w, cy = rnd.nextFloat() * (float) h, rad = 60.0f + rnd.nextFloat() * 110.0f;
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6b5a2a).withAlpha (0.10f + 0.06f * rnd.nextFloat()), cx, cy,
                                                     juce::Colours::transparentBlack, cx + rad, cy, true));
            g.fillEllipse (cx - rad, cy - rad, rad * 2.0f, rad * 2.0f);
        }
        g.setColour (juce::Colours::black.withAlpha (0.45f));     // abgestoßene Kante
        g.drawRect (0, 0, w, h, 2);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawRect (2, 2, w - 4, h - 4, 1);
    }

    // --- Overlay: Vergilbung, Vignette, Kratzer, Staub (über alles gelegt) ---
    overlayImage = juce::Image (juce::Image::ARGB, w, h, true);
    juce::Graphics g (overlayImage);
    g.fillAll (juce::Colour (0xff8a6a2a).withAlpha (0.07f));
    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, (float) w * 0.5f, (float) h * 0.5f,
                                             juce::Colours::black.withAlpha (0.42f), 0.0f, 0.0f, true));
    g.fillRect (0, 0, w, h);
    for (int i = 0; i < 55; ++i)
    {
        const float x1 = rnd.nextFloat() * (float) w, y1 = rnd.nextFloat() * (float) h;
        const float len = 15.0f + rnd.nextFloat() * 80.0f, ang = (rnd.nextFloat() - 0.5f) * 1.2f + (rnd.nextBool() ? 0.0f : 1.57f);
        g.setColour ((rnd.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.05f + 0.09f * rnd.nextFloat()));
        g.drawLine (x1, y1, x1 + len * std::cos (ang), y1 + len * std::sin (ang), 0.6f);
    }
    for (int i = 0; i < 220; ++i)
    {
        const float s = 0.8f + rnd.nextFloat() * 1.6f;
        g.setColour ((rnd.nextInt (3) == 0 ? juce::Colours::white : juce::Colours::black).withAlpha (0.06f + 0.14f * rnd.nextFloat()));
        g.fillEllipse (rnd.nextFloat() * (float) w, rnd.nextFloat() * (float) h, s, s);
    }
}

void WavesBreakerEditor::resized()
{
    auto r = getLocalBounds();

    // Preset-Leiste im Header
    const int by = 10, bh = 24;
    nextButton.setBounds (getWidth() - 18 - 26, by, 26, bh);
    presetBox.setBounds (nextButton.getX() - 4 - 210, by, 210, bh);
    prevButton.setBounds (presetBox.getX() - 4 - 26, by, 26, bh);

    r.removeFromTop (44); // Header

    auto left = r.removeFromLeft (210).reduced (10, 14);
    {
        auto modes = left.removeFromBottom (24);
        left.removeFromBottom (10);
        const int bw = (modes.getWidth() - 8) / 3;
        for (int i = 0; i < 3; ++i)
            modeButtons[i].setBounds (modes.getX() + i * (bw + 4), modes.getY(), bw, modes.getHeight());
    }
    left.removeFromTop (18);
    meterArea = left.removeFromLeft (26).toFloat();
    left.removeFromLeft (22);
    rideFader.setBounds (left.withTrimmedBottom (4));

    r.removeFromLeft (8);
    auto grid = r.reduced (6, 14);
    const int cols = 4, rows = 3;
    const int cw = grid.getWidth() / cols, rh = grid.getHeight() / rows;
    auto cell = [&] (int i) { return juce::Rectangle<int> (grid.getX() + (i % cols) * cw, grid.getY() + (i / cols) * rh, cw, rh); };

    Knob* knobs[] = { &target, &range, &gate, &music, &boost, &cut, &idle, &speed, &output };
    for (int i = 0; i < 9; ++i)
    {
        auto c = cell (i);
        knobs[i]->label.setBounds (c.removeFromTop (18));
        knobs[i]->slider.setBounds (c.reduced (2, 0));
    }
    lookahead.setBounds (cell (9).withTrimmedTop (30).withHeight (100));
    breath.setBounds    (cell (10).withTrimmedTop (30).withHeight (100));
    ledArea = cell (11).withTrimmedTop (22).toFloat().reduced (6.0f, 0.0f);
}

void WavesBreakerEditor::paint (juce::Graphics& g)
{
    g.drawImageAt (panelImage, 0, 0);

    // Header
    auto head = getLocalBounds().removeFromTop (44).toFloat();
    g.setColour (headerC.withAlpha (0.92f));
    g.fillRect (head);
    g.setColour (juce::Colours::black.withAlpha (0.40f));
    g.drawLine (0.0f, head.getBottom(), (float) getWidth(), head.getBottom(), 2.0f);
    g.setColour (printC);
    g.setFont (VintageLookAndFeel::serif (26.0f, true));
    g.drawText ("WAVES BREAKER", juce::Rectangle<int> (18, 0, 300, 44), juce::Justification::centredLeft);

    // Schrauben (angelaufen)
    for (auto p : { juce::Point<float> (10, 10), juce::Point<float> ((float) getWidth() - 10, 10),
                    juce::Point<float> (10, (float) getHeight() - 10), juce::Point<float> ((float) getWidth() - 10, (float) getHeight() - 10) })
    {
        g.setColour (juce::Colour (0xff8b8f8a));
        g.fillEllipse (p.x - 4, p.y - 4, 8, 8);
        g.setColour (juce::Colour (0xff3c4144));
        g.drawLine (p.x - 3, p.y + 0.5f, p.x + 3, p.y - 0.5f, 1.2f);
    }

    // Trennlinie
    g.setColour (juce::Colours::black.withAlpha (0.30f));
    g.drawLine (212.0f, 56.0f, 212.0f, (float) getHeight() - 16.0f, 1.5f);
    g.setColour (printC.withAlpha (0.16f));
    g.drawLine (213.0f, 56.0f, 213.0f, (float) getHeight() - 16.0f, 1.0f);

    // Beschriftung Fader-Sektion
    g.setColour (printC);
    g.setFont (VintageLookAndFeel::serif (13.0f, true));
    g.drawText ("LEVEL", juce::Rectangle<float> (meterArea.getX() - 12.0f, meterArea.getY() - 18.0f, 50.0f, 14.0f).toNearestInt(), juce::Justification::centred);
    g.drawText ("RIDER FADER", rideFader.getBounds().withHeight (14).translated (0, -18), juce::Justification::centred);

    // LED-Meter (-60 ... 0 dB, 2-dB-Segmente) + Zielband-Marken
    const float level = proc.meterLevelDb.load();
    const int segs = 30;
    const float segH = meterArea.getHeight() / (float) segs;
    g.setColour (juce::Colour (0xff14181a));
    g.fillRoundedRectangle (meterArea.expanded (3.0f), 3.0f);
    for (int i = 0; i < segs; ++i)
    {
        const float db = -60.0f + (float) i * 2.0f;
        const juce::Colour base = db < -12.0f ? juce::Colour (0xff6cc070) : db < -4.0f ? juce::Colour (0xffe0a53a) : juce::Colour (0xffd2492f);
        const float y = meterArea.getBottom() - (float) (i + 1) * segH;
        g.setColour (level >= db ? base : base.withAlpha (0.13f));
        g.fillRect (meterArea.getX(), y + 1.0f, meterArea.getWidth(), segH - 2.0f);
    }

    const float tgt  = proc.apvts.getRawParameterValue ("target")->load();
    const float half = 0.5f * proc.apvts.getRawParameterValue ("range")->load();
    auto yOf = [&] (float db) { return meterArea.getBottom() - juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f) * meterArea.getHeight(); };
    g.setColour (printC);
    for (float db : { tgt + half, tgt - half })
    {
        const float y = yOf (db);
        juce::Path tri;
        tri.addTriangle (meterArea.getRight() + 4.0f, y, meterArea.getRight() + 12.0f, y - 4.0f, meterArea.getRight() + 12.0f, y + 4.0f);
        g.fillPath (tri);
    }
    g.setColour (capRed.brighter (0.5f));
    g.drawLine (meterArea.getX() - 5.0f, yOf (tgt), meterArea.getX() - 1.0f, yOf (tgt), 2.0f);

    // Kontroll-Lampen: VOCAL (Gesang erkannt), MUSIC (Sidechain greift ein), CLIP
    const float music = std::fabs (proc.meterMusicShiftDb.load());
    struct Lamp { const char* text; float amount; juce::Colour col; };
    const Lamp lamps[] = {
        { "VOCAL", proc.meterVocalActive.load() ? 1.0f : 0.0f, juce::Colour (0xff6cc070) },
        { "MUSIC", juce::jlimit (0.0f, 1.0f, music / 3.0f),    juce::Colour (0xffffa63a) },
        { "CLIP",  proc.meterClip.load() ? 1.0f : 0.0f,        juce::Colour (0xffe0432f) },
    };
    float ly = ledArea.getY() + 12.0f;
    for (const auto& l : lamps)
    {
        const float cx = ledArea.getX() + 14.0f;
        if (l.amount > 0.05f)
        {
            g.setGradientFill (juce::ColourGradient (l.col.withAlpha (0.45f * l.amount), cx, ly, juce::Colours::transparentBlack, cx + 13.0f, ly, true));
            g.fillEllipse (cx - 13.0f, ly - 13.0f, 26.0f, 26.0f);
        }
        g.setColour (l.col.withMultipliedBrightness (0.22f + 0.78f * l.amount));
        g.fillEllipse (cx - 5.5f, ly - 5.5f, 11.0f, 11.0f);
        g.setColour (juce::Colours::black);
        g.drawEllipse (cx - 5.5f, ly - 5.5f, 11.0f, 11.0f, 1.0f);
        g.setColour (printC);
        g.setFont (VintageLookAndFeel::serif (13.0f, true));
        g.drawText (l.text, juce::Rectangle<float> (cx + 12.0f, ly - 8.0f, 60.0f, 16.0f).toNearestInt(), juce::Justification::centredLeft);
        ly += 32.0f;
    }

    // Hersteller-Schriftzug unten rechts
    g.setColour (printC);
    g.setFont (VintageLookAndFeel::serif (13.0f, true));
    g.drawText ("PHILIP JIN",
                juce::Rectangle<int> (getWidth() - 20 - 170, getHeight() - 38, 170, 16),
                juce::Justification::centredRight);
}

void WavesBreakerEditor::paintOverChildren (juce::Graphics& g)
{
    g.drawImageAt (overlayImage, 0, 0);
}
