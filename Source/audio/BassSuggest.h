#pragma once
#include "Analysis.h"
#include "BassTemplates.h"

// Rule-based bass lines for an analysed riff: the chord root of each bar becomes the bar's transposition, the onsets of the
// riff decide where the bass plays. Every suggestion is a complete BassMachine::Snapshot.
namespace BassSuggest
{
    struct Suggestion { juce::String name; BassMachine::Snapshot snap; };

    inline int rootMidiFor (int pc) { return BassMachine::rootMin + ((pc - BassMachine::rootMin) % 12 + 12) % 12; }

    // degree whose interval above the root is exactly `semis` in this scale (none = -999)
    inline int exactDegree (int scale, int semis)
    {
        const int n = BassMachine::scaleSize (scale);
        for (int d = -n; d <= 2 * n; ++d) if (BassMachine::semitones (scale, d) == semis) return d;
        return -999;
    }

    inline std::vector<Suggestion> suggest (const Analysis::Result& r, const Analysis::Key& key, const std::vector<Analysis::Chord>& chords,
                                            int barSteps, int beat, const DrumMachine::Snapshot* drums)
    {
        using BM = BassMachine;
        std::vector<Suggestion> out;
        const int nb = juce::jlimit (1, (int) BM::maxBars, r.bars);
        if (chords.empty() || r.bars <= 0) return out;

        BM::Snapshot base;
        base.bars = nb; base.barSteps = barSteps; base.root = rootMidiFor (key.rootPc); base.scale = key.scale;
        auto shiftOf = [&] (int bar)
        {
            int s = ((chords[(size_t) (bar % (int) chords.size())].rootPc - key.rootPc) % 12 + 12) % 12;
            return s > 6 ? s - 12 : s;
        };
        for (int b = 0; b < BM::maxBars; ++b) base.shift[b] = shiftOf (b % nb);

        const int perBar = juce::jmax (1, barSteps / juce::jmax (1, beat));
        auto put = [&] (BM::Snapshot& s, int bar, int k, int degree, int len, bool accent)
        {
            if (bar < 0 || bar >= nb || k < 0 || k >= barSteps || degree == -999) return;
            s.steps[bar * barSteps + k] = BM::pack (degree, juce::jmin (len, barSteps - k), false, accent);
        };
        auto third = [&] (int bar) -> int      // chord third in this bar if the key scale holds it, else the fifth
        {
            const int q = chords[(size_t) (bar % (int) chords.size())].quality;
            const int d = q == 2 ? -999 : exactDegree (key.scale, q == 1 ? 3 : 4);
            return d != -999 ? d : exactDegree (key.scale, 7);
        };
        const int dFifth = exactDegree (key.scale, 7), dOct = exactDegree (key.scale, 12);
        auto onset = [&] (int bar, int k) { return r.onsets.empty() ? 0.0f : r.onsets[(size_t) (bar % (int) r.onsets.size())][(size_t) k]; };
        auto lenToNext = [&] (const std::vector<int>& ks, size_t i, int cap)
        {
            const int next = i + 1 < ks.size() ? ks[i + 1] : barSteps;
            return juce::jmax (1, juce::jmin (next - ks[i], cap));
        };

        {   // 1. root on the strong beats of the riff
            auto s = base;
            for (int b = 0; b < nb; ++b)
            {
                std::vector<int> ks { 0 };
                for (int k = beat; k < barSteps; k += beat) if (onset (b, k) >= 0.5f) ks.push_back (k);
                if (barSteps >= 16 && std::find (ks.begin(), ks.end(), 8) == ks.end() && onset (b, 8) > 0.2f) { ks.push_back (8); std::sort (ks.begin(), ks.end()); }
                for (size_t i = 0; i < ks.size(); ++i) put (s, b, ks[i], 0, lenToNext (ks, i, beat * 2), ks[i] == 0);
            }
            out.push_back ({ "Root follow", s });
        }
        {   // 2. the rhythm of the riff, root and fifth
            auto s = base;
            for (int b = 0; b < nb; ++b)
            {
                std::vector<std::pair<float, int>> cand;
                for (int k = 0; k < barSteps; ++k) if (onset (b, k) >= 0.3f || k == 0) cand.push_back ({ k == 0 ? 2.0f : onset (b, k), k });
                const size_t maxNotes = (size_t) juce::jmax (2, barSteps * 6 / 10);
                if (cand.size() > maxNotes) { std::sort (cand.begin(), cand.end(), [] (auto& a, auto& c) { return a.first > c.first; }); cand.resize (maxNotes); }
                std::vector<int> ks; for (auto& c : cand) ks.push_back (c.second);
                std::sort (ks.begin(), ks.end());
                int off = 0;
                for (size_t i = 0; i < ks.size(); ++i)
                {
                    const bool onBeat = ks[i] % beat == 0;
                    int d = 0;
                    if (! onBeat) { d = (off++ % 2 == 1 && dFifth != -999) ? dFifth : 0; }
                    put (s, b, ks[i], d, lenToNext (ks, i, beat), ks[i] == 0);
                }
            }
            out.push_back ({ "Riff rhythm", s });
        }
        {   // 3. one long root per bar
            auto s = base;
            for (int b = 0; b < nb; ++b) put (s, b, 0, 0, barSteps, true);
            out.push_back ({ "Pedal", s });
        }
        {   // 4. walking: root, chord tones, an approach note into the next bar's root
            auto s = base;
            for (int b = 0; b < nb; ++b)
                for (int j = 0; j < perBar; ++j)
                {
                    int d = 0;
                    if (j == perBar - 1 && perBar > 2)
                    {
                        const int want = shiftOf ((b + 1) % nb) - 1 - shiftOf (b);        // a half step under the next root
                        d = BM::nearestDegree (key.scale, want);
                    }
                    else if (j > 0) d = (j % 2 == 1) ? third (b) : (dFifth != -999 ? dFifth : 0);
                    put (s, b, j * beat, d, beat, j == 0);
                }
            out.push_back ({ "Walking", s });
        }
        {   // 5. octave pulse in eighths
            auto s = base;
            for (int b = 0; b < nb; ++b)
                for (int k = 0; k < barSteps; k += 2)
                    put (s, b, k, (k / 2) % 2 == 0 || dOct == -999 ? 0 : dOct, 2, k % beat == 0);
            out.push_back ({ "Octave pulse", s });
        }
        if (drums != nullptr && drums->bars > 0)
        {   // 6. follow the drum kick, with the chord roots
            bool anyKick = false;
            for (int k = 0; k < drums->bars * drums->barSteps; ++k) anyKick |= drums->steps[DrumMachine::Kick][k] != 0;
            if (anyKick)
            {
                auto s = BassTemplates::apply (base, BassTemplates::lockToKickId, drums, beat);
                for (int b = 0; b < BM::maxBars; ++b) s.shift[b] = base.shift[b % nb];
                out.push_back ({ "Lock to kick", s });
            }
        }
        return out;
    }
}
