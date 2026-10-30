#include "ProjectIO.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace ProjectIO
{
bool save (AudioEngine& e, const juce::File& dir, juce::String& error)
{
    if (! dir.createDirectory()) { error = "Could not create " + dir.getFullPathName(); return false; }

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    juce::WavAudioFormat wav;

    juce::Array<juce::var> tracks;
    int pos = 0;
    for (int slot : e.order)
    {
        const int i = pos++;
        auto& tr = e.tracks[slot];
        juce::String fileName;
        const auto len = tr.length.load();
        if (len > 0)
        {
            fileName = "track" + juce::String (i + 1) + ".wav";
            auto f = dir.getChildFile (fileName);
            f.deleteFile();
            std::unique_ptr<juce::FileOutputStream> os (f.createOutputStream());
            if (os == nullptr) { error = "Could not write " + f.getFullPathName(); return false; }
            std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), e.getSampleRate(), 2, 24, {}, 0));
            if (w == nullptr) { error = "WAV writer failed"; return false; }
            os.release();   // writer owns the stream
            juce::SpinLock::ScopedLockType l (tr.lock);
            w->writeFromAudioSampleBuffer (tr.data, 0, (int) len);
        }

        auto* o = new juce::DynamicObject();
        o->setProperty ("name", tr.name);
        o->setProperty ("file", fileName);
        o->setProperty ("volume", (double) tr.volume.load());
        o->setProperty ("pan", (double) tr.pan.load());
        o->setProperty ("mute", tr.mute.load());
        o->setProperty ("solo", tr.solo.load());
        o->setProperty ("input", tr.inputCode.load());
        o->setProperty ("nudgeMs", tr.nudge.load() * 1000.0 / juce::jmax (1.0, e.getSampleRate()));
        tracks.add (juce::var (o));
    }
    // remove stale wav files from removed/cleared tracks
    for (auto& f : dir.findChildFiles (juce::File::findFiles, false, "track*.wav"))
    {
        bool used = false;
        for (auto& t : tracks) used |= (t["file"].toString() == f.getFileName());
        if (! used) f.deleteFile();
    }

    auto* loop = new juce::DynamicObject();
    loop->setProperty ("enabled", e.loopEnabled.load());
    loop->setProperty ("startBar", 0);
    loop->setProperty ("bars", e.loopBars.load());

    auto* root = new juce::DynamicObject();
    root->setProperty ("version", 1);
    root->setProperty ("name", dir.getFileName());
    root->setProperty ("bpm", e.bpm.load());
    root->setProperty ("timeSignature", "4/4");
    root->setProperty ("clickEnabled", e.clickEnabled.load());
    root->setProperty ("clickVolume", (double) e.clickVolume.load());
    root->setProperty ("countInBars", e.countInBars.load());
    root->setProperty ("sampleRate", e.getSampleRate());
    root->setProperty ("masterVolume", (double) e.masterVolume.load());
    root->setProperty ("loop", juce::var (loop));
    root->setProperty ("tracks", tracks);

    if (! dir.getChildFile ("project.json").replaceWithText (juce::JSON::toString (juce::var (root))))
    {
        error = "Could not write project.json";
        return false;
    }
    return true;
}

bool load (AudioEngine& e, const juce::File& dir, juce::String& error)
{
    auto json = juce::JSON::parse (dir.getChildFile ("project.json"));
    auto* arr = json["tracks"].getArray();
    if (! json.isObject() || arr == nullptr) { error = "Not a RiffCatch project: " + dir.getFullPathName(); return false; }

    e.removeAllTracks();

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    const double devRate = e.getSampleRate();
    const int count = juce::jmin (arr->size(), (int) AudioEngine::maxTracks);

    for (int i = 0; i < AudioEngine::maxTracks; ++i)
    {
        if (i >= count) continue;
        e.addTrack();
        auto& tr = e.tracks[i];

        auto t = arr->getReference (i);
        tr.name = t["name"].toString();
        tr.volume = (float) (double) t.getProperty ("volume", 1.0);
        tr.pan = (float) (double) t.getProperty ("pan", 0.0);
        tr.mute = (bool) t.getProperty ("mute", false);
        tr.solo = (bool) t.getProperty ("solo", false);
        tr.inputCode = (int) t.getProperty ("input", 0);
        tr.nudge = (int) std::lround ((double) t.getProperty ("nudgeMs", 0.0) * devRate / 1000.0);

        auto file = dir.getChildFile (t["file"].toString());
        if (t["file"].toString().isNotEmpty() && file.existsAsFile())
        {
            std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (file));
            if (r == nullptr) continue;
            const int len = (int) r->lengthInSamples;
            juce::AudioBuffer<float> buf (2, len);
            r->read (&buf, 0, len, 0, true, true);

            if (std::abs (r->sampleRate - devRate) > 0.5 && devRate > 0)
            {
                const double ratio = r->sampleRate / devRate;
                const int newLen = (int) std::ceil (len / ratio);
                juce::AudioBuffer<float> rs (2, newLen);
                for (int ch = 0; ch < 2; ++ch)
                {
                    juce::LagrangeInterpolator li;
                    li.process (ratio, buf.getReadPointer (ch), rs.getWritePointer (ch), newLen);
                }
                buf = std::move (rs);
            }
            tr.setAudio (std::move (buf));
        }
    }

    e.bpm = juce::jlimit (30.0, 300.0, (double) json.getProperty ("bpm", 120.0));
    e.loopEnabled = (bool) json["loop"].getProperty ("enabled", true);
    e.loopBars = juce::jlimit (1, 64, (int) json["loop"].getProperty ("bars", 4));
    e.clickEnabled = (bool) json.getProperty ("clickEnabled", true);
    e.clickVolume = (float) (double) json.getProperty ("clickVolume", 0.5);
    e.countInBars = json.hasProperty ("countInBars") ? juce::jlimit (0, 2, (int) json.getProperty ("countInBars", 1))
                                                     : ((double) json.getProperty ("countInSeconds", 3.0) > 0.0 ? 1 : 0);
    e.masterVolume = (float) (double) json.getProperty ("masterVolume", 1.0);
    if (count == 0) e.addTrack();
    return true;
}
}
