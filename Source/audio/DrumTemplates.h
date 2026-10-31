#pragma once
#include "DrumMachine.h"

// Ready-made two-bar drum patterns (bar 2 ends in a small fill). Rows: Kick, Snare, Clap, Hat, Open hat, Cymbal.
// x = hit, X = accent, . = off, 16 steps per bar.
namespace DrumTemplates
{
    struct Template { const char* group; const char* name; const char* bar[2][DrumMachine::numVoices]; };

    inline const Template* all (int& count)
    {
        static const Template t[] =
        {
            { "Rock", "Rock 1", { { "x.......x.......", "....X.......X...", "................", "x.x.x.x.x.x.x.x.", "................", "x..............." },
                                  { "x.......x.......", "....X.......xxxx", "................", "x.x.x.x.x.x.....", "................", "................" } } },
            { "Rock", "Rock 2", { { "x.......x.x.....", "....X.......X...", "................", "x.x.x.x.x.x.x.x.", "................", "x..............." },
                                  { "x.......x.x..x..", "....X.......xXxX", "................", "x.x.x.x.x.x.x...", "................", "................" } } },
            { "Rock", "Rock 3 (half-time)", { { "x.........x.....", "........X.......", "................", "x.x.x.x.x.x.x.x.", "................", "x..............." },
                                              { "x.........x.....", "........X.....xx", "................", "x.x.x.x.x.x.x.x.", "................", "................" } } },
            { "Rock", "Rock 4 (driving)", { { "x.x...x.x.x.....", "....X.......X...", "................", "x.x.x.x.x.x.x.x.", "..............x.", "x..............." },
                                            { "x.x...x.x.x.....", "....X...x.x.xxxx", "................", "x.x.x.x.x.x.x...", "................", "................" } } },
            { "Pop", "Pop 1", { { "x.......x.......", "................", "....X.......X...", "x.x.x.x.x.x.x.x.", "......x.......x.", "x..............." },
                                { "x.......x.......", "............xxxx", "....X.......X...", "x.x.x.x.x.x.x...", "................", "................" } } },
            { "Pop", "Pop 2 (four on the floor)", { { "x...x...x...x...", "................", "....x.......x...", ".x.x.x.x.x.x.x.x", "................", "x..............." },
                                                    { "x...x...x...x...", "............xxxx", "....x.......x...", ".x.x.x.x.x.x....", "................", "................" } } },
            { "Pop", "Pop 3 (16th hats)", { { "x..x..x...x.x...", "....X.......X...", "................", "XxxxXxxxXxxxXxxx", "................", "x..............." },
                                            { "x..x..x...x.x...", "....X.....x.xxxx", "................", "XxxxXxxxXxxx....", "................", "................" } } },
            { "Pop", "Pop 4 (laid back)", { { "x.......x..x....", "....x.......x...", "................", "x.x.x.x.x.x.x.x.", "......x.......x.", "x..............." },
                                            { "x.......x..x....", "....x.......x.x.", "................", "x.x.x.x.x.x.x...", "................", "................" } } },
        };
        count = (int) (sizeof (t) / sizeof (t[0]));
        return t;
    }

    // Replaces all steps of the snapshot with the template (bars = at least 2; longer patterns repeat the two bars).
    inline DrumMachine::Snapshot apply (const DrumMachine::Snapshot& cur, int index)
    {
        int n = 0;
        const auto* t = all (n);
        auto out = cur;
        if (index < 0 || index >= n) return out;
        out.bars = juce::jlimit (2, DrumMachine::maxBars, cur.bars);
        for (int bar = 0; bar < DrumMachine::maxBars; ++bar)
            for (int v = 0; v < DrumMachine::numVoices; ++v)
                for (int k = 0; k < DrumMachine::stepsPerBar; ++k)
                {
                    uint8_t val = 0;
                    if (bar < out.bars)
                    {
                        const char c = t[index].bar[bar % 2][v][k];
                        val = c == 'X' ? 2 : (c == 'x' ? 1 : 0);
                    }
                    out.steps[v][bar * DrumMachine::stepsPerBar + k] = val;
                }
        return out;
    }
}