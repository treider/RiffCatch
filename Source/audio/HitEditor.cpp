#include "HitEditor.h"

namespace HitEditor
{
std::vector<juce::int64> detectOnsets (const juce::AudioBuffer<float>& b, double sr, float riseDb)
{
    std::vector<juce::int64> out;
    const int len = b.getNumSamples(), hop = 256, frames = len / hop;
    if (frames < 4) return out;

    const float* l = b.getReadPointer (0);
    const float* r = b.getReadPointer (1);
    auto mono = [&] (int i) { return 0.5f * (l[i] + r[i]); };

    std::vector<float> db ((size_t) frames);
    for (int f = 0; f < frames; ++f)
    {
        double sum = 0.0;
        for (int i = 0; i < hop; ++i) { const float m = mono (f * hop + i); sum += (double) m * m; }
        db[(size_t) f] = 20.0f * std::log10 ((float) std::sqrt (sum / hop) + 1.0e-6f);
    }

    const int refractory = (int) (0.09 * sr / hop);
    int lastFrame = -1000000;
    for (int f = 2; f < frames; ++f)
    {
        const float prev = juce::jmin (db[(size_t) f - 1], db[(size_t) f - 2]);
        if (db[(size_t) f] - prev >= riseDb && db[(size_t) f] > -48.0f && f - lastFrame > refractory)
        {
            const int s0 = (f - 1) * hop, s1 = juce::jmin (len, (f + 1) * hop);
            float peak = 0.0f;
            for (int i = s0; i < s1; ++i) peak = juce::jmax (peak, std::abs (mono (i)));
            int pos = s0;
            for (int i = s0; i < s1; ++i) if (std::abs (mono (i)) >= 0.3f * peak) { pos = i; break; }
            out.push_back (pos);
            lastFrame = f;
        }
    }
    return out;
}

// Stretch/compress src[sStart, sStart+a) into out[dStart, dStart+b) using WSOLA (waveform-similarity overlap-add).
static void stretchInto (const juce::AudioBuffer<float>& src, int sStart, int a,
                         juce::AudioBuffer<float>& out, int dStart, int b)
{
    const float* sl = src.getReadPointer (0) + sStart;
    const float* sr = src.getReadPointer (1) + sStart;
    float* ol = out.getWritePointer (0) + dStart;
    float* orr = out.getWritePointer (1) + dStart;

    constexpr int N = 1024, H = N / 2, tol = 192, cmpLen = 256;
    if (a < 2 * N + tol || b < 2 * N)   // too short for grains: plain linear resample
    {
        for (int i = 0; i < b; ++i)
        {
            const double p = (double) i * a / b;
            const int i0 = juce::jmin (a - 1, (int) p), i1 = juce::jmin (a - 1, i0 + 1);
            const float fr = (float) (p - i0);
            ol[i]  = sl[i0] + fr * (sl[i1] - sl[i0]);
            orr[i] = sr[i0] + fr * (sr[i1] - sr[i0]);
        }
        return;
    }

    std::vector<float> mono ((size_t) a), win (N), accL ((size_t) b, 0.0f), accR ((size_t) b, 0.0f), wsum ((size_t) b, 0.0f);
    for (int i = 0; i < a; ++i) mono[(size_t) i] = 0.5f * (sl[i] + sr[i]);
    for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) N);

    const double ratio = (double) a / b;
    int prevStart = -1;
    bool first = true;
    for (int outPos = 0; outPos < b + H; outPos += H)
    {
        const int nominal = (int) std::lround (outPos * ratio);
        int start = nominal;
        if (prevStart >= 0)
        {
            const int tmpl = prevStart + H;   // natural continuation of the previous grain
            float best = -1.0e30f;
            if (tmpl + cmpLen <= a)
                for (int s = nominal - tol; s <= nominal + tol; ++s)
                {
                    if (s < 0 || s + cmpLen > a) continue;
                    float corr = 0.0f;
                    for (int i = 0; i < cmpLen; ++i) corr += mono[(size_t) (tmpl + i)] * mono[(size_t) (s + i)];
                    if (corr > best) { best = corr; start = s; }
                }
        }
        start = juce::jlimit (0, juce::jmax (0, a - N), start);
        prevStart = start;

        for (int i = 0; i < N; ++i)
        {
            const int o = outPos + i;
            if (o >= b) break;
            const int si = start + i;
            if (si >= a) break;
            const float w = (first && i < H) ? 1.0f : win[(size_t) i];
            accL[(size_t) o] += w * sl[si];
            accR[(size_t) o] += w * sr[si];
            wsum[(size_t) o] += w;
        }
        first = false;
    }
    for (int i = 0; i < b; ++i)
    {
        const float w = juce::jmax (wsum[(size_t) i], 0.25f);
        ol[i] = accL[(size_t) i] / w;
        orr[i] = accR[(size_t) i] / w;
    }
}

juce::AudioBuffer<float> render (const juce::AudioBuffer<float>& src, const std::vector<Hit>& hits, double sr)
{
    const int len = src.getNumSamples();
    juce::AudioBuffer<float> out (2, len);
    out.clear();
    if (len <= 0) return out;

    struct P { juce::int64 s, d; };
    std::vector<P> pts { { 0, 0 } };
    for (auto& h : hits)
    {
        const auto s = juce::jlimit<juce::int64> (1, len - 1, h.src), d = juce::jlimit<juce::int64> (1, len - 1, h.dst);
        if (s > pts.back().s && d > pts.back().d) pts.push_back ({ s, d });
    }
    pts.push_back ({ len, len });

    const int attackMax = (int) (0.015 * sr);
    std::vector<bool> modified (pts.size() - 1, false);
    for (size_t k = 0; k + 1 < pts.size(); ++k) modified[k] = (pts[k + 1].s - pts[k].s) != (pts[k + 1].d - pts[k].d);

    for (size_t k = 0; k + 1 < pts.size(); ++k)
    {
        const int s0 = (int) pts[k].s, s1 = (int) pts[k + 1].s, d0 = (int) pts[k].d, d1 = (int) pts[k + 1].d;
        const int sl = s1 - s0, dl = d1 - d0;
        if (sl <= 0 || dl <= 0) continue;

        if (! modified[k])
        {
            for (int c = 0; c < 2; ++c) out.copyFrom (c, d0, src, c, s0, sl);
        }
        else
        {
            const int attack = k == 0 ? 0 : juce::jmin (juce::jmin (sl, dl), attackMax);
            for (int c = 0; c < 2; ++c) out.copyFrom (c, d0, src, c, s0, attack);
            const int a = sl - attack, b = dl - attack;
            if (a > 0 && b > 0) stretchInto (src, s0 + attack, a, out, d0 + attack, b);

            const int fo = juce::jmin (96, dl);               // fade out the tail at a re-timed join
            for (int c = 0; c < 2; ++c) out.applyGainRamp (c, d1 - fo, fo, 1.0f, 0.0f);
        }
        if (k > 0 && (modified[k] || modified[k - 1]))          // soft fade-in at a re-timed join
        {
            const int fi = juce::jmin (24, dl);
            for (int c = 0; c < 2; ++c) out.applyGainRamp (c, d0, fi, 0.0f, 1.0f);
        }
    }
    return out;
}
}
