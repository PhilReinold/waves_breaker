// Standalone-Test des DSP-Kerns: g++ -std=c++17 -O2 -I../Source dsp_test.cpp -o dsp_test
#include "VocalRiderDSP.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

static float rmsDb (const std::vector<float>& v, size_t a, size_t b)
{
    double s = 0; for (size_t i = a; i < b; ++i) s += (double) v[i] * v[i];
    return 10.0f * std::log10 ((float) (s / (double) (b - a)) + 1e-12f);
}

static std::vector<float> sine (float db, double sec, double sr, float hz = 200.0f)
{
    std::vector<float> v ((size_t) (sec * sr));
    const float amp = std::pow (10.0f, db / 20.0f) * 1.41421356f;
    for (size_t i = 0; i < v.size(); ++i) v[i] = amp * std::sin (2.0f * 3.14159265f * hz * (float) i / (float) sr);
    return v;
}

int main()
{
    bool ok = true;
    const double sr = 48000.0;

    // ---------- 1) Funktionstest: leise / laut / leise / Stille ----------
    {
        autorider::VocalRider r; r.prepare (sr, 1);
        autorider::Params p; p.targetDb = -18; p.rangeDb = 2; p.speed = 0.7f; p.lookahead = true;
        std::vector<float> in;
        for (auto& s : { sine (-30, 3, sr), sine (-8, 3, sr), sine (-30, 3, sr), sine (-90, 1, sr) }) in.insert (in.end(), s.begin(), s.end());
        std::vector<float> out = in; float* ch[1] = { out.data() };
        r.process (ch, (int) out.size(), p);
        const size_t s3 = (size_t) (3 * sr);
        float q1 = rmsDb (out, s3 - s3 / 2, s3), l1 = rmsDb (out, 2 * s3 - s3 / 2, 2 * s3), tail = rmsDb (out, out.size() - 1000, out.size());
        std::printf ("1) leise -30 -> %.1f dB | laut -8 -> %.1f dB | Stille -> %.1f dB\n", q1, l1, tail);
        ok &= std::fabs (l1 + 18.0f) < 2.5f && q1 > -24.0f && std::isfinite (tail) && tail < -70.0f;
    }

    // ---------- 2) Idle: nach Pause wandert der Fader zur Idle-Position ----------
    {
        autorider::VocalRider r; r.prepare (sr, 1);
        autorider::Params p; p.idleDb = -6.0f;
        auto in = sine (-30, 2, sr); auto sil = sine (-100, 8, sr); in.insert (in.end(), sil.begin(), sil.end());
        float* ch[1] = { in.data() }; r.process (ch, (int) in.size(), p);
        std::printf ("2) Idle -6 dB: Fader nach 8 s Pause = %.2f dB\n", r.getGainDb());
        ok &= std::fabs (r.getGainDb() + 6.0f) < 0.5f;
    }

    // ---------- 3) Atem-Erkennung: hochfrequentes Rauschen darf den Fader nicht hochziehen ----------
    {
        autorider::VocalRider r; r.prepare (sr, 1);
        autorider::Params p; p.gateDb = -60.0f;
        auto in = sine (-18, 1, sr); auto br = sine (-40, 2, sr, 9000.0f); in.insert (in.end(), br.begin(), br.end());
        float* ch[1] = { in.data() }; r.process (ch, (int) in.size(), p);
        std::printf ("3) Atem (9 kHz, -40 dB): Gain %.2f dB, aktiv=%d (erwartet ~0 dB, aktiv=0)\n", r.getGainDb(), (int) r.isVocalActive());
        ok &= std::fabs (r.getGainDb()) < 3.0f && ! r.isVocalActive();
    }

    // ---------- 4) Musik-Sidechain: lauter werdende Musik hebt das Ziel (Music Sens > 0) ----------
    {
        autorider::VocalRider r; r.prepare (sr, 1);
        autorider::Params p; p.musicSensDb = 12.0f; p.targetDb = -30.0f; p.speed = 1.0f;
        auto voc = sine (-30, 6, sr); auto mus = sine (-30, 3, sr, 90.0f); auto loud = sine (-18, 3, sr, 90.0f);
        mus.insert (mus.end(), loud.begin(), loud.end());
        float* io[1] = { voc.data() }; const float* sc[1] = { mus.data() };
        r.process (io, sc, 1, (int) voc.size(), p);
        std::printf ("4) Musik +12 dB lauter: Ziel-Verschiebung %.1f dB, Fader %.1f dB\n", r.getMusicShiftDb(), r.getGainDb());
        ok &= r.getMusicShiftDb() > 2.0f && r.getGainDb() > 2.0f; // Referenz = Langzeitmittel der Musik
    }

    // ---------- 5) Robustheit (Fuzz): NaN/Inf, Riesenpegel, DC, Denormals, Extrem-Parameter, alle Sample-Raten ----------
    {
        std::srand (1234);
        const double rates[] = { 8000, 22050, 44100, 48000, 96000, 192000 };
        bool finiteAll = true; float maxAbs = 0;
        for (double rate : rates)
            for (int chs = 1; chs <= 2; ++chs)
            {
                autorider::VocalRider r; r.prepare (rate, chs);
                for (int iter = 0; iter < 6; ++iter)
                {
                    autorider::Params p;
                    auto rnd = [] (float a, float b) { return a + (b - a) * (float) std::rand() / (float) RAND_MAX; };
                    p.targetDb = rnd (-60, 0); p.rangeDb = rnd (0, 24); p.maxBoostDb = rnd (0, 24); p.maxCutDb = rnd (0, 24);
                    p.speed = rnd (-1, 2); p.gateDb = rnd (-90, 0); p.musicSensDb = rnd (-30, 30); p.idleDb = rnd (-40, 40);
                    p.outputDb = rnd (-40, 40); p.lookahead = iter & 1; p.breathDetect = (iter >> 1) & 1;
                    const int N = 4096;
                    std::vector<std::vector<float>> buf ((size_t) chs, std::vector<float> (N)), scb (2, std::vector<float> (N));
                    for (int c = 0; c < chs; ++c)
                        for (int i = 0; i < N; ++i)
                        {
                            float v = rnd (-1, 1);
                            switch (iter % 6) {
                                case 1: v *= 1.0e6f; break;
                                case 2: v = (i % 97 == 0) ? std::numeric_limits<float>::quiet_NaN() : v; break;
                                case 3: v = (i % 101 == 0) ? std::numeric_limits<float>::infinity() : v; break;
                                case 4: v = 0.9f; break;
                                case 5: v = 1.0e-40f; break;
                                default: break; }
                            buf[(size_t) c][(size_t) i] = v;
                        }
                    for (auto& s : scb) for (auto& v : s) v = (iter == 2) ? std::numeric_limits<float>::quiet_NaN() : rnd (-1, 1);
                    float* io[2] = { buf[0].data(), chs > 1 ? buf[1].data() : nullptr };
                    const float* sc[2] = { scb[0].data(), scb[1].data() };
                    r.process (io, sc, 2, N, p);
                    for (int c = 0; c < chs; ++c) for (float v : buf[(size_t) c]) { finiteAll &= std::isfinite (v); maxAbs = std::max (maxAbs, std::fabs (v)); }
                    finiteAll &= std::isfinite (r.getGainDb()) && std::isfinite (r.getLevelDb()) && std::isfinite (r.getMusicShiftDb());
                }
            }
        std::printf ("5) Fuzz: alles endlich = %d, max |Ausgang| = %.2f (Sicherung %.1f)\n", (int) finiteAll, maxAbs, autorider::VocalRider::safetyClamp);
        ok &= finiteAll && maxAbs <= autorider::VocalRider::safetyClamp + 1e-4f;
    }


    // ---------- 6) Automation Read: Fader folgt dem manuellen Wert, unabhängig vom Pegel ----------
    {
        autorider::VocalRider r; r.prepare (sr, 2);
        autorider::Params p; p.manual = true; p.manualGainDb = -4.5f;
        auto a = sine (-30, 1, sr), b = a;
        float* io[2] = { a.data(), b.data() }; r.process (io, (int) a.size(), p);
        const float outDb = rmsDb (a, a.size() / 2, a.size());
        std::printf ("6) Read-Modus -4,5 dB: Fader %.2f dB, Ausgang %.1f dB (erwartet -34,5)\n", r.getGainDb(), outDb);
        ok &= std::fabs (r.getGainDb() + 4.5f) < 0.05f && std::fabs (outDb + 34.5f) < 0.3f;
    }

    // ---------- 7) Gate-Hysterese: Pegel, der um die Schwelle zittert, schaltet nicht ständig ----------
    {
        autorider::VocalRider r; r.prepare (sr, 1);
        autorider::Params p; p.gateDb = -40.0f; p.breathDetect = false;
        int toggles = 0; bool last = false;
        for (int k = 0; k < 40; ++k)
        {
            auto s = sine ((k & 1) ? -39.0f : -41.0f, 0.1, sr);
            float* ch[1] = { s.data() }; r.process (ch, (int) s.size(), p);
            if (k > 2 && r.isVocalActive() != last) ++toggles;
            last = r.isVocalActive();
        }
        std::printf ("7) Gate-Hysterese: %d Umschaltungen bei Pegel ±1 dB um die Schwelle (erwartet 0)\n", toggles);
        ok &= toggles == 0;
    }

    // ---------- 8) Keine Zipper-/Knackgeräusche: Gain ändert sich pro Sample nur minimal ----------
    {
        autorider::VocalRider r; r.prepare (sr, 1);
        autorider::Params p; p.speed = 1.0f; p.lookahead = false; p.maxBoostDb = 12; p.maxCutDb = 12;
        std::vector<float> in (sr * 2, 0.0f);
        for (size_t i = 0; i < in.size(); ++i) in[i] = (i < in.size() / 2) ? 0.01f : 0.5f; // DC-Sprung als Messsignal
        auto out = in; float* ch[1] = { out.data() }; r.process (ch, (int) out.size(), p);
        float maxStepDb = 0;
        for (size_t i = 2000; i < out.size(); ++i)
            if (i != in.size() / 2)
            {
                const float g0 = out[i - 1] / in[i - 1], g1 = out[i] / in[i];
                maxStepDb = std::max (maxStepDb, std::fabs (20.0f * std::log10 (g1 / g0)));
            }
        std::printf ("8) Größter Gain-Schritt pro Sample: %.4f dB (erwartet < 0,05)\n", maxStepDb);
        ok &= maxStepDb < 0.05f;
    }

    // ---------- 9) Zielbereich: Pegel im Bereich -> Fader bewegt sich nicht ----------
    {
        autorider::VocalRider r; r.prepare (sr, 1);
        autorider::Params p; p.targetDb = -18; p.rangeDb = 6;
        auto in = sine (-20, 2, sr); auto in2 = sine (-16, 2, sr); in.insert (in.end(), in2.begin(), in2.end());
        float* ch[1] = { in.data() }; r.process (ch, (int) in.size(), p);
        const float o1 = rmsDb (in, (size_t) (1.5 * sr), (size_t) (2 * sr)), o2 = rmsDb (in, (size_t) (3.5 * sr), (size_t) (4 * sr));
        std::printf ("9) Eingang -20/-16 dB, Ziel -18 ±3: Ausgang %.1f / %.1f dB, Fader %.2f dB (erwartet: Ausgang im Bereich, Fader klein)\n", o1, o2, r.getGainDb());
        ok &= o1 >= -21.2f && o1 <= -14.8f && o2 >= -21.2f && o2 <= -14.8f && std::fabs (r.getGainDb()) < 1.5f;
    }

    std::printf (ok ? "ALLE TESTS OK\n" : "TEST FEHLGESCHLAGEN\n");
    return ok ? 0 : 1;
}
