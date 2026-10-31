#pragma once
#include <juce_core/juce_core.h>
#if JUCE_WINDOWS
 // not <shlobj.h>: it drags in the Windows macros (near, far, ...) that clash with other code
 extern "C" __declspec (dllimport) void __stdcall SHChangeNotify (long eventId, unsigned flags, const void* item1, const void* item2);
#endif

// Registers ".riffcatch" for the current Windows user (HKCU, no admin rights needed), so a double-click in Explorer opens the project.
namespace FileAssoc
{
    inline juce::String command()
    {
        return "\"" + juce::File::getSpecialLocation (juce::File::currentExecutableFile).getFullPathName() + "\" \"%1\"";
    }

    inline bool isRegistered()
    {
       #if JUCE_WINDOWS
        return juce::WindowsRegistry::getValue ("HKEY_CURRENT_USER\\Software\\Classes\\.riffcatch\\") == "RiffCatch.Project"
            && juce::WindowsRegistry::getValue ("HKEY_CURRENT_USER\\Software\\Classes\\RiffCatch.Project\\shell\\open\\command\\") == command();
       #else
        return true;
       #endif
    }

    inline bool registerForUser()
    {
       #if JUCE_WINDOWS
        using R = juce::WindowsRegistry;
        const auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getFullPathName();
        bool ok = true;
        ok &= R::setValue ("HKEY_CURRENT_USER\\Software\\Classes\\.riffcatch\\", "RiffCatch.Project");
        ok &= R::setValue ("HKEY_CURRENT_USER\\Software\\Classes\\RiffCatch.Project\\", "RiffCatch project");
        ok &= R::setValue ("HKEY_CURRENT_USER\\Software\\Classes\\RiffCatch.Project\\DefaultIcon\\", "\"" + exe + "\",0");
        ok &= R::setValue ("HKEY_CURRENT_USER\\Software\\Classes\\RiffCatch.Project\\shell\\open\\command\\", command());
        if (ok) SHChangeNotify (0x08000000L /* SHCNE_ASSOCCHANGED */, 0 /* SHCNF_IDLIST */, nullptr, nullptr);
        return ok;
       #else
        return false;
       #endif
    }
}
