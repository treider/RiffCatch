#include "ProjectIO.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace ProjectIO
{
bool save (AudioEngine& e, const juce::File& dir, double bpm, juce::String& error)
{
    if (! dir.createDirectory()) { error = "Could not create " + dir.getFullPathName(); return false; }

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    juce::WavAudioFormat wav;

    juce::Array<juce::var> tracks;
    for (int i = 0; i < e.getNumTracks(); ++i)
    {
        auto& tr = e.tracks[i];
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
    loop->setProperty ("enabled", false);
    loop->setProperty ("startBar", 0);
    loop->setProperty ("bars", 4);

    auto* root = new juce::DynamicObject();
    root->setProperty ("version", 1);
    root->setProperty ("name", dir.getFileName());
    root->setProperty ("bpm", bpm);
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

bool load (AudioEngine& e, const juce::File& dir, double& bpm, juce::String& error)
{
    auto json = juce::JSON::parse (dir.getChildFile ("project.json"));
    auto* arr = json["tracks"].getArray();
    if (! json.isObject() || arr == nullptr) { error = "Not a RiffCatch project: " + dir.getFullPathName(); return false; }

    e.stop();
    e.setNumTracks (0);

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    const double devRate = e.getSampleRate();
    const int count = juce::jmin (arr->size(), (int) AudioEngine::maxTracks);

    for (int i = 0; i < AudioEngine::maxTracks; ++i)
    {
        auto& tr = e.tracks[i];
        tr.resetToDefaults (i);
        if (i >= count) continue;

        auto t = arr->getReference (i);
        tr.name = t["name"].toString();
        tr.volume = (float) (double) t.getProperty ("volume", 1.0);
        tr.pan = (float) (double) t.getProperty ("pan", 0.0);
        tr.mute = (bool) t.getProperty ("mute", false);
        tr.solo = (bool) t.getProperty ("solo", false);
        tr.inputCode = (int) t.getProperty ("input", 0);

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

    bpm = (double) json.getProperty ("bpm", 120.0);
    e.masterVolume = (float) (double) json.getProperty ("masterVolume", 1.0);
    e.setNumTracks (juce::jmax (1, count));
    return true;
}
}
