#pragma once
#include "Theme.h"

// User themes: one small JSON file per theme in %APPDATA%\RiffCatch\themes.
// They are appended to Theme::presets after the seven built-ins.
namespace ThemeStore
{
    inline juce::File dir()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("RiffCatch").getChildFile ("themes");
    }
    inline juce::File fileFor (const juce::String& name)
    {
        return dir().getChildFile (juce::File::createLegalFileName (name.trim()) + ".json");
    }

    inline juce::String hex (juce::uint32 c) { return juce::Colour (c).toDisplayString (false); }
    inline juce::uint32 parse (const juce::var& v, juce::uint32 fallback)
    {
        const auto s = v.toString();
        return s.isEmpty() ? fallback : (juce::uint32) (0xff000000u | (juce::uint32) s.getHexValue32());
    }

    inline bool save (const Theme::Preset& p)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("name", p.name);
        o->setProperty ("primary", hex (p.primary));      o->setProperty ("secondary", hex (p.secondary));
        o->setProperty ("accent", hex (p.accent));        o->setProperty ("background", hex (p.background));
        o->setProperty ("surface", hex (p.surface));      o->setProperty ("text", hex (p.text));
        dir().createDirectory();
        return fileFor (p.name).replaceWithText (juce::JSON::toString (juce::var (o)));
    }

    // Reloads Theme::presets so that it holds the built-ins followed by every theme found on disk.
    inline void reload()
    {
        if ((int) Theme::presets.size() > Theme::builtinCount) Theme::presets.resize ((size_t) Theme::builtinCount);
        auto files = dir().findChildFiles (juce::File::findFiles, false, "*.json");
        files.sort();
        for (auto& f : files)
        {
            const auto v = juce::JSON::parse (f);
            auto* o = v.getDynamicObject();
            if (o == nullptr) continue;
            const auto name = o->getProperty ("name").toString();
            if (name.isEmpty()) continue;
            // A user file with a built-in name (for example an older installed
            // Neo-Retro) must not duplicate the built-in entry.
            bool builtin = false;
            for (int i = 0; i < Theme::builtinCount; ++i)
                if (Theme::presets[(size_t) i].name == name) { builtin = true; break; }
            if (builtin) continue;
            Theme::presets.push_back ({ name,
                parse (o->getProperty ("primary"), 0xff7aa2f7),   parse (o->getProperty ("secondary"), 0xffbb9af7),
                parse (o->getProperty ("accent"), 0xff7dcfff),    parse (o->getProperty ("background"), 0xff1a1b26),
                parse (o->getProperty ("surface"), 0xff1f2133),   parse (o->getProperty ("text"), 0xffc0caf5) });
        }
    }

    inline bool remove (int index)
    {
        if (index < Theme::builtinCount || index >= Theme::numPresets()) return false;
        const bool ok = fileFor (Theme::presets[(size_t) index].name).deleteFile();
        reload();
        return ok;
    }

    inline int indexOf (const juce::String& name)
    {
        for (int i = 0; i < Theme::numPresets(); ++i) if (Theme::presets[(size_t) i].name == name) return i;
        return -1;
    }
}
