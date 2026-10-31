#pragma once
#include <juce_audio_formats/juce_audio_formats.h>

// Reads an audio file (WAV, AIFF, FLAC, MP3, OGG where available) as stereo float at the device rate.
namespace AudioImport
{
    inline bool isAudioFile (const juce::File& f)
    {
        return f.hasFileExtension ("wav;wave;aif;aiff;flac;mp3;ogg");
    }

    inline bool readFile (const juce::File& file, double deviceRate, juce::AudioBuffer<float>& out, double& fileRate, juce::String& error)
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (file));
        if (r == nullptr) { error = "Could not read \"" + file.getFileName() + "\" as audio"; return false; }
        fileRate = r->sampleRate;
        const double maxSeconds = 20.0 * 60.0;
        if (r->lengthInSamples <= 0 || (double) r->lengthInSamples / juce::jmax (1.0, r->sampleRate) > maxSeconds)
        {
            error = "The file is empty or longer than 20 minutes";
            return false;
        }
        const int len = (int) r->lengthInSamples;
        juce::AudioBuffer<float> buf (2, len);
        buf.clear();
        r->read (&buf, 0, len, 0, true, true);   // mono files are copied to both channels
        if (r->numChannels == 1) buf.copyFrom (1, 0, buf, 0, 0, len);

        if (deviceRate > 0.0 && std::abs (r->sampleRate - deviceRate) > 0.5)
        {
            const double ratio = r->sampleRate / deviceRate;
            const int newLen = (int) std::ceil (len / ratio);
            juce::AudioBuffer<float> rs (2, newLen);
            for (int ch = 0; ch < 2; ++ch)
            {
                juce::LagrangeInterpolator li;
                li.process (ratio, buf.getReadPointer (ch), rs.getWritePointer (ch), newLen);
            }
            buf = std::move (rs);
        }
        out = std::move (buf);
        return true;
    }
}