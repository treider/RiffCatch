#include "AudioEngine.h"

void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    sampleRate = device->getCurrentSampleRate();
    numInputs = device->getActiveInputChannels().countNumberOfSetBits();
    deviceLatency = device->getInputLatencyInSamples() + device->getOutputLatencyInSamples();
    playPos = 0; countPos = 0;
    state = Stopped;
    masterCur = masterVolume.load();
    clickLeft = 0;
}

void AudioEngine::audioDeviceStopped() {}

// ------------------------------------------------------------------ transport

void AudioEngine::play()
{
    if (state.load() == Stopped) { jumpTo = cursorPos.load(); state = Playing; }
    else if (state.load() == Recording) state = Playing;
}

void AudioEngine::stop() { state = Stopped; }   // audio thread returns the playhead to the cursor

void AudioEngine::seek (juce::int64 pos)
{
    pos = juce::jmax<juce::int64> (0, pos);
    const auto st = state.load();
    if (st == Recording || st == CountIn) return;   // loop recording must stay locked to bar 1
    cursorPos = pos;
    if (st == Playing) jumpTo = pos;
}

bool AudioEngine::record()
{
    bool any = false;
    for (int i = 0; i < maxTracks; ++i)
        any |= tracks[i].active.load() && tracks[i].armed.load();
    if (! any) return false;

    const bool loopMode = loopEnabled.load();
    const auto loopLen = getLoopLength();
    sessionLen = (int) loopLen;

    for (int i = 0; i < maxTracks; ++i)
    {
        auto& tr = tracks[i];
        if (! tr.active.load()) continue;
        if (! tr.armed.load()) { tr.recActive = false; tr.recSession = false; continue; }

        if (loopMode) { tr.prepareStage (sessionLen); tr.recToStage = true; }
        else          { tr.prepareRecording (0, sampleRate.load()); tr.recToStage = false; }
        tr.seenSeq = tr.stageSeq.load();
        tr.sessionCommits = 0; tr.idleTicks = 0;
        tr.recSession = true;
        tr.stagePos = 0;
        tr.recActive = true;
    }

    // count-in: whole bars of 4 beats
    const double spb = getSamplesPerBeat();
    const int beats = juce::jlimit (0, 8, countInBars.load()) * 4;
    countInLen = (juce::int64) std::ceil (beats * spb);

    jumpTo = 0;                       // recording always starts at bar 1
    state = beats > 0 ? (int) CountIn : (int) Recording;
    return true;
}

// Message thread: pick up finished loop passes (and a partial first pass if stopped early).
void AudioEngine::pollRecording()
{
    const auto st = getState();
    const bool running = (st == Recording || st == CountIn);

    for (int i = 0; i < maxTracks; ++i)
    {
        auto& tr = tracks[i];
        if (! tr.active.load() || ! tr.recSession) continue;

        const auto seq = tr.stageSeq.load();
        if (seq != tr.seenSeq)
        {
            tr.seenSeq = seq;
            const auto pc = tr.commitTake (tr.stage[tr.completedStage.load()], sessionLen, getCompensationSamples());
            if (onTakeCommitted) onTakeCommitted (i, pc.first, pc.second);
            ++tr.sessionCommits;
        }

        if (running) { tr.idleTicks = 0; continue; }

        if (++tr.idleTicks >= 2)   // audio thread has certainly left the stage buffer
        {
            tr.recSession = false;
            tr.recActive = false;
            tr.idleTicks = 0;
            const auto written = tr.stagePos.load();
            if (! tr.recToStage.load())
                tr.shiftEarlier (getCompensationSamples());
            else if (tr.sessionCommits == 0 && written > 0)   // nothing completed yet: keep what was played
            {
                auto& sb = tr.stage[tr.writeStage.load()];
                if (written < sessionLen) sb.clear ((int) written, sessionLen - (int) written);
                const auto pc = tr.commitTake (sb, sessionLen, getCompensationSamples());
                if (onTakeCommitted) onTakeCommitted (i, pc.first, pc.second);
            }
        }
    }
}

// ------------------------------------------------------------------ track slots

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

int AudioEngine::addTrackAt (int slot)
{
    if (slot >= 0 && slot < maxTracks && ! tracks[slot].active.load())
    {
        tracks[slot].resetToDefaults (slot);
        tracks[slot].active = true;
        order.push_back (slot);
        return slot;
    }
    return addTrack();
}

void AudioEngine::removeTrack (int slot)
{
    tracks[slot].active = false;
    tracks[slot].armed = false;
    tracks[slot].recActive = false;
    tracks[slot].recSession = false;
    tracks[slot].clearAudio();
    order.erase (std::remove (order.begin(), order.end(), slot), order.end());
}

void AudioEngine::removeAllTracks()
{
    stop();
    for (int i = 0; i < maxTracks; ++i) removeTrack (i);
    order.clear();
}

// ------------------------------------------------------------------ audio thread

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

    for (int done = 0; done < numSamples;)
        done += processChunk (in, numIn, out, numOut, done, juce::jmin (chunk, numSamples - done));
}

// Processes up to n samples; may process fewer when a boundary (count-in end, loop end) is reached.
int AudioEngine::processChunk (const float* const* in, int numIn, float* const* out, int numOut, int offset, int n)
{
    if (const auto j = jumpTo.exchange (-1); j >= 0) { playPos = j; countPos = 0; }

    auto st = (State) state.load();
    const double sr = sampleRate.load();
    const double spb = getSamplesPerBeat();
    const auto loopLen = getLoopLength();

    if (st == Stopped) { playPos = cursorPos.load(); countPos = 0; }

    if (st == CountIn && countPos.load() >= countInLen.load())   // count-in finished -> record at bar 1
    {
        int expected = CountIn;
        state.compare_exchange_strong (expected, (int) Recording);
        playPos = 0;
        st = (State) state.load();
    }

    auto pos = playPos.load();
    const bool playing = (st == Playing || st == Recording);
    const bool looping = playing && loopEnabled.load();

    if (looping)
    {
        if (pos >= loopLen) pos %= loopLen;
        n = (int) juce::jmin<juce::int64> (n, loopLen - pos);
    }
    if (st == CountIn)
        n = (int) juce::jmin<juce::int64> (n, countInLen.load() - countPos.load());
    n = juce::jmax (1, n);

    juce::FloatVectorOperations::clear (mixL, n);
    juce::FloatVectorOperations::clear (mixR, n);

    bool anySolo = false;
    for (int t = 0; t < maxTracks; ++t) anySolo |= tracks[t].active.load() && tracks[t].solo.load();

    bool recordFull = false;

    for (int t = 0; t < maxTracks; ++t)
    {
        auto& tr = tracks[t];
        if (! tr.active.load()) continue;

        const bool armed = tr.armed.load();
        const bool recording = tr.recActive.load() && st == Recording;
        const bool toStage = recording && tr.recToStage.load();
        const bool needData = (playing && ! recording) || (recording && ! toStage);

        juce::SpinLock::ScopedTryLockType lk (tr.lock, false);
        const bool haveData = needData && lk.retryLock();

        const bool audible = ! tr.mute.load() && (! anySolo || tr.solo.load());
        const float vol = audible ? tr.volume.load() : 0.0f;
        const float theta = (juce::jlimit (-1.0f, 1.0f, tr.pan.load()) + 1.0f) * juce::MathConstants<float>::pi * 0.25f;   // equal-power
        const float tgtL = vol * std::cos (theta), tgtR = vol * std::sin (theta);

        const float* inL = nullptr;   // live input (monitoring + recording)
        const float* inR = nullptr;
        const float* pbL = nullptr;   // playback of recorded audio
        const float* pbR = nullptr;
        int pbAvail = 0;

        if (armed || recording)
        {
            const int code = tr.inputCode.load();
            const int l = code >= 100 ? code - 100 : code;
            const int r = code >= 100 ? l + 1 : l;
            if (l < numIn && in[l] != nullptr) inL = in[l] + offset;
            if (r < numIn && in[r] != nullptr) inR = in[r] + offset;
        }
        if (playing && ! recording && haveData)
        {
            const auto len = tr.length.load();
            const int nd = tr.nudge.load();
            if (len > 0 && tr.data.getNumSamples() >= len)
            {
                if (nd == 0)
                {
                    if (pos < len)
                    {
                        pbL = tr.data.getReadPointer (0) + pos;
                        pbR = tr.data.getReadPointer (1) + pos;
                        pbAvail = (int) juce::jmin<juce::int64> (n, len - pos);
                    }
                }
                else   // manual nudge: shifted read, circular when the take is exactly one loop
                {
                    const bool wrap = loopEnabled.load() && std::abs (len - loopLen) <= 2;
                    const float* dl = tr.data.getReadPointer (0);
                    const float* dr = tr.data.getReadPointer (1);
                    for (int i = 0; i < n; ++i)
                    {
                        auto idx = pos + i - nd;
                        if (wrap) { idx %= len; if (idx < 0) idx += len; }
                        const bool ok = idx >= 0 && idx < len;
                        nudgeL[i] = ok ? dl[idx] : 0.0f;
                        nudgeR[i] = ok ? dr[idx] : 0.0f;
                    }
                    pbL = nudgeL; pbR = nudgeR; pbAvail = n;
                }
            }
        }

        float pl = 0.0f, pr = 0.0f;
        if (pbAvail > 0 || armed)
        {
            float gl = tr.curGainL, gr = tr.curGainR;
            const float sl = (tgtL - gl) / (float) n, sr2 = (tgtR - gr) / (float) n;
            for (int i = 0; i < n; ++i)
            {
                gl += sl; gr += sr2;
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
        }
        tr.curGainL = tgtL; tr.curGainR = tgtR;
        if (pl > tr.peakL.load()) tr.peakL = pl;
        if (pr > tr.peakR.load()) tr.peakR = pr;

        if (toStage)   // loop pass goes to staging buffer; the take is only replaced when the pass completes
        {
            auto& sb = tr.stage[tr.writeStage.load()];
            if (sb.getNumSamples() >= pos + n)
            {
                auto* dl = sb.getWritePointer (0) + pos;
                auto* dr = sb.getWritePointer (1) + pos;
                for (int i = 0; i < n; ++i)
                {
                    const float a = inL != nullptr ? inL[i] : 0.0f;
                    dl[i] = a;
                    dr[i] = inR != nullptr ? inR[i] : a;
                }
                tr.stagePos = pos + n;
            }
        }
        else if (recording && haveData)   // linear recording straight into the track buffer
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

    // ---- click (sample-accurate: beats derived from the transport sample counter)
    const bool clickOn = st == CountIn || (playing && clickEnabled.load());
    int trig[4]; float trigFreq[4]; int nTrig = 0;
    if (clickOn)
    {
        const juce::int64 t0 = st == CountIn ? countPos.load() : pos;
        auto beatSample = [spb] (juce::int64 k) { return (juce::int64) std::ceil ((double) k * spb); };
        juce::int64 k = (juce::int64) std::floor ((double) t0 / spb);
        while (k > 0 && beatSample (k - 1) >= t0) --k;
        while (beatSample (k) < t0) ++k;
        for (; nTrig < 4; ++k)
        {
            const auto off = beatSample (k) - t0;
            if (off >= n) break;
            trig[nTrig] = (int) off;
            trigFreq[nTrig] = (k % 4 == 0) ? 1600.0f : 1000.0f;   // accent on the downbeat
            ++nTrig;
        }
    }
    if (nTrig > 0 || clickLeft > 0)
    {
        const float cv = clickVolume.load();
        int ti = 0;
        for (int i = 0; i < n; ++i)
        {
            while (ti < nTrig && trig[ti] == i)
            {
                clickLen = clickLeft = (int) (sr * 0.045);
                clickPhase = 0.0f;
                clickInc = juce::MathConstants<float>::twoPi * trigFreq[ti] / (float) sr;
                ++ti;
            }
            if (clickLeft > 0)
            {
                const float env = (float) clickLeft / (float) clickLen;
                const float s = std::sin (clickPhase) * env * env * cv * 0.7f;
                clickPhase += clickInc;
                --clickLeft;
                mixL[i] += s; mixR[i] += s;
            }
        }
    }

    // ---- master
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

    // ---- advance clock
    if (st == CountIn)
        countPos = countPos.load() + n;
    else if (playing)
    {
        auto newPos = pos + n;
        if (looping && newPos >= loopLen)
        {
            newPos = 0;
            if (st == Recording)   // loop pass complete: hand the staging buffer to the UI thread
                for (int t = 0; t < maxTracks; ++t)
                {
                    auto& tr = tracks[t];
                    if (tr.active.load() && tr.recActive.load() && tr.recToStage.load())
                    {
                        tr.completedStage = tr.writeStage.load();
                        tr.writeStage = tr.writeStage.load() ^ 1;
                        tr.stagePos = 0;
                        tr.stageSeq.fetch_add (1);
                    }
                }
        }
        playPos = newPos;
        if (recordFull)
        {
            int expected = Recording;
            state.compare_exchange_strong (expected, (int) Playing);
        }
    }
    return n;
}
