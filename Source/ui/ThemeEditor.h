#pragma once
#include "ThemeStore.h"

// Dialog content: pick Primary and Accent, choose dark or light, everything else is derived.
// The app updates live through onPreview; Save stores the theme, Cancel restores the previous one.
class ThemeEditor : public juce::Component, private juce::ChangeListener
{
public:
    std::function<void (const Theme::Preset&)> onPreview;
    std::function<void (const Theme::Preset&)> onSave;
    std::function<void()> onCancel;
    std::function<void()> onDiscard;     // called when the dialog closes without saving (Cancel, Esc or the X button)
    bool saved = false;
    ~ThemeEditor() override { if (! saved && onDiscard) onDiscard(); }

    ThemeEditor (juce::Colour primary, juce::Colour accent, bool dark)
        : primarySel (flags), accentSel (flags)
    {
        nameEd.setText ("My theme");
        nameEd.setSelectAllWhenFocused (true);
        addAndMakeVisible (nameEd);
        darkBtn.setToggleState (dark, juce::dontSendNotification);
        darkBtn.setRadioGroupId (1); lightBtn.setRadioGroupId (1);
        lightBtn.setToggleState (! dark, juce::dontSendNotification);
        for (auto* b : { &darkBtn, &lightBtn })
        {
            addAndMakeVisible (b);
            b->onClick = [this] { changed(); };
        }
        primarySel.setCurrentColour (primary, juce::dontSendNotification);
        accentSel.setCurrentColour (accent, juce::dontSendNotification);
        primarySel.addChangeListener (this);
        accentSel.addChangeListener (this);
        addAndMakeVisible (primarySel);
        addAndMakeVisible (accentSel);
        saveBtn.onClick   = [this] { if (nameEd.getText().trim().isNotEmpty() && onSave) { saved = true; onSave (current()); } };
        cancelBtn.onClick = [this] { if (onCancel) onCancel(); };
        addAndMakeVisible (saveBtn);
        addAndMakeVisible (cancelBtn);
        setSize (680, 470);
    }

    Theme::Preset current() const
    {
        return Theme::generate (nameEd.getText().trim(), primarySel.getCurrentColour().withAlpha (1.0f),
                                accentSel.getCurrentColour().withAlpha (1.0f), darkBtn.getToggleState());
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Theme::bg);
        g.setColour (Theme::text);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText ("Primary", primarySel.getX(), 40, 300, 18, juce::Justification::left);
        g.drawText ("Accent", accentSel.getX(), 40, 300, 18, juce::Justification::left);
        g.setColour (Theme::dim);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText ("Secondary, background, surface and text are derived from the primary colour.",
                    10, getHeight() - 70, getWidth() - 20, 16, juce::Justification::left);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        auto top = r.removeFromTop (26);
        nameEd.setBounds (top.removeFromLeft (200));
        top.removeFromLeft (16);
        darkBtn.setBounds (top.removeFromLeft (70));
        lightBtn.setBounds (top.removeFromLeft (70));
        r.removeFromTop (22);
        auto left = r.removeFromLeft (r.getWidth() / 2 - 5);
        r.removeFromLeft (10);
        primarySel.setBounds (left.removeFromTop (330));
        accentSel.setBounds (r.removeFromTop (330));
        auto bottom = getLocalBounds().reduced (10).removeFromBottom (28);
        cancelBtn.setBounds (bottom.removeFromRight (90));
        bottom.removeFromRight (8);
        saveBtn.setBounds (bottom.removeFromRight (90));
    }

private:
    static constexpr int flags = juce::ColourSelector::showSliders | juce::ColourSelector::showColourspace | juce::ColourSelector::editableColour;
    void changeListenerCallback (juce::ChangeBroadcaster*) override { changed(); }
    void changed() { if (onPreview) onPreview (current()); }

    juce::ColourSelector primarySel, accentSel;
    juce::TextEditor nameEd;
    juce::ToggleButton darkBtn { "Dark" }, lightBtn { "Light" };
    juce::TextButton saveBtn { "Save" }, cancelBtn { "Cancel" };
};
