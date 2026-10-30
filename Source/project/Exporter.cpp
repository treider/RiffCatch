#include "Exporter.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace Exporter
{
bool bounceWav (AudioEngine& e, const juce::File& file, int loops, juce::String& error)
{
    const double sr = e.getSampleRate();
    const bool loopMode = e.loopEnabled.load();
    const juce::int64 loopLen = e.getLoopLength();

    juce::int64 passLen = loopMode ? loopLen : 0;
    if (! loopMode)
        for (int slot : e.order) passLen = juce::jmax (passLen, e.tracks[slot].length.load());
    if (passLen <= 0) { error = "Nothing to export - no recorded audio."; return false; }

    const juce::int64 total = passLen * juce::jmax (1, loopMode ? loops : 1);
    if (total > (juce::int64) sr * 3600) { error = "Export too long."; return false; }

    juce::AudioBuffer<float> mix (2, (int) total);
    mix.clear();

    bool anySolo = false;
    for (int slot : e.order) anySolo |= e.tracks[slot].solo.load();

    for (int slot : e.order)
    {
        auto& tr = e.tracks[slot];
        if (tr.mute.load() || (anySolo && ! tr.solo.load())) continue;

        const float theta = (juce::jlimit (-1.0f, 1.0f, tr.pan.load()) + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        const float gl = tr.volume.load() * std::cos (theta), gr = tr.volume.load() * std::sin (theta);
        const int nd = tr.nudge.load();

        juce::SpinLock::ScopedLockType lk (tr.lock);
        const juce::int64 len = juce::jmin<juce::int64> (tr.length.load(), tr.data.getNumSamples());
        if (len <= 0) continue;
        const bool wrap = loopMode && std::abs (len - loopLen) <= 2;
        const float* dl = tr.data.getReadPointer (0);
        const float* dr = tr.data.getReadPointer (1);
        float* ml = mix.getWritePointer (0);
        float* mr = mix.getWritePointer (1);

        for (juce::int64 p = 0; p < total; ++p)
        {
            const juce::int64 q = loopMode ? p % loopLen : p;
            auto idx = q - nd;
            if (wrap) { idx %= len; if (idx < 0) idx += len; }
            if (idx < 0 || idx >= len) continue;
            ml[p] += dl[idx] * gl;
            mr[p] += dr[idx] * gr;
        }
    }
    mix.applyGain (e.masterVolume.load());

    file.deleteFile();
    std::unique_ptr<juce::FileOutputStream> os (file.createOutputStream());
    if (os == nullptr) { error = "Could not write " + file.getFullPathName(); return false; }
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, 2, 24, {}, 0));
    if (w == nullptr) { error = "WAV writer failed"; return false; }
    os.release();
    if (! w->writeFromAudioSampleBuffer (mix, 0, mix.getNumSamples())) { error = "Write failed"; return false; }
    return true;
}
}
