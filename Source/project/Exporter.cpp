#include "Exporter.h"
#include <juce_audio_formats/juce_audio_formats.h>
#if RC_MP3
 #include <lame.h>
#endif

namespace Exporter
{
static bool renderMix (AudioEngine& e, int loops, juce::AudioBuffer<float>& mix, juce::String& error)
{
    const double sr = e.getSampleRate();
    const bool loopMode = e.loopEnabled.load();
    const juce::int64 loopLen = e.getLoopLength();

    juce::int64 passLen = loopMode ? loopLen : 0;
    if (! loopMode)
        for (int slot : e.order) passLen = juce::jmax<juce::int64> (passLen, e.tracks[slot].length.load() + juce::jmax (0, e.tracks[slot].offset.load()));
    if (! loopMode && e.drums.hasAnySteps())
        passLen = juce::jmax<juce::int64> (passLen, (juce::int64) std::ceil (e.drums.patternSteps() * e.getSamplesPerBeat() / 4.0));
    if (passLen <= 0) { error = "Nothing to export - no recorded audio."; return false; }

    const juce::int64 total = passLen * juce::jmax (1, loopMode ? loops : 1);
    if (total > (juce::int64) sr * 3600) { error = "Export too long."; return false; }

    mix.setSize (2, (int) total);
    mix.clear();

    bool anySolo = e.drums.solo.load();
    for (int slot : e.order) anySolo |= e.tracks[slot].solo.load();

    for (int slot : e.order)
    {
        auto& tr = e.tracks[slot];
        if (tr.mute.load() || (anySolo && ! tr.solo.load())) continue;

        const float theta = (juce::jlimit (-1.0f, 1.0f, tr.pan.load()) + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        const float gl = tr.volume.load() * std::cos (theta), gr = tr.volume.load() * std::sin (theta);
        const int nd = tr.offset.load();

        juce::SpinLock::ScopedLockType lk (tr.lock);
        const juce::int64 len = juce::jmin<juce::int64> (tr.length.load(), tr.data.getNumSamples());
        if (len <= 0) continue;
        const float* dl = tr.data.getReadPointer (0);
        const float* dr = tr.data.getReadPointer (1);
        const bool useFx = tr.echoOn.load() || tr.reverbOn.load();
        juce::AudioBuffer<float> fxBuf;
        if (useFx) { fxBuf.setSize (2, (int) total); fxBuf.clear(); }
        float* ml = useFx ? fxBuf.getWritePointer (0) : mix.getWritePointer (0);
        float* mr = useFx ? fxBuf.getWritePointer (1) : mix.getWritePointer (1);

        for (juce::int64 p = 0; p < total; ++p)
        {
            const juce::int64 q = loopMode ? p % loopLen : p;
            auto idx = q - nd;
            if (idx < 0 || idx >= len) continue;
            ml[p] += dl[idx] * gl;
            mr[p] += dr[idx] * gr;
        }
        if (useFx)
        {
            TrackFx fx;
            fx.prepare (sr);
            fx.process (fxBuf.getWritePointer (0), fxBuf.getWritePointer (1), (int) total, tr.echoOn.load(), tr.echoDiv.load(), tr.reverbOn.load(), e.getSamplesPerBeat());
            for (int c = 0; c < 2; ++c) mix.addFrom (c, 0, fxBuf, c, 0, (int) total);
        }
    }
    // drum machine: rendered on the same absolute clock (loop passes continue the pattern phase)
    if (! e.drums.mute.load() && (! anySolo || e.drums.solo.load()) && e.drums.hasAnySteps())
    {
        DrumMachine dm;
        dm.copyFrom (e.drums);
        dm.prepare (sr);
        for (juce::int64 p = 0; p < total; p += 4096)
        {
            const int n = (int) juce::jmin<juce::int64> (4096, total - p);
            dm.process (p, n, e.getSamplesPerBeat() / 4.0, dm.volume.load(), mix.getWritePointer (0) + p, mix.getWritePointer (1) + p, true);
        }
    }
    mix.applyGain (e.masterVolume.load());
    return true;
}

bool bounceWav (AudioEngine& e, const juce::File& file, int loops, juce::String& error)
{
    juce::AudioBuffer<float> mix;
    if (! renderMix (e, loops, mix, error)) return false;
    const double sr = e.getSampleRate();

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

bool bounceMp3 (AudioEngine& e, const juce::File& file, int loops, int kbps, juce::String& error)
{
#if RC_MP3
    juce::AudioBuffer<float> mix;
    if (! renderMix (e, loops, mix, error)) return false;

    for (int c = 0; c < 2; ++c)   // LAME expects +-1.0 floats; avoid overs
        juce::FloatVectorOperations::clip (mix.getWritePointer (c), mix.getReadPointer (c), -1.0f, 1.0f, mix.getNumSamples());

    lame_t gf = lame_init();
    if (gf == nullptr) { error = "LAME init failed"; return false; }
    lame_set_in_samplerate (gf, (int) std::lround (e.getSampleRate()));
    lame_set_num_channels (gf, 2);
    lame_set_mode (gf, JOINT_STEREO);
    lame_set_brate (gf, kbps);
    lame_set_quality (gf, 2);
    if (lame_init_params (gf) < 0) { lame_close (gf); error = "LAME could not be configured"; return false; }

    file.deleteFile();
    std::unique_ptr<juce::FileOutputStream> os (file.createOutputStream());
    if (os == nullptr) { lame_close (gf); error = "Could not write " + file.getFullPathName(); return false; }

    constexpr int block = 8192;
    std::vector<unsigned char> out ((size_t) (block * 5 / 4 + 7200));
    bool ok = true;
    for (int pos = 0; pos < mix.getNumSamples() && ok; pos += block)
    {
        const int n = juce::jmin (block, mix.getNumSamples() - pos);
        const int bytes = lame_encode_buffer_ieee_float (gf, mix.getReadPointer (0) + pos, mix.getReadPointer (1) + pos,
                                                         n, out.data(), (int) out.size());
        if (bytes < 0) { error = "LAME encode error " + juce::String (bytes); ok = false; }
        else if (bytes > 0) ok = os->write (out.data(), (size_t) bytes);
    }
    if (ok)
    {
        const int bytes = lame_encode_flush (gf, out.data(), (int) out.size());
        if (bytes > 0) ok = os->write (out.data(), (size_t) bytes);
    }
    lame_close (gf);
    os->flush();
    if (! ok && error.isEmpty()) error = "Write failed";
    return ok;
#else
    juce::ignoreUnused (e, file, loops, kbps);
    error = "This build was made without LAME (MP3 support).";
    return false;
#endif
}
}