#include "Exporter.h"
#include <juce_audio_formats/juce_audio_formats.h>
#if RC_MP3
 #include <lame.h>
#endif

namespace Exporter
{
// One pattern, rendered from the pattern bank (not from the live tracks): `repeats` passes of `loopLen` samples, plus `tail` samples
// of echo / reverb decay. `mix` must be (loopLen * repeats + tail) samples long and cleared.
static void mixPattern (AudioEngine& e, const PatternBank& bank, const PatternBank::Pattern& p, juce::int64 loopLen, int repeats, juce::AudioBuffer<float>& mix)
{
    const double sr = e.getSampleRate(), spb = e.getSamplesPerBeat();
    const juce::int64 total = loopLen * repeats;
    const int size = mix.getNumSamples();
    bool anySolo = p.drumMix.solo || p.bassMix.solo;
    for (auto& t : p.tracks) anySolo |= t.solo;

    for (auto& tr : p.tracks)
    {
        if (! tr.has() || tr.mute || (anySolo && ! tr.solo)) continue;
        const float theta = (juce::jlimit (-1.0f, 1.0f, tr.pan) + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        const float gl = tr.volume * std::cos (theta), gr = tr.volume * std::sin (theta);
        const juce::int64 len = tr.audio.getNumSamples();
        const float* dl = tr.audio.getReadPointer (0);
        const float* dr = tr.audio.getReadPointer (1);
        const bool useFx = tr.echoOn || tr.reverbOn;
        juce::AudioBuffer<float> fxBuf;
        if (useFx) { fxBuf.setSize (2, size); fxBuf.clear(); }
        float* ml = useFx ? fxBuf.getWritePointer (0) : mix.getWritePointer (0);
        float* mr = useFx ? fxBuf.getWritePointer (1) : mix.getWritePointer (1);
        for (juce::int64 q = 0; q < total; ++q)
        {
            const auto idx = (q % loopLen) - tr.offset;
            if (idx < 0 || idx >= len) continue;
            ml[q] += dl[idx] * gl;
            mr[q] += dr[idx] * gr;
        }
        if (useFx)
        {
            TrackFx fx;
            fx.prepare (sr);
            fx.process (fxBuf.getWritePointer (0), fxBuf.getWritePointer (1), size, tr.echoOn, tr.echoDiv, tr.reverbOn, spb);
            for (int c = 0; c < 2; ++c) mix.addFrom (c, 0, fxBuf, c, 0, size);
        }
    }
    {   // drums
        DrumMachine dm;
        dm.copyFrom (e.drums);
        dm.apply (bank.drumLib[(size_t) juce::jlimit (0, PatternBank::maxItems - 1, p.drumRef)]);
        const auto& d = p.drumMix;
        dm.volume = d.volume;
        for (int v = 0; v < DrumMachine::numVoices; ++v) { dm.level[v] = d.level[v]; dm.pan[v] = d.pan[v]; dm.voiceMute[v] = d.voiceMute[v]; }
        if (! d.mute && (! anySolo || d.solo) && dm.hasAnySteps())
        {
            dm.prepare (sr);
            for (juce::int64 q = 0; q < total; q += 4096)
            {
                const int n = (int) juce::jmin<juce::int64> (4096, total - q);
                dm.process (q, n, spb / 4.0, dm.volume.load(), mix.getWritePointer (0) + q, mix.getWritePointer (1) + q, true);
            }
        }
    }
    {   // bass
        BassMachine bm;
        bm.copyFrom (e.bass);
        bm.apply (bank.bassLib[(size_t) juce::jlimit (0, PatternBank::maxItems - 1, p.bassRef)]);
        const auto& b = p.bassMix;
        bm.volume = b.volume; bm.echoOn = b.echoOn; bm.reverbOn = b.reverbOn; bm.echoDiv = b.echoDiv;
        if (! b.mute && (! anySolo || b.solo) && bm.hasAnySteps())
        {
            bm.prepare (sr);
            bm.fx.prepare (sr);
            for (juce::int64 q = 0; q < total; q += 4096)
            {
                const int n = (int) juce::jmin<juce::int64> (4096, total - q);
                bm.render (q, n, spb / 4.0, bm.volume.load(), mix.getWritePointer (0) + q, mix.getWritePointer (1) + q, true, spb);
            }
        }
    }
}

bool renderSongBuffer (AudioEngine& e, juce::AudioBuffer<float>& mix, juce::String& error, bool applyMaster)
{
    auto& bank = e.patterns;
    bank.storeCurrent (e);                        // the open pattern is as it is on screen
    const double sr = e.getSampleRate();
    const juce::int64 barLen = e.getBarLength();
    struct Seg { const PatternBank::Pattern* p; juce::int64 loopLen; int repeats; juce::int64 start; };
    std::vector<Seg> segs;
    juce::int64 total = 0;
    for (auto& c : bank.song)
    {
        if (! bank.isUsed (c.pattern)) continue;
        const auto* p = &bank.pats[(size_t) c.pattern];
        const juce::int64 loopLen = juce::jmax<juce::int64> (1, (juce::int64) p->loopBars * barLen);
        const int reps = juce::jlimit (1, 64, c.repeats);
        segs.push_back ({ p, loopLen, reps, total });
        total += loopLen * reps;
    }
    if (segs.empty()) { error = "The song is empty - add patterns to it."; return false; }
    if (total > (juce::int64) sr * 3600) { error = "The song is too long."; return false; }
    const juce::int64 tail = (juce::int64) (sr * 1.5);
    mix.setSize (2, (int) (total + tail));
    mix.clear();
    for (auto& s : segs)
    {
        juce::AudioBuffer<float> seg (2, (int) (s.loopLen * s.repeats + tail));
        seg.clear();
        mixPattern (e, bank, *s.p, s.loopLen, s.repeats, seg);
        for (int c = 0; c < 2; ++c) mix.addFrom (c, (int) s.start, seg, c, 0, seg.getNumSamples());
    }
    if (applyMaster) mix.applyGain (e.masterVolume.load());
    return true;
}

bool renderPatternBuffer (AudioEngine& e, int pattern, int repeats, juce::AudioBuffer<float>& mix, juce::String& error, bool applyMaster)
{
    auto& bank = e.patterns;
    bank.storeCurrent (e);                        // the open pattern is as it is on screen
    if (pattern < 0 || pattern >= PatternBank::maxPatterns || ! bank.isUsed (pattern))
    {
        error = "That pattern is empty.";
        return false;
    }
    const double sr = e.getSampleRate();
    const juce::int64 barLen = e.getBarLength();
    const auto& p = bank.pats[(size_t) pattern];
    const juce::int64 loopLen = juce::jmax<juce::int64> (1, (juce::int64) p.loopBars * barLen);
    const int reps = juce::jlimit (1, 64, repeats);
    const juce::int64 total = loopLen * reps;
    if (total > (juce::int64) sr * 3600) { error = "Export too long."; return false; }
    const juce::int64 tail = (juce::int64) (sr * 1.5);
    mix.setSize (2, (int) (total + tail));
    mix.clear();
    mixPattern (e, bank, p, loopLen, reps, mix);
    if (applyMaster) mix.applyGain (e.masterVolume.load());
    return true;
}

static bool renderMix (AudioEngine& e, int loops, juce::AudioBuffer<float>& mix, juce::String& error)
{
    if (e.patterns.songMode && ! e.patterns.song.empty()) return renderSongBuffer (e, mix, error, true);     // song mode: export the whole song
    const double sr = e.getSampleRate();
    const bool loopMode = e.loopEnabled.load();
    const juce::int64 loopLen = e.getLoopLength();

    juce::int64 passLen = loopMode ? loopLen : 0;
    if (! loopMode)
        for (int slot : e.order) passLen = juce::jmax<juce::int64> (passLen, e.tracks[slot].length.load() + juce::jmax (0, e.tracks[slot].offset.load()));
    if (! loopMode && e.drums.hasAnySteps())
        passLen = juce::jmax<juce::int64> (passLen, (juce::int64) std::ceil (e.drums.patternSteps() * e.getSamplesPerBeat() / 4.0));
    if (! loopMode && e.bass.hasAnySteps())
        passLen = juce::jmax<juce::int64> (passLen, (juce::int64) std::ceil (e.bass.patternSteps() * e.getSamplesPerBeat() / 4.0));
    if (passLen <= 0) { error = "Nothing to export - no recorded audio."; return false; }

    const juce::int64 total = passLen * juce::jmax (1, loopMode ? loops : 1);
    if (total > (juce::int64) sr * 3600) { error = "Export too long."; return false; }

    mix.setSize (2, (int) total);
    mix.clear();

    bool anySolo = e.drums.solo.load() || e.bass.solo.load();
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
    if (! e.bass.mute.load() && (! anySolo || e.bass.solo.load()) && e.bass.hasAnySteps())
    {
        BassMachine bm;
        bm.copyFrom (e.bass);
        bm.prepare (sr);
        bm.fx.prepare (sr);
        for (juce::int64 p = 0; p < total; p += 4096)
        {
            const int n = (int) juce::jmin<juce::int64> (4096, total - p);
            bm.render (p, n, e.getSamplesPerBeat() / 4.0, bm.volume.load(), mix.getWritePointer (0) + p, mix.getWritePointer (1) + p, true, e.getSamplesPerBeat());
        }
    }
    mix.applyGain (e.masterVolume.load());
    return true;
}

static bool render (AudioEngine& e, Source source, int pattern, int loops, juce::AudioBuffer<float>& mix, juce::String& error)
{
    if (source == Source::Song) return renderSongBuffer (e, mix, error, true);
    if (source == Source::Pattern) return renderPatternBuffer (e, pattern, loops, mix, error, true);
    return renderMix (e, loops, mix, error);
}

static bool writeWav (const juce::File& file, const juce::AudioBuffer<float>& mix, double sr, juce::String& error)
{
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

static bool writeMp3 (const juce::File& file, juce::AudioBuffer<float>& mix, double sr, int kbps, juce::String& error)
{
#if RC_MP3
    for (int c = 0; c < 2; ++c)   // LAME expects +-1.0 floats; avoid overs
        juce::FloatVectorOperations::clip (mix.getWritePointer (c), mix.getReadPointer (c), -1.0f, 1.0f, mix.getNumSamples());

    lame_t gf = lame_init();
    if (gf == nullptr) { error = "LAME init failed"; return false; }
    lame_set_in_samplerate (gf, (int) std::lround (sr));
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
    juce::ignoreUnused (file, mix, sr, kbps);
    error = "This build was made without LAME (MP3 support).";
    return false;
#endif
}

bool bounceWav (AudioEngine& e, const juce::File& file, Source source, int pattern, int loops, juce::String& error)
{
    juce::AudioBuffer<float> mix;
    if (! render (e, source, pattern, loops, mix, error)) return false;
    return writeWav (file, mix, e.getSampleRate(), error);
}

bool bounceMp3 (AudioEngine& e, const juce::File& file, Source source, int pattern, int loops, int kbps, juce::String& error)
{
    juce::AudioBuffer<float> mix;
    if (! render (e, source, pattern, loops, mix, error)) return false;
    return writeMp3 (file, mix, e.getSampleRate(), kbps, error);
}
}