#pragma once
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <vector>
#include "BassMachine.h"

// Classic signal analysis of a recorded riff (no machine learning): tempo, key / scale / mode, a chord root per bar and the
// rhythm of the riff on the 1/16 grid. Everything runs on the message thread on a copy of the audio.
namespace Analysis
{
    constexpr int maxStepsPerBar = BassMachine::maxStepsPerBar;

    // ---- FFT (radix-2) and a windowed magnitude spectrum
    struct Spectrum
    {
        int n = 0;
        std::vector<int> rev;
        std::vector<std::complex<double>> tw, buf;
        std::vector<double> win;

        void init (int size)
        {
            n = size;
            int lg = 0; while ((1 << lg) < n) ++lg;
            rev.assign ((size_t) n, 0);
            for (int i = 0; i < n; ++i) rev[(size_t) i] = (rev[(size_t) (i >> 1)] >> 1) | ((i & 1) << (lg - 1));
            tw.resize ((size_t) n / 2);
            for (int k = 0; k < n / 2; ++k) tw[(size_t) k] = std::polar (1.0, -2.0 * juce::MathConstants<double>::pi * k / n);
            win.resize ((size_t) n);
            for (int i = 0; i < n; ++i) win[(size_t) i] = 0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * i / n);
            buf.assign ((size_t) n, {});
        }
        // out has n/2 magnitudes; x may be shorter than n (zero padded)
        void mags (const float* x, int avail, std::vector<float>& out)
        {
            for (int i = 0; i < n; ++i) buf[(size_t) rev[(size_t) i]] = { i < avail ? (double) x[i] * win[(size_t) i] : 0.0, 0.0 };
            for (int len = 2; len <= n; len <<= 1)
            {
                const int half = len / 2, step = n / len;
                for (int i = 0; i < n; i += len)
                    for (int j = 0; j < half; ++j)
                    {
                        const auto u = buf[(size_t) (i + j)], v = buf[(size_t) (i + j + half)] * tw[(size_t) (j * step)];
                        buf[(size_t) (i + j)] = u + v;
                        buf[(size_t) (i + j + half)] = u - v;
                    }
            }
            out.resize ((size_t) n / 2);
            for (int k = 0; k < n / 2; ++k) out[(size_t) k] = (float) std::abs (buf[(size_t) k]);
        }
    };

    // ---- results
    struct Key { int rootPc = 0; int scale = 0; float score = 0.0f; };          // scale = BassMachine scale index
    struct Chord { int rootPc = -1; int quality = 0; float conf = 0.0f; };       // quality: 0 major, 1 minor, 2 power chord (no third)

    struct Result
    {
        bool valid = false;
        double tempo = 0.0;                      // detected BPM (quarter notes), 0 = unknown
        float tempoConf = 0.0f;
        std::vector<Key> keys;                   // best first
        int bar0 = 0, bars = 0, barSteps = 16;   // first bar with audio, bars analysed, steps per bar
        std::vector<std::array<float, 12>> barChroma, barBass;
        std::vector<std::array<float, maxStepsPerBar>> onsets;   // strength 0..1 per bar and step
        std::array<float, 12> chroma {}, bass {};
    };

    inline const char* pcName (int pc)
    {
        static const char* const n[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return n[((pc % 12) + 12) % 12];
    }
    inline juce::String keyName (const Key& k)
    {
        return juce::String (pcName (k.rootPc)) + " " + BassMachine::scaleName (k.scale);
    }
    inline juce::String chordName (const Chord& c)
    {
        if (c.rootPc < 0) return "-";
        return juce::String (pcName (c.rootPc)) + (c.quality == 1 ? "m" : c.quality == 2 ? "5" : "");
    }

    // ---- tempo: spectral flux onset envelope, autocorrelation, a gentle prior around 110 BPM
    inline void detectTempo (const std::vector<float>& x, double sr, double& bpm, float& conf)
    {
        bpm = 0.0; conf = 0.0f;
        const int N = 1024, hop = 512;
        if ((int) x.size() < N + hop * 64) return;
        Spectrum sp; sp.init (N);
        const int frames = ((int) x.size() - N) / hop;
        std::vector<float> env ((size_t) frames, 0.0f), cur, prev ((size_t) N / 2, 0.0f);
        for (int f = 0; f < frames; ++f)
        {
            sp.mags (x.data() + (size_t) f * hop, N, cur);
            float flux = 0.0f;
            for (int k = 1; k < N / 2; ++k)
            {
                const float v = std::log1p (cur[(size_t) k] * 0.1f);
                flux += std::max (0.0f, v - prev[(size_t) k]);
                prev[(size_t) k] = v;
            }
            env[(size_t) f] = flux;
        }
        {   // remove the slow trend, keep only the peaks
            std::vector<float> tmp = env;
            const int w = 21;
            for (int f = 0; f < frames; ++f)
            {
                double s = 0.0; int c = 0;
                for (int j = std::max (0, f - w); j <= std::min (frames - 1, f + w); ++j) { s += tmp[(size_t) j]; ++c; }
                env[(size_t) f] = std::max (0.0f, tmp[(size_t) f] - (float) (s / c));
            }
        }
        const double hopSec = (double) hop / sr;
        const int maxLag = (int) (2.2 / hopSec) + 2;
        if (frames < maxLag * 2) return;
        std::vector<double> acf ((size_t) maxLag + 1, 0.0);
        for (int l = 1; l <= maxLag; ++l)
        {
            double s = 0.0;
            for (int i = 0; i + l < frames; ++i) s += (double) env[(size_t) i] * env[(size_t) (i + l)];
            acf[(size_t) l] = s / (frames - l);
        }
        auto at = [&] (double lag)
        {
            if (lag < 1.0 || lag >= maxLag) return 0.0;
            const int i = (int) lag; const double fr = lag - i;
            return acf[(size_t) i] * (1.0 - fr) + acf[(size_t) i + 1] * fr;
        };
        double best = -1.0, bestBpm = 0.0, sum = 0.0; int cnt = 0;
        std::vector<double> scores;
        for (double b = 60.0; b <= 200.0; b += 0.25)
        {
            const double lag = 60.0 / (b * hopSec);
            const double prior = std::exp (-0.5 * std::pow (std::log2 (b / 110.0) / 0.9, 2.0));
            const double s = (at (lag) + 0.5 * at (2.0 * lag) + 0.25 * at (4.0 * lag)) * prior;
            scores.push_back (s); sum += s; ++cnt;
            if (s > best) { best = s; bestBpm = b; }
        }
        if (best <= 0.0) return;
        bpm = bestBpm;
        std::nth_element (scores.begin(), scores.begin() + (long) scores.size() / 2, scores.end());
        conf = (float) juce::jlimit (0.0, 1.0, (best - scores[scores.size() / 2]) / best);
    }

    // The tempo or its double / half, whichever is closest to a reference tempo.
    inline double foldNear (double bpm, double ref)
    {
        if (bpm <= 0.0 || ref <= 0.0) return bpm;
        double bestV = bpm;
        for (double c : { bpm * 0.5, bpm, bpm * 2.0 })
            if (std::abs (std::log2 (c / ref)) < std::abs (std::log2 (bestV / ref))) bestV = c;
        return bestV;
    }

    // ---- key / mode candidates
    inline std::vector<int> analysisScales() { return { 0, 1, 2, 3, 4, 5, 7, 8, 9 }; }   // not the chromatic scale

    inline std::vector<Key> rankKeys (const std::array<float, 12>& chroma, const std::array<float, 12>& bass)
    {
        float total = 0.0f, btotal = 0.0f;
        for (int i = 0; i < 12; ++i) { total += chroma[(size_t) i]; btotal += bass[(size_t) i]; }
        std::vector<Key> out;
        if (total <= 0.0f) return out;
        for (int s : analysisScales())
        {
            const int n = BassMachine::scaleSize (s);
            for (int r = 0; r < 12; ++r)
            {
                float in = 0.0f; int weak = 0;
                for (int i = 0; i < n; ++i)
                {
                    const float c = chroma[(size_t) ((r + BassMachine::scaleSemi (s, i)) % 12)] / total;
                    in += c;
                    if (c < 0.25f / (float) n) ++weak;
                }
                const float missing = (float) weak / (float) n;
                const float tonic = 0.5f * chroma[(size_t) r] / total + 0.5f * (btotal > 0.0f ? bass[(size_t) r] / btotal : chroma[(size_t) r] / total);
                out.push_back ({ r, s, in * (1.0f - 0.7f * missing) + 0.6f * tonic });
            }
        }
        std::sort (out.begin(), out.end(), [] (const Key& a, const Key& b) { return a.score > b.score; });
        return out;
    }

    // ---- chord root (and rough quality) per bar, helped by the chosen key
    inline std::vector<Chord> detectChords (const Result& r, const Key& key)
    {
        std::vector<Chord> out;
        for (int b = 0; b < r.bars; ++b)
        {
            const auto& ch = r.barChroma[(size_t) b]; const auto& bs = r.barBass[(size_t) b];
            float cs = 0.0f, bsum = 0.0f;
            for (int i = 0; i < 12; ++i) { cs += ch[(size_t) i]; bsum += bs[(size_t) i]; }
            Chord best;
            if (cs <= 1.0e-6f) { out.push_back (best); continue; }
            float bestScore = -1.0e9f;
            for (int root = 0; root < 12; ++root)
            {
                const float rootE = ch[(size_t) root] / cs, thirdMin = ch[(size_t) ((root + 3) % 12)] / cs, thirdMaj = ch[(size_t) ((root + 4) % 12)] / cs;
                const float fifth = ch[(size_t) ((root + 7) % 12)] / cs;
                const float bassE = bsum > 0.0f ? bs[(size_t) root] / bsum : 0.0f;
                const bool inKey = BassMachine::hasSemitone (key.scale, ((root - key.rootPc) % 12 + 12) % 12);
                const float common = 1.0f * rootE + 0.7f * fifth + 1.2f * bassE + (inKey ? 0.12f : 0.0f);
                const float rest = 1.0f - rootE - fifth - thirdMin - thirdMaj;
                struct Q { int q; float s; };
                const Q qs[3] = { { 2, common - 0.5f * (thirdMin + thirdMaj) - 0.3f * rest + 0.05f },
                                  { 1, common + 0.8f * thirdMin - 0.5f * thirdMaj - 0.3f * rest },
                                  { 0, common + 0.8f * thirdMaj - 0.5f * thirdMin - 0.3f * rest } };
                for (const auto& q : qs)
                    if (q.s > bestScore) { bestScore = q.s; best = { root, q.q, q.s }; }
            }
            out.push_back (best);
        }
        // a silent bar takes the previous bar's chord (or the key root)
        for (size_t i = 0; i < out.size(); ++i)
            if (out[i].rootPc < 0) out[i] = i > 0 ? out[i - 1] : Chord { key.rootPc, 2, 0.0f };
        return out;
    }

    // ---- the whole analysis. `clip` is the track audio, `offset` its start on the timeline (samples).
    inline Result analyse (const juce::AudioBuffer<float>& clip, int len, juce::int64 offset, double sr, double samplesPerBeat,
                           double barBeats, int maxBars)
    {
        Result res;
        len = juce::jmin (len, clip.getNumSamples());
        if (len < (int) (sr * 1.0) || clip.getNumChannels() < 1) return res;
        const int ch = clip.getNumChannels();
        const double barLenD = barBeats * samplesPerBeat;
        const juce::int64 barLen = juce::jmax<juce::int64> (1, (juce::int64) std::llround (barLenD));
        res.barSteps = juce::jlimit (4, maxStepsPerBar, (int) std::lround (barBeats * 4.0));

        auto sample = [&] (juce::int64 timelinePos) -> float
        {
            const juce::int64 i = timelinePos - offset;
            if (i < 0 || i >= len) return 0.0f;
            float s = clip.getSample (0, (int) i);
            if (ch > 1) s = 0.5f * (s + clip.getSample (1, (int) i));
            return s;
        };

        // tempo from the clip itself
        {
            std::vector<float> mono ((size_t) len);
            for (int i = 0; i < len; ++i) mono[(size_t) i] = sample (offset + i);
            detectTempo (mono, sr, res.tempo, res.tempoConf);
        }

        // bars that hold audio
        const juce::int64 firstBarAll = offset / barLen, endPos = offset + len;
        auto barEnergy = [&] (juce::int64 b)
        {
            double e = 0.0; const juce::int64 a = b * barLen, z = a + barLen;
            for (juce::int64 p = std::max (a, offset); p < std::min (z, endPos); p += 8) e += std::abs (sample (p));
            return e / (double) (barLen / 8 + 1);
        };
        juce::int64 b0 = firstBarAll, b1 = (endPos + barLen - 1) / barLen;
        while (b0 < b1 && barEnergy (b0) < 0.002) ++b0;
        while (b1 > b0 && barEnergy (b1 - 1) < 0.002) --b1;
        res.bar0 = (int) b0;
        res.bars = (int) juce::jlimit<juce::int64> (0, maxBars, b1 - b0);
        if (res.bars <= 0) return res;
        const juce::int64 t0 = b0 * barLen, total = (juce::int64) res.bars * barLen;
        std::vector<float> sig ((size_t) total);
        for (juce::int64 i = 0; i < total; ++i) sig[(size_t) i] = sample (t0 + i);

        // chroma per bar (spectral peaks, with a tuning correction)
        {
            const int N = 8192, hop = 4096;
            Spectrum sp; sp.init (N);
            struct Peak { float m, w; int bar; bool low; };
            std::vector<Peak> peaks;
            std::vector<float> mg;
            const int kLo = (int) std::floor (45.0 * N / sr), kHi = std::min (N / 2 - 2, (int) std::ceil (2500.0 * N / sr));
            for (juce::int64 st = 0; st < total; st += hop)
            {
                const int avail = (int) std::min<juce::int64> (N, total - st);
                double rms = 0.0; for (int i = 0; i < avail; ++i) rms += (double) sig[(size_t) (st + i)] * sig[(size_t) (st + i)];
                if (std::sqrt (rms / avail) < 0.001) continue;
                sp.mags (sig.data() + st, avail, mg);
                float mx = 0.0f; for (int k = kLo; k <= kHi; ++k) mx = std::max (mx, mg[(size_t) k]);
                if (mx <= 0.0f) continue;
                const int bar = (int) std::min<juce::int64> (res.bars - 1, (st + N / 2) / barLen);
                for (int k = std::max (kLo, 1); k <= kHi; ++k)
                {
                    const float a = mg[(size_t) k - 1], b = mg[(size_t) k], c = mg[(size_t) k + 1];
                    if (! (b > a && b >= c) || b < 0.06f * mx) continue;
                    const float la = std::log (a + 1.0e-9f), lb = std::log (b + 1.0e-9f), lc = std::log (c + 1.0e-9f);
                    const float den = la - 2.0f * lb + lc;
                    const float delta = std::abs (den) > 1.0e-9f ? juce::jlimit (-0.5f, 0.5f, 0.5f * (la - lc) / den) : 0.0f;
                    const double f = ((double) k + delta) * sr / N;
                    peaks.push_back ({ (float) (69.0 + 12.0 * std::log2 (f / 440.0)), std::sqrt (b / mx), bar, f < 250.0 });
                }
            }
            if (peaks.empty()) return res;
            float hist[25] = {};
            for (const auto& p : peaks) { const float off = p.m - std::round (p.m); hist[juce::jlimit (0, 24, (int) std::floor ((off + 0.5f) * 25.0f))] += p.w; }
            int hb = 0; for (int i = 1; i < 25; ++i) if (hist[i] > hist[hb]) hb = i;
            const float tune = ((float) hb + 0.5f) / 25.0f - 0.5f;
            res.barChroma.assign ((size_t) res.bars, {}); res.barBass.assign ((size_t) res.bars, {});
            for (const auto& p : peaks)
            {
                const int pc = ((int) std::lround (p.m - tune) % 12 + 12) % 12;
                res.barChroma[(size_t) p.bar][(size_t) pc] += p.w;
                res.chroma[(size_t) pc] += p.w;
                if (p.low) { res.barBass[(size_t) p.bar][(size_t) pc] += p.w; res.bass[(size_t) pc] += p.w; }
            }
            res.keys = rankKeys (res.chroma, res.bass);
        }

        // rhythm: onset strengths on the step grid of every bar
        {
            const int N = 1024, hop = 256;
            Spectrum sp; sp.init (N);
            const int frames = (int) ((total - N) / hop);
            res.onsets.assign ((size_t) res.bars, {});
            if (frames > 8)
            {
                std::vector<float> env ((size_t) frames, 0.0f), cur, prev ((size_t) N / 2, 0.0f);
                for (int f = 0; f < frames; ++f)
                {
                    sp.mags (sig.data() + (size_t) f * hop, N, cur);
                    float flux = 0.0f;
                    for (int k = 1; k < N / 2; ++k)
                    {
                        const float v = std::log1p (cur[(size_t) k] * 0.1f);
                        flux += std::max (0.0f, v - prev[(size_t) k]);
                        prev[(size_t) k] = v;
                    }
                    env[(size_t) f] = flux;
                }
                float mx = 0.0f; for (float v : env) mx = std::max (mx, v);
                const double stepLen = barLenD / res.barSteps;
                int lastPeak = -1000;
                for (int f = 1; f + 1 < frames && mx > 0.0f; ++f)
                {
                    const float v = env[(size_t) f];
                    if (v < 0.2f * mx || v < env[(size_t) f - 1] || v < env[(size_t) f + 1] || f - lastPeak < 6) continue;
                    lastPeak = f;
                    const double t = (double) f * hop + N / 4;                    // the flux peaks while the onset sits in the first part of the window
                    const int stepAll = (int) std::lround (t / stepLen);
                    const int bar = stepAll / res.barSteps, step = stepAll % res.barSteps;
                    if (bar >= 0 && bar < res.bars) res.onsets[(size_t) bar][(size_t) step] = std::max (res.onsets[(size_t) bar][(size_t) step], v / mx);
                }
            }
        }
        res.valid = ! res.keys.empty();
        return res;
    }
}
