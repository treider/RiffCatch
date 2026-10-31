#pragma once
#include "BassMachine.h"
#include "DrumMachine.h"
#include <vector>

// Ready-made bass patterns. A template is a 16-step note list tiled over the bar (so it fits every time signature);
// notes are written in semitones above the root and mapped to the nearest degree of the current scale.
namespace BassTemplates
{
    struct Note { int step, semis, len; bool accent, slide; };
    struct Template { const char* name; std::vector<Note> notes; };

    inline const std::vector<Template>& all (int scale)
    {
        const int third = BassMachine::hasSemitone (scale, 3) ? 3 : 4;
        static thread_local std::vector<Template> t;
        t.clear();
        t.push_back ({ "Root pulse",   { { 0, 0, 4, true, false }, { 4, 0, 4, false, false }, { 8, 0, 4, false, false }, { 12, 0, 4, false, false } } });
        {
            Template e { "Eighths", {} };
            for (int k = 0; k < 16; k += 2) e.notes.push_back ({ k, 0, 2, k % 4 == 0, false });
            t.push_back (e);
        }
        {
            Template e { "Root + octave", {} };
            for (int k = 0; k < 16; k += 2) e.notes.push_back ({ k, (k / 2) % 2 == 0 ? 0 : 12, 2, k % 4 == 0, false });
            t.push_back (e);
        }
        t.push_back ({ "Root + fifth", { { 0, 0, 3, true, false }, { 4, 0, 2, false, false }, { 6, 0, 2, false, false }, { 8, 7, 3, true, false },
                                         { 12, 0, 2, false, false }, { 14, 7, 2, false, false } } });
        t.push_back ({ "Funk",         { { 0, 0, 2, true, false }, { 3, 0, 1, false, false }, { 6, 10, 1, false, false }, { 8, 0, 2, true, false },
                                         { 10, 0, 1, false, false }, { 12, 7, 2, false, false }, { 14, 5, 1, false, false } } });
        t.push_back ({ "Arpeggio",     { { 0, 0, 2, true, false }, { 2, third, 2, false, false }, { 4, 7, 2, false, false }, { 6, third, 2, false, false },
                                         { 8, 0, 2, true, false }, { 10, third, 2, false, false }, { 12, 7, 2, false, false }, { 14, 12, 2, false, false } } });
        t.push_back ({ "Walking",      { { 0, 0, 4, true, false }, { 4, third, 4, false, false }, { 8, 7, 4, false, false }, { 12, 9, 4, false, false } } });
        t.push_back ({ "Acid slides",  { { 0, 0, 2, true, false }, { 2, 0, 1, false, false }, { 3, 12, 1, false, true }, { 4, 10, 2, false, false },
                                         { 6, 0, 1, false, false }, { 7, 7, 1, false, true }, { 8, 0, 2, true, false }, { 10, 0, 1, false, false },
                                         { 11, 12, 1, false, false }, { 12, 10, 1, false, true }, { 14, 0, 2, false, false }, { 15, 7, 1, false, true } } });
        return t;
    }
    constexpr int numPatterns = 8;
    constexpr int lockToKickId = numPatterns;          // index after the pattern templates

    inline const char* name (int id) { return id == lockToKickId ? "Lock to kick" : all (0)[(size_t) id].name; }

    // Fills every bar of the pattern. Notes outside the bar are cut; a note never runs past the end of its bar.
    inline BassMachine::Snapshot apply (const BassMachine::Snapshot& cur, int id, const DrumMachine::Snapshot* drums, int beat)
    {
        auto out = cur;
        std::memset (out.steps, 0, sizeof (out.steps));
        const int bs = cur.barSteps;
        if (id == lockToKickId)
        {
            if (drums == nullptr) return out;
            out.bars = juce::jlimit (1, BassMachine::maxBars, drums->bars);
            const int ps = out.bars * bs;
            for (int k = 0; k < ps; ++k)
            {
                if (drums->steps[DrumMachine::Kick][k] == 0) continue;
                int next = k + 1;
                while (next < ps && next < k + beat && drums->steps[DrumMachine::Kick][next] == 0) ++next;
                const int barEnd = (k / bs + 1) * bs;
                out.steps[k] = BassMachine::pack (0, juce::jmin (next, barEnd) - k, false, drums->steps[DrumMachine::Kick][k] >= 2);
            }
            return out;
        }
        const auto& tpl = all (cur.scale)[(size_t) juce::jlimit (0, numPatterns - 1, id)];
        for (int b = 0; b < cur.bars; ++b)
            for (int k = 0; k < bs; ++k)
                for (const auto& n : tpl.notes)
                    if (n.step == k % 16)
                        out.steps[b * bs + k] = BassMachine::pack (BassMachine::nearestDegree (cur.scale, n.semis), juce::jmin (n.len, bs - k), n.slide, n.accent);
        return out;
    }

    struct Progression { const char* name; int shift[4]; };
    constexpr int numProgressions = 6;
    inline const Progression& progression (int i)
    {
        static const Progression p[numProgressions] = {
            { "None", { 0, 0, 0, 0 } }, { "I-IV-V-IV", { 0, 5, 7, 5 } }, { "I-V-vi-IV", { 0, 7, 9, 5 } },
            { "I-vi-IV-V", { 0, 9, 5, 7 } }, { "i-bVI-bIII-bVII", { 0, 8, 3, 10 } }, { "i-bVII-bVI-V", { 0, 10, 8, 7 } } };
        return p[juce::jlimit (0, numProgressions - 1, i)];
    }

    // Sets the per-bar transposition; grows a short pattern to four bars (bar 1 copied) so the progression is audible.
    inline BassMachine::Snapshot applyProgression (const BassMachine::Snapshot& cur, int id)
    {
        auto out = cur;
        const auto& p = progression (id);
        if (id > 0 && out.bars < 4)
        {
            const int bs = out.barSteps;
            for (int b = out.bars; b < 4; ++b)
                for (int k = 0; k < bs; ++k) out.steps[b * bs + k] = out.steps[k];
            out.bars = 4;
        }
        for (int b = 0; b < BassMachine::maxBars; ++b) out.shift[b] = p.shift[b % 4];
        return out;
    }

    inline juce::String shiftName (int s)
    {
        switch (s)
        {
            case 0: return "I";   case 2: return "II";  case 3: return "bIII"; case 4: return "III";
            case 5: return "IV";  case 7: return "V";   case 8: return "bVI";  case 9: return "VI";
            case 10: return "bVII"; case 11: return "VII"; default: return (s > 0 ? "+" : "") + juce::String (s);
        }
    }
}
