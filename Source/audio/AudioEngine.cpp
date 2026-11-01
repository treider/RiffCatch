#include "AudioEngine.h"

void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    sampleRate = device->getCurrentSampleRate();
    numInputs = device->getActiveInputChannels().countNumberOfSetBits();
    deviceLatency = device->getInputLatencyInSamples() + device->getOutputLatencyInSamples();
    prepareFx();
    allocCatch();
    playPos = 0; countPos = 0;
    state = Stopped;
    masterCur = masterVolume.load();
    clickLeft = 0;
}

void AudioEngine::audioDeviceStopped() {}

void AudioEngine::allocCatch()
{
    const int size = (int) (catchSeconds * sampleRate.load());
    if (size == catchSize) return;
    catchSize = 0;                       // the audio thread is not running while this is called
    catchL.allocate ((size_t) size, true);
    catchR.allocate ((size_t) size, true);
    catchWrite = 0;
    catchSize = size;
}

void AudioEngine::updateCaptureChannels()
{
    int slot = -1;
    for (int s : order) if (tracks[s].armed.load()) { slot = s; break; }
    if (slot < 0 && ! order.empty()) slot = order.front();
    const int code = slot >= 0 ? tracks[slot].inputCode.load() : 0;
    catchChanL = code >= 100 ? code - 100 : code;
    catchChanR = code >= 100 ? code - 100 + 1 : -1;
}

bool AudioEngine::readCatch (juce::AudioBuffer<float>& out, double seconds) const
{
    const auto w = catchWrite.load (std::memory_order_acquire);
    const juce::int64 want = (juce::int64) (seconds * sampleRate.load());
    const juce::int64 n = juce::jmin (want, w, (juce::int64) catchSize - (juce::int64) (5.0 * sampleRate.load()));   // never read the part being overwritten
    if (n <= 0) return false;
    out.setSize (2, (int) n);
    for (juce::int64 i = 0; i < n; ++i)
    {
        const int idx = (int) ((w - n + i) % catchSize);
        out.setSample (0, (int) i, catchL[idx]);
        out.setSample (1, (int) i, catchR[idx]);
    }
    return true;
}

bool AudioEngine::recordAtNextLoop()
{
    if (state.load() != Playing || ! loopEnabled.load()) return false;
    bool any = false;
    for (int i = 0; i < maxTracks; ++i) any |= tracks[i].active.load() && tracks[i].armed.load();
    if (! any) return false;
    recStart = 0;
    sessionLen = (int) getLoopLength();
    armTracksForRecording (true);
    pendingRecord = true;                // the audio thread switches to Recording when the loop wraps
    return true;
}

// ------------------------------------------------------------------ transport

void AudioEngine::play()
{
    if (state.load() == Stopped) { jumpTo = cursorPos.load(); state = Playing; }
    else if (state.load() == Recording) state = Playing;
}

void AudioEngine::stop() { pendingRecord = false; capAfterCount = false; state = Stopped; drumReset = true; }   // audio thread returns the playhead to the cursor

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
    const auto bar = juce::jmax<juce::int64> (1, getBarLength());
    juce::int64 rs = (cursorPos.load() / bar) * bar;       // recording starts at the bar the playhead is in
    if (punchActive()) rs = (punchStart.load() / bar) * bar;   // punch: from the bar where the punch-in point is
    if (loopMode && rs >= loopLen) rs = 0;
    recStart = rs;
    sessionLen = loopMode ? (int) (loopLen - rs) : 0;      // loop passes record from rs to the loop end

    armTracksForRecording (loopMode);

    // count-in: whole bars
    const double spb = getSamplesPerBeat();
    const int beats = juce::jlimit (0, 8, countInBars.load());
    countInLen = (juce::int64) std::ceil (beats * getBarBeats() * spb);

    jumpTo = rs;                      // recording starts at the chosen bar (after the count-in)
    state = beats > 0 ? (int) CountIn : (int) Recording;
    return true;
}

void AudioEngine::armTracksForRecording (bool loopMode)
{
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
}

// Message thread: pick up finished loop passes (and a partial first pass if stopped early).
void AudioEngine::pollRecording()
{
    const auto st = getState();
    const bool running = (st == Recording || st == CountIn || pendingRecord.load());   // a queued overdub must keep its session alive

    for (int i = 0; i < maxTracks; ++i)
    {
        auto& tr = tracks[i];
        if (! tr.active.load() || ! tr.recSession) continue;

        const auto seq = tr.stageSeq.load();
        if (seq != tr.seenSeq)
        {
            tr.seenSeq = seq;
            const int rsI = (int) recStart.load();
            const auto pc = tr.commitTake (tr.stage[tr.completedStage.load()], sessionLen, getCompensationSamples(), rsI == 0, rsI,
                                    punchActive() ? punchStart.load() : -1, punchActive() ? punchEnd.load() : -1);
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
                tr.finishLinearRecording (getCompensationSamples(), getBarLength(), (int) recStart.load());
            else if (tr.sessionCommits == 0 && written > 0)   // nothing completed yet: keep what was played, rounded up to whole bars
            {
                auto& sb = tr.stage[tr.writeStage.load()];
                const auto bar = juce::jmax<juce::int64> (1, getBarLength());
                const int len = (int) juce::jmin<juce::int64> (sessionLen, ((written + bar - 1) / bar) * bar);
                if (written < len) sb.clear ((int) written, len - (int) written);
                const int rsI = (int) recStart.load();
                const auto pc = tr.commitTake (sb, len, getCompensationSamples(), rsI == 0 && len == sessionLen, rsI,
                                        punchActive() ? punchStart.load() : -1, punchActive() ? punchEnd.load() : -1);
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

    if (const int tc = tunerChannel.load(); tc >= 0 && tc < numIn && in[tc] != nullptr)
        pushTunerSamples (in[tc], numSamples);
    if (const int cl = catchChanL.load(); cl >= 0 && cl < numIn && in[cl] != nullptr)
    {
        const int cr = catchChanR.load();
        writeCatch (in[cl], (cr >= 0 && cr < numIn) ? in[cr] : nullptr, numSamples);
    }

    for (int done = 0; done < numSamples;)
        done += processChunk (in, numIn, out, numOut, done, juce::jmin (chunk, numSamples - done));
}

// Processes up to n samples; may process fewer when a boundary (count-in end, loop end) is reached.
bool AudioEngine::startBassCapture()
{
    const auto len = getLoopLength();
    if (! loopEnabled.load() || len <= 0 || len > (juce::int64) (sampleRate.load() * 600.0)) return false;
    capState = 0;
    capBuf.setSize (2, (int) len);
    capBuf.clear();
    capLen = len;
    capState = 1;
    if (state.load() == Stopped)    // from standstill: count-in (as for recording), then the loop plays from bar 1
    {
        const int beats = juce::jlimit (0, 8, countInBars.load());
        if (beats > 0)
        {
            countInLen = (juce::int64) std::ceil (beats * getBarBeats() * getSamplesPerBeat());
            recStart = 0;
            countPos = 0;
            capAfterCount = true;
            state = (int) CountIn;
        }
        else { jumpTo = 0; state = (int) Playing; }
    }
    return true;
}

int AudioEngine::processChunk (const float* const* in, int numIn, float* const* out, int numOut, int offset, int n)
{
    if (drumReset.exchange (false)) { drums.reset(); bass.reset(); }
    if (const auto j = jumpTo.exchange (-1); j >= 0) { playPos = j; countPos = 0; drumPass = 0; drums.reset(); bass.reset(); }

    auto st = (State) state.load();
    const double sr = sampleRate.load();
    const double spb = getSamplesPerBeat();
    const auto loopLen = getLoopLength();

    if (st == Stopped) { playPos = cursorPos.load(); countPos = 0; drumPass = 0; }

    if (st == CountIn && countPos.load() >= countInLen.load())   // count-in finished -> record at bar 1
    {
        int expected = CountIn;
        state.compare_exchange_strong (expected, capAfterCount.exchange (false) ? (int) Playing : (int) Recording);
        playPos = recStart.load();
        st = (State) state.load();
    }

    if (songActive.load() && st == Playing)     // song mode: play the rendered arrangement
    {
        const auto len = songLen.load();
        const auto sp = playPos.load();
        if (sp >= len) { state = (int) Stopped; return n; }
        n = (int) juce::jmax<juce::int64> (1, juce::jmin<juce::int64> (n, len - sp));
        juce::FloatVectorOperations::clear (mixL, n);
        juce::FloatVectorOperations::clear (mixR, n);
        {
            juce::SpinLock::ScopedTryLockType lk (songLock);
            if (lk.isLocked() && songBuf != nullptr && songBuf->getNumSamples() >= sp + n)
            {
                juce::FloatVectorOperations::copy (mixL, songBuf->getReadPointer (0) + sp, n);
                juce::FloatVectorOperations::copy (mixR, songBuf->getReadPointer (1) + sp, n);
            }
        }
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
            for (int i = 0; i < n; ++i) out[0][offset + i] += 0.5f * (mixL[i] + mixR[i]);
        playPos = sp + n;
        if (sp + n >= len) state = (int) Stopped;
        return n;
    }
    auto pos = playPos.load();
    const bool playing = (st == Playing || st == Recording);
    const bool looping = playing && loopEnabled.load();

    if (looping)
    {
        if (pos >= loopLen) pos %= loopLen;
        n = (int) juce::jmin<juce::int64> (n, loopLen - pos);
    }
    const juce::int64 rs = recStart.load();
    if (st == Recording && looping && pos < rs)       // recording window starts at rs: do not straddle it
        n = (int) juce::jmin<juce::int64> (n, rs - pos);
    if (st == CountIn)
        n = (int) juce::jmin<juce::int64> (n, countInLen.load() - countPos.load());
    n = juce::jmax (1, n);

    juce::FloatVectorOperations::clear (mixL, n);
    juce::FloatVectorOperations::clear (mixR, n);

    bool anySolo = false;
    for (int t = 0; t < maxTracks; ++t) anySolo |= tracks[t].active.load() && tracks[t].solo.load();
    anySolo |= drums.solo.load() || bass.solo.load();

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
            const juce::int64 start = pos - (juce::int64) tr.offset.load();   // data index of the first sample of this chunk
            if (len > 0 && tr.data.getNumSamples() >= len && start < len)
            {
                if (start >= 0)
                {
                    pbL = tr.data.getReadPointer (0) + start;
                    pbR = tr.data.getReadPointer (1) + start;
                    pbAvail = (int) juce::jmin<juce::int64> (n, len - start);
                }
                else if (-start < n)   // the clip starts inside this chunk: silence first, then the audio
                {
                    const int lead = (int) -start;
                    const int cnt = (int) juce::jmin<juce::int64> (n - lead, len);
                    juce::FloatVectorOperations::clear (nudgeL, n);
                    juce::FloatVectorOperations::clear (nudgeR, n);
                    juce::FloatVectorOperations::copy (nudgeL + lead, tr.data.getReadPointer (0), cnt);
                    juce::FloatVectorOperations::copy (nudgeR + lead, tr.data.getReadPointer (1), cnt);
                    pbL = nudgeL; pbR = nudgeR; pbAvail = n;
                }
            }
        }
        float pl = 0.0f, pr = 0.0f;
        const bool fxOn = tr.echoOn.load() || tr.reverbOn.load();
        const bool fxRun = fxOn || tr.fx.wasActive();
        if (fxRun) { juce::FloatVectorOperations::clear (fxL, n); juce::FloatVectorOperations::clear (fxR, n); }
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
                if (fxRun) { fxL[i] = yl; fxR[i] = yr; } else { mixL[i] += yl; mixR[i] += yr; }
                pl = juce::jmax (pl, std::abs (yl)); pr = juce::jmax (pr, std::abs (yr));
            }
        }
        if (fxRun)   // insert effects: echo (tempo synced) and reverb; tails keep ringing while the track is silent
        {
            tr.fx.process (fxL, fxR, n, tr.echoOn.load(), tr.echoDiv.load(), tr.reverbOn.load(), spb);
            juce::FloatVectorOperations::add (mixL, fxL, n);
            juce::FloatVectorOperations::add (mixR, fxR, n);
        }
        tr.curGainL = tgtL; tr.curGainR = tgtR;
        if (pl > tr.mixPeakL.load()) tr.mixPeakL = pl;
        if (pr > tr.mixPeakR.load()) tr.mixPeakR = pr;
        if (pl > tr.peakL.load()) tr.peakL = pl;
        if (pr > tr.peakR.load()) tr.peakR = pr;

        if (toStage)   // loop pass goes to staging buffer; the take is only replaced when the pass completes
        {
            auto& sb = tr.stage[tr.writeStage.load()];
            const juce::int64 si = pos - rs;       // index inside the pass window
            if (si >= 0 && sb.getNumSamples() >= si + n)
            {
                auto* dl = sb.getWritePointer (0) + si;
                auto* dr = sb.getWritePointer (1) + si;
                for (int i = 0; i < n; ++i)
                {
                    const float a = inL != nullptr ? inL[i] : 0.0f;
                    dl[i] = a;
                    dr[i] = inR != nullptr ? inR[i] : a;
                }
                tr.stagePos = si + n;
            }
        }
        else if (recording && haveData)   // linear recording straight into the track buffer
        {
            const auto cap = (juce::int64) tr.data.getNumSamples();
            const juce::int64 wi = pos - rs;       // write index in the clip
            const int w = wi >= 0 ? (int) juce::jlimit<juce::int64> (0, n, cap - wi) : 0;
            if (w > 0)
            {
                auto* dl = tr.data.getWritePointer (0) + wi;
                auto* dr = tr.data.getWritePointer (1) + wi;
                for (int i = 0; i < w; ++i)
                {
                    const float a = inL != nullptr ? inL[i] : 0.0f;
                    dl[i] = a;
                    dr[i] = inR != nullptr ? inR[i] : a;
                }
                if (wi + w > tr.length.load()) tr.length = wi + w;
            }
            if (w < n) recordFull = true;
        }
    }

    // ---- drum machine: runs on the transport clock; keeps its phase across loop passes
    {
        if (drumSr != sr) { drums.prepare (sr); bass.prepare (sr); drumSr = sr; }
        const bool dAudible = ! drums.mute.load() && (! anySolo || drums.solo.load());
        drums.process (looping ? drumPass * loopLen + pos : pos, n, spb / 4.0, dAudible ? drums.volume.load() : 0.0f, mixL, mixR, playing);
        const bool bAudible = ! bass.mute.load() && (! anySolo || bass.solo.load());
        const auto bAbs = looping ? drumPass * loopLen + pos : pos;
        const float bGain = bAudible ? bass.volume.load() : 0.0f;
        int cs = capState.load();
        if ((cs == 1 || cs == 2) && ! looping && st != CountIn) { capState = 0; cs = 0; capAfterCount = false; }           // transport stopped or loop switched off: cancel
        if (cs == 1 && pos == 0) { capState = 2; cs = 2; }
        if (cs == 2 && capBuf.getNumSamples() >= capLen && pos + n <= capLen)
        {
            juce::FloatVectorOperations::clear (fxL, n);
            juce::FloatVectorOperations::clear (fxR, n);
            bass.render (bAbs, n, spb / 4.0, bGain, fxL, fxR, playing, spb);
            juce::FloatVectorOperations::copy (capBuf.getWritePointer (0) + pos, fxL, n);
            juce::FloatVectorOperations::copy (capBuf.getWritePointer (1) + pos, fxR, n);
            juce::FloatVectorOperations::add (mixL, fxL, n);
            juce::FloatVectorOperations::add (mixR, fxR, n);
            if (pos + n >= capLen) capState = 3;
        }
        else
        {
            if (cs == 2) capState = 0;
            bass.render (bAbs, n, spb / 4.0, bGain, mixL, mixR, playing, spb);
        }
    }

    // ---- click (sample-accurate: beats derived from the transport sample counter)
    const bool clickOn = st == CountIn || (playing && clickEnabled.load());
    int trig[4]; float trigFreq[4]; int nTrig = 0;
    if (clickOn)
    {
        const juce::int64 t0 = st == CountIn ? countPos.load() : pos;
        const double pulse = getPulseSamples();
        const int perBar = getPulsesPerBar();
        auto beatSample = [pulse] (juce::int64 k) { return (juce::int64) std::ceil ((double) k * pulse); };
        juce::int64 k = (juce::int64) std::floor ((double) t0 / pulse);
        while (k > 0 && beatSample (k - 1) >= t0) --k;
        while (beatSample (k) < t0) ++k;
        for (; nTrig < 4; ++k)
        {
            const auto off = beatSample (k) - t0;
            if (off >= n) break;
            trig[nTrig] = (int) off;
            trigFreq[nTrig] = (k % perBar == 0) ? 1600.0f : 1000.0f;   // accent on the downbeat
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
            ++drumPass;
            if (st == Playing && pendingRecord.exchange (false))
                state.store ((int) Recording);       // armed tracks start recording exactly at the loop start
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
