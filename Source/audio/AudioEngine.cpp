#include "AudioEngine.h"

void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    sampleRate = device->getCurrentSampleRate();
    numInputs = device->getActiveInputChannels().countNumberOfSetBits();
    playPos = 0;
    state = Stopped;
    masterCur = masterVolume.load();
}

void AudioEngine::audioDeviceStopped() {}

void AudioEngine::play()
{
    if (state.load() == Stopped) state = Playing;
    else if (state.load() == Recording) state = Playing;
}

bool AudioEngine::record()
{
    const auto pos = state.load() == Stopped ? 0 : playPos.load();
    bool any = false;
    for (int i = 0; i < maxTracks; ++i)
        if (tracks[i].active.load() && tracks[i].armed.load())
        {
            tracks[i].prepareRecording (pos, sampleRate.load());
            any = true;
        }
    if (any) state = Recording;
    return any;
}

int AudioEngine::addTrack()
{
    for (int i = 0; i < maxTracks; ++i)
        if (! tracks[i].active.load())
        {
            tracks[i].resetToDefaults (i);
            tracks[i].active = true;
            order.push_back (i);
            return i;
        }
    return -1;
}

void AudioEngine::removeTrack (int slot)
{
    tracks[slot].active = false;
    tracks[slot].armed = false;
    tracks[slot].clearAudio();
    order.erase (std::remove (order.begin(), order.end(), slot), order.end());
}

void AudioEngine::removeAllTracks()
{
    stop();
    for (int i = 0; i < maxTracks; ++i) removeTrack (i);
    order.clear();
}

void AudioEngine::stop() { state = Stopped; }   // audio thread resets position

void AudioEngine::audioDeviceIOCallbackWithContext (const float* const* in, int numIn,
                                                    float* const* out, int numOut,
                                                    int numSamples, const juce::AudioIODeviceCallbackContext&)
{
    for (int c = 0; c < numOut; ++c)
        if (out[c] != nullptr)
            juce::FloatVectorOperations::clear (out[c], numSamples);

    for (int ch = 0; ch < juce::jmin (numIn, 16); ++ch)
    {
        float p = 0.0f;
        if (in[ch] != nullptr)
        {
            auto r = juce::FloatVectorOperations::findMinAndMax (in[ch], numSamples);
            p = juce::jmax (std::abs (r.getStart()), std::abs (r.getEnd()));
        }
        inputLevel[ch] = juce::jmax (p, inputLevel[ch].load() * 0.93f);
    }

    for (int done = 0; done < numSamples; done += chunk)
        processChunk (in, numIn, out, numOut, done, juce::jmin (chunk, numSamples - done));
}

void AudioEngine::processChunk (const float* const* in, int numIn, float* const* out, int numOut, int offset, int n)
{
    auto st = (State) state.load();
    if (st == Stopped) playPos = 0;
    const auto pos = playPos.load();

    juce::FloatVectorOperations::clear (mixL, n);
    juce::FloatVectorOperations::clear (mixR, n);

    bool anySolo = false;
    for (int t = 0; t < maxTracks; ++t) anySolo |= tracks[t].active.load() && tracks[t].solo.load();

    bool recordFull = false;

    for (int t = 0; t < maxTracks; ++t)
    {
        auto& tr = tracks[t];
        if (! tr.active.load()) continue;
        juce::SpinLock::ScopedTryLockType lk (tr.lock);
        if (! lk.isLocked()) continue;

        const bool armed = tr.armed.load();
        const bool recording = armed && st == Recording;
        const bool audible = ! tr.mute.load() && (! anySolo || tr.solo.load());
        const float vol = audible ? tr.volume.load() : 0.0f;
        const float theta = (juce::jlimit (-1.0f, 1.0f, tr.pan.load()) + 1.0f) * juce::MathConstants<float>::pi * 0.25f;   // equal-power
        const float tgtL = vol * std::cos (theta), tgtR = vol * std::sin (theta);

        const float* inL = nullptr;   // live input (monitoring + recording)
        const float* inR = nullptr;
        const float* pbL = nullptr;   // playback of recorded audio
        const float* pbR = nullptr;
        int pbAvail = 0;

        if (armed)   // input monitoring
        {
            const int code = tr.inputCode.load();
            const int l = code >= 100 ? code - 100 : code;
            const int r = code >= 100 ? l + 1 : l;
            if (l < numIn && in[l] != nullptr) inL = in[l] + offset;
            if (r < numIn && in[r] != nullptr) inR = in[r] + offset;
        }
        if (st != Stopped && ! recording)   // an armed track still plays its audio, except while it is being re-recorded
        {
            const auto len = tr.length.load();
            if (pos < len && tr.data.getNumSamples() >= len)
            {
                pbL = tr.data.getReadPointer (0) + pos;
                pbR = tr.data.getReadPointer (1) + pos;
                pbAvail = (int) juce::jmin<juce::int64> (n, len - pos);
            }
        }

        float pl = 0.0f, pr = 0.0f;
        if (pbAvail > 0 || armed)
        {
            float gl = tr.curGainL, gr = tr.curGainR;
            const float sl = (tgtL - gl) / (float) n, sr = (tgtR - gr) / (float) n;
            for (int i = 0; i < n; ++i)
            {
                gl += sl; gr += sr;
                float a = 0.0f, b = 0.0f;
                if (i < pbAvail) { a += pbL[i]; b += pbR[i]; }
                if (armed)
                {
                    const float x = inL != nullptr ? inL[i] : 0.0f;
                    a += x;
                    b += inR != nullptr ? inR[i] : x;
                }
                const float yl = a * gl, yr = b * gr;
                mixL[i] += yl; mixR[i] += yr;
                pl = juce::jmax (pl, std::abs (yl)); pr = juce::jmax (pr, std::abs (yr));
            }
        }        tr.curGainL = tgtL; tr.curGainR = tgtR;
        if (pl > tr.peakL.load()) tr.peakL = pl;
        if (pr > tr.peakR.load()) tr.peakR = pr;

        if (recording)
        {
            const auto cap = (juce::int64) tr.data.getNumSamples();
            const int w = (int) juce::jlimit<juce::int64> (0, n, cap - pos);
            if (w > 0)
            {
                auto* dl = tr.data.getWritePointer (0) + pos;
                auto* dr = tr.data.getWritePointer (1) + pos;
                for (int i = 0; i < w; ++i)
                {
                    const float a = inL != nullptr ? inL[i] : 0.0f;
                    dl[i] = a;
                    dr[i] = inR != nullptr ? inR[i] : a;
                }
                if (pos + w > tr.length.load()) tr.length = pos + w;
            }
            if (w < n) recordFull = true;
        }
    }

    // master
    const float mt = masterVolume.load();
    const float ms = (mt - masterCur) / (float) n;
    float g = masterCur, pl = 0.0f, pr = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        g += ms;
        mixL[i] *= g; mixR[i] *= g;
        pl = juce::jmax (pl, std::abs (mixL[i])); pr = juce::jmax (pr, std::abs (mixR[i]));
    }
    masterCur = mt;
    if (pl > masterPeakL.load()) masterPeakL = pl;
    if (pr > masterPeakR.load()) masterPeakR = pr;

    if (numOut >= 2)
    {
        if (out[0] != nullptr) juce::FloatVectorOperations::add (out[0] + offset, mixL, n);
        if (out[1] != nullptr) juce::FloatVectorOperations::add (out[1] + offset, mixR, n);
    }
    else if (numOut == 1 && out[0] != nullptr)
    {
        for (int i = 0; i < n; ++i) out[0][offset + i] += 0.5f * (mixL[i] + mixR[i]);
    }

    if (st != Stopped)
    {
        playPos = pos + n;
        if (recordFull)
        {
            int expected = Recording;
            state.compare_exchange_strong (expected, (int) Playing);
        }
    }
}
