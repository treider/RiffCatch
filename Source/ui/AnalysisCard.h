#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "audio/BassSuggest.h"
#include "Theme.h"

// Result of "Analyze riff": tempo, key / scale / mode, a chord per bar, and ready-made bass lines to try.
class AnalysisCard : public juce::Component
{
public:
    struct Setup
    {
        Analysis::Result result;
        juce::String trackName;
        double projectBpm = 120.0;
        bool stopped = true;
        int barSteps = 16, beat = 4;
        DrumMachine::Snapshot drums;
        bool haveDrums = false;
    };
    std::function<void (double)> onSetTempo;
    std::function<void (const juce::String&, const BassMachine::Snapshot&)> onUse;

    explicit AnalysisCard (Setup s) : setup (std::move (s))
    {
        title.setText ("Riff analysis: " + setup.trackName, juce::dontSendNotification);
        title.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
        addAndMakeVisible (title);

        // The tempo is only worth a line when the riff clearly does not fit the project tempo (the bass follows the project grid).
        foldedTempo = Analysis::foldNear (setup.result.tempo, setup.projectBpm);
        showTempo = setup.result.tempo > 0.0 && setup.result.tempoConf >= 0.25f && std::abs (foldedTempo - setup.projectBpm) / setup.projectBpm > 0.05;
        tempoLabel.setText ("Heads up: the riff sounds like " + juce::String (foldedTempo, 1) + " BPM, the project is " + juce::String (setup.projectBpm, 1), juce::dontSendNotification);
        tempoLabel.setColour (juce::Label::textColourId, Theme::yellow);
        tempoLabel.setVisible (showTempo);
        addChildComponent (tempoLabel);
        setTempoBtn.setButtonText ("Set tempo");
        setTempoBtn.setTooltip ("Use the detected tempo for the project (only while stopped)");
        setTempoBtn.onClick = [this] { if (onSetTempo) onSetTempo (foldedTempo); };
        setTempoBtn.setEnabled (setup.stopped);
        setTempoBtn.setVisible (showTempo);
        addChildComponent (setTempoBtn);

        keyLabel.setText ("Key:", juce::dontSendNotification);
        addAndMakeVisible (keyLabel);
        const int nk = juce::jmin (6, (int) setup.result.keys.size());
        const float top = nk > 0 ? setup.result.keys[0].score : 1.0f;
        for (int i = 0; i < nk; ++i)
            keyBox.addItem (Analysis::keyName (setup.result.keys[(size_t) i]) + "  (" + juce::String ((int) std::lround (100.0f * setup.result.keys[(size_t) i].score / juce::jmax (0.001f, top))) + "%)", i + 1);
        keyBox.setSelectedId (1, juce::dontSendNotification);
        keyBox.setTooltip ("Best matches first. Pick another one if the first guess is wrong - the chords and bass lines follow");
        keyBox.onChange = [this] { rebuild(); };
        addAndMakeVisible (keyBox);

        chordLabel.setJustificationType (juce::Justification::topLeft);
        chordLabel.setMinimumHorizontalScale (1.0f);
        addAndMakeVisible (chordLabel);

        hint.setText ("Use puts the line in the Bass tab (undo with Ctrl+Z). Try another to replace it.", juce::dontSendNotification);
        hint.setColour (juce::Label::textColourId, Theme::dim);
        hint.setFont (juce::Font (juce::FontOptions (12.0f)));
        addAndMakeVisible (hint);

        rebuild();
        setSize (540, 110 + (showTempo ? 30 : 0) + rowCount * 30 + 40);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (14, 10);
        title.setBounds (r.removeFromTop (24)); r.removeFromTop (6);
        if (showTempo)
        {
            auto t = r.removeFromTop (26);
            setTempoBtn.setBounds (t.removeFromRight (90));
            tempoLabel.setBounds (t); r.removeFromTop (4);
        }
        auto k = r.removeFromTop (26);
        keyLabel.setBounds (k.removeFromLeft (40));
        keyBox.setBounds (k); r.removeFromTop (6);
        chordLabel.setBounds (r.removeFromTop (44)); r.removeFromTop (4);
        for (auto& row : rows)
        {
            auto line = r.removeFromTop (28); r.removeFromTop (2);
            row.use->setBounds (line.removeFromRight (70));
            row.name->setBounds (line.reduced (4, 0));
        }
        r.removeFromTop (4);
        hint.setBounds (r.removeFromTop (20));
    }

    void paint (juce::Graphics& g) override { g.fillAll (Theme::panel); }

private:
    struct Row { std::unique_ptr<juce::Label> name; std::unique_ptr<juce::TextButton> use; };
    using Suggestion = BassSuggest::Suggestion;

    void rebuild()
    {
        rows.clear();
        const int ki = juce::jmax (0, keyBox.getSelectedId() - 1);
        if (setup.result.keys.empty()) return;
        const auto key = setup.result.keys[(size_t) ki];
        chords = Analysis::detectChords (setup.result, key);
        juce::String ct = "Chords (bar " + juce::String (setup.result.bar0 + 1) + " on):  ";
        for (size_t i = 0; i < chords.size(); ++i) ct << (i > 0 ? "  |  " : "") << Analysis::chordName (chords[i]);
        chordLabel.setText (ct, juce::dontSendNotification);
        suggestions = BassSuggest::suggest (setup.result, key, chords, setup.barSteps, setup.beat, setup.haveDrums ? &setup.drums : nullptr);
        for (size_t i = 0; i < suggestions.size(); ++i)
        {
            Row row;
            row.name.reset (new juce::Label ({}, juce::String ((int) i + 1) + ".  " + suggestions[i].name));
            row.use.reset (new juce::TextButton ("Use"));
            row.use->onClick = [this, i] { if (onUse && i < suggestions.size()) onUse (suggestions[i].name, suggestions[i].snap); };
            addAndMakeVisible (*row.name); addAndMakeVisible (*row.use);
            rows.push_back (std::move (row));
        }
        rowCount = (int) rows.size();
        resized();
        repaint();
    }

    Setup setup;
    juce::Label title, tempoLabel, keyLabel, chordLabel, hint;
    juce::TextButton setTempoBtn;
    juce::ComboBox keyBox;
    std::vector<Row> rows;
    std::vector<Analysis::Chord> chords;
    std::vector<Suggestion> suggestions;
    double foldedTempo = 0.0;
    bool showTempo = false;
    int rowCount = 6;
};
