#include "ProjectIO.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace ProjectIO
{
bool isSafeTrackFile (const juce::File& dir, const juce::String& name)
{
    if (name.isEmpty() || name.length() > 80) return false;
    if (name != juce::File::createLegalFileName (name)) return false;          // no separators, drive letters or reserved characters
    if (name.contains ("..") || ! name.endsWithIgnoreCase (".wav")) return false;
    return dir.getChildFile (name).getParentDirectory() == dir;
}

bool save (AudioEngine& e, const juce::File& projectFile, juce::String& error)
{
    const auto dir = projectFile.getParentDirectory();
    // never write into a folder that holds somebody else's files
    if (dir.exists() && ! projectFile.existsAsFile())
    {
        if (dir.getNumberOfChildFiles (juce::File::findFiles, "*.riffcatch") > 0)
        {
            error = "\"" + dir.getFileName() + "\" already holds another RiffCatch project. Give this one its own folder.";
            return false;
        }
        if (dir.getNumberOfChildFiles (juce::File::findFilesAndDirectories) > 0)
        {
            error = "\"" + dir.getFileName() + "\" is not empty and is not a RiffCatch project. Choose a new or empty folder.";
            return false;
        }
    }
    if (! dir.createDirectory()) { error = "Could not create " + dir.getFullPathName(); return false; }

    // the track files an earlier save of this project wrote: only these may be cleaned up later
    juce::StringArray ownedBefore;
    const auto oldJson = juce::JSON::parse (projectFile);    // kept alive while its array is read
    if (auto* oldTracks = oldJson["tracks"].getArray())
        for (auto& t : *oldTracks)
            if (isSafeTrackFile (dir, t["file"].toString())) ownedBefore.add (t["file"].toString());

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
        o->setProperty ("echo", tr.echoOn.load());
        o->setProperty ("echoDiv", tr.echoDiv.load());
        o->setProperty ("reverb", tr.reverbOn.load());
        o->setProperty ("offsetMs", tr.offset.load() * 1000.0 / juce::jmax (1.0, e.getSampleRate()));
        tracks.add (juce::var (o));
    }
    // remove stale wav files from removed/cleared tracks (only files this project wrote earlier)
    for (auto& name : ownedBefore)
    {
        bool used = false;
        for (auto& t : tracks) used |= (t["file"].toString() == name);
        if (! used) dir.getChildFile (name).deleteFile();
    }

    auto* loop = new juce::DynamicObject();
    loop->setProperty ("enabled", e.loopEnabled.load());
    loop->setProperty ("startBar", 0);
    loop->setProperty ("bars", e.loopBars.load());

    auto* root = new juce::DynamicObject();
    root->setProperty ("version", 1);
    root->setProperty ("appVersion", JUCE_APPLICATION_VERSION_STRING);
    root->setProperty ("name", projectFile.getFileNameWithoutExtension());
    root->setProperty ("bpm", e.bpm.load());
    root->setProperty ("timeSignature", juce::String (e.tsNum.load()) + "/" + juce::String (e.tsDen.load()));
    root->setProperty ("notes", e.projectNotes.substring (0, maxNotesLength));
    root->setProperty ("clickEnabled", e.clickEnabled.load());
    root->setProperty ("clickVolume", (double) e.clickVolume.load());
    root->setProperty ("countInBars", e.countInBars.load());
    root->setProperty ("sampleRate", e.getSampleRate());
    root->setProperty ("masterVolume", (double) e.masterVolume.load());
    root->setProperty ("loop", juce::var (loop));
    root->setProperty ("tracks", tracks);

    {   // drum machine: pattern as one string per voice ('0' off, '1' hit, '2' accent)
        auto& dm = e.drums;
        auto* d = new juce::DynamicObject();
        d->setProperty ("bars", dm.bars.load());
        d->setProperty ("volume", (double) dm.volume.load());
        d->setProperty ("mute", dm.mute.load());
        d->setProperty ("solo", dm.solo.load());
        d->setProperty ("cymbalPitch", (double) dm.cymbalPitch.load());
        juce::Array<juce::var> voices;
        for (int v = 0; v < DrumMachine::numVoices; ++v)
        {
            auto* vo = new juce::DynamicObject();
            juce::String pat;
            for (int k = 0; k < DrumMachine::maxSteps; ++k) pat << (char) ('0' + juce::jlimit (0, 2, (int) dm.steps[v][k].load()));
            vo->setProperty ("steps", pat);
            vo->setProperty ("level", (double) dm.level[v].load());
            vo->setProperty ("pan", (double) dm.pan[v].load());
            vo->setProperty ("mute", dm.voiceMute[v].load());
            voices.add (juce::var (vo));
        }
        d->setProperty ("voices", voices);
        root->setProperty ("drums", juce::var (d));
    }

    {   // bass synth: cells as numbers (see BassMachine::pack), per-bar transposition, key, sound
        auto& bm = e.bass;
        auto* b = new juce::DynamicObject();
        b->setProperty ("bars", bm.bars.load());
        b->setProperty ("root", bm.root.load());
        b->setProperty ("scale", bm.scale.load());
        b->setProperty ("volume", (double) bm.volume.load());
        b->setProperty ("mute", bm.mute.load());
        b->setProperty ("solo", bm.solo.load());
        b->setProperty ("echoOn", bm.echoOn.load());
        b->setProperty ("reverbOn", bm.reverbOn.load());
        b->setProperty ("echoDiv", bm.echoDiv.load());
        b->setProperty ("wave", bm.wave.load());
        b->setProperty ("sub", (double) bm.sub.load());
        b->setProperty ("cutoff", (double) bm.cutoff.load());
        b->setProperty ("reso", (double) bm.reso.load());
        b->setProperty ("env", (double) bm.envAmt.load());
        b->setProperty ("decay", (double) bm.decay.load());
        b->setProperty ("sustain", (double) bm.sustain.load());
        b->setProperty ("glide", (double) bm.glide.load());
        b->setProperty ("drive", (double) bm.drive.load());
        juce::StringArray cells, shifts;
        for (int k = 0; k < BassMachine::maxSteps; ++k) cells.add (juce::String ((juce::int64) bm.steps[k].load()));
        for (int k = 0; k < BassMachine::maxBars; ++k) shifts.add (juce::String (bm.shift[k].load()));
        b->setProperty ("steps", cells.joinIntoString (","));
        b->setProperty ("shift", shifts.joinIntoString (","));
        root->setProperty ("bass", juce::var (b));
    }

    if (! projectFile.replaceWithText (juce::JSON::toString (juce::var (root))))
    {
        error = "Could not write " + projectFile.getFileName();
        return false;
    }
    return true;
}

bool load (AudioEngine& e, const juce::File& projectFile, juce::String& error)
{
    const auto dir = projectFile.getParentDirectory();
    if (! projectFile.existsAsFile()) { error = "Project file not found: " + projectFile.getFullPathName(); return false; }
    auto json = juce::JSON::parse (projectFile);
    auto* arr = json["tracks"].getArray();
    if (! json.isObject() || arr == nullptr) { error = "Not a RiffCatch project: " + projectFile.getFullPathName(); return false; }

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
        tr.name = t["name"].toString().substring (0, maxNameLength);
        tr.volume = juce::jlimit (0.0f, 4.0f, (float) (double) t.getProperty ("volume", 1.0));
        tr.pan = juce::jlimit (-1.0f, 1.0f, (float) (double) t.getProperty ("pan", 0.0));
        tr.mute = (bool) t.getProperty ("mute", false);
        tr.solo = (bool) t.getProperty ("solo", false);
        {
            const int in = (int) t.getProperty ("input", 0);                   // 0..15 = mono input, 100..115 = stereo pair
            tr.inputCode = (in >= 100 && in <= 115) ? in : juce::jlimit (0, 15, in);
        }
        tr.echoOn = (bool) t.getProperty ("echo", false);
        tr.echoDiv = juce::jlimit (0, (int) TrackFx::numDivs - 1, (int) t.getProperty ("echoDiv", (int) TrackFx::Quarter));
        tr.reverbOn = (bool) t.getProperty ("reverb", false);
        {
            const double ms = juce::jlimit (-20.0 * 60000.0, 20.0 * 60000.0, (double) (t.hasProperty ("offsetMs") ? t.getProperty ("offsetMs", 0.0) : t.getProperty ("nudgeMs", 0.0)));
            tr.offset = (int) std::lround (ms * devRate / 1000.0);
        }

        const auto fileName = t["file"].toString();
        auto file = dir.getChildFile (fileName);
        if (isSafeTrackFile (dir, fileName) && file.existsAsFile())
        {
            std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (file));
            if (r == nullptr) continue;
            if (r->sampleRate < 8000.0 || r->sampleRate > 384000.0 || r->lengthInSamples <= 0
                || (double) r->lengthInSamples > 20.0 * 60.0 * r->sampleRate)   // same limit as importing audio
                continue;
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

    e.projectNotes = json.getProperty ("notes", "").toString().substring (0, maxNotesLength);
    {
        const auto ts = json.getProperty ("timeSignature", "4/4").toString();
        const int num = ts.upToFirstOccurrenceOf ("/", false, false).getIntValue(), den = ts.fromFirstOccurrenceOf ("/", false, false).getIntValue();
        e.setTimeSignature (num >= 1 ? num : 4, den == 8 ? 8 : 4);
    }
    e.bpm = juce::jlimit (30.0, 300.0, (double) json.getProperty ("bpm", 120.0));
    e.loopEnabled = (bool) json["loop"].getProperty ("enabled", true);
    e.loopBars = juce::jlimit (1, 64, (int) json["loop"].getProperty ("bars", 4));
    e.clickEnabled = (bool) json.getProperty ("clickEnabled", false);
    e.clickVolume = (float) (double) json.getProperty ("clickVolume", 0.5);
    e.countInBars = json.hasProperty ("countInBars") ? juce::jlimit (0, 2, (int) json.getProperty ("countInBars", 1))
                                                     : ((double) json.getProperty ("countInSeconds", 3.0) > 0.0 ? 1 : 0);
    e.masterVolume = (float) (double) json.getProperty ("masterVolume", 1.0);
    {   // drum machine (absent in older projects)
        auto& dm = e.drums;
        dm.resetAll();
        const auto d = json["drums"];
        if (d.isObject())
        {
            dm.bars = juce::jlimit (1, DrumMachine::maxBars, (int) d.getProperty ("bars", 1));
            dm.volume = (float) (double) d.getProperty ("volume", 0.8);
            dm.mute = (bool) d.getProperty ("mute", false);
            dm.solo = (bool) d.getProperty ("solo", false);
            dm.cymbalPitch = (float) (double) d.getProperty ("cymbalPitch", 0.0);
            if (auto* vs = d["voices"].getArray())
                for (int v = 0; v < juce::jmin (vs->size(), (int) DrumMachine::numVoices); ++v)
                {
                    const auto vo = vs->getReference (v);
                    const auto pat = vo["steps"].toString();
                    for (int k = 0; k < juce::jmin (pat.length(), (int) DrumMachine::maxSteps); ++k)
                        dm.steps[v][k] = (uint8_t) juce::jlimit (0, 2, (int) (pat[k] - '0'));
                    dm.level[v] = (float) (double) vo.getProperty ("level", 1.0);
                    dm.pan[v] = (float) (double) vo.getProperty ("pan", 0.0);
                    dm.voiceMute[v] = (bool) vo.getProperty ("mute", false);
                }
        }
    }
    {   // bass synth (absent in older projects)
        auto& bm = e.bass;
        bm.resetAll();
        const auto b = json["bass"];
        if (b.isObject())
        {
            bm.bars = juce::jlimit (1, BassMachine::maxBars, (int) b.getProperty ("bars", 1));
            bm.root = juce::jlimit (BassMachine::rootMin, BassMachine::rootMax, (int) b.getProperty ("root", 33));
            bm.scale = juce::jlimit (0, BassMachine::numScales - 1, (int) b.getProperty ("scale", 0));
            bm.volume = juce::jlimit (0.0f, 1.5f, (float) (double) b.getProperty ("volume", 0.8));
            bm.mute = (bool) b.getProperty ("mute", false);
            bm.solo = (bool) b.getProperty ("solo", false);
            bm.echoOn = (bool) b.getProperty ("echoOn", false);
            bm.reverbOn = (bool) b.getProperty ("reverbOn", false);
            bm.echoDiv = juce::jlimit (0, (int) TrackFx::numDivs - 1, (int) b.getProperty ("echoDiv", (int) TrackFx::Quarter));
            bm.wave = juce::jlimit (0, 2, (int) b.getProperty ("wave", 0));
            bm.sub = juce::jlimit (0.0f, 1.0f, (float) (double) b.getProperty ("sub", 0.5));
            bm.cutoff = juce::jlimit (30.0f, 12000.0f, (float) (double) b.getProperty ("cutoff", 400.0));
            bm.reso = juce::jlimit (0.0f, 0.97f, (float) (double) b.getProperty ("reso", 0.2));
            bm.envAmt = juce::jlimit (0.0f, 1.0f, (float) (double) b.getProperty ("env", 0.4));
            bm.decay = juce::jlimit (0.03f, 2.0f, (float) (double) b.getProperty ("decay", 0.3));
            bm.sustain = juce::jlimit (0.0f, 1.0f, (float) (double) b.getProperty ("sustain", 0.7));
            bm.glide = juce::jlimit (0.0f, 400.0f, (float) (double) b.getProperty ("glide", 0.0));
            bm.drive = juce::jlimit (0.0f, 1.0f, (float) (double) b.getProperty ("drive", 0.2));
            const auto cells = juce::StringArray::fromTokens (b["steps"].toString(), ",", "");
            for (int k = 0; k < juce::jmin (cells.size(), (int) BassMachine::maxSteps); ++k)
            {
                const auto c = (juce::uint32) juce::jlimit<juce::int64> (0, 0x3ffff, cells[k].getLargeIntValue());
                const bool note = BassMachine::isNote (c);
                bm.steps[k] = note ? BassMachine::pack (BassMachine::degreeOf (c), BassMachine::lenOf (c), BassMachine::slideOf (c), BassMachine::accentOf (c)) : 0u;
            }
            const auto sh = juce::StringArray::fromTokens (b["shift"].toString(), ",", "");
            for (int k = 0; k < juce::jmin (sh.size(), (int) BassMachine::maxBars); ++k) bm.shift[k] = juce::jlimit (-24, 24, sh[k].getIntValue());
        }
    }
    if (count == 0) e.addTrack();
    return true;
}
}
