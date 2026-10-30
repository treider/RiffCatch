#include "WinTitleBar.h"

#if JUCE_WINDOWS
 #define NOMINMAX
 #include <windows.h>
 #include <dwmapi.h>
 #pragma comment (lib, "dwmapi.lib")

 static COLORREF toColorRef (juce::Colour c) { return RGB (c.getRed(), c.getGreen(), c.getBlue()); }
#endif

void applyDarkTitleBar (juce::ComponentPeer* peer, juce::Colour caption, juce::Colour text)
{
#if JUCE_WINDOWS
    if (peer == nullptr) return;
    auto hwnd = (HWND) peer->getNativeHandle();
    const BOOL dark = TRUE;
    DwmSetWindowAttribute (hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &dark, sizeof (dark));
    const COLORREF cap = toColorRef (caption), txt = toColorRef (text);
    DwmSetWindowAttribute (hwnd, 34 /*DWMWA_BORDER_COLOR*/, &cap, sizeof (cap));      // Windows 11 only; ignored elsewhere
    DwmSetWindowAttribute (hwnd, 35 /*DWMWA_CAPTION_COLOR*/, &cap, sizeof (cap));
    DwmSetWindowAttribute (hwnd, 36 /*DWMWA_TEXT_COLOR*/, &txt, sizeof (txt));
#else
    juce::ignoreUnused (peer, caption, text);
#endif
}
