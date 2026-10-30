#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Windows only: make the native title bar match the app (dark mode + caption colours on Windows 11).
void applyDarkTitleBar (juce::ComponentPeer* peer, juce::Colour caption, juce::Colour text);
