#pragma once
// Waves Breaker – automatischer Vocal-Fader (reiner C++-DSP-Kern, ohne JUCE-Abhängigkeit)
//
// Aufbau:
//  * Detektor pro Sample: 100-Hz-Hochpass + K-Weighting-Shelf (ITU-R BS.1770) -> gehörrichtiger RMS (40 ms)
//  * Atem/Zischlaut-Erkennung über HF-Anteil (> ~4 kHz)
//  * Regelung im Kontrolltakt (alle 16 Samples): Totzone um das Ziel, Gate mit Hysterese, Idle-Rückkehr,
//    Musik-Sidechain (Ziel folgt der Lautheit der Musik relativ zu ihrem Langzeitmittel)
//  * Gain wird pro Sample linear interpoliert (keine Zipper-Geräusche), optional mit 12 ms Lookahead
//  * Manueller Modus (Automation Read): Gain kommt vom Rider-Fader-Parameter
#include <algorithm>
#include <cmath>
#include <vector>

namespace autorider
{
struct Params
{
    float targetDb    = -18.0f; // gewünschter Vocal-Pegel (K-gewichteter RMS)
    float rangeDb     = 3.0f;   // Zielbereich gesamt (±range/2 = Totzone, kein Eingriff)
    float maxBoostDb  = 9.0f;   // maximale Anhebung (Range Max)
    float maxCutDb    = 9.0f;   // maximale Absenkung (Range Min)
    float speed       = 0.5f;   // 0..1  (0 = langsam, 1 = schnell; entspricht Attack Slow..Fast)
    float gateDb      = -50.0f; // Vocal Sensitivity: darunter = kein Gesang erkannt
    float musicSensDb = 0.0f;   // Music Sensitivity -12..+12 (nur mit Sidechain wirksam)
    float idleDb      = 0.0f;   // Idle-Position des Faders, wenn kein Gesang aktiv ist
    float outputDb    = 0.0f;
    bool  lookahead   = true;   // Gain-Kurve eilt dem Audio voraus
    bool  breathDetect = true;  // Atmer/Zischlaute (HF-dominant) nicht als Gesang werten
    bool  manual      = false;  // true = Automation Read: Fader folgt manualGainDb statt der Automatik
    float manualGainDb = 0.0f;
};

class VocalRider
{
public:
    static constexpr float lookaheadMs   = 12.0f;
    static constexpr float safetyClamp   = 4.0f;  // harte Sicherung gegen Extrempegel (~ +12 dBFS)
    static constexpr int   controlBlock  = 16;    // Regel-Takt in Samples
    static constexpr float gateHystDb    = 3.0f;  // Gate schließt erst 3 dB unter der Schwelle

    void prepare (double sampleRate, int numChannels)
    {
        sr = std::max (8000.0, sampleRate);
        channels = std::max (1, numChannels);
        lookaheadSamples = (int) std::round (lookaheadMs * 0.001 * sr);

        delay.assign ((size_t) channels, std::vector<float> ((size_t) lookaheadSamples + 1, 0.0f));
        writePos = 0;
        hpState.assign ((size_t) channels, 0.0f);
        hpPrevIn.assign ((size_t) channels, 0.0f);
        hfState.assign ((size_t) channels, 0.0f);
        shelf.assign ((size_t) channels, makeKShelf (sr));
        scHp.assign (2, makeKHighpass (sr));
        scShelf.assign (2, makeKShelf (sr));

        const float fs = (float) sr;
        hpCoeff    = std::exp (-2.0f * 3.14159265f * 100.0f / fs);                      // Detektor-HP 100 Hz
        hfCoeff    = std::exp (-2.0f * 3.14159265f * std::min (4000.0f, 0.4f * fs) / fs); // Atem-/Zisch-Band
        rmsCoeff   = std::exp (-1.0f / (0.040f * fs));                                   // 40 ms RMS-Fenster
        const float ctl = (float) controlBlock;
        musicFastC = std::exp (-ctl / (0.8f * fs));   // Musik-Lautheit kurzfristig
        musicRefC  = std::exp (-ctl / (60.0f * fs));  // Langzeit-Referenz (≈ Song-Mittel)
        manualC    = std::exp (-ctl / (0.020f * fs)); // Glättung im Read-Modus (20 ms)
        gateHoldSamples = (int) (1.0 * sr);
        gateRelaxCoeff  = std::exp (-ctl / (1.5f * fs)); // τ = 1,5 s
        clipHoldSamples = (int) (0.5 * sr);

        meanSquare = meanSquareHf = meanSquareRaw = scMeanSquare = 0.0f;
        musicCount = 0;
        musicFast = musicRef = 0.0f;
        musicInit = false;
        gainDb = 0.0f;
        curLin = 1.0f; stepLin = 0.0f;
        levelDb = -120.0f;
        musicShiftDb = 0.0f;
        active = false;
        gateCount = 0;
        clipHold = 0;
        ctlCount = 0;
    }

    int   getLatencySamples (bool on) const { return on ? lookaheadSamples : 0; }
    float getGainDb()       const { return gainDb; }
    float getLevelDb()      const { return levelDb; }
    float getMusicShiftDb() const { return musicShiftDb; }
    bool  isVocalActive()   const { return active; }
    bool  isClipping()      const { return clipHold > 0; }

    void process (float* const* io, int numSamples, const Params& p)
    {
        process (io, nullptr, 0, numSamples, p);
    }

    // io: Haupt-Kanäle (in-place). sc: optionaler Sidechain (Musik) – darf nullptr sein.
    void process (float* const* io, const float* const* sc, int scChannels, int numSamples, const Params& p)
    {
        // Speed → Zeitkonstanten pro Regel-Takt. Absenken schneller als Anheben (natürlicher Fader-Ride).
        const float spd    = std::clamp (finiteOr (p.speed, 0.5f), 0.0f, 1.0f);
        const float downMs = 400.0f * std::pow (0.05f / 0.4f, spd);  // 400 ms … 50 ms
        const float upMs   = downMs * 2.0f;
        const float ctl    = (float) controlBlock;
        const float aDown  = std::exp (-ctl / (0.001f * downMs * (float) sr));
        const float aUp    = std::exp (-ctl / (0.001f * upMs   * (float) sr));

        const float half   = 0.5f * std::max (0.0f, finiteOr (p.rangeDb, 3.0f));
        const float maxUp  = std::clamp (finiteOr (p.maxBoostDb, 9.0f), 0.0f, 24.0f);
        const float maxDn  = std::clamp (finiteOr (p.maxCutDb,   9.0f), 0.0f, 24.0f);
        const float idle   = std::clamp (finiteOr (p.idleDb, 0.0f), -24.0f, 24.0f);
        const float outDb  = std::clamp (finiteOr (p.outputDb, 0.0f), -24.0f, 24.0f);
        const float musicRatio = std::clamp (finiteOr (p.musicSensDb, 0.0f), -12.0f, 12.0f) / 12.0f;
        const float gate   = finiteOr (p.gateDb, -50.0f);
        const float target = finiteOr (p.targetDb, -18.0f);
        const float manualDb = std::clamp (finiteOr (p.manualGainDb, 0.0f), -24.0f, 24.0f);
        const int   delaySamples = p.lookahead ? lookaheadSamples : 0;
        const int   dlen = lookaheadSamples + 1;
        const int   scCh = (sc != nullptr) ? std::clamp (scChannels, 0, 2) : 0;
        if (scCh == 0) { musicInit = false; musicShiftDb = 0.0f; scMeanSquare = 0.0f; }

        for (int n = 0; n < numSamples; ++n)
        {
            // --- Detektor (kanalgekoppelt): K-gewichteter Pegel und Hochband (> ~4 kHz) ---
            float sumSq = 0.0f, sumRaw = 0.0f, sumHf = 0.0f;
            for (int c = 0; c < channels; ++c)
            {
                float x = io[c][n];
                if (! std::isfinite (x)) { x = 0.0f; io[c][n] = 0.0f; }
                const float xp = hpPrevIn[(size_t) c];
                const float y  = hpCoeff * (hpState[(size_t) c] + x - xp);
                const float h  = hfCoeff * (hfState[(size_t) c] + x - xp);
                hpState[(size_t) c] = y;
                hfState[(size_t) c] = h;
                hpPrevIn[(size_t) c] = x;
                const float k = shelf[(size_t) c].process (y);
                sumSq  += k * k;
                sumRaw += y * y;
                sumHf  += h * h;
            }
            meanSquare   = rmsCoeff * meanSquare   + (1.0f - rmsCoeff) * (sumSq / (float) channels);
            meanSquareHf = rmsCoeff * meanSquareHf + (1.0f - rmsCoeff) * (sumHf / (float) channels);
            meanSquareRaw = rmsCoeff * meanSquareRaw + (1.0f - rmsCoeff) * (sumRaw / (float) channels);

            if (scCh > 0)
            {
                float s2 = 0.0f;
                for (int c = 0; c < scCh; ++c)
                {
                    float v = sc[c][n];
                    if (! std::isfinite (v)) v = 0.0f;
                    const float k = scShelf[(size_t) c].process (scHp[(size_t) c].process (v));
                    s2 += k * k;
                }
                scMeanSquare = rmsCoeff * scMeanSquare + (1.0f - rmsCoeff) * (s2 / (float) scCh);
            }

            // --- Regel-Takt ---
            if (ctlCount == 0)
            {
                updateControl (p, scCh > 0, target, gate, half, maxUp, maxDn, idle, musicRatio,
                               manualDb, aDown, aUp);
                const float newLin = std::min (dbToLin (gainDb + outDb), 16.0f);
                stepLin = (newLin - curLin) / ctl;
            }
            if (++ctlCount >= controlBlock) ctlCount = 0;
            curLin += stepLin;

            // --- Verzögerung (Lookahead) + Anwenden + Sicherung ---
            for (int c = 0; c < channels; ++c)
            {
                auto& d = delay[(size_t) c];
                d[(size_t) writePos] = io[c][n];
                int readPos = writePos - delaySamples;
                if (readPos < 0) readPos += dlen;
                const float o = d[(size_t) readPos] * curLin;
                if (std::fabs (o) > 1.0f) clipHold = clipHoldSamples;
                io[c][n] = std::clamp (o, -safetyClamp, safetyClamp);
            }
            if (clipHold > 0) --clipHold;
            if (++writePos >= dlen) writePos = 0;
        }
    }

private:
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        float process (float x)
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            if (! std::isfinite (z1) || ! std::isfinite (z2)) { z1 = z2 = 0.0f; return 0.0f; }
            return y;
        }
    };

    // ITU-R BS.1770 Stufe 1 (High-Shelf ~+4 dB ab ~1,7 kHz), für beliebige Samplerate
    static Biquad makeKShelf (double fs)
    {
        const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
        const double K = std::tan (3.141592653589793 * f0 / fs);
        const double Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
        const double a0 = 1.0 + K / Q + K * K;
        Biquad b;
        b.b0 = (float) ((Vh + Vb * K / Q + K * K) / a0);
        b.b1 = (float) (2.0 * (K * K - Vh) / a0);
        b.b2 = (float) ((Vh - Vb * K / Q + K * K) / a0);
        b.a1 = (float) (2.0 * (K * K - 1.0) / a0);
        b.a2 = (float) ((1.0 - K / Q + K * K) / a0);
        return b;
    }

    // ITU-R BS.1770 Stufe 2 (RLB-Hochpass ~38 Hz) – für den Musik-Sidechain
    static Biquad makeKHighpass (double fs)
    {
        const double f0 = 38.13547087602444, Q = 0.5003270373238773;
        const double K = std::tan (3.141592653589793 * f0 / fs);
        const double a0 = 1.0 + K / Q + K * K;
        Biquad b;
        b.b0 = 1.0f; b.b1 = -2.0f; b.b2 = 1.0f;
        b.a1 = (float) (2.0 * (K * K - 1.0) / a0);
        b.a2 = (float) ((1.0 - K / Q + K * K) / a0);
        return b;
    }

    void updateControl (const Params& p, bool hasSc, float target, float gate, float half,
                        float maxUp, float maxDn, float idle, float musicRatio, float manualDb,
                        float aDown, float aUp)
    {
        levelDb = 10.0f * std::log10 (meanSquare + 1.0e-12f);

        // --- Musik-Sidechain: Ziel folgt der Musik-Lautheit relativ zu ihrem Langzeitmittel ---
        float shift = 0.0f;
        if (hasSc)
        {
            const float ms = scMeanSquare;
            if (ms > 1.0e-6f) // Musik über ca. -60 dBFS; Stille verschiebt die Referenz nicht
            {
                if (! musicInit) { musicFast = musicRef = ms; musicCount = 0; musicInit = true; }
                // Anfangs echter Mittelwert (schnell eingeschwungen), danach 60-s-Langzeitmittel
                musicCount = std::min (musicCount + 1, 1 << 30);
                const float wFast = std::max (1.0f - musicFastC, 1.0f / (float) musicCount);
                const float wRef  = std::max (1.0f - musicRefC,  1.0f / (float) musicCount);
                musicFast += wFast * (ms - musicFast);
                musicRef  += wRef  * (ms - musicRef);
                const float delta = 10.0f * std::log10 ((musicFast + 1.0e-12f) / (musicRef + 1.0e-12f));
                shift = musicRatio * std::clamp (delta, -12.0f, 12.0f);
            }
        }
        musicShiftDb = shift;
        const float tgt = std::clamp (target + shift, -60.0f, 0.0f);

        // --- Gesang aktiv? (Pegel über Sensitivity mit Hysterese, nicht Atem-/Zisch-dominiert) ---
        const float hfRatio    = meanSquareHf / (meanSquareRaw + 1.0e-12f); // ungewichtet vergleichen
        const bool  breathLike = p.breathDetect && hfRatio > 0.3f;
        const float thr        = active ? gate - gateHystDb : gate;
        active = (levelDb >= thr) && ! breathLike;

        if (p.manual)
        {
            // Automation Read: Fader kommt aus der DAW-Automation
            gainDb = manualC * gainDb + (1.0f - manualC) * manualDb;
            return;
        }

        float desired;
        if (! active)
        {
            // Kein Gesang: Fader halten (kein Rauschen/Atmer hochziehen), nach ~1 s sanft zur Idle-Position.
            gateCount += controlBlock;
            if (gateCount > gateHoldSamples) gainDb = idle + (gainDb - idle) * gateRelaxCoeff;
            desired = gainDb;
        }
        else
        {
            gateCount = 0;
            // Feed-forward: benötigter Gain = Ziel - Eingangspegel. Liegt der Ausgang (Pegel + aktueller Gain)
            // schon im Zielbereich, bleibt der Fader stehen; sonst nur bis an den Rand des Bereichs fahren.
            const float err = tgt - levelDb;
            desired = std::clamp (gainDb, err - half, err + half);
            desired = std::clamp (desired, -maxDn, maxUp);
        }

        const float a = (desired < gainDb) ? aDown : aUp;
        gainDb = a * gainDb + (1.0f - a) * desired;
        if (! std::isfinite (gainDb)) gainDb = 0.0f;
    }

    static float dbToLin (float db) { return std::pow (10.0f, db * 0.05f); }
    static float finiteOr (float v, float fallback) { return std::isfinite (v) ? v : fallback; }

    double sr = 44100.0;
    int channels = 2;
    int lookaheadSamples = 0;
    int writePos = 0;
    std::vector<std::vector<float>> delay;
    std::vector<float> hpState, hpPrevIn, hfState;
    std::vector<Biquad> shelf, scHp, scShelf;
    float hpCoeff = 0.99f, hfCoeff = 0.5f, rmsCoeff = 0.999f;
    float musicFastC = 0.999f, musicRefC = 0.9999f, manualC = 0.9f;
    float meanSquare = 0.0f, meanSquareHf = 0.0f, meanSquareRaw = 0.0f, scMeanSquare = 0.0f, musicFast = 0.0f, musicRef = 0.0f;
    float gainDb = 0.0f, levelDb = -120.0f, musicShiftDb = 0.0f;
    float curLin = 1.0f, stepLin = 0.0f;
    bool musicInit = false, active = false;
    int musicCount = 0;
    int gateCount = 0, gateHoldSamples = 48000, clipHold = 0, clipHoldSamples = 24000, ctlCount = 0;
    float gateRelaxCoeff = 0.9999f;
};
} // namespace autorider
