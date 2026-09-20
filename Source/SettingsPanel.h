#pragma once

#include "AppConfig.h"
#include <juce_gui_extra/juce_gui_extra.h>

namespace defeedback
{
// A staged settings editor. No routing or plugin mutation occurs until Apply.
class SettingsPanel final : public juce::Component
{
public:
    using Apply = std::function<juce::String (const AppConfig&)>;

    SettingsPanel (AppConfig current, juce::StringArray inputs, juce::StringArray outputs, Apply apply)
        : draft (std::move (current)), inputNames (std::move (inputs)),
          outputNames (std::move (outputs)), onApply (std::move (apply))
    {
        modeLabel.setText ("PLUGIN FOR ALL LANES", juce::dontSendNotification);
        populatePlugins (allPlugins);
        allPlugins.setSelectedId (static_cast<int> (draft.pluginMode) + 1, juce::dontSendNotification);
        applyAllButton.onClick = [this]
        {
            commitPatches();
            draft.switchPluginMode (static_cast<PluginMode> (allPlugins.getSelectedId() - 1));
            rebuildPatches();
        };
        info.setText ("Choose a plugin for each lane, or apply one plugin to all lanes. Each lane remembers its plugin settings.\n"
                      "Applying plugin or patch changes stops the engine and mutes all outputs.", juce::dontSendNotification);
        info.setFont (juce::FontOptions (13.0f));
        header.setText ("LANE", juce::dontSendNotification);
        pluginHeader.setText ("PLUGIN", juce::dontSendNotification);
        inputHeader.setText ("INPUT PATCH", juce::dontSendNotification);
        outputHeader.setText ("OUTPUT PATCH", juce::dontSendNotification);
        viewport.setViewedComponent (&patches, false);
        viewport.setScrollBarsShown (true, false);
        applyButton.onClick = [this]
        {
            commitPatches();
            const auto validation = validateExclusiveRoutes (draft.lanes);
            const auto result = validation.isNotEmpty() ? validation : onApply (draft);
            if (result.isEmpty()) close();
            else error.setText (result, juce::dontSendNotification);
        };
        cancelButton.onClick = [this] { close(); };
        error.setColour (juce::Label::textColourId, juce::Colour (0xffffb454));
        for (auto* component : std::initializer_list<juce::Component*> {
                 &modeLabel, &allPlugins, &applyAllButton, &info, &header, &pluginHeader, &inputHeader,
                 &outputHeader, &viewport, &applyButton, &cancelButton, &error })
            addAndMakeVisible (component);
        rebuildPatches();
        setSize (900, 570);
    }

    ~SettingsPanel() override { viewport.setViewedComponent (nullptr, false); }

    void resized() override
    {
        auto area = getLocalBounds().reduced (18);
        modeLabel.setBounds (area.removeFromTop (22));
        auto modes = area.removeFromTop (34);
        allPlugins.setBounds (modes.removeFromLeft (260));
        modes.removeFromLeft (8);
        applyAllButton.setBounds (modes.removeFromLeft (180));
        area.removeFromTop (8);
        info.setBounds (area.removeFromTop (48));
        auto buttons = area.removeFromBottom (36);
        applyButton.setBounds (buttons.removeFromRight (180));
        buttons.removeFromRight (8);
        cancelButton.setBounds (buttons.removeFromRight (100));
        error.setBounds (area.removeFromBottom (56));
        auto headers = area.removeFromTop (26);
        header.setBounds (headers.removeFromLeft (160));
        pluginHeader.setBounds (headers.removeFromLeft (210));
        inputHeader.setBounds (headers.removeFromLeft ((headers.getWidth() - 10) / 2));
        headers.removeFromLeft (10);
        outputHeader.setBounds (headers);
        viewport.setBounds (area);
        patches.setSize (viewport.getMaximumVisibleWidth(), juce::jmax (area.getHeight(), rows.size() * 44));
        for (int i = 0; i < rows.size(); ++i)
        {
            auto row = juce::Rectangle<int> (0, i * 44, patches.getWidth(), 38);
            rows[i]->label.setBounds (row.removeFromLeft (160));
            rows[i]->plugin.setBounds (row.removeFromLeft (200));
            row.removeFromLeft (10);
            rows[i]->input.setBounds (row.removeFromLeft ((row.getWidth() - 10) / 2));
            row.removeFromLeft (10);
            rows[i]->output.setBounds (row);
        }
    }

private:
    struct PatchRow { juce::Label label; juce::ComboBox plugin, input, output; };

    static void populatePlugins (juce::ComboBox& combo)
    {
        combo.addItem ("De-Feedback", 1);
        combo.addItem ("Clear Voice Live 1.1", 2);
        combo.addItem ("VoiceISO", 3);
    }

    void close()
    {
        if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
            window->exitModalState (0);
    }

    void commitPatches()
    {
        for (int i = 0; i < rows.size(); ++i)
        {
            draft.lanes.getReference (i).selectPlugin (static_cast<PluginMode> (rows[i]->plugin.getSelectedId() - 1));
            draft.lanes.getReference (i).inputChannel = rows[i]->input.getSelectedId() - 1;
            draft.lanes.getReference (i).outputChannel = rows[i]->output.getSelectedId() - 1;
        }
    }

    void rebuildPatches()
    {
        rows.clear();
        const auto populate = [] (juce::ComboBox& combo, const juce::StringArray& names, int selected)
        {
            for (int channel = 0; channel < juce::jmax (names.size(), selected + 1); ++channel)
                combo.addItem (juce::String (channel + 1) + " - "
                    + (channel < names.size() ? names[channel] : "Unavailable"), channel + 1);
            combo.setSelectedId (selected + 1, juce::dontSendNotification);
        };
        for (const auto& lane : draft.lanes)
        {
            auto* row = rows.add (new PatchRow());
            row->label.setText (juce::String (lane.id) + "  " + lane.name, juce::dontSendNotification);
            populatePlugins (row->plugin);
            row->plugin.setSelectedId (static_cast<int> (lane.pluginMode) + 1, juce::dontSendNotification);
            row->plugin.setTitle (lane.name + " plugin");
            patches.addAndMakeVisible (row->plugin);
            populate (row->input, inputNames, lane.inputChannel);
            populate (row->output, outputNames, lane.outputChannel);
            patches.addAndMakeVisible (row->label);
            patches.addAndMakeVisible (row->input);
            patches.addAndMakeVisible (row->output);
        }
        error.setText ("Clear Voice: 48 / 96 / 192 kHz. CPU and latency changes require a stopped engine.\n"
                       "VoiceISO: choose Full / Lite in the lane's UI on the Mac with the engine stopped.",
                       juce::dontSendNotification);
        resized();
    }

    AppConfig draft;
    juce::StringArray inputNames, outputNames;
    Apply onApply;
    juce::Label modeLabel, info, header, pluginHeader, inputHeader, outputHeader, error;
    juce::ComboBox allPlugins;
    juce::TextButton applyAllButton { "APPLY TO ALL LANES" };
    juce::Viewport viewport;
    juce::Component patches;
    juce::OwnedArray<PatchRow> rows;
    juce::TextButton applyButton { "APPLY SETTINGS" }, cancelButton { "CANCEL" };
};
}
